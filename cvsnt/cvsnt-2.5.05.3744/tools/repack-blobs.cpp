//to build on linux: clang++ -std=c++17 -O2 gc-blobs.cpp blob_operations.cpp ./sha256/sha256.c -lz -lzstd -ogc-blobs

#include "simpleOsWrap.cpp.inc"
//#include "sha_blob_reference.h"
#include "src/details.h"
#include "content_addressed_fs.h"
#include <string>
#include <filesystem>
#include <iostream>
#include <chrono>
#include <fstream>
#include <vector>
#include <ctime>
#ifdef _WIN32
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif
using namespace caddressed_fs;

static size_t repacked_files = 0, affected_files = 0;
static int64_t data_saved = 0;
static void usage()
{
  printf("CVS_TMP env var (or /tmp) should point to same filesystem as root\n");
  printf("Usage: repack-blobs <full_root> [days_newer]\n");
  printf("example:repack-blobs /cvs/some_repo 32\n");
  printf("safe to run in working production environment. Uses one thread only, but better decrease nicenesess\n");
  printf("This utility will find all blobs created newer than time_newer and will try to repack them with best possible compression\n");
  printf("Safe to run in production. Suppose usage - you run it once-a-month/week, and set days_newer as 32/8\n");
}

static void process_sha_files_directory(const char *dir, unsigned char sha0, unsigned char sha1, time_t start_time)
{
  char hash[65];hash[64]=0;
  for (const auto & entry : fs::directory_iterator(dir))
  {
    if (entry.is_directory())
      continue;
    std::string filename = entry.path().filename().string();
    if (filename.length()!=64-4)
    {
      printf("[E] <%s> is not a sha blob!\n", filename.c_str());
      continue;
    }
    time_t cftime = get_file_mtime(filename.c_str());
    if (cftime >= start_time)
    {
      affected_files++;
      std::snprintf(hash, sizeof(hash), "%02x%02x%.60s", sha0, sha1, filename.c_str());
      int64_t ds = caddressed_fs::repack(hash);
      if (ds != 0)
        repacked_files++;
      data_saved += ds;
    }
  }
}

inline unsigned char decode_symbol(unsigned char s, bool &err)
{
  if (s >= '0' && s <= '9') return s-'0';
  if (s >= 'a' && s <= 'f') return s-'a'+10;
  err = true;
  return 0;
}

static void process_sha_directory(time_t start_time)
{
  std::string dirPath = blobs_dir_path();
  for (const auto & entry : fs::directory_iterator(dirPath.c_str()))
  {
    if (!entry.is_directory())
      continue;
    auto checkShaDirName = [](const std::string &dirName, unsigned char &symbol)
    {
      if (dirName.length() != 2)
        return false;
      bool err = false;
      symbol = (decode_symbol(dirName[0], err)<<4)|(decode_symbol(dirName[1], err));
      if (err)
        return false;
      return true;
    };
    unsigned char sha0;
    if (!checkShaDirName(entry.path().filename().string(), sha0))
    {
      printf("[E] <%s>(%s) is not a sha directory!\n", entry.path().string().c_str(), entry.path().filename().string().c_str());
      continue;
    }
    printf("process sha dir <%s>\n", entry.path().string().c_str());
    for (const auto & entry2 : fs::directory_iterator(entry.path()))
    {
      unsigned char sha1;
      if (!checkShaDirName(entry2.path().filename().string(), sha1))
      {
        printf("[E] <%s>(%s) is not a sha directory!\n", entry2.path().string().c_str(), entry2.path().filename().string().c_str());
        continue;
      }
      process_sha_files_directory(entry2.path().string().c_str(), sha0, sha1, start_time);
    }
  }
}

int main(int ac, const char* argv[])
{
  if (ac < 2)
  {
    usage();
    exit(1);
  }
  time_t start = 0;
  if (ac > 2)
    start = time(NULL) - (atoi(argv[2])*60*60*24);
  init_temp_dir();
  set_temp_dir(def_tmp_dir);
  set_root(argv[1]);
  process_sha_directory(start);
  printf("processed %lld files, repacked %lld files, saved %lld bytes\n",
  (long long)affected_files, (long long)repacked_files, (long long)data_saved);
  return 0;
}
