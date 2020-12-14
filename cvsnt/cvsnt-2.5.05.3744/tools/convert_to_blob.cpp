//to build on linux: clang++ -std=c++17 -O2 -I../src repack-blobs.cpp ../src/blob_operations.cpp ../src/sha256/sha256.c -lz -lzstd -orepack-blobs

#include "simpleLock.cpp.inc"
#include "streaming_compressors.h"
#include "content_addressed_fs.h"
#include "src/details.h"
#include "sha_blob_reference.h"
//#include "ca_blob_format.h"
#include "calc_hash.h"
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
#include "tsl/sparse_map.h"
#include "tsl/sparse_set.h"
#ifdef _WIN32
namespace fs = std::experimental::filesystem;
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
namespace fs = std::filesystem;
#endif

// enable if we want to allow concurrent conversion of SAME dir (it's ok to convert different dirs)
// that's insane to do, really
#define CONCURRENT_CONVERSION_TOOL 0
#define VERBOSE 0

using namespace caddressed_fs;
using namespace streaming_compression;

//this, and only this is amount of work to be done in parallel.
static size_t max_files_to_process = 256;// to limit amount of double sized data

static void ensure_blob_mtime(time_t ver_atime, const char *blob_file)
{
  time_t blob_mtime = get_file_mtime(blob_file);
  if (ver_atime > blob_mtime)
    set_file_mtime(blob_file, ver_atime);
}

static int find_rcs_data(const char* rcs_data, const char *text_to_find, size_t text_to_find_length)
{
  const char *data = rcs_data;
  #define TEXT_COMMAND "@\ntext\n@"
  while (const char *text_command = strstr(data, TEXT_COMMAND))//we found something looking like text command in rcs
  {
    //text command found
    data = text_command + strlen(TEXT_COMMAND);
    if (*data == '@')//that last @ was escaped !
      continue;
    //text command found
    bool text_found = false;
    for (; *data; ++data)
    {
      if (*data == '@')
      {
        if (data[1] != '@')//end of text command
          break;
        data++;//escaped '@'
        continue;
      }
      if (strncmp(data, text_to_find, text_to_find_length) == 0)
      {
        text_found = true;
        break;
      }
    }
    if (text_found)
      return data - rcs_data;
  }
  return -1;
}

static bool replace_rcs_data(std::string &rcs_data, const std::string &text_to_replace, const char *replace_text, size_t replace_text_length)
{
  const char* data = rcs_data.data();
  bool found = false;
  int text_found_start;
  while ((text_found_start = find_rcs_data(data, text_to_replace.c_str(), text_to_replace.length())) >= 0)
  {
    size_t wasAt = data - rcs_data.data();
    rcs_data.replace(text_found_start, text_to_replace.length(), replace_text);
    data = rcs_data.data() + wasAt + text_found_start + replace_text_length;
    found = true;
  }
  return found;
}

static std::atomic<uint64_t> deduplicatedData = 0, readData = 0, writtenData = 0;
std::atomic<uint32_t> processed_files = 0;

static std::mutex lock_id_mutex;
static std::mutex  file_version_remap_mutex;
static tsl::sparse_map<std::string, char[hash_encoded_size+1]> file_version_remap;

struct ThreadData
{
  std::vector<char> fileUnpackedData, filePackedData;
};

static int lock_server_socket = -1;

size_t get_lock(const char *lock_file_name)
{
  if (lock_server_socket == -1)
    return 0;
  size_t lockId = 0;
  #if VERBOSE
  printf("obtaining lock for <%s>...", lock_file_name);
  #endif
  lockId = 0;
  do {
    {
      std::unique_lock<std::mutex> lockGuard(lock_id_mutex);
      lockId = do_lock_file(lock_server_socket, lock_file_name, 1, 0);
    }
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
  std::unique_lock<std::mutex> lockGuard(lock_id_mutex);
  do_unlock_file(lock_server_socket, lockId);
};

static void process_file_ver(const char *rootDir,
  std::string &filePath,//v file
  const std::filesystem::path &path,//blob reference file
  ThreadData& data)
{
  std::vector<char> &fileUnpackedData = data.fileUnpackedData, &filePackedData  = data.filePackedData;
  #if CONCURRENT_CONVERSION_TOOL
    size_t lockId = get_lock(filePath.c_str());//Lock1
  #endif
  std::string srcPathString = path.c_str();
  const bool wasPacked = (srcPathString[srcPathString.length()-1] == 'z' && srcPathString[srcPathString.length()-2] == '#');
  char hash_encoded[hash_encoded_size+1];hash_encoded[hash_encoded_size] = 0;

  time_t ver_atime = get_file_mtime(path.c_str());
  const size_t fsz = get_file_size(path.c_str());
  readData += fsz;

  int fd = open(srcPathString.c_str(), O_RDONLY);
  if (fd == -1)
    error(1, errno, "can't open for read <%s> of %d sz", srcPathString.c_str(), (int)fsz);
  const char* begin = (const char*)(mmap(NULL, fsz, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0));
  close(fd);
  const size_t unpackedSz = wasPacked  ? ntohl(*(int*)begin) : fsz;
  const size_t readDataSz = wasPacked ? fsz - sizeof(int) : fsz;
  const char* readData = wasPacked ? begin + sizeof(int) : begin;
  PushResult pr = PushResult::IO_ERROR;
  //just push data as is
  PushData* pd = start_push(get_default_ctx(), nullptr);
  BlobHeader hdr = get_header(wasPacked ? zlib_magic : noarc_magic, unpackedSz, 0);
  if (!stream_push(pd, &hdr, sizeof(hdr)) || !stream_push(pd, readData, readDataSz))
  {
    error(0, errno, "can't push blob for <%s>", srcPathString.c_str());
    destroy(pd);
  } else
    pr = finish(pd, hash_encoded);
  munmap((void*)begin, fsz);

  #if CONCURRENT_CONVERSION_TOOL
    unlock(lockId, filePath.c_str());//unLock1, so some other utility can lock
  #endif

  size_t wr = pr == PushResult::OK ? get_size(get_default_ctx(), hash_encoded) : 0;

  //this details are only for conversion. We want to make times relevant, so repacking can be addressed to only recent blobs
  std::string sha_file_name = get_file_path(get_default_ctx(), hash_encoded);
  if (pr == PushResult::OK)
    set_file_mtime(sha_file_name.c_str(), ver_atime);
  else if (pr == PushResult::DEDUPLICATED)
    ensure_blob_mtime(ver_atime, sha_file_name.c_str());//so later incremental repack still works correctly

  //now we can write reference
  if (pr == PushResult::DEDUPLICATED)
  {
    #if VERBOSE
    printf("deduplication %d for %s\n", int(sz), filePath.c_str());
    #endif
    deduplicatedData += fsz;
  }
  writtenData += wr;
  if (is_ok(pr))
  {
    std::unique_lock<std::mutex> lockGuard(file_version_remap_mutex);
    memcpy(&file_version_remap[path.filename().string()][0], hash_encoded, hash_encoded_size+1);
  } else
    fprintf(stderr, "[E] Can't process %s->%s %d\n", filePath.c_str(), hash_encoded, (int)pr);
}

struct ProcessTask
{
  std::string rcsFilePath;
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
    {
      process_file_ver(processor->base_repo.c_str(), task.rcsFilePath, task.path, data);
      ++processed_files;
    }
  }
  void emplace(const std::string &rcsFilePath, const std::filesystem::path &p)
  {
    queue.emplace(ProcessTask{rcsFilePath, p});
  }
  std::vector<std::thread> threads;//soa

  concurrent_queue<ProcessTask> queue;
  std::string base_repo;
};

static FileVerProcessor processor;

void process_queued_files(const char *filename, const char *lock_rcs_file_name, const std::string &rcs_file_name_full_path, const std::string &path_to_versions, int files_to_process)
{
  if (processor.is_inited())//finish conversion
  {
    ThreadData data;
    ProcessTask task;
    while (files_to_process != processed_files.load() && processor.queue.try_pop(task))
    {
      process_file_ver(processor.base_repo.c_str(), task.rcsFilePath, task.path, data);
      ++processed_files;
    }
    while (files_to_process != processed_files.load())//wait for other jobs to finish
      sleep_ms(1);
    processed_files.store(0);
  }

  {
    //replace references
    size_t lockId = get_lock(lock_rcs_file_name);//lock2
    mode_t rcs_mode; size_t rcs_sz;
    if (!get_file_mode_and_size(rcs_file_name_full_path.c_str(), rcs_mode, rcs_sz))
    {
      unlock(lockId, lock_rcs_file_name);//unlock2
      fprintf(stderr, "[E] no rcs file <%s>\n", rcs_file_name_full_path.c_str());
      file_version_remap.clear();
      return;
    }
    std::string rcsData;rcsData.resize(rcs_sz+1);
    {
      std::ifstream f(rcs_file_name_full_path.c_str(), std::ios::in | std::ios::binary);
      f.read(rcsData.data(), rcs_sz);rcsData[rcs_sz] = 0;
    }
    std::string oldVerRCS;
    char sha_ref[blob_reference_size+2];
    memcpy(sha_ref, HASH_TYPE_REV_STRING, hash_type_magic_len);
    sha_ref[blob_reference_size] = '@';
    sha_ref[blob_reference_size+1] = 0;
    tsl::sparse_set<std::string> keep_files_list;
    std::string filenameDir = std::string(filename) + "/";
    bool anyReplaced = false;
    for (auto &fv: file_version_remap)
    {
      oldVerRCS = filenameDir + fv.first;
      if (fv.first[fv.first.length() - 1] == 'z' && fv.first[fv.first.length() - 2] == '#')
        oldVerRCS.erase(oldVerRCS.length()-2);
      oldVerRCS += "@";
      memcpy(sha_ref+hash_type_magic_len, fv.second, hash_encoded_size);
      if (!replace_rcs_data(rcsData, oldVerRCS, sha_ref, sizeof(sha_ref)-1))
      {
        //it is dangerous to remove file then
        printf("[E] can't find references to <%s> in <%s>. Keeping file!\n", oldVerRCS.c_str(), rcs_file_name_full_path.c_str());
        keep_files_list.emplace(fv.first);
      } else
        anyReplaced = true;
    }
    bool rcsChanged = false;
    if (anyReplaced)
    {
      char *tempFilename;
      FILE *tmpf = cvs_temp_file (&tempFilename, "wb");
      if (tmpf)
      {
        if (fwrite(rcsData.c_str(), 1, rcsData.length()-1, tmpf) == rcsData.length()-1)
        {
          change_file_mode(tempFilename, rcs_mode);
          if (rename_file(tempFilename, rcs_file_name_full_path.c_str(), false))
            rcsChanged = true;
          else
          {
            printf("[E] can'rename  temp file <%s> to <%s>\n", tempFilename, rcs_file_name_full_path.c_str());
            unlink(tempFilename);
          }
        } else
          printf("[E] can't write temp file for <%s>\n", tempFilename);
        fclose(tmpf);
        blob_free(tempFilename);
      } else
        printf("[E] can't write temp file for <%s>\n", rcs_file_name_full_path.c_str());
    }
    unlock(lockId, lock_rcs_file_name);//unlock2

    if (rcsChanged)//unlink all replaced files
    {
      std::string pathtoVersionDir = path_to_versions + "/";
      for (auto &fv: file_version_remap)
        if (keep_files_list.find(fv.first) == keep_files_list.end())
          unlink((pathtoVersionDir + fv.first).c_str());
      rmdir(path_to_versions.c_str());//will delete only empty folder
    }
    file_version_remap.clear();
  }
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
    //printf("<%s> is not a writeable directory (not a kB file?)!\n", pathToVersions.c_str());
    return;
  }
  #if VERBOSE
  printf("process <%s>/<%s>\n", dir, file);
  #endif

  int files_to_process = 0;
  if (processed_files.load() != 0)
    fprintf(stderr, "[E] not all files were processed");
  processed_files.store(0);
  std::string filePath = dirPath + file;
  std::string rcsFilePath = filePath+",v";
  ThreadData data;
  for (const auto & entry : fs::directory_iterator(pathToVersions))
  {
    if (entry.is_directory())
      continue;
    //read data
    ++files_to_process;
    if (processor.is_inited())
      processor.emplace(rcsFilePath, entry.path());
    else
      process_file_ver(rootDir, rcsFilePath, entry.path(), data);
    if (files_to_process > max_files_to_process)
    {
      process_queued_files(file, rcsFilePath.c_str(), rcsFilePath, pathToVersions, files_to_process);
      files_to_process = 0;
    }
  }
  process_queued_files(file, rcsFilePath.c_str(), rcsFilePath, pathToVersions, files_to_process);
  files_to_process = 0;
}

static void process_directory(const char *rootDir, const char *dir)
{
  #if VERBOSE
  printf("process dir <%s>\n", dir);
  #endif
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
          strcmp(entry.path().filename().c_str(), blobs_subfolder()) != 0)
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
    printf("processed dir <%s>, saved %gkb\n", dir, double(int64_t(readData-readData_old) - int64_t(writtenData-writtenData_old))/1024.0);
}

int main(int ac, const char* argv[])
{
  auto options = flags::flags{ac, argv};
  options.info("convert_to_blob",
    "conversion utility from old CVS to blob de-duplicated one"
  );

  auto lock_url = options.arg("-lock_url", "Url for lock server. 'offline' - means you disconnected from network, and noone can work with CVS.");
  bool offline = (lock_url == "offline");
  std::string lock_user;
  if (offline)
    lock_user = options.arg("-user", "User name for lock server");
  else
    lock_user = options.arg_or("-user", "not_needed in offline", "User name for lock server");
  auto rootDir = options.arg("-root", "Root dir for CVS");
  init_temp_dir();
  auto tmpDir = options.arg_or("-tmp", "", "Tmp dir for blobs");
  if (tmpDir.length() > 1)
    def_tmp_dir = tmpDir.c_str();
  auto dir = options.arg_or("-dir", "", "Folder to process (inside root)");
  auto file = options.arg_or("-file", "", "File to process (inside dir)");
  auto threads = options.arg_as_or<int>("-j", 0,"concurrency level(threads to run)");
  max_files_to_process = options.arg_as_or<int>("-max_files", 256, "max versions to process before changing rcs. Limits amount of space needed");

  bool help = options.passed("-h", "print help usage");
  if (help || !options.sane()) {
    if (!options.sane())
      printf("Missing required arguments:%s\n", options.print_missing().c_str());
    std::cout << options.usage();
    return help ? 0 : -1;
  }
  tcp_init();
  if (!offline)
  {
    lock_server_socket = lock_register_client(lock_user.c_str(), rootDir.c_str(), lock_url.c_str());
    if (lock_server_socket == -1)
    {
      fprintf(stderr, "[E] Can't connect to lock server\n");
      exit(1);
    }
  } else
    printf("Running in lockless mode. That can damage your repo, if someone will work with it, during conversion.\n");
  if (threads>1)
  {
    printf("using %d threads\n", threads);
    processor.init(rootDir.c_str(), threads);
  }
  set_root(caddressed_fs::get_default_ctx(), rootDir.c_str());
  if (tmpDir.length() > 1)
    caddressed_fs::set_temp_dir(tmpDir.c_str());
  else
  {
    tmpDir = caddressed_fs::blobs_dir_path(caddressed_fs::get_default_ctx());//use blobs folder for default tmp dir
    def_tmp_dir = tmpDir.c_str();
  }
  mkdir(blobs_dir_path(caddressed_fs::get_default_ctx()).c_str(), 0777);

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
