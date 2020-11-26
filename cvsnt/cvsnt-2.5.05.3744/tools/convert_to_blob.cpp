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
#include "tsl/sparse_map.h"
#ifdef _WIN32
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif

static size_t max_files_to_process = 64;// to limit amount of double sized data

static bool fastest_conversion = true;//if true, we won't repack, just calc sha256 and put zlib block as is.
static void ensure_blob_mtime(const char* verfile, const char *blob_file)
{
  time_t blob_mtime = get_file_mtime(blob_file);
  time_t ver_atime = get_file_atime(verfile);
  if (ver_atime > blob_mtime)
    set_file_mtime(blob_file, ver_atime);
}

static int find_rcs_data(const char* rcs_data, const char *text_to_find, size_t text_to_find_length)
{
  const char *data = rcs_data;
  #define TEXT_COMMAND "@\ntext\n@"
  while (const char *text_command = strstr(data, TEXT_COMMAND))
  {
    //text command found
    bool text_found = false;
    for (data = text_command + strlen(TEXT_COMMAND); *data; ++data)
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
  const char* data = rcs_data.c_str();
  bool found = false;
  int text_found_start;
  while ((text_found_start = find_rcs_data(data, text_to_replace.c_str(), text_to_replace.length())) >= 0)
  {
    rcs_data.replace(text_found_start, text_to_replace.length(), replace_text);
    text_found_start += data - rcs_data.c_str();
    data = rcs_data.c_str() + text_found_start + replace_text_length;
    found = true;
  }
  return found;
}

static std::atomic<uint64_t> deduplicatedData = 0, readData = 0, writtenData = 0;
std::atomic<uint32_t> processed_files = 0;

static std::mutex lock_id_mutex;
static std::mutex  file_version_remap_mutex;
static tsl::sparse_map<std::string, char[sha256_encoded_size+1]> file_version_remap;

struct ThreadData
{
  std::vector<char> fileUnpackedData, filePackedData, sha_file_name;
};

static int lock_server_socket = -1;

size_t get_lock(const char *lock_file_name)
{
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
      sleep(500);
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
  std::vector<char> &fileUnpackedData = data.fileUnpackedData, &filePackedData  = data.filePackedData, &sha_file_name = data.sha_file_name;
  size_t lockId = get_lock(filePath.c_str());//Lock1
  const auto sz = fs::file_size(path);
  fileUnpackedData.resize(sz);
  std::ifstream f(path, std::ios::in | std::ios::binary);
  f.read(fileUnpackedData.data(), sz);
  unlock(lockId, filePath.c_str());//unLock1, so some other utility can lock

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
      fclose(dest);
      if (!isreadable(sha_file_name))
      {
        if (rename_file (temp_filename, sha_file_name, false))//we dont care if blob is written independently
          wr = hdr.compressedLen + sizeof(hdr);
      } else
        unlink_file(temp_filename);
      blob_free (temp_filename);
    } else
    {
      ensure_blob_mtime(srcPathString.c_str(), sha_file_name);//so later incremental repack still works correctly
    }
  } else
    wr = write_binary_blob(rootDir, sha256, path.c_str(), fileUnpackedData.data(), fileUnpackedData.size(), BlobPackType::FAST, false);

  //now we can write reference
  if (!wr)
  {
    #if VERBOSE
    printf("deduplication %d for %s\n", int(sz), filePath.c_str());
    #endif
    deduplicatedData += sz;
  }
  std::unique_lock<std::mutex> lockGuard(file_version_remap_mutex);
  auto &i = file_version_remap[path.filename().string()];
  encode_sha256(sha256, i, sha256_encoded_size+1);
  writtenData += wr;
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
    while (files_to_process != processed_files.load())
      sleep(100);
    processed_files.store(0);
  }

  {
    //replace references
    size_t lockId = get_lock(lock_rcs_file_name);//lock2
    std::string rcsData;
    {
      std::ifstream f(rcs_file_name_full_path);
      std::stringstream buffer;
      buffer << f.rdbuf();
      rcsData = buffer.str();
    }
    std::string oldVerRCS;
    char sha_ref[blob_reference_size+2];
    memcpy(sha_ref, SHA256_REV_STRING, sha256_magic_len);
    sha_ref[blob_reference_size] = '@';
    sha_ref[blob_reference_size+1] = 0;
    tsl::sparse_map<std::string, char[sha256_encoded_size+1]> file_version_remap_process;
    {
      std::unique_lock<std::mutex> lockGuard(file_version_remap_mutex);
      file_version_remap_process.swap(file_version_remap);
    }
    std::string filenameDir = std::string(filename) + "/";
    for (auto &fv: file_version_remap_process)
    {
      oldVerRCS = filenameDir + fv.first + "@";
      if (fv.first[fv.first.length() - 1] == 'z' && fv.first[fv.first.length() - 2] == '#')
        oldVerRCS.erase(oldVerRCS.length()-2);
      memcpy(sha_ref+sha256_magic_len, fv.second, sha256_encoded_size);
      bool replaced = replace_rcs_data(rcsData, oldVerRCS, sha_ref, sizeof(sha_ref)-1);
      if (!replaced)
      {
        printf("[E] can't find references to <%s> in <%s>. Keeping file!\n", oldVerRCS.c_str(), rcs_file_name_full_path.c_str());
        //it is dangerous to remove file then
        continue;
      }
      char *tempFilename;
      FILE *tmpf = cvs_temp_file (&tempFilename, "wb");
      if (tmpf)
      {
        if (fwrite(rcsData.c_str(), 1, rcsData.length(), tmpf) == rcsData.length())
        {
          if (rename_file(tempFilename, rcs_file_name_full_path.c_str(), false))
          {
            std::string fullPath = (path_to_versions + "/") + fv.first;
            unlink(fullPath.c_str());
            rmdir(path_to_versions.c_str());//will delete only empty folder
          } else
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
    if (processor.is_inited())
    {
      ++files_to_process;
      processor.emplace(rcsFilePath, entry.path());
    } else
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
  if (tmpDir.length() > 1)
    def_tmp_dir = tmpDir.c_str();
  auto dir = options.arg_or("-dir", "", "Folder to process (inside root)");
  auto file = options.arg_or("-file", "", "File to process (inside dir)");
  auto threads = options.arg_as_or<int>("-j", 0,"concurrency level(threads to run)");
  max_files_to_process = options.arg_as_or<int>("-j", 64, "max versions to process before changing rcs. Limits amount of space needed");
  fastest_conversion = !options.passed("-repack", "repack zlib to zstd, slow");

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
