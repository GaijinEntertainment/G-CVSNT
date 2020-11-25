#include <stdio.h>
#include "httplib.h"
#include "sha_blob_format.h"
#include "sha_blob_reference.h"
#include <vector>
#include <string>
#include "concurrent_queue.h"
#include <thread>

#include "error.h"
extern int change_mode(const char *filename, const char *mode_string, int respect_umask);
extern void change_utime(const char* filename, time_t timestamp);
extern int unlink_file(const char* filename);

//may be better to switch to libcurl https://curl.se/libcurl/c/http2-download.html
struct BlobDownloadTask
{
  std::string message, dirpath, filename, encoded_sha256, file_mode;
  time_t timestamp;
};

static bool download_blob_ref_file(httplib::Client *client, const char *base_repo, const BlobDownloadTask &task);

struct BackgroundDownloader
{
  BackgroundDownloader():queue(&threads){}
  ~BackgroundDownloader()
  {
  }
  void finishDownloads()
  {
    queue.finishWork();
  }

  bool is_inited() const {return !clients.empty();}
  void init();
  void wait();

  static void downloader_thread_loop(BackgroundDownloader *downloader, httplib::Client *client)
  {
    BlobDownloadTask task;
    while (downloader->queue.wait_and_pop(task))
      download_blob_ref_file(client, downloader->base_repo.c_str(), task);
  }
  void emplace(BlobDownloadTask &&task)
  {
    if (!is_inited())
      return;
    if (threads.empty())
      download_blob_ref_file(clients[0].get(), base_repo.c_str(), task);
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
      unlink_file(task.filename.c_str());
      queue.emplace(std::move(task));
    }
  }
  std::vector<std::thread> threads;//soa
  std::vector<std::unique_ptr<httplib::Client>> clients;//soa

  concurrent_queue<BlobDownloadTask> queue;
  std::string base_repo;
  protected:
#if defined(_WIN32) && 0
  httplib::detail::WSInit wsinit_;
#endif
};

static std::unique_ptr<BackgroundDownloader> downloader;

//link time resolved dependency
extern void get_download_source(const char *&url, int &port, const char *&auth_user, const char *&auth_passwd, const char *&repo, int &threads_count);

void BackgroundDownloader::init()
{
  if (is_inited())
    return;
  const char *url, *repo, *user = nullptr, *passwd = nullptr; int port;
  int threads_count = std::min(4, std::max(1, (int)std::thread::hardware_concurrency()-2));//limit concurrency to fixed
  get_download_source(url, port, user, passwd, repo, threads_count);
  clients.resize(std::max(1, threads_count));
  for (auto &cli : clients)
  {
    cli.reset(new httplib::Client(url, port));
    cli->set_keep_alive(true);
    cli->set_tcp_nodelay(true);
    //cli.enable_server_certificate_verification(true);//if we will switch to https
    if (user && passwd)
      cli->set_basic_auth(user, passwd);//read me from User registry once
    base_repo = repo;
    if (base_repo[0] != '/')
      base_repo = "/" + base_repo;
    if (base_repo[base_repo.length()] != '/')
      base_repo += "/";
  }
  for (int ti = 0; ti < threads_count; ++ti)
    threads.emplace_back(std::thread(downloader_thread_loop, this, clients[ti].get()));
}

void BackgroundDownloader::wait()
{
  for (auto &t:threads)
    t.join();
}


extern char *xgetwd();

void add_download_queue(const char *message, const char *filename, const char *encoded_sha256, const char *file_mode, time_t timestamp)
{
  if (!downloader)
  {
  	downloader.reset(new BackgroundDownloader);
    downloader->init();
  }
  char *cdir = xgetwd();
  downloader->emplace(BlobDownloadTask{message, cdir, filename, encoded_sha256, file_mode, timestamp });
  blob_free(cdir);
}

void wait_for_download_queue()
{
  if (!downloader || !downloader->is_inited())
    return;
  downloader->wait();
}

void wait_threads()
{
  if (downloader)
    downloader->finishDownloads();
  downloader.reset();
}

static bool download_blob_ref_file(httplib::Client *client, const char *base_repo, const BlobDownloadTask &task)
{
  if (!client)
    return false;
  char buf[128];
  std::snprintf(buf, sizeof(buf),
     "%s%c%c/%c%c/%.*s",
     base_repo,
     task.encoded_sha256[0], task.encoded_sha256[1],
     task.encoded_sha256[2], task.encoded_sha256[3],
     64-4, task.encoded_sha256.data()+4);

  std::vector<char> temp_buf;
  auto res = client->Get(buf,
    [&](const char *data, size_t data_length) {
      //printf("receive %lld\n", data_length);
      temp_buf.insert(temp_buf.end(), data, data+data_length);
      return true;
    },
    [&](uint64_t len, uint64_t total) {
      //printf("len %lld out of %lld\n", len, total);
      temp_buf.reserve(total);
      return true;
    }
  );
  if (!res || res->status != 200)
  {
    printf("can't download <%s>, err = %d, %s\n", buf, res ? res->status : -1, res ? httplib::detail::status_message(res->status) : "unknown");
    return false;
  }
  void *data = nullptr; bool need_free = false;
  size_t decodedData = decode_binary_blob(task.filename.c_str(), temp_buf.data(), temp_buf.size(), &data, need_free);
  std::string temp_filename = task.dirpath +"/_new_";
  temp_filename += task.filename;
  FILE *tmp = fopen(temp_filename.c_str(), "wb");
  if (fwrite(data, 1, decodedData, tmp) != decodedData)
  {
    printf("can't write <%s>\n", temp_filename.c_str());
    fclose(tmp);
    return false;
  }
  fclose(tmp);
  if (need_free)
    free(data);

  change_utime(temp_filename.c_str(), task.timestamp);
  {
    int status = change_mode (temp_filename.c_str(), task.file_mode.c_str(), 1);
    if (status != 0)
      error (0, status, "cannot change mode of %s", task.filename.c_str());
  }
  std::string fullPath = (task.dirpath+"/")+task.filename;
  rename_file (temp_filename.c_str(), fullPath.c_str());
  printf("U %s\n", task.message.c_str());
  return true;
}