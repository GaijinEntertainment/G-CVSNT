//to build on linux: clang++ -std=c++17 -O2 convert_to_blob.cpp blob_operations.cpp ./sha256/sha256.c -lz -lzstd -ocvtblob

#include "simpleLock.cpp.inc"
#include "sha_blob_reference.h"
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include "zlib.h"
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
static bool fastest_conversion = false;//if true, we won't repack, just calc sha256 and put zlib block as is.

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
  std::vector<char> fileUnpackedData, filePackedData;
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
    //read data
    const auto sz = fs::file_size(entry.path());
    fileUnpackedData.resize(sz);
    std::ifstream f(entry.path(), std::ios::in | std::ios::binary);
    f.read(fileUnpackedData.data(), sz);
    unlock();
    readData += sz;
    std::string srcPathString = entry.path().c_str();
    const bool wasPacked = (srcPathString[srcPathString.length()-1] == 'z' && srcPathString[srcPathString.length()-2] == '#');
    size_t unpackedDataSize = wasPacked ? size_t(ntohl(*(int*)fileUnpackedData.data())) : sz;
    //process packed data
    if (wasPacked)
    {
      //printf("was packed %d ->%d\n", (int)unpackedDataSize,(int)sz);
      //we need to unpack data first, so we can repack.
      //ofc, for fastest possible conversion we can just write blobs as is (i.e. keep them zlib)
      std::swap(fileUnpackedData, filePackedData);
      fileUnpackedData.resize(unpackedDataSize);//we then swap it with fileUnpackedData
      z_stream stream = {0};
      inflateInit(&stream);
      stream.avail_in = filePackedData.size() - sizeof(int);
      stream.next_in = (Bytef*)(filePackedData.data() + sizeof(int));
      stream.avail_out = unpackedDataSize;
      stream.next_out = (Bytef*)fileUnpackedData.data();
      if (unpackedDataSize && inflate(&stream, Z_FINISH)!=Z_STREAM_END)
      {
        printf("[E] can't unpack %s of sz=%d unpacked=%d \n", srcPathString.c_str(), (int)sz, (int)unpackedDataSize);
        continue;
      }
    }
    //if wasPacked, originally packed data is in filePackedData
    unsigned char sha256[32];
    size_t wr = 0;
    if (fastest_conversion)
    {
      calc_sha256(srcPathString.c_str(), fileUnpackedData.data(), fileUnpackedData.size(), false, unpackedDataSize, sha256);
      const size_t sha_file_name_len = 1024;
      char sha_file_name[sha_file_name_len];//can be dynamically allocated, as 64+3+1 + strlen(root). Yet just don't put it that far!
      char sha256_encoded[sha256_encoded_size+1];
      encode_sha256(sha256, sha256_encoded, sizeof(sha256_encoded));
      get_blob_filename_from_encoded_sha256(rootDir, sha256_encoded, sha_file_name, sha_file_name_len);
      if (!isreadable(sha_file_name))// otherwise blob exist
      {
        char *temp_filename = NULL;
        FILE *dest = cvs_temp_file(&temp_filename, "wb");
        if (!dest)
          error(1, errno, "can't open write temp_filename <%s> for <%s>(<%s>)", temp_filename, sha_file_name, srcPathString.c_str());
        BlobHeader hdr = wasPacked ? get_header(zlib_magic, fileUnpackedData.size(), filePackedData.size()-sizeof(int), 0) : get_header(noarc_magic, fileUnpackedData.size(), fileUnpackedData.size(), 0);
        if (fwrite(&hdr,1,sizeof(hdr),dest) != sizeof(hdr))
          error(1, 0, "can't write temp_filename <%s> for <%s>(<%s>) of %d len", temp_filename, sha_file_name, srcPathString.c_str(), (uint32_t)sizeof(hdr));
        if (fwrite(wasPacked ? filePackedData.data() + sizeof(int) : fileUnpackedData.data(), 1, hdr.compressedLen, dest) != hdr.compressedLen)
          error(1, 0, "can't write temp_filename <%s> for <%s>(<%s>) of %d len", temp_filename, sha_file_name, srcPathString.c_str(), (uint32_t)hdr.compressedLen);
        create_dirs(rootDir, sha256);
        change_file_mode(temp_filename, 0666);
        rename_file (temp_filename, sha_file_name, false);//we dont care if blob is written independently
        fclose(dest);
        blob_free (temp_filename);
        wr = hdr.compressedLen + sizeof(hdr);
      }
    } else
      wr = write_binary_blob(rootDir, sha256, entry.path().c_str(), fileUnpackedData.data(), fileUnpackedData.size(), BlobPackType::FAST, false);

    //now we can write reference
    if (!wr)
    {
      printf("deduplication %d for %s", int(sz), filePath.c_str());
      deduplicatedData += sz;
    }
    get_lock(entry.path().string().c_str());
    if (write_blob_reference(entry.path().c_str(), sha256, false))
    {
      if (wasPacked)
        rename_z_to_normal(entry.path().c_str());
    } else
      printf("[E] blob reference %s couldn't be saved\n", entry.path().c_str());
    unlock();
    writtenData += wr;
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
  printf("written %g mb, saved %g mb, de-duplicated %g mb\n", double(writtenData)/(1<<20),
    (double(readData)-double(writtenData))/(1<<20),
    double(deduplicatedData)/(1<<20));

  cvs_tcp_close(lock_server_socket);
  return 0;
}
