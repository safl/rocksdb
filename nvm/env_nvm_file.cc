#include <exception>
#include <iostream>
#include <fstream>
#include <csignal>

#include "util/coding.h"
#include "env_nvm.h"
#include <liblightnvm.h>

int count = 0;

namespace rocksdb {

NvmFile::NvmFile(
  EnvNVM* env, const FPathInfo& info
) :
  env_(env), refs_(), info_(info), fsize_(),
  dev_name_("unknown"), dev_(), geo_(), wretry_(20),
  vpage_nbytes_(), vblock_nbytes_(), vblock_nvpages_(), vblocks_(),
  buf_nbytes_(), buf_nvpages_(), buf_nflushed_(0), buffers_() {
  NVM_DBG(this, "");

  dev_name_ = env_->GetDevName();
  dev_ = nvm_dev_open(dev_name_.c_str());
  if (!dev_) {
    NVM_DBG(this, "failed opening(" << dev_name_ << ")");
    throw std::runtime_error("Failed opening nvm device.");
  }
  geo_ = nvm_dev_attr_geo(dev_);

  vpage_nbytes_ = nvm_dev_attr_vpage_nbytes(dev_);
  vblock_nbytes_ = nvm_dev_attr_vblock_nbytes(dev_);
  vblock_nvpages_ = vblock_nbytes_ / vpage_nbytes_;

  buf_nbytes_ = vpage_nbytes_;
  buf_nvpages_ = buf_nbytes_ / vpage_nbytes_;

  NVM_DBG(this, "vpage_nbytes_(" << vpage_nbytes_ << ") ");
  NVM_DBG(this, "vblock_nbytes_(" << vblock_nbytes_ << ") ");
  NVM_DBG(this, "vblock_nvpages_(" << vblock_nvpages_ << ") ");
}

NvmFile::NvmFile(
  EnvNVM* env, const FPathInfo& info, const std::string mpath
) :
  env_(env), refs_(), info_(info), fsize_(),
  dev_name_("unknown"), dev_(), geo_(), wretry_(20),
  vpage_nbytes_(), vblock_nbytes_(), vblock_nvpages_(), vblocks_(),
  buf_nbytes_(), buf_nvpages_(), buf_nflushed_(0), buffers_() {
  NVM_DBG(this, "");

  // Read .meta

  dev_ = nvm_dev_open(dev_name_.c_str());
  if (!dev_)
    throw std::runtime_error("Failed opening nvm device.");
  geo_ = nvm_dev_attr_geo(dev_);

  vpage_nbytes_ = nvm_dev_attr_vpage_nbytes(dev_);
  vblock_nbytes_ = nvm_dev_attr_vblock_nbytes(dev_);
  vblock_nvpages_ = vblock_nbytes_ / vpage_nbytes_;

  buf_nbytes_ = vpage_nbytes_;
  buf_nvpages_ = buf_nbytes_ / vpage_nbytes_;

  NVM_DBG(this, "vpage_nbytes_(" << vpage_nbytes_ << ") ");
  NVM_DBG(this, "vblock_nbytes_(" << vblock_nbytes_ << ") ");
  NVM_DBG(this, "vblock_nvpages_(" << vblock_nvpages_ << ") ");
}

NvmFile::~NvmFile(void) {
  NVM_DBG(this, "");

  for (auto &buf : buffers_) {
    if (buf) {
      free(buf);
    }
  }

  for (auto &blk : vblocks_) {
    if (blk) {
      nvm_vblock_free(&blk);
    }
  }

  nvm_dev_close(dev_);
}

bool NvmFile::IsNamed(const std::string& fname) const {
  NVM_DBG(this, "fname(" << fname << ")");

  return !info_.fname().compare(fname);
}

const std::string& NvmFile::GetFname(void) const {
  NVM_DBG(this, "return(" << info_.fname() << ")");

  return info_.fname();
}

const std::string& NvmFile::GetDpath(void) const {
  NVM_DBG(this, "return(" << info_.dpath() << ")");

  return info_.dpath();
}

void NvmFile::Rename(const std::string& fname) {
  NVM_DBG(this, "fname(" << fname << ")");

  info_.fname(fname);
}

size_t NvmFile::GetRequiredBufferAlignment(void) const {
  NVM_DBG(this, "returning(" << vpage_nbytes_ << ")");

  return vpage_nbytes_;
}

void NvmFile::Ref(void) {
  MutexLock lock(&refs_mutex_);

  NVM_DBG(this, "refs_(" << refs_ << ")");

  ++refs_;

  NVM_DBG(this, "refs_(" << refs_ << ")");
}

void NvmFile::Unref(void) {
  NVM_DBG(this, "");

  bool do_delete = false;

  {
    MutexLock lock(&refs_mutex_);
    --refs_;
    if (refs_ < 0) {
      do_delete = true;
    }
  }

  if (do_delete) {
    NVM_DBG(this, "deleting!");
    delete this;
  }
}

std::string NvmFile::txt(void) const {
  std::stringstream ss;
  ss << "fname(" << info_.fname() << ") ";
  return ss.str();
}

// Used by WritableFile
bool NvmFile::UseDirectIO(void) const {
  NVM_DBG(this, "hard-coded return(true)");

  return true;
}

// Used by WritableFile
bool NvmFile::UseOSBuffer(void) const {
  NVM_DBG(this, "hard-coded return(false)");

  return false;
}

// Used by WritableFile
bool NvmFile::IsSyncThreadSafe(void) const {
  NVM_DBG(this, "hard-coded return(false)");

  return false;
}

//
// Implemented using the address of the NvmFile instance. This seem to satisfy
// the requirements: it lives as long as need be, is unique and probably won't
// have the same address after termination.
//
// Although... depending on what is meant by prefix... they probably could be.
//
//
// Used by RandomAccessFile, WritableFile
size_t NvmFile::GetUniqueId(char* id, size_t max_size) const {
  NVM_DBG(this, "");

  if (max_size < (kMaxVarint64Length*3)) {
    return 0;
  }

  char* rid = id;
  rid = EncodeVarint64(rid, (uint64_t)this);
  rid = EncodeVarint64(rid, (uint64_t)this);
  rid = EncodeVarint64(rid, (uint64_t)this);
  assert(rid >= id);

  return static_cast<size_t>(rid - id);
}

// Used by WritableFile
uint64_t NvmFile::GetFileSize(void) const {
  NVM_DBG(this, "return(" << fsize_ << ")");

  return fsize_;
}

// Used by WritableFile
void NvmFile::PrepareWrite(size_t offset, size_t len) {
  NVM_DBG(this, "offset(" << offset << "), len(" << len << ") ignoring...");
}

// Used by WritableFile
Status NvmFile::Allocate(uint64_t offset, uint64_t len) {
  NVM_DBG(this, "offset(" << offset << "), len(" << len << ") ignoring");

  return Status::OK();
}

// Used by WritableFile
Status NvmFile::Append(const Slice& data) {
  NVM_DBG(this, "forwarding");

  return PositionedAppend(data, GetFileSize());
}

// Used by WritableFile
Status NvmFile::Sync(void) {
  NVM_DBG(this, "writing file meta to default env...");

  return Flush();
}

// Used by WritableFile
Status NvmFile::Fsync(void) {
  NVM_DBG(this, "writing file meta to default env...");

  return Flush();
}

// Used by WritableFile
Status NvmFile::RangeSync(uint64_t offset, uint64_t nbytes) {
  NVM_DBG(
    this,
    "offset(" << offset << "), nbytes(" << nbytes << ")"
  );

  return Flush();
}

// Used by WritableFile
Status NvmFile::Close(void) {
  NVM_DBG(this, "ignoring...");

  return Status::OK();
}

// Deletes any buffers covering the range [offset; offset+length].
//
// Used by SequentialFile, RandomAccessFile, WritableFile
Status NvmFile::InvalidateCache(size_t offset, size_t length) {

  size_t first = offset / vpage_nbytes_;
  size_t count = length / vpage_nbytes_;

  for (size_t idx = first; idx < (first+count); ++idx) {
    if (buffers_[idx]) {
      free(buffers_[idx]);
      buffers_[idx] = NULL;
    }
  }

  return Status::OK();
}

// Used by WritableFile
Status NvmFile::PositionedAppend(const Slice& slice, uint64_t offset) {
  NVM_DBG(this, "offset(" << offset << "), slice-size(" << slice.size() << ")");

  const char* data = slice.data();
  const size_t data_nbytes = slice.size();

  size_t nbufs = (offset + data_nbytes) / vpage_nbytes_;
  for (size_t i = buffers_.size(); i < nbufs; ++i) {
    buffers_.push_back((char*)nvm_vpage_buf_alloc(nvm_dev_attr_geo(dev_)));
  }

  if (offset < fsize_) {
    std::cout << "writing: offset(" << offset << "),  < fsize_(" << fsize_ << ")" << std::endl;
  }

  // Translate offset to buffer and offset within buffer
  size_t buf_idx = offset / vpage_nbytes_;
  size_t buf_offset = offset % vpage_nbytes_;

  std::cout << "buf_idx(" << buf_idx << "), buf_nflushed_(" << buf_nflushed_ << ")" << std::endl;
  std::cout << "buf_offset(" << buf_offset << ")" << std::endl;
  if (buf_idx <= buf_nflushed_) {
    std::cout << "THIS IS BAD, writing to buf that has already been flushed" << std::endl;
  }

  size_t nbytes_remaining = data_nbytes;
  size_t nbytes_written = 0;
  while(nbytes_remaining > 0) {
    size_t avail = vpage_nbytes_ - buf_offset;
    size_t nbytes = std::min(nbytes_remaining, avail);

    if (!buffers_[buf_idx]) {
      buffers_[buf_idx] = (char*)nvm_vpage_buf_alloc(nvm_dev_attr_geo(dev_));
    }
    memcpy(buffers_[buf_idx] + buf_offset, data + nbytes_written, nbytes);

    nbytes_remaining -= nbytes;
    nbytes_written += nbytes;
    ++buf_idx;
    buf_offset = 0;
  }

  fsize_ += nbytes_written;

  return Status::OK();
}

Status NvmFile::wmeta(void) {
  NVM_DBG(this, "");

  std::string mpath = info_.fpath() + kNvmMetaExt;

  return env_->wmeta(mpath, dev_name_, fsize_, vblocks_);
}

// Used by WritableFile
Status NvmFile::Flush(void) {
  NVM_DBG(this, "flushing to media...");

  size_t blks_needed = (buffers_.size() + vblock_nvpages_ -1) / vblock_nvpages_;
  NVM_DBG(this, "blks_needed(" << blks_needed << ")");

  // Ensure that have enough blocks reserved for flushing the file content
  for (size_t i = vblocks_.size(); i < blks_needed; ++i) {
    NVM_VBLOCK blk = env_->store_->get();
    if (!blk) {
      NVM_DBG(this, "failed allocating block i(" << i << ")");
      return Status::IOError("Failed reserving NVM (ENOMEM)");
    }

    vblocks_.push_back(blk);
  }

  // Write to media and free buffers
  for (size_t buf_idx = 0; buf_idx < buffers_.size(); ++buf_idx) {
    if (!buffers_[buf_idx])
      continue;

    size_t blk_idx = buf_idx / vblock_nvpages_;
    size_t blk_off = buf_idx % vblock_nvpages_;

    NVM_DBG(this, "buf_idx(" << buf_idx
            << "), blk_idx(" << blk_idx
            << "), blk_off(" << blk_off << ")");

    int nfail = 0;
    while(nfail < wretry_) {
      ssize_t err;
      err = nvm_vblock_pwrite(vblocks_[blk_idx], buffers_[buf_idx], blk_off);
      if (!err)
        break;

      ++nfail;
      NVM_DBG(this, "failed write, err(" << err << ")");
    }

    if (nfail == wretry_) {
      NVM_DBG(this, "failures (write) exceeded retry");
      return Status::IOError("failures (write) exceeded retry");
    }

    free(buffers_[buf_idx]);    // Remove buffered data
    buffers_[buf_idx] = NULL;

    buf_nflushed_ = buf_idx+1;
  }

  return wmeta();
}

// Used by WritableFile
Status NvmFile::Truncate(uint64_t size) {
  NVM_DBG(this, "fsize_(" << fsize_ << ")" <<
                ", size(" << size << ")" <<
                ", nvblocks(" << vblocks_.size() << ")");

  size_t bufs_required = (size + vpage_nbytes_ - 1) / vpage_nbytes_;
  size_t blks_required = (bufs_required + vblock_nvpages_ -1) / vblock_nvpages_;

  // De-allocate buffers that are no longer required
  for(size_t buf_idx = bufs_required; buf_idx < buffers_.size(); ++buf_idx) {
    NVM_DBG(this, "buf_idx(" << buf_idx << ")");
    free(buffers_[buf_idx]);
    buffers_[buf_idx] = NULL;
  }

  // Release blocks that are no longer required
  for(size_t blk_idx = blks_required; blk_idx < vblocks_.size(); ++blk_idx) {
    NVM_DBG(this, "blk_idx(" << blk_idx << ")");

    if (!vblocks_[blk_idx]) {
      NVM_DBG(this, "nothing here... skipping...");
      continue;
    }

    env_->store_->put(vblocks_[blk_idx]);
    vblocks_[blk_idx] = NULL;
  }

  fsize_ = size;

  return wmeta();
}

Status NvmFile::pad_last_block(void) {
  const size_t pad_blk_idx = fsize_ / vblock_nbytes_;
  const size_t pad_nbytes = vblock_nbytes_ - (fsize_ % vblock_nbytes_);
  const size_t pad_blk_offset = (vblock_nbytes_ - pad_nbytes) / vpage_nbytes_;

  NVM_DBG(this, "pad_nbytes(" << pad_nbytes << ")");
  NVM_DBG(this, "pad_blk_idx(" << pad_blk_idx << ")");
  NVM_DBG(this, "pad_blk_offset(" << pad_blk_offset << ")");

  if (!pad_nbytes)
    return Status::OK();

  if (!fsize_)
    return Status::OK();

  char *buf = (char*)nvm_vpage_buf_alloc(geo_);
  nvm_buf_fill(buf, vpage_nbytes_);

  for (size_t off = pad_blk_offset; off < vblock_nvpages_; ++off) {
    NVM_DBG(this, "pad blk_idx(" << pad_blk_idx << "), off(" << off << ")");

    ssize_t err = nvm_vblock_pwrite(vblocks_[pad_blk_idx], buf, off);

    if (err) {
      return Status::IOError("nvm_block_pwrite(...) failed when padding");
    }
  }

  return Status::OK();
}

Status NvmFile::fill_buffers(uint64_t offset, size_t n, char* scratch) {
  NVM_DBG(this, "offset(" << offset << "), n(" << n << ")");

  // TODO: only fill required buffers instead of all of them
  size_t first_buf_idx = 0;
  size_t bufs_required = (fsize_ + vpage_nbytes_ - 1) / vpage_nbytes_;

  NVM_DBG(this, "first_buf_idx(" << first_buf_idx << "), bufs_required(" << bufs_required << ")");

  // Make sure there are entries
  for (size_t i = buffers_.size(); i < bufs_required; ++i)
    buffers_.push_back(NULL);

  // Read vblock pages and fill buffers
  for (size_t buf_idx = first_buf_idx; buf_idx < bufs_required; ++buf_idx) {
    size_t blk_idx, blk_off, err;

    if (buffers_[buf_idx])
      continue;

    buffers_[buf_idx] = (char*)nvm_vpage_buf_alloc(nvm_dev_attr_geo(dev_));

    blk_idx = buf_idx / vblock_nvpages_;
    blk_off = buf_idx % vblock_nvpages_;

    err = nvm_vblock_pread(vblocks_[blk_idx], buffers_[buf_idx], blk_off);
    if (err) {
      NVM_DBG(this, "failed nvm_vblock_pread, "
              << "buf_idx(" << buf_idx << "), "
              << "blk_idx(" << blk_idx << "), "
              << "blk_off(" << blk_off << "), "
              << "err(" << err << ")");
      return Status::IOError("nvm_vblock_pread(...) failed.");
    }
  }

  return Status::OK();
}

// Assumes that data is available in buffers_.
// Used by SequentialFile, RandomAccessFile
Status NvmFile::Read(
  uint64_t offset, size_t n, Slice* result, char* scratch
) const {
  NVM_DBG(this, "offset(" << offset << "), n(" << n << "), fsize(" << fsize_ << ")");

  const uint64_t available = fsize_ - std::min(fsize_, offset);
  if (n > available) {
    n = available;
  }
  if (n == 0) {
    NVM_DBG(this, "no buffers...");
    *result = Slice();
    return Status::OK();
  }

  size_t buf = offset / vpage_nbytes_;
  size_t buf_offset = offset % vpage_nbytes_;

  if (n <= vpage_nbytes_ - buf_offset) {        // Available in a single buffer
    NVM_DBG(this, "single buffer...");
    *result = Slice(buffers_[buf] + buf_offset, n);
    return Status::OK();
  }

  NVM_DBG(this, "multi buffer...");
  size_t bytes_to_copy = n;                     // Multiple buffers
  char* dst = scratch;

  while (bytes_to_copy > 0) {
    size_t avail = vpage_nbytes_ - buf_offset;
    NVM_DBG(this, "buf(" << buf << "), offset(" << buf_offset << "), avail(" << avail << ")");
    if (avail > bytes_to_copy) {
      avail = bytes_to_copy;
    }
    memcpy(dst, buffers_[buf] + buf_offset, avail);

    bytes_to_copy -= avail;
    dst += avail;
    buf++;
    buf_offset = 0;
  }

  *result = Slice(scratch, n);
  return Status::OK();
}

}       // namespace rocksdb

