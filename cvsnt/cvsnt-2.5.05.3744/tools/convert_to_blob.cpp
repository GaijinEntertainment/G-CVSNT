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
typedef tsl::sparse_map<std::string, char[hash_encoded_size+1]> db_map;

static void ensure_blob_mtime(time_t ver_atime, const char *blob_file)
{
  time_t blob_mtime = get_file_mtime(blob_file);
  if (ver_atime > blob_mtime)
    set_file_mtime(blob_file, ver_atime);
}

static size_t get_lock(const char *lock_file_name);
static void unlock(size_t lockId, const char *debug_text = nullptr);
static bool replace_rcs_data(std::string &rcs_data, const std::string &text_to_replace, const char *replace_text, size_t replace_text_length);

void actual_rcs_replace(const char *filename, const char *lock_rcs_file_name, const std::string &rcs_file_name_full_path, const std::string &path_to_versions, const db_map &db, bool remove_old)
{
  size_t lockId = get_lock(lock_rcs_file_name);//lock2
  mode_t rcs_mode; size_t rcs_sz;
  if (!get_file_mode_and_size(rcs_file_name_full_path.c_str(), rcs_mode, rcs_sz))
  {
    unlock(lockId, lock_rcs_file_name);//unlock2
    fprintf(stderr, "[E] no rcs file <%s>\n", rcs_file_name_full_path.c_str());
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
  for (auto &fv: db)
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
      if (remove_old)
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

  if (rcsChanged && remove_old)//unlink all replaced files
  {
    std::string pathtoVersionDir = path_to_versions + "/";
    for (auto &fv: db)
      if (keep_files_list.find(fv.first) == keep_files_list.end())
        unlink((pathtoVersionDir + fv.first).c_str());
    rmdir(path_to_versions.c_str());//will delete only empty folder
  }
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
static std::atomic<uint32_t> processed_files = 0;
static std::atomic<uint32_t> already_in_db = 0;

static std::mutex lock_id_mutex;
static std::mutex  file_version_remap_mutex;
static db_map file_version_remap;
static db_map assist_db;
static bool cvt_rcs_files = true, remove_old_blobs = true;
static std::string assist_db_file_name = "";

struct ThreadData
{
  std::vector<char> fileUnpackedData, filePackedData;
};

static int lock_server_socket = -1;

static size_t get_lock(const char *lock_file_name)
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

static void unlock(size_t lockId, const char *debug_text)
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
  char hash_encoded[hash_encoded_size+1];hash_encoded[0] = 0;hash_encoded[hash_encoded_size] = 0;

  time_t ver_atime = get_file_mtime(srcPathString.c_str());
  if (ver_atime > time(NULL) - 60)//if file is written less than 60 seconds ago, it may be is still being written by old CVS. Old CVS wasn't writing atomic! It can only happen if we are in old CVS
    return;

  const size_t fsz = get_file_size(srcPathString.c_str());
  readData += fsz;

  auto assistIt = assist_db.find(srcPathString);
  if (assistIt != assist_db.end())
    already_in_db++;

  PushResult pr = PushResult::IO_ERROR;
  if (assistIt != assist_db.end() && get_size(get_default_ctx(), assistIt->second) > 0)
  {
    pr = PushResult::DEDUPLICATED;
    memcpy(hash_encoded, assistIt->second, 64);
  } else
  {
    int fd = open(srcPathString.c_str(), O_RDONLY);
    if (fd == -1)
      error(1, errno, "can't open for read <%s> of %d sz", srcPathString.c_str(), (int)fsz);
    const char* begin = (const char*)(mmap(NULL, fsz, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0));
    close(fd);
    const size_t unpackedSz = wasPacked  ? ntohl(*(int*)begin) : fsz;
    const size_t readDataSz = wasPacked ? fsz - sizeof(int) : fsz;
    const char* readData = wasPacked ? begin + sizeof(int) : begin;
    //just push data as is
    BlobHeader hdr = get_header(wasPacked ? zlib_magic : noarc_magic, unpackedSz, 0);
    PushData* pd = start_push(get_default_ctx(), assistIt == assist_db.end() ? nullptr : assistIt->second);
    if (!stream_push(pd, &hdr, sizeof(hdr)) || !stream_push(pd, readData, readDataSz))
    {
      error(0, errno, "can't push blob for <%s>", srcPathString.c_str());
      destroy(pd);
    } else
      pr = finish(pd, hash_encoded);
    munmap((void*)begin, fsz);
  }
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

static void finish_processing(int files_to_process)
{
  if (!processor.is_inited())//finish conversion
    return;
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

static void read_db(const char* DBName, db_map &db)
{
  FILE *dbf = fopen(DBName, "rb");
  if (!dbf)
  {
    printf("Can't read database %s\n", DBName);
    return;
  }
  char buf[1024]; int line = 0;
  std::string key;
  for (;fgets(buf, sizeof(buf), dbf); ++line)
  {
    key = "";
    char *hst = strstr(buf, ":");
    if (!hst || hst == buf || !is_encoded_hash(hst+1, strlen(hst+1)))
    {
      fprintf(stderr, "[E] invalid DB at line %d, <%s>\n", line, buf);
      if (!hst || hst == buf)
        break;
      fprintf(stderr, "[E] incorrect hash %s (sz = %d)\n", hst+1, (int)strlen(hst+1));
      *hst = 0;key = buf;
      if (strlen(hst+1) < 64 || db.find(key) != db.end())
        continue;
    }
    *hst = 0;hst++;
    hst[64] = 0;
    key = hst;
    if (db.find(key) == db.end())
      memcpy(db[buf], hst, 65);
  }
  fclose(dbf);
  printf("Database %s was read %d lines processed, with %d unique entries\n", DBName, line, (int)db.size());
}

static bool write_db(const char* DBName, const std::string &path_to_versions, const db_map &db, const char *mode = "a+")
{
  FILE *dbf = fopen(DBName, mode);
  if (!dbf)
    return false;
  for (auto &fi: db)
  {
    if (fprintf(dbf, "%s%s%s:%s\n", path_to_versions.c_str(), path_to_versions.length() ? "/" : "", fi.first.c_str(), fi.second)<0)
    {
      fprintf(stderr, "[E] can't write DB\n");
      fclose(dbf);
      return false;
    }
  }
  fclose(dbf);
  return true;
}


void process_queued_files(const char *filename, const char *lock_rcs_file_name, const std::string &rcs_file_name_full_path, const std::string &path_to_versions, const db_map &db, int files_to_process)
{
  finish_processing(files_to_process);
  if (assist_db_file_name.length() && !write_db(assist_db_file_name.c_str(), path_to_versions, db))
    assist_db_file_name = "";
  if (cvt_rcs_files)//if we are just writing assist DB, we don't need change rcs file
    actual_rcs_replace(filename, lock_rcs_file_name, rcs_file_name_full_path, path_to_versions, db, remove_old_blobs);//replace references, remove old
  file_version_remap.clear();
}

static void prepare_strings(const char *rootDir, const char *dir, const char *file,
  std::string &dirPath, std::string &pathToVersions, std::string &filePath, std::string &rcsFilePath)
{
  dirPath = rootDir;
  if (dirPath[dirPath.length()-1] != '/' && dir[0]!='/')
    dirPath+="/";
  dirPath += dir;
  if (dirPath[dirPath.length()-1] != '/')
    dirPath+="/";
  pathToVersions = (dirPath + "CVS/")+file;
  filePath = dirPath + file;
  rcsFilePath = filePath+",v";
}

static void process_file(const char *rootDir, const char *dir, const char *file)
{
  std::string dirPath, pathToVersions, filePath, rcsFilePath;
  prepare_strings(rootDir, dir, file, dirPath, pathToVersions, filePath, rcsFilePath);

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
      process_queued_files(file, rcsFilePath.c_str(), rcsFilePath, pathToVersions, file_version_remap, files_to_process);
      files_to_process = 0;
    }
  }
  process_queued_files(file, rcsFilePath.c_str(), rcsFilePath, pathToVersions, file_version_remap, files_to_process);
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

void process_db(const char *rootDir, const db_map &db)
{
  tsl::sparse_map<std::string, db_map> rcs_files_map;
  int entries = 0;
  {
    const size_t rootDirLen = strlen(rootDir);
    std::string rcs_path;
    for (const auto &i: db)
    {
      if (strncmp(i.first.c_str(), rootDir, rootDirLen) != 0)
      {
        fprintf(stderr, "incorrect db entry path (not from root): %s, root = %s", i.first.c_str(), rootDir);
        continue;
      }
      rcs_path = i.first.c_str() + rootDirLen + (rootDir[rootDirLen-1] == '/' ? 0 : 1);
      const size_t at = rcs_path.rfind("/CVS/");
      if (at == std::string::npos)
      {
        fprintf(stderr, "incorrect db entry path (no /CVS/): %s", rcs_path.c_str());
        continue;
      }
      rcs_path.erase(at, 4);
      const size_t verAt = rcs_path.rfind("/");
      rcs_path[verAt] = 0;
      memcpy(rcs_files_map[rcs_path.c_str()][rcs_path.c_str() + verAt+1], i.second, 65);
      entries++;
    }
    printf("DB conversion finished, %d processed entries, %d rcs files\n", entries, (int)rcs_files_map.size());
  }

  std::vector<std::pair<std::string, db_map>> rcs_files;rcs_files.resize(rcs_files_map.size());
  size_t vi = 0;
  for (auto &&i:rcs_files_map)
  {
    rcs_files[vi].first = std::move(i.first);
    rcs_files[vi].second = std::move(i.second);
    ++vi;
  }
  rcs_files_map.clear();
  printf("converting RCS\n");
  std::atomic<int> processed = 0, last_processed = 0;
  const size_t cnt = rcs_files.size();
  const int startTime = time(NULL);
  #pragma omp parallel for
  for (size_t i = 0; i < cnt; i++)
  {
    const auto &rcs_map = *(rcs_files.cbegin() + i);
    std::string dir, file;
    std::string dirPath, pathToVersions, filePath, rcsFilePath;
    dir = rcs_map.first;
    size_t e = dir.rfind("/");
    if (e == std::string::npos)
    {
      file = dir;
      dir = "";
    } else
    {
      dir[e] = 0;
      file = dir.c_str() + e + 1;
    }
    #if VERBOSE
    printf("process %d %s: <%s>/<%s>\n", (int)i, rcs_map.first.c_str(), dir.c_str(), file.c_str());
    #endif
    prepare_strings(rootDir, dir.c_str(), file.c_str(), dirPath, pathToVersions, filePath, rcsFilePath);
    actual_rcs_replace(file.c_str(), rcsFilePath.c_str(), rcsFilePath, pathToVersions, rcs_map.second, false);
    processed += rcs_map.second.size();
    const int cProcessed = processed.load();
    if (cProcessed > last_processed.load()+4096)
    {
      last_processed = cProcessed;
      const int ctime = time(NULL);
      printf("%d: processed %d/%d, eta left %gmin\n", (int)ctime, cProcessed, entries, (double(entries-cProcessed)*cProcessed)/double(ctime - startTime+1)/60.);
    }
  }
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
  auto tmpDirRCS = options.arg_or("-tmp_rcs", "", "Tmp dir for RCS");
  if (tmpDirRCS.length() > 1)
    def_tmp_dir = tmpDirRCS.c_str();

  auto tmpDirBlobs = options.arg_or("-tmp_blobs", "", "Tmp dir for blobs");
  auto dir = options.arg_or("-dir", "", "Folder to process (inside root)");
  auto file = options.arg_or("-file", "", "File to process (inside dir)");
  auto threads = options.arg_as_or<int>("-j", 0,"concurrency level(threads to run)");
  cvt_rcs_files = !options.passed("-no_rcs", "Don't change rcs files (will not remove old versions)");
  remove_old_blobs = !options.passed("-no_remove", "Don't remove old version files, when changing rcs.");
  assist_db_file_name = options.arg_or("-db", "cvs_cvt_db.txt", "Assist DB path");
  max_files_to_process = options.arg_as_or<int>("-max_files", 256, "max versions to process before changing rcs. Limits amount of space needed");
  bool use_db_only = options.passed("-use_db_only", "Use DB and RCS only for source");

  bool help = options.passed("-h", "print help usage");
  if (help || !options.sane()) {
    if (!options.sane())
      printf("Missing required arguments:%s\n", options.print_missing().c_str());
    std::cout << options.usage();
    return help ? 0 : -1;
  }

  if (assist_db_file_name.length())
    read_db(assist_db_file_name.c_str(), assist_db);
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
  set_root(caddressed_fs::get_default_ctx(), rootDir.c_str());
  if (tmpDirBlobs.length() > 1)
    caddressed_fs::set_temp_dir(tmpDirBlobs.c_str());

  if (tmpDirRCS.length() < 1)
  {
    tmpDirRCS = caddressed_fs::blobs_dir_path(caddressed_fs::get_default_ctx());//use blobs folder for default tmp dir
    def_tmp_dir = tmpDirRCS.c_str();
  }
  mkdir(blobs_dir_path(caddressed_fs::get_default_ctx()).c_str(), 0777);

  if (use_db_only)
  {
    if (assist_db.empty())
      fprintf(stderr, "Can't use only DB for conversion, db is empty");
    process_db(rootDir.c_str(), assist_db);
  } else
  {
    if (threads>1)
    {
      printf("using %d threads\n", threads);
      processor.init(rootDir.c_str(), threads);
    }
    if (file.length()>0)
      process_file(rootDir.c_str(), dir.c_str(), file.c_str());
    else
      process_directory(rootDir.c_str(), dir.c_str());
    processor.finish();
  }
  printf("written %g mb, saved %g mb, de-duplicated %g mb, already_in_db = %d\n", double(writtenData)/(1<<20),
    (double(readData)-double(writtenData))/(1<<20),
    double(deduplicatedData)/(1<<20),
    already_in_db.load());

  cvs_tcp_close(lock_server_socket);
  if (assist_db_file_name.length())
  {
    assist_db.clear();
    read_db(assist_db_file_name.c_str(), assist_db);
    std::string tmp = assist_db_file_name + "_tmp";
    if (write_db(tmp.c_str(), std::string(""), assist_db, "wb+"))
      ;//rename_file(tmp.c_str(), assist_db_file_name.c_str(), false);
  }
  return 0;
}
