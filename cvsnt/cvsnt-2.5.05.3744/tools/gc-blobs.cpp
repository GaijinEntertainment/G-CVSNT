//to build on linux: clang++ -std=c++17 -O2 gc-blobs.cpp blob_operations.cpp ./sha256/sha256.c -lz -lzstd -ogc-blobs

//#include "simpleOsWrap.cpp.inc"
#include "simpleLock.cpp.inc"
#include "sha_blob_reference.h"
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include "tsl/sparse_set.h"
#include "flags.h"
#ifdef _WIN32
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif

static int lock_server_socket = -1;

bool gather_used = false;

struct ShaKey
{
  union {
    uint64_t keys[4];//32 bytes
    unsigned char hash[32];
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
    k.v.hash[i] = (decode_symbol(sha[i*2], err)<<4)|(decode_symbol(sha[i*2+1], err));//strtol(sha+i*2, sha+i*2+2, 16);//can be
  return !err;
}

size_t get_lock(const char *lock_file_name)
{
  size_t lockId = 0;
  #if VERBOSE
  printf("obtaining lock for <%s>...", lock_file_name);
  #endif
  do {
    lockId = do_lock_file(lock_server_socket, lock_file_name, 0, 0);
    if (lockId == 0)
      sleep_ms(100);
  } while(lockId == 0);
  #if VERBOSE
  printf(" lock=%lld, start processing\n", (long long int) lockId);
  #endif
  return lockId;
}

static void unlock(size_t lockId, const char *debug_text = nullptr)
{
  if (!lockId)
    return;
  #if VERBOSE
  printf("releasing lock %lld for %s\n", (long long int) lockId, debug_text ? debug_text : "");
  #endif
  do_unlock_file(lock_server_socket, lockId);
};

static void process_file(const char *rootDir, const char *dir, const char *rcs_file)
{
  std::string dirPath = rootDir;
  if (dirPath[dirPath.length()-1] != '/' && dir[0]!='/')
    dirPath+="/";
  dirPath += dir;
  if (dirPath[dirPath.length()-1] != '/')
    dirPath+="/";
  std::string fullPathToRCS = dirPath+rcs_file;

  if (!iswriteable(fullPathToRCS.c_str()))
  {
    printf("<%s> is not a writeable file. We can't process it!\n", fullPathToRCS.c_str());
    return;
  }

  size_t lockId = get_lock(fullPathToRCS.c_str());//lock for read
  std::ifstream t(fullPathToRCS.c_str());
  std::stringstream buffer;
  buffer << t.rdbuf();
  std::string rcsData = buffer.str();
  unlock(lockId);

  const char *data = rcsData.c_str(), *end = data + rcsData.length();
  #define TEXT_COMMAND "@\ntext\n@"
  while (const char *text_command = strstr(data, TEXT_COMMAND))//we found something looking like text command in rcs
  {
    //text command found
    data = text_command + strlen(TEXT_COMMAND);
    if (*data == '@')//that last @ was escaped !
      continue;
    for (; *data; ++data)
    {
      if (*data == '@')
      {
        if (data[1] != '@')//end of text command
          break;
        data++;//escaped '@', how that it is in text?
        continue;
      }

      if (data + blob_reference_size >= end)
        break;
      if (is_blob_reference_data(data, blob_reference_size)//is a blob reference
          && data[blob_reference_size] == '@')//that's the end of text
      {
          ShaKey k;
          const char *hash_encoded = data + hash_type_magic_len;
          if (!sh_from_encoded(hash_encoded, k))
            fprintf(stderr, "[E] in <%s> some string <%*.s> like a blob reference, but it's part is not a sha!\n",
              fullPathToRCS.c_str(), (int)blob_reference_size, data);
          else
          {
            //printf("%s sha blob!\n", hash_encoded);
            if (gather_used)
              collected_shas.insert(k);
            else
              collected_shas.erase(k);
          }
      }
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
    ShaKey k;k.v.hash[0] = sha0;k.v.hash[1] = sha1;
    for (int i = 0; i < 30; ++i)
      k.v.hash[i+2] = (decode_symbol(filename[i*2], err)<<4)|(decode_symbol(filename[i*2+1], err));//strtol(sha+i*2, sha+i*2+2, 16);//can be
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
  auto options = flags::flags{ac, argv};
  options.info("gc-blobs",
    "conversion utility from old CVS to blob de-duplicated one"
    "Usage: gc-blobs -root <rootDir> -lock_url <lock_url> -user <lock_user> used|unused|broken|delete_unused\n"
    "safe to run in working production environment. Uses one thread only\n"
    "Warning! It doesnt delete anything, unless called with delete_unused, it will just output to stdout the list of referenced(used)/unreferenced/broken sha paths\n"
    "Warning! run only on server!\n"
  );

  auto lock_url = options.arg("-lock_url", "Url for lock server");
  auto lock_user = options.arg("-user", "User name for lock server");
  auto rootDir = options.arg("-root", "Root dir for CVS");

  bool help = options.passed("-h", "print help usage");
  if (help || !options.sane()) {
    if (!options.sane())
      printf("Missing required arguments:%s\n", options.print_missing().c_str());
    std::cout << options.usage();
    return help ? 0 : -1;
  }
  tcp_init();
  lock_server_socket = lock_register_client(lock_user.c_str(), rootDir.c_str(), lock_url.c_str());
  if (lock_server_socket == -1)
  {
    fprintf(stderr, "[E] Can't connect to lock server\n");
    exit(1);
  }

  bool gather_broken = false, delete_unused = false;
  if (strcmp(argv[ac-1], "used") == 0)
    gather_used = true;
  else if (strcmp(argv[ac-1], "unused") == 0)
    gather_used = false;
  else if (strcmp(argv[ac-1], "delete_unused") == 0)
  {
    gather_used = false;delete_unused = true;
  } else if (strcmp(argv[ac-1], "broken") == 0)
    gather_used = true, gather_broken = true;
  else
  {
    printf("[E] Unknown command <%s>\n", argv[ac-1]);
    std::cout << options.usage();
    exit(1);
  }
  size_t storedBlobs = 0;
  if (!gather_used)
  {
    process_sha_directory(rootDir.c_str());
    storedBlobs = collected_shas.size();
    printf("gathered %lld blobs\n", (unsigned long long)storedBlobs);
  }
  process_directory(rootDir.c_str(), "");

  if (!gather_used)
  {
    printf("referenced %lld blobs %lld unused\n", (unsigned long long)(storedBlobs - collected_shas.size()), (unsigned long long)collected_shas.size());
  } else
  {
    printf("referenced %lld blobs\n", (unsigned long long)collected_shas.size());
    if (gather_broken)
    {
      process_sha_directory(rootDir.c_str());
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
         rootDir.c_str(), BLOBS_SUBDIR, HASH_LIST(i.v.hash));
        printf("deleting <%s>...%s\n", buf, unlink(buf) ? "ERR" : "OK");
      } else
        printf(
         "%02x/%02x/%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
          HASH_LIST(i.v.hash));
    }
  }

  cvs_tcp_close(lock_server_socket);
  return 0;
}
