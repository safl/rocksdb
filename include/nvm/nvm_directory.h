#ifndef _NVM_DIRECTORY_H_
#define _NVM_DIRECTORY_H_

namespace rocksdb {

class nvm_file;

class nvm_directory {
  private:
    char *name;

    //list of nvm_entries (nvm_file or nvm_directory)
    list_node *head;
    nvm *nvm_api;

    pthread_mutex_t list_update_mtx;
    pthread_mutexattr_t list_update_mtx_attr;

    void *create_node(const char *name, const nvm_entry_type type);
    nvm_directory *parent;

  public:
    nvm_directory(const char *_name, const int n, nvm *_nvm_api, nvm_directory *_parent);
    ~nvm_directory();

    char *GetName();
    bool HasName(const char *_name, const int n);
    void EnumerateNames(std::vector<std::string>* result);
    void Delete(nvm *_nvm_api);

    list_node *node_look_up(list_node *prev, const char *look_up_name);
    list_node *node_look_up(const char *look_up_name, const nvm_entry_type type);
    nvm_directory *directory_look_up(const char *directory_name);
    nvm_file *file_look_up(const char *filename);

    nvm_file *open_file_if_exists(const char *filename);
    nvm_directory *OpenParentDirectory(const char *filename);
    nvm_directory *OpenDirectory(const char *name);
    nvm_directory *GetParent();
    nvm_file *nvm_fopen(const char *filename, const char *mode);
    nvm_file *create_file(const char *filename);

    nvm *GetNVMApi();

    int CreateDirectory(const char *name);
    int DeleteDirectory(const char *name);
    int GetFileSize(const char *filename, unsigned long *size);
    int GetFileModificationTime(const char *filename, time_t *mtime);
    int LinkFile(const char *src, const char *target);
    int RenameFile(const char *crt_filename, const char *new_filename);
    int RenameDirectory(const char *crt_filename, const char *new_filename);
    int DeleteFile(const char *filename);
    int GetChildren(const char *name, std::vector<std::string>* result);

    void GetChildren(std::vector<std::string>* result);

    bool FileExists(const char *name);

    void Remove(nvm_file *fd);
    void Add(nvm_file *fd);
    void Remove(nvm_directory *fd);
    void Add(nvm_directory *fd);
    void ChangeName(const char *_name, const int n);

    void nvm_fclose(nvm_file *file, const char *mode);

    Status Save(const int fd);
    Status Load(const int fd);
};

class NVMDirectory : public Directory {
  private:
    nvm_directory *fd_;

  public:
    explicit NVMDirectory(nvm_directory *fd);
    ~NVMDirectory();

    virtual Status Fsync() override;
};

} //rocksdb namespace

#endif //_NVM_DIRECTORY_H_
