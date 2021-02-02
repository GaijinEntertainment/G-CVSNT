#include <stdio.h>
#include "sha_blob_reference.h"
#include <vector>
#include <string>
#include "concurrent_queue.h"
#include <thread>
#include "blob_network_processor.h"
#include "../ca_blobs_fs/streaming_blobs.h"

#include "error.h"
extern int change_mode(const char *filename, const char *mode_string, int respect_umask);
extern void change_utime(const char* filename, time_t timestamp);
extern int unlink_file(const char* filename);

struct BlobTask
{
  std::string message, dirpath, filename, encoded_hash, file_mode;
  time_t timestamp;
  bool compress;
  enum class Type {Download, Upload} type;
};

static bool download_blob_ref_file(BlobNetworkProcessor *processor, const BlobTask &task);
static bool upload_blob_ref_file(BlobNetworkProcessor *processor, const BlobTask &task);

static bool process_blob_task(BlobNetworkProcessor *download_processor, BlobNetworkProcessor *upload_processor, const BlobTask &task)
{
  if (task.type == BlobTask::Type::Download)
    return download_blob_ref_file(download_processor, task);
  return upload_blob_ref_file(upload_processor, task);
}

struct BackgroundProcessor
{
  BackgroundProcessor():queue(&threads){}
  ~BackgroundProcessor()
  {
  }
  void finishDownloads()
  {
    queue.finishWork();
  }

  bool is_inited() const {return !download_clients.empty();}
  void init();
  void wait();

  static void processor_thread_loop(BackgroundProcessor *processors, BlobNetworkProcessor *download_processor, BlobNetworkProcessor *upload_processor)
  {
    BlobTask task;
    while (processors->queue.wait_and_pop(task))
      process_blob_task(download_processor, upload_processor, task);
  }
  void emplace(BlobTask &&task)
  {
    if (!is_inited())
      return;
    if (threads.empty())
      process_blob_task(download_clients[0].get(), upload_clients[0].get(), task);
    else
    {
      //we can't guarantee, that user will finish download (not cancel it, kill the process or whatever)
      //if he won't, then entries would still be updated with new version, but the file will be old (while be shown as Modified)
      //the correct way would be to keep old entries, and update them only when download is finished
      //that's doable, but will require to:
      //  a) implement locking (so same entries are not updated from different threads, including main)
      //  b) format of entries has to be known here
      //  While not a big deal, is still not that easy task
      // so we use simplest solution - we simply kill old binary file. It can also be renamed, but better to not bother
      if (task.type == BlobTask::Type::Download)
        unlink_file(task.filename.c_str());
      else
      {
        //we preferrably lock file (by opening file handler) to prevent changing during commit
        //but total amount of file handlers is limited, so we trust people to be reasonable
      }
      queue.emplace(std::move(task));
    }
  }
  concurrent_queue<BlobTask> queue;
  std::vector<std::unique_ptr<BlobNetworkProcessor>> download_clients;//soa
  std::vector<std::unique_ptr<BlobNetworkProcessor>> upload_clients;//soa
  std::vector<std::thread> threads;//soa

  std::string base_repo;
  protected:
#if defined(_WIN32) && 0
  httplib::detail::WSInit wsinit_;
#endif
};

static std::unique_ptr<BackgroundProcessor> processor;

//link time resolved dependency
extern void get_download_source(const char *&master_url, const char *&proxy_down_url, int &proxy_down_port, const char *&up_url, int &up_port, const char *&auth_user, const char *&auth_passwd, const char *&repo, int &threads_count);

extern BlobNetworkProcessor *get_http_processor(const char *url, int port, const char* repo, const char *user, const char *passwd);
extern BlobNetworkProcessor *get_kv_processor(const char *url, int port, const char* repo, const char *user, const char *passwd);

void BackgroundProcessor::init()
{
  if (is_inited())
    return;
  const char *master_url = nullptr;
  const char *download_url, *repo, *user = nullptr, *passwd = nullptr; int download_port;
  const char *upload_url; int upload_port;
  int threads_count = std::min(8, std::max(1, (int)std::thread::hardware_concurrency()-1));//limit concurrency to fixed
  get_download_source(master_url, download_url, download_port, upload_url, upload_port, user, passwd, repo, threads_count);
  base_repo = repo;
  if (base_repo[0] != '/')
    base_repo = "/" + base_repo;
  if (base_repo[base_repo.length()] != '/')
    base_repo += "/";
  download_clients.resize(std::max(1, threads_count));
  upload_clients.resize(download_clients.size());
  bool use_http = strstr(download_url, "http") == download_url;
  int http_port = use_http ? download_port : 80;
  for (auto &cli : download_clients)
  {
    BlobNetworkProcessor * pr = nullptr;
    if (!use_http)
    {
      pr = get_kv_processor(download_url, download_port, base_repo.c_str(), user, passwd);
      if (!pr && strcmp(master_url, download_url) != 0)
      {
        pr = get_kv_processor(master_url, 2403, base_repo.c_str(), user, passwd);
        if (pr)
        {
          error(0,0, "Proxy <%s:%d> is not available, switching to master <%s:%d>. Contact IT!\n",
            download_url, download_port, master_url, 2403);
          download_url = master_url;
          download_port = 2403;
        } else
        {
          error(1,0, "Nor proxy <%s:%d> neither master <%s:%d> are not available. Contact IT!\n",
            download_url, download_port, master_url, 2403);
        }
      }
    } else
      pr = get_http_processor(download_url, http_port, base_repo.c_str(), user, passwd);//we can't check if http is available for now
    cli.reset(pr);
  }
  for (auto &cli : upload_clients)
    cli.reset(get_kv_processor(upload_url, upload_port, base_repo.c_str(), user, passwd));

  for (int ti = 0; ti < threads_count; ++ti)
    threads.emplace_back(std::thread(processor_thread_loop, this, download_clients[ti].get(), upload_clients[ti].get()));
}

void BackgroundProcessor::wait()
{
  for (auto &t:threads)
    t.join();
}


extern char *xgetwd();
extern void blob_free(void*);
void add_upload_queue(const char *filename, bool compress, const char *message)
{
  if (!processor)
  {
  	processor.reset(new BackgroundProcessor);
    processor->init();
  }
  if (!processor->upload_clients[0])
    return;
  char *cdir = xgetwd();
  processor->emplace(BlobTask{message, cdir, filename, "", "", 0, compress, BlobTask::Type::Upload});
  blob_free(cdir);
}
int cvs_output(const char *, size_t);
int cvs_outerr(const char *, size_t);

#include <unordered_set>
static std::unordered_set<std::string> download_dirs;
bool dir_being_downloaded_to(const char *s)
{
  return download_dirs.find(s) != download_dirs.end();
}

bool cur_dir_being_downloaded_to()
{
  char *cdir = xgetwd();
  const bool ret = dir_being_downloaded_to(cdir);
  blob_free(cdir);
  return ret;
}

void add_download_queue(const char *message, const char *filename, const char *encoded_hash, const char *file_mode, time_t timestamp)
{
  if (!processor)
  {
  	processor.reset(new BackgroundProcessor);
    processor->init();
  }
  char *cdir = xgetwd();
  download_dirs.emplace(cdir);
  processor->emplace(BlobTask{message, cdir, filename, encoded_hash, file_mode, timestamp, false, BlobTask::Type::Download});
  blob_free(cdir);
}

void wait_threads()
{
  if (processor)
    processor->finishDownloads();
  processor.reset();

}
void rename_file (const char *from, const char *to);
static bool validate_downloaded_blobs = true;

static bool download_blob_ref_file(BlobNetworkProcessor *processor, const BlobTask &task)
{
  std::string temp_filename = task.dirpath +"/_new_";
  temp_filename += task.filename;
  FILE *tmp = fopen(temp_filename.c_str(), "wb");
  if (!tmp)
  {
    char buf[512];std::snprintf(buf, sizeof(buf), "can't write temp %s\n", temp_filename.c_str());
    cvs_outerr(buf, 0);
    return false;
  }
  std::string err;
  caddressed_fs::DownloadBlobInfo info;
  char hashCtx[HASH_CONTEXT_SIZE];
  if (validate_downloaded_blobs)
    init_blob_hash_context(hashCtx, sizeof(hashCtx));
  if (!processor->download(task.encoded_hash.data(),
      [&](const char *data, size_t data_length) {
        return caddressed_fs::decode_stream_blob_data(info, data, data_length,
          [&](const void *data, size_t sz) {
            if (validate_downloaded_blobs)
              update_blob_hash(hashCtx, (const char *)data, sz);
            return fwrite(data, 1, sz, tmp) == sz;
          });//we can easily add hash validation here. but seems unnessasry
      },
      err))
  {
    if (tmp)
      fclose(tmp);
    unlink_file(temp_filename.c_str());
    char buf[256];std::snprintf(buf, sizeof(buf), "can't download <%.64s>, err = %s\n", task.encoded_hash.data(), err.c_str());
    cvs_outerr(buf, 0);
    return false;
  }
  if (tmp)
    fclose(tmp);
  extern size_t get_file_size(const char* file);
  if (validate_downloaded_blobs)
  {
    char recievedHash[65];recievedHash[0]=recievedHash[64]=0;
    unsigned char digest[32];
    finalize_blob_hash(hashCtx, digest, sizeof(digest));
    bin_hash_to_hex_string_64(digest, recievedHash);
    if (memcmp(task.encoded_hash.data(), recievedHash, 64) != 0)
    {
      unlink_file(temp_filename.c_str());
      char buf[256];std::snprintf(buf, sizeof(buf),
        "downloaded hash <%.64s> is different from requested <%.64s> for <%s>\n", recievedHash, task.encoded_hash.data(), task.message.c_str());
      cvs_outerr(buf, 0);
      return false;
    }

    const size_t fsz = get_file_size(temp_filename.c_str());//validate file system
    if (fsz != info.realUncompressedSize)
    {
      unlink_file(temp_filename.c_str());
      char buf[256];std::snprintf(buf, sizeof(buf),
        "file <%s> has size of %lld after downloading, while we downloaded %lld\n", temp_filename.c_str(), (long long)fsz, (long long)info.realUncompressedSize);
      cvs_outerr(buf, 0);
      return false;
    }
  }
  change_utime(temp_filename.c_str(), task.timestamp);
  {
    int status = change_mode (temp_filename.c_str(), task.file_mode.c_str(), 1);
    if (status != 0)
      error (0, status, "cannot change mode of %s", task.filename.c_str());
  }
  std::string fullPath = (task.dirpath+"/")+task.filename;
  rename_file (temp_filename.c_str(), fullPath.c_str());
  if (validate_downloaded_blobs)
  {
    const size_t fsz = get_file_size(fullPath.c_str());//validate file system again
    if (fsz != info.realUncompressedSize)
    {
      char buf[256];std::snprintf(buf, sizeof(buf),
        "file <%s> has size of %lld after downloading, while we downloaded %lld\n", fullPath.c_str(), (long long)fsz, (long long)info.realUncompressedSize);
      cvs_outerr(buf, 0);
      return false;
    }
  }
  char buf[256];std::snprintf(buf, sizeof(buf),"u %s\n", task.message.c_str());
  cvs_output(buf, 0);
  return true;
}
bool is_blob_file_sent(const char* filepath, const char* fileopen, char* hash_encoded);
bool finish_send_blob_file(const char* filepath, const char* fileopen, const char* hash_encoded);

static bool upload_blob_ref_file(BlobNetworkProcessor *processor, const BlobTask &task)
{
  std::string fullPath = (task.dirpath+"/")+task.filename;
  char hash[65]; hash[0]=hash[64] = 0;
  if (is_blob_file_sent(task.filename.c_str(), fullPath.c_str(), hash))
    return true;
  std::string err;
  if (!processor->upload(fullPath.c_str(), task.compress, hash, err))
  {
    char buf[512];std::snprintf(buf, sizeof(buf), "can't upload file <%s>, err = %s\n", fullPath.c_str(), err.c_str());
    cvs_outerr(buf, 0);
    return false;
  }
  if (finish_send_blob_file(task.filename.c_str(), fullPath.c_str(), hash))
  {
    char buf[256];std::snprintf(buf, sizeof(buf),"b %s\n", task.message.c_str());
    cvs_output(buf, 0);
  } else
    error(1,0,"can't finish sending blob %s\n", task.message.c_str());
  return true;
}

