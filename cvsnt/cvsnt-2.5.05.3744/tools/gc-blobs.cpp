//to build on linux: clang++ -std=c++17 -O2 gc-blobs.cpp blob_operations.cpp ./sha256/sha256.c -lz -lzstd -ogc-blobs

#include "simpleOsWrap.cpp.inc"
#include "sha_blob_reference.h"
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include "tsl/sparse_set.h"
#ifdef _WIN32
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif

bool gather_used = false;

static void usage()
{
  printf("Usage: gc-blobs <full_root> used|unused|broken|delete_unused\n");
  printf("example:gc-blobs /cvs/some_repo unused\n");
  printf("example:gc-blobs /cvs/some_repo used\n");
  printf("safe to run in working production environment. uses one thread only\n");
  printf("Warning! It doesnt delete anything, unless called with delete_unused, it will just output to stdout the list of referenced(used)/unreferenced/broken sha paths\n");
  printf("Warning! run only on server!\n");
}

struct ShaKey
{
  union {
    uint64_t keys[4];//32 bytes
    unsigned char sha256[32];
  } v;
  bool operator == (const ShaKey& k) const {return k.v.keys[0] == v.keys[0] && k.v.keys[1] == v.keys[1] && k.v.keys[2] == v.keys[2] && k.v.keys[3] == v.keys[3];}
};

struct HashShaKey {
  size_t operator ()(const ShaKey& k) const {return k.v.keys[0];}
};

static tsl::sparse_set<ShaKey, HashShaKey> collected_shas;
inline unsigned char decode_symbol(unsigned char s, bool &err)
{
  if (s >= '0' && s <= '9') return s-'0';
  if (s >= 'a' && s <= 'f') return s-'a'+10;
  err = true;
  return 0;
}

inline const bool sh_from_encoded(const char *sha, ShaKey &k)
{
  bool err = false;
  for (int i = 0; i < 32; ++i)
    k.v.sha256[i] = (decode_symbol(sha[i*2], err)<<4)|(decode_symbol(sha[i*2+1], err));//strtol(sha+i*2, sha+i*2+2, 16);//can be
  return !err;
}

static void process_file(const char *rootDir, const char *dir, const char *file)
{
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
  std::vector<char> sha_file_name(filePath.length() + sha256_encoded_size + 7);
  size_t readData = 0, writtenData = 0;
  for (const auto & entry : fs::directory_iterator(pathToVersions))
  {
    if (entry.is_directory())
      continue;
    char sha256_encoded[sha256_encoded_size+1];
    if (!get_blob_reference_sha256(entry.path().string().c_str(), sha256_encoded))//sha256_encoded==char[65], encoded 32 bytes + \0
    {
      printf("[E] %s not a blob reference\n", entry.path().string().c_str());
      //rename_z_to_normal(entry.path().c_str());
      continue;
    }
    ShaKey k;
    if (!sh_from_encoded(sha256_encoded, k))
      printf("[E] <%s> looks like a blob reference, but it's <%s> is not a sha!\n", entry.path().string().c_str(), sha256_encoded);
    else
    {
      //printf("%s sha blob!\n", sha256_encoded);
      if (gather_used)
        collected_shas.insert(k);
      else
        collected_shas.erase(k);
    }
  }
}

static void process_directory(const char *rootDir, const char *dir)
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
      if (strcmp(entry.path().filename().string().c_str(), "CVS") != 0 &&
          strcmp(entry.path().filename().string().c_str(), "CVSROOT") != 0 &&
          strcmp(entry.path().filename().string().c_str(), BLOBS_SUBDIR_BASE) != 0)
        process_directory(rootDir, entry.path().lexically_relative(rootDir).string().c_str());
    } else
    {
      std::string filename = entry.path().filename().string();

      if (filename.length()>2 && filename[filename.length()-1] == 'v' && filename[filename.length()-2] == ',')
      {
        filename[filename.length()-2] = 0;
        process_file(rootDir, dir, filename.c_str());
      }
    }
  }
}

static void process_sha_files_directory(const char *dir, unsigned char sha0, unsigned char sha1)
{
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
    bool err = false;
    ShaKey k;k.v.sha256[0] = sha0;k.v.sha256[1] = sha1;
    for (int i = 0; i < 30; ++i)
      k.v.sha256[i+2] = (decode_symbol(filename[i*2], err)<<4)|(decode_symbol(filename[i*2+1], err));//strtol(sha+i*2, sha+i*2+2, 16);//can be
    if (err)
    {
      printf("[E] <%s> is not a sha blob!\n", filename.c_str());
      continue;
    }
    if (gather_used)
      collected_shas.erase(k);
    else
      collected_shas.insert(k);
  }
}

static void process_sha_directory(const char *rootDir)
{
  std::string dirPath = rootDir;
  if (dirPath[dirPath.length()-1] != '/')
    dirPath+="/";
  dirPath += BLOBS_SUBDIR_BASE;
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
      process_sha_files_directory(entry2.path().string().c_str(), sha0, sha1);
    }
  }
}

int main(int ac, const char* argv[])
{
  if (ac < 3)
  {
    usage();
    exit(1);
  }
  bool gather_broken = false, delete_unused = false;
  if (strcmp(argv[2], "used") == 0)
    gather_used = true;
  else if (strcmp(argv[2], "unused") == 0)
    gather_used = false;
  else if (strcmp(argv[2], "delete_unused") == 0)
  {
    gather_used = false;delete_unused = true;
  } else if (strcmp(argv[2], "broken") == 0)
    gather_used = true, gather_broken = true;
  else
  {
    printf("[E] Unkown command <%s>\n", argv[2]);
    usage();
    exit(1);
  }
  size_t storedBlobs = 0;
  if (!gather_used)
  {
    process_sha_directory(argv[1]);
    storedBlobs = collected_shas.size();
    printf("gathered %lld blobs\n", (unsigned long long)storedBlobs);
  }
  process_directory(argv[1], "");
  if (!gather_used)
  {
    printf("referenced %lld blobs %lld unused\n", (unsigned long long)(storedBlobs - collected_shas.size()), (unsigned long long)collected_shas.size());
  } else
  {
    printf("referenced %lld blobs\n", (unsigned long long)collected_shas.size());
    if (gather_broken)
    {
      process_sha_directory(argv[1]);
      printf("broken %lld references\n", (unsigned long long)collected_shas.size());
    }
  }
  if (collected_shas.size())
  {
    printf("========================================================================\n");
    for (auto &i : collected_shas)
    {
      if (delete_unused)
      {
        char buf[1024];
        snprintf(buf, sizeof(buf),
         "%s%s%02x/%02x/%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
         argv[1], BLOBS_SUBDIR, SHA256_LIST(i.v.sha256));
        printf("deleting <%s>...%s\n", buf, unlink(buf) ? "ERR" : "OK");
      } else
        printf(
         "%02x/%02x/%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
          SHA256_LIST(i.v.sha256));
    }
  }

  return 0;
}
