#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <memory>
#include "fileio.h"
#include "../blake3/blake3.h"
#include "../content_addressed_fs.h"

inline bool bin_hash_to_hex_string(const unsigned char *blob_hash, char *to_hash_hex_string)//copy pasre
{
  static const char *hex_remap ="0123456789abcdef";
  for (int i = 0; i < 32; ++i, ++blob_hash, to_hash_hex_string+=2)
  {
    to_hash_hex_string[0] = hex_remap[(*blob_hash)>>4];
    to_hash_hex_string[1] = hex_remap[(*blob_hash)&0xF];
  }
  *to_hash_hex_string = 0;
  return true;
}

//we can implement remapping of hashes, or even different hash structure (hash_type->root_folder) with symlinks to same content
namespace caddressed_fs
{

static std::string root_path = ".";
static std::string default_tmp_dir = "";
void set_tmp_dir(const char *p){default_tmp_dir = p;}
void set_root(const char *p){ root_path = p;}

static inline bool make_blob_dirs(std::string &fp)
{
  if (fp.length()<67)
    return false;
  if (fp[fp.length()-61] != '/' || fp[fp.length()-64] != '/')
    return false;
  fp[fp.length()-64] = 0;
  blob_fileio_ensure_dir(fp.c_str());
  fp[fp.length()-64] = '/'; fp[fp.length()-61] = 0;
  blob_fileio_ensure_dir(fp.c_str());
  fp[fp.length()-61] = '/';
  return true;
}

static inline std::string get_file_path(const char* hash_hex_string)
{
  char buf[68];
  std::snprintf(buf,sizeof(buf), "/%.2s/%.2s/%.60s", hash_hex_string, hash_hex_string+2, hash_hex_string+4);
  return root_path + buf;
}

size_t get_size(const char* hash_hex_string)
{
  return blob_fileio_get_file_size(get_file_path(hash_hex_string).c_str());
}

bool exists(const char* hash_hex_string)
{
  return blob_fileio_is_file_readable(get_file_path(hash_hex_string).c_str());
}

class PushData
{
public:
  std::string temp_file_name;
  std::string provided_hash;
  FILE *fp;
  blake3_hasher hasher;
};

PushData* start_push(const char* hash_hex_string)
{
  if (hash_hex_string && blob_fileio_is_file_readable(get_file_path(hash_hex_string).c_str()))
    return (PushData*)uintptr_t(1);//magic number. we dont need to do anything, but that's not an error
  std::string temp_file_name;
  FILE * tmpf = blob_fileio_get_temp_file(temp_file_name, default_tmp_dir.length()>0 ? default_tmp_dir.c_str() : "");
  if (!tmpf)
    return 0;
  auto r = new PushData{std::move(temp_file_name), hash_hex_string ? hash_hex_string : "", tmpf};
  blake3_hasher_init(&r->hasher);
  return r;
}

bool stream_push(PushData *fp, const void *data, size_t data_size)
{
  if (uintptr_t(fp) == 1)
    return true;
  if (fp == nullptr)
    return false;
  if (!fp->fp)
    return false;
  blake3_hasher_update(&fp->hasher, data, data_size);
  if (fwrite(data, 1, data_size, fp->fp) == data_size)
    return true;
  fclose(fp->fp);
  blob_fileio_unlink_file(fp->temp_file_name.c_str());
  fp->fp = nullptr;
  return false;
}

void destroy(PushData *fp)
{
  if (uintptr_t(fp) == 1 || fp == nullptr)
    return;
  std::unique_ptr<PushData> kill(fp);
  if (!fp->fp)
    return;
  fclose(fp->fp);
  blob_fileio_unlink_file(fp->temp_file_name.c_str());
}

PushResult finish(PushData *fp, char *actual_hash_str)
{
  PushResult pr;
  if (uintptr_t(fp) == 1)
    return PushResult::OK;
  if (fp == nullptr)
    return PushResult::EMPTY_DATA;
  std::unique_ptr<PushData> kill(fp);
  if (!fp->fp)
    return PushResult::IO_ERROR;
  unsigned char digest[32];
  blake3_hasher_finalize(&fp->hasher, digest, sizeof(digest));
  bin_hash_to_hex_string(digest, actual_hash_str);
  fclose(fp->fp);
  std::string filepath = get_file_path(actual_hash_str);
  if (blob_fileio_is_file_readable(filepath.c_str()))//was pushed by someone else
  {
    blob_fileio_unlink_file(fp->temp_file_name.c_str());
    return PushResult::OK;
  }
  if (fp->provided_hash.length() > 0 && fp->provided_hash != actual_hash_str)
    return PushResult::WRONG_HASH;
  make_blob_dirs(filepath);
  return blob_fileio_rename_file(fp->temp_file_name.c_str(), filepath.c_str()) ?  PushResult::OK : PushResult::IO_ERROR;
}

#if !_WIN32
//memory mapped files
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

class PullData
{
public:
  const char* begin;
  uint64_t size;
};

PullData* start_pull(const char* hash_hex_string, size_t &blob_sz)
{
  std::string filepath = get_file_path(hash_hex_string);
  blob_sz = blob_fileio_get_file_size(filepath.c_str());
  if (!blob_sz)
    return 0;
  int fd = open(filepath.c_str(), O_RDONLY);
  if (fd == -1)
    return 0;

  const char* begin = (const char*)(mmap(NULL, blob_sz, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0));
  close(fd);
  return uintptr_t(new FilePullData{begin, blob_sz});
}

const char *pull_data(uintptr_t up, uint64_t from, size_t &data_pulled)
{
  FilePullData* fp = (FilePullData*)up;
  if (!fp)
    return nullptr;
  const int64_t left = fp->size - from;
  if (left < 0)
    return nullptr;
  data_pulled = left;
  return fp->begin;
}

extern bool destroy(PullData* up)
{
  if (!up)
    return false;
  munmap((void*)up->begin, up->size);
  delete up;
  return true;
}

#else
//file io
//todo: on windows we can also make mmap files
class PullData
{
public:
  FILE *fp;
  std::unique_ptr<char[]> tempBuf;
  uint64_t pos;
  enum {SIZE = 1<<10};
};

PullData* start_pull(const char* hash_hex_string, size_t &blob_sz)
{
  std::string filepath = get_file_path(hash_hex_string);
  blob_sz = blob_fileio_get_file_size(filepath.c_str());
  if (!blob_sz)
    return 0;
  FILE* f;
  if (fopen_s(&f, filepath.c_str(), "rb") != 0)
      return nullptr;
  return new PullData{f, std::make_unique<char[]>(PullData::SIZE), 0};
}

const char *pull(PullData* fp, uint64_t from, size_t &data_pulled)
{
  if (!fp)
    return nullptr;
  if (fp->pos != from)
    fseek(fp->fp, int64_t(from) - int64_t(fp->pos), SEEK_CUR);
  data_pulled = fread(fp->tempBuf.get(), 1, PullData::SIZE, fp->fp);
  return fp->tempBuf.get();
}

extern bool destroy(PullData* fp)
{
  if (!fp)
    return false;
  fclose(fp->fp);
  delete fp;
  return true;
}

bool get_file_content_hash(const char *filename, char *hash_hex_str, size_t hash_len)
{
  hash_hex_str[0] = 0;hash_hex_str[hash_len-1] = 0;
  FILE*fp;
  if (!fopen_s(&fp, filename, "rb"))
    return false;
  blake3_hasher hasher;
  blake3_hasher_init(&hasher);
  char buf[16384];
  while (1)
  {
    int read = fread(buf, 1, sizeof(buf), fp);
    blake3_hasher_update(&hasher, buf, read);
    if (read < sizeof(buf))
      break;
  };
  fclose(fp);
  unsigned char digest[32];
  blake3_hasher_finalize(&hasher, digest, sizeof(digest));
}
#endif

}