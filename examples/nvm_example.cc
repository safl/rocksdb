#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "rocksdb/c.h"

#include <unistd.h>

using namespace std;

const char DBPath[] = "rocksdb";

int main(int argc, char **argv) {
  if (argc != 4) {
    if (argc == 3 && strcmp(argv[1], "read") == 0) {
      goto proceed;
    } else if (argc == 2 && strcmp(argv[1], "fill") == 0) {
      goto proceed;
    }

    cout << "[write/read/fill key value]" << endl << flush;
    return 0;
  }

proceed:

  const char *key = argv[2];
  const char *value = argv[3];

  char *err = NULL;
  char *returned_value;

  size_t len;

  rocksdb_t *db;
  rocksdb_options_t *options;
  rocksdb_writeoptions_t *writeoptions;
  rocksdb_readoptions_t *readoptions;

  long cpus = sysconf(_SC_NPROCESSORS_ONLN);

  options = rocksdb_options_create();

  rocksdb_options_increase_parallelism(options, (int)(cpus));
  rocksdb_options_optimize_level_style_compaction(options, 0);
  rocksdb_options_set_create_if_missing(options, 1);

  db = rocksdb_open(options, DBPath, &err);
  if(err) {
    cout << "OPEN ERROR: " << err << endl << flush;
    return EXIT_FAILURE;
  }

  cout << "Database is open\n" << flush;

  if(strcmp(argv[1], "write") == 0) {
    writeoptions = rocksdb_writeoptions_create();

    cout << "PUT DATA " << key << ":" << value << endl << flush;

    rocksdb_put(db, writeoptions, key, strlen(key), value, strlen(value) + 1,
                                                                          &err);
    if(err) {
      cout << "WRITE ERROR: " << err << endl << flush;
      return EXIT_FAILURE;
    }

    rocksdb_writeoptions_destroy(writeoptions);
  } else if(strcmp(argv[1], "read") == 0) {
    readoptions = rocksdb_readoptions_create();

    returned_value = rocksdb_get(db, readoptions, key, strlen(key), &len, &err);
    if(err) {
      cout << "GET ERROR: " << err << endl << flush;
      return EXIT_FAILURE;
    }

    cout << "GOT DATA " << (returned_value ? returned_value : "null") << endl << flush;
    free(returned_value);
    rocksdb_readoptions_destroy(readoptions);
  } else if(strcmp(argv[1], "fill") == 0) {
    writeoptions = rocksdb_writeoptions_create();

    for(unsigned long i = 0; i < 1000; ++i) {
      char k_data[20];
      char v_data[20];

      sprintf(k_data, "%lu", i);
      sprintf(v_data, "%lu", i);

      rocksdb_put(db, writeoptions, k_data, strlen(k_data), v_data,
                                                    strlen(v_data) + 1, &err);
    }

    rocksdb_writeoptions_destroy(writeoptions);
  }

  rocksdb_garbage_collect(db, &err);

  if(err) {
    cout << "GARBAGE COLLECTION FAILED " << err << endl << flush;
    return EXIT_FAILURE;
  }

  rocksdb_options_destroy(options);
  rocksdb_close(db);

  cout << "DONE\n" << flush;

  return EXIT_SUCCESS;
}
