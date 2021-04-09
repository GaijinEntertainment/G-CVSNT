#pragma once
#include <stdio.h>
#include <string>
//this is link time dependencies, some implementation is in fileio.cpp
//if you use static library, you can override all of it in your app, or replace with your own fileio.cpp
static constexpr uint64_t invalid_blob_file_size = uint64_t(~uint64_t(0));
extern uint64_t blob_fileio_get_file_size(const char*);//to get blob file size for SIZE request, invalid_blob_file_size if missing
extern bool blob_fileio_is_file_readable(const char*);//to check if blob already exist (CHCK, PULL and PUSH requests)
//we store blobs in sub folders, so FS wont get mad on millions of files in one folder. We need to either create all 65536 folders initially (xx/yy) or create them on demand
// returns true if dir exist, or if what was created (if it was created, it will be created with 0777 provided)
extern bool blob_fileio_ensure_dir(const char* name);

//these three (rename, unlink, blob_get_temp_file) is only to work with temp files. Basically, we want to ensure that blobs are correct, so we atomically rename tmpfile to final blob
extern bool blob_fileio_rename_file(const char*from, const char*to);
extern void blob_fileio_unlink_file(const char*f);
extern int blob_fileio_get_last_error();

inline bool blob_fileio_rename_file_if_nexist(const char*from, const char*to)
{
  if (blob_fileio_is_file_readable(to))
  {
    blob_fileio_unlink_file(from);
    return true;
  }
  return blob_fileio_rename_file(from, to);
}
//with tmp_path = nullptr /tmp will be used on non-Windows, on windows we will use default temp dir
extern FILE* blob_fileio_get_temp_file(std::string &fn, const char *tmp_path=nullptr, const char *mode="wb");

class BlobFileIOPullData;
BlobFileIOPullData* blobe_fileio_start_pull(const char* filepath, uint64_t &blob_sz);
const char *blobe_fileio_pull(BlobFileIOPullData* fp, uint64_t from, size_t &data_pulled);
bool blobe_fileio_destroy(BlobFileIOPullData* fp);

uint64_t blob_fwrite64(const void *data_, size_t esz, uint64_t cnt, FILE *f);
