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
  printf("CVS_TMP env var (or /tmp) should point to same filesystem as root\n");
  printf("Usage: <lock_server> <user_name> <full_root> full_directory file\n");
  printf("or: <lock_server> <user_name> <full_root> [full_directory]\n");
  printf("example:convert_to_blob 127.0.0.1 some_user /cvs/some_repo testDir a.png\n");
  printf("or:convert_to_blob 127.0.0.1 some_user /cvs/some_repo \n");
  printf("or:convert_to_blob 127.0.0.1 some_user /cvs/some_repo SomeFolderThere\n");
  printf("Warning! no ,v in the end is needed. Warning, do not enter CVS directories\n");
  printf("Warning! run only on server!\n");
  printf("Warning! it can lock one file for a short while, but will do that numerous time. \n");
}

static void rename_z_to_normal(const char *f)
{
  std::string pathStr = f;
  if (pathStr[pathStr.length()-1] == 'z' && pathStr[pathStr.length()-2] == '#')
  {
    pathStr[pathStr.length()-2] = 0;
    if (!rename_attempts(f, pathStr.c_str(), 100))
      printf("[E] can't rename file <%s> to <%s>\n", f, pathStr.c_str());
  }
}

static uint64_t deduplicatedData = 0, readData = 0, writtenData = 0;

static void process_file(int lock_server_socket, const char *rootDir, const char *dir, const char *file)
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
  std::vector<char> fileData;
  std::vector<char> sha_file_name(filePath.length() + sha256_encoded_size + 7);
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

    size_t lockId = 0;
    auto get_lock = [&](const char *fp)
    {
      if (lockId)
        return;
      printf("obtaining lock for <%s>...", fp);
      lockId = 0;
      do {
        lockId = do_lock_file(lock_server_socket, filePath.c_str(), 1, 0);
        if (lockId == 0)
          sleep(500);
      } while(lockId == 0);
      printf(" lock=%lld, start processing\n", (long long int) lockId);
    };
    auto unlock = [&](){
      if (!lockId)
        return;
      printf("releasing lock %lld for %s\n", (long long int) lockId, filePath.c_str());
      do_unlock_file(lock_server_socket, lockId);
      lockId = 0;
    };

    get_lock(entry.path().string().c_str());
    if (is_blob_reference(rootDir, entry.path().string().c_str(), sha_file_name.data(), sha_file_name.size()))
    {
      //became reference
      unlock();
      continue;
    }
    const auto sz = fs::file_size(entry.path());
    fileData.resize(sz);
    std::ifstream f(entry.path(), std::ios::in | std::ios::binary);
    f.read(fileData.data(), sz);
    unlock();
    readData += sz;
    {
      unsigned char sha256[32];
      size_t wr = write_binary_blob(rootDir, sha256, entry.path().c_str(), fileData.data(), sz, BlobPackType::FAST, false);
      if (!wr)
      {
        printf("deduplication %d for %s", int(sz), filePath.c_str());
        deduplicatedData += sz;
      }
      get_lock(entry.path().string().c_str());
      if (write_blob_reference(entry.path().c_str(), sha256, false))
        rename_z_to_normal(entry.path().c_str());
      else
        printf("[E] blob reference %s couldn't be saved", entry.path().c_str());
      unlock();
      writtenData += wr;
    }
  }
}

static void process_directory(int lock_server_socket, const char *rootDir, const char *dir)
{
  printf("process dir <%s>\n", dir);
  std::string dirPath = rootDir;
  if (dirPath[dirPath.length()-1] != '/' && dir[0]!='/')
    dirPath+="/";
  dirPath += dir;
  size_t readData_old = readData, writtenData_old = writtenData;
  for (const auto & entry : fs::directory_iterator(dirPath.c_str()))
  {
    if (entry.is_directory())
    {
      if (strcmp(entry.path().filename().c_str(), "CVS") != 0 &&
          strcmp(entry.path().filename().c_str(), "CVSROOT") != 0 &&
          strcmp(entry.path().filename().c_str(), BLOBS_SUBDIR_BASE) != 0)
        process_directory(lock_server_socket, rootDir, entry.path().lexically_relative(rootDir).c_str());
    } else
    {
      std::string filename = entry.path().filename();

      if (filename.length()>2 && filename[filename.length()-1] == 'v' && filename[filename.length()-2] == ',')
      {
        filename[filename.length()-2] = 0;
        process_file(lock_server_socket, rootDir, dir, filename.c_str());
      }
    }
  }
  if (writtenData_old != writtenData)
    printf("processed dir <%s>, saved %gkb\n", dir, double(int64_t(readData_old - readData) - int64_t(writtenData_old-writtenData))/1024.0);
}

int main(int ac, const char* argv[])
{
  if (ac < 4)
  {
    printf("not enough params <%d>\n", ac);
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
    process_file(lock_server_socket, rootDir, argv[4], argv[5]);
  } else
  {
    process_directory(lock_server_socket, rootDir, ac > 4 ? argv[4] : "");
  }
  printf("written %gmb, saved %g mb, de-duplicated %g mb\n", double(writtenData)/(1<<20),
    (double(readData)-double(writtenData))/(1<<20),
    double(deduplicatedData)/(1<<20));

  cvs_tcp_close(lock_server_socket);
  return 0;
}
