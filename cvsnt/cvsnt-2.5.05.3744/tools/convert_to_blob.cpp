//to build on linux: clang++ -std=c++17 -O2 -I../src repack-blobs.cpp ../src/blob_operations.cpp ../src/sha256/sha256.c -lz -lzstd -orepack-blobs

#include "simpleLock.cpp.inc"
#include "sha_blob_reference.h"
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <atomic>
#include "zlib.h"
#include "flags.h"
#include "concurrent_queue.h"
#ifdef _WIN32
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif


static bool fastest_conversion = true;//if true, we won't repack, just calc sha256 and put zlib block as is.
static void ensure_blob_mtime(const char* verfile, const char *blob_file)
{
  time_t blob_mtime = get_file_mtime(blob_file);
  time_t ver_atime = get_file_atime(verfile);
  if (ver_atime > blob_mtime)
    set_file_mtime(blob_file, ver_atime);
}

static void rename_z_to_normal(const char *f)
{
  std::string pathStr = f;
  if (pathStr[pathStr.length()-1] == 'z' && pathStr[pathStr.length()-2] == '#')
  {
    pathStr[pathStr.length()-2] = 0;
    if (!rename_attempts(f, pathStr.c_str(), 100))
      fprintf(stderr, "[E] can't rename file <%s> to <%s>\n", f, pathStr.c_str());
  }
}

static std::atomic<uint64_t> deduplicatedData = 0, readData = 0, writtenData = 0;
static std::mutex lock_id_mutex;

struct ThreadData
{
  std::vector<char> fileUnpackedData, filePackedData, sha_file_name;
};

static int lock_server_socket = -1;

static void process_file_ver(const char *rootDir,
  std::string &filePath,//v file
  const std::filesystem::path &path,//blob reference file
  ThreadData& data, bool mutex_needed)
{
  std::vector<char> &fileUnpackedData = data.fileUnpackedData, &filePackedData  = data.filePackedData, &sha_file_name = data.sha_file_name;
  sha_file_name.resize(filePath.length() + sha256_encoded_size + 7);
  if (is_blob_reference(rootDir, path.string().c_str(), sha_file_name.data(), sha_file_name.size()))
  {
    printf("%s is already blob reference\n", path.string().c_str());
    //rename_z_to_normal(entry.path().c_str());
    return;
  }
  size_t lockId = 0;
  auto get_lock = [&](const char *fp)
  {
    if (lockId)
      return;
    printf("obtaining lock for <%s>...", fp);
    lockId = 0;
    do {
      {
        std::unique_lock<std::mutex> lockGuard(lock_id_mutex);
        lockId = do_lock_file(lock_server_socket, filePath.c_str(), 1, 0);
      }
      if (lockId == 0)
        sleep(500);
    } while(lockId == 0);
    printf(" lock=%lld, start processing\n", (long long int) lockId);
  };
  auto unlock = [&](){
    if (!lockId)
      return;
    printf("releasing lock %lld for %s\n", (long long int) lockId, filePath.c_str());
    std::unique_lock<std::mutex> lockGuard(lock_id_mutex);
    do_unlock_file(lock_server_socket, lockId);
    lockId = 0;
  };

  get_lock(path.string().c_str());//Lock1
  if (is_blob_reference(rootDir, path.string().c_str(), sha_file_name.data(), sha_file_name.size()))
  {
    //became reference!
    unlock();//unLock1
    return;
  }

  const auto sz = fs::file_size(path);
  fileUnpackedData.resize(sz);
  std::ifstream f(path, std::ios::in | std::ios::binary);
  f.read(fileUnpackedData.data(), sz);
  unlock();//unLock1, because some other utility can lock

  readData += sz;
  std::string srcPathString = path.c_str();
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
      fprintf(stderr, "[E] can't unpack %s of sz=%d unpacked=%d \n", srcPathString.c_str(), (int)sz, (int)unpackedDataSize);
      return;
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
    } else
    {
      ensure_blob_mtime(srcPathString.c_str(), sha_file_name);//so later incremental repack still works correctly
    }
  } else
    wr = write_binary_blob(rootDir, sha256, path.c_str(), fileUnpackedData.data(), fileUnpackedData.size(), BlobPackType::FAST, false);

  //now we can write reference
  if (!wr)
  {
    printf("deduplication %d for %s", int(sz), filePath.c_str());
    deduplicatedData += sz;
  }

  get_lock(path.string().c_str());//lock2
  if (write_blob_reference(path.c_str(), sha256, false))
  {
    if (wasPacked)
      rename_z_to_normal(path.c_str());
  } else
    fprintf(stderr, "[E] blob reference %s couldn't be saved\n", path.c_str());
  unlock();//unlock2
  writtenData += wr;
}

struct ProcessTask
{
  std::string filePath;
  std::filesystem::path path;
};

struct FileVerProcessor
{
  FileVerProcessor():queue(&threads){}
  void finish()
  {
    if (is_inited())
      queue.finishWork();
  }

  bool is_inited() const {return !threads.empty();}
  void init(const char *root, int threads_count)
  {
    base_repo = root;
    for (int ti = 0; ti < threads_count; ++ti)
      threads.emplace_back(std::thread(processor_thread_loop, this));
  }
  void wait();

  static void processor_thread_loop(FileVerProcessor *processor)
  {
    ProcessTask task;
    ThreadData data;
    while (processor->queue.wait_and_pop(task))
      process_file_ver(processor->base_repo.c_str(), task.filePath, task.path, data, true);
  }
  void emplace(const std::string &filePath, const std::filesystem::path &p)
  {
    if (!is_inited())
      return;
    queue.emplace(ProcessTask{filePath, p});
  }
  std::vector<std::thread> threads;//soa

  concurrent_queue<ProcessTask> queue;
  std::string base_repo;
};

static FileVerProcessor processor;

static void process_file(const char *rootDir, const char *dir, const char *file)
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
  ThreadData data;
  for (const auto & entry : fs::directory_iterator(pathToVersions))
  {
    if (entry.is_directory())
      continue;
    //read data
    if (processor.is_inited())
      processor.emplace(filePath, entry.path());
    else
      process_file_ver(rootDir, filePath, entry.path(), data, false);
  }
}

static void process_directory(const char *rootDir, const char *dir)
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
        process_directory(rootDir, entry.path().lexically_relative(rootDir).c_str());
    } else
    {
      std::string filename = entry.path().filename();

      if (filename.length()>2 && filename[filename.length()-1] == 'v' && filename[filename.length()-2] == ',')
      {
        filename[filename.length()-2] = 0;
        process_file(rootDir, dir, filename.c_str());
      }
    }
  }
  if (writtenData_old != writtenData)
    printf("processed dir <%s>, saved %gkb\n", dir, double(int64_t(readData_old - readData) - int64_t(writtenData_old-writtenData))/1024.0);
}

int main(int ac, const char* argv[])
{
  auto options = flags::flags{ac, argv};
  options.info("convert_to_blob",
    "conversion utility from old CVS to blob de-duplicated one"
  );

  auto lock_url = options.arg("-lock_url", "Url for lock server");
  auto lock_user = options.arg("-user", "User name for lock server");
  auto rootDir = options.arg("-root", "Root dir for CVS");
  auto tmpDir = options.arg_or("-tmp", "", "Tmp dir for blobs");
  auto dir = options.arg_or("-dir", "", "Folder to process (inside root)");
  auto file = options.arg_or("-file", "", "File to process (inside dir)");
  auto threads = options.arg_as_or<int>("-j", 0,"concurrency level(threads to run)");
  fastest_conversion = !options.passed("-repack", "repack zlib to zstd, slow");

  bool help = options.passed("-h", "print help usage");
  if (help || !options.sane()) {
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
  if (threads>1)
  {
    printf("using %d threads\n", threads);
    processor.init(rootDir.c_str(), threads);
  }
  mkdir((rootDir+"/blobs").c_str(), 0777);

  if (file.length()>0)
  {
    process_file(rootDir.c_str(), dir.c_str(), file.c_str());
  } else
  {
    process_directory(rootDir.c_str(), dir.c_str());
  }
  processor.finish();
  printf("written %g mb, saved %g mb, de-duplicated %g mb\n", double(writtenData)/(1<<20),
    (double(readData)-double(writtenData))/(1<<20),
    double(deduplicatedData)/(1<<20));

  cvs_tcp_close(lock_server_socket);
  return 0;
}
