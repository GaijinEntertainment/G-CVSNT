//to build on linux: clang++ -std=c++17 -O2 convert_to_blob.cpp blob_operations.cpp ./sha256/sha256.c -lz -lzstd -ocvtblob

#include "simpleLock.cpp.inc"
#include "sha_blob_reference.h"
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#ifdef _WIN32
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif


static void usage()
{
  printf("Usage: <lock_server> <user_name> <full_root> full_directory file [max_lock_time_in_seconds]\n");
  printf("or: <lock_server> <user_name> <full_root> max_lock_time_in_seconds\n");
  printf("example:convert_to_blob 127.0.0.1 some_user /home/some_user/test testDir a.png\n");
  printf("or:convert_to_blob 127.0.0.1 some_user /home/some_user/test \n");
  printf("Warning! no ,v in the end is needed. Warning, do not enter CVS directories\n");
  printf("Warning! run only on server!\n");
  printf("Warning! it can lock one file for a while. The total required time depends on amount of version. In order to be safe, use max_lock_time == 600 (10 minutes) during production times\n");
  printf("max_lock_time_in_seconds == -1 means infinite\n");
}

static size_t write_blob_and_blob_reference_saved(const char *root, const char *fn, const void *data, size_t len, bool store_packed, bool src_packed)
{
  unsigned char sha256[32];
  size_t wr = write_binary_blob(root, sha256, fn, data, len, BlobPackType::BEST, src_packed);
  write_blob_reference(fn, sha256);
  return wr;
}

static void rename_z_to_normal(const char *f)
{
  std::string pathStr = f;
  if (pathStr[pathStr.length()-1] == 'z' && pathStr[pathStr.length()-2] == '#')
  {
    pathStr[pathStr.length()-2] = 0;
    rename_file(f, pathStr.c_str());//not packed anymore
  }
}

static void process_file(int lock_server_socket, const char *rootDir, const char *dir, const char *file, size_t max_lock_time)
{
  printf("process <%s>/<%s>\n", dir, file);
  std::string dirPath = rootDir;
  if (dirPath[dirPath.length()-1] != '/' && dir[0]!='/')
    dirPath+="/";
  dirPath += dir;
  if (dirPath[dirPath.length()-1] != '/')
    dirPath+="/";
  std::string pathToVersions = (dirPath + "CVS/")+file;

  if (!iswriteable(pathToVersions.c_str()))
  {
    printf("<%s> is not a writeable directory (not a kB file?)!\n", pathToVersions.c_str());
    return;
  }

  std::string filePath = dirPath + file;
  printf("obtaining lock for <%s>\n", filePath.c_str());
  size_t lockId = 0;
  do {
    lockId = do_lock_file(lock_server_socket, filePath.c_str(), 1, 0);
  } while(lockId == 0);

  printf("for <%s> obtained lock %lld, start processing\n", filePath.c_str(), (long long int) lockId);
  std::vector<char> fileData;
  std::vector<char> sha_file_name(filePath.length() + sha256_encoded_size + 7);
  size_t readData = 0, writtenData = 0;
  std::time_t startTime = std::time(nullptr);
  for (const auto & entry : fs::directory_iterator(pathToVersions))
  {
    if (entry.is_directory())
      continue;
    if (is_blob_reference(rootDir, entry.path().string().c_str(), sha_file_name.data(), sha_file_name.size()))
    {
      printf("%s is already blob reference\n", entry.path().string().c_str());
      //rename_z_to_normal(entry.path().c_str());
      continue;
    }
    const auto sz = fs::file_size(entry.path());
    fileData.resize(sz);
    std::ifstream f(entry.path(), std::ios::in | std::ios::binary);
    f.read(fileData.data(), sz);
    readData += sz;
    writtenData += write_blob_and_blob_reference_saved(rootDir, entry.path().c_str(), fileData.data(), sz, true, false);
    rename_z_to_normal(entry.path().c_str());
    if (std::time(nullptr) - startTime > max_lock_time)
      printf("stopping, as time limit is exhausted.\n");
  }
  if (writtenData != readData)
    printf("de-duplicated %lld\n", (long long int)(readData-writtenData));
  printf("releasing lock %lld for %s\n", (long long int) lockId, filePath.c_str());
  do_unlock_file(lock_server_socket, lockId);
}

static void process_directory(int lock_server_socket, const char *rootDir, const char *dir, size_t max_lock_time)
{
  printf("process dir <%s>\n", dir);
  std::string dirPath = rootDir;
  if (dirPath[dirPath.length()-1] != '/' && dir[0]!='/')
    dirPath+="/";
  dirPath += dir;
  for (const auto & entry : fs::directory_iterator(dirPath.c_str()))
  {
    if (entry.is_directory())
    {
      if (strcmp(entry.path().filename().c_str(), "CVS") != 0 &&
          strcmp(entry.path().filename().c_str(), "CVSROOT") != 0 &&
          strcmp(entry.path().filename().c_str(), BLOBS_SUBDIR_BASE) != 0)
        process_directory(lock_server_socket, rootDir, entry.path().lexically_relative(rootDir).c_str(), max_lock_time);
    } else
    {
      std::string filename = entry.path().filename();

      if (filename.length()>2 && filename[filename.length()-1] == 'v' && filename[filename.length()-2] == ',')
      {
        filename[filename.length()-2] = 0;
        process_file(lock_server_socket, rootDir, dir, filename.c_str(), max_lock_time);
      }
    }
  }
}

int main(int ac, const char* argv[])
{
  if (ac < 5)
  {
    usage();
    exit(1);
  }
  tcp_init();
  const char *rootDir = argv[3];
  int lock_server_socket = lock_register_client(argv[2], rootDir, argv[1]);
  if (lock_server_socket == -1)
  {
    printf("[E] Can't connect\n");
    exit(1);
  }
  if (ac > 5)
  {
    const size_t max_lock_time = ac < 7 ? size_t(~size_t(0)) : unsigned(atoi(argv[6]));
    process_file(lock_server_socket, rootDir, argv[4], argv[5],max_lock_time);
  } else
  {
    const size_t max_lock_time = ac == 4 ? size_t(~size_t(0)) : unsigned(atoi(argv[4]));
    process_directory(lock_server_socket, rootDir, "", max_lock_time);
  }

  cvs_tcp_close(lock_server_socket);
  return 0;
}
