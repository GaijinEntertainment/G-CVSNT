#include <stdio.h>
#include "sha_blob_format.h"
#include "sha_blob_reference.h"
#include <vector>
#include <string>
#include "concurrent_queue.h"
#include <thread>
#include "blob_network_processor.h"

#include "error.h"
extern int change_mode(const char *filename, const char *mode_string, int respect_umask);
extern void change_utime(const char* filename, time_t timestamp);
extern int unlink_file(const char* filename);

struct BlobTask
{
  std::string message, dirpath, filename, encoded_sha256, file_mode;
  time_t timestamp;
  enum class Type {Download, Upload} type;
};

static bool download_blob_ref_file(BlobNetworkProcessor *processor, const char *base_repo, const BlobTask &task);

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

  static void downloader_thread_loop(BackgroundDownloader *downloader, BlobNetworkProcessor *processor)
  {
    BlobTask task;
    while (downloader->queue.wait_and_pop(task))
      download_blob_ref_file(processor, downloader->base_repo.c_str(), task);
  }
  void emplace(BlobTask &&task)
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
  std::vector<std::unique_ptr<BlobNetworkProcessor>> clients;//soa

  concurrent_queue<BlobTask> queue;
  std::string base_repo;
  protected:
#if defined(_WIN32) && 0
  httplib::detail::WSInit wsinit_;
#endif
};

static std::unique_ptr<BackgroundDownloader> downloader;

//link time resolved dependency
extern void get_download_source(const char *&url, int &port, const char *&auth_user, const char *&auth_passwd, const char *&repo, int &threads_count);

extern BlobNetworkProcessor *get_http_processor(const char *url, int port, const char *user, const char *passwd);

void BackgroundDownloader::init()
{
  if (is_inited())
    return;
  const char *url, *repo, *user = nullptr, *passwd = nullptr; int port;
  int threads_count = std::min(8, std::max(1, (int)std::thread::hardware_concurrency()-1));//limit concurrency to fixed
  get_download_source(url, port, user, passwd, repo, threads_count);
  base_repo = repo;
  if (base_repo[0] != '/')
    base_repo = "/" + base_repo;
  if (base_repo[base_repo.length()] != '/')
    base_repo += "/";
  clients.resize(std::max(1, threads_count));
  for (auto &cli : clients)
    cli.reset(get_http_processor(url, port, user, passwd));
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
  downloader->emplace(BlobTask{message, cdir, filename, encoded_sha256, file_mode, timestamp, BlobTask::Type::Download});
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

static bool download_blob_ref_file(BlobNetworkProcessor *processor, const char *base_repo, const BlobTask &task)
{
  std::vector<char> temp_buf;
  std::string err;
  if (!processor->download(base_repo, task.encoded_sha256.data(),
      [&](const char *data, size_t at, size_t data_length) {
        if (!data)
          temp_buf.reserve(at + data_length);
        else
          temp_buf.insert(temp_buf.begin() + std::min(at, temp_buf.size()), data, data+data_length);
        return true;
      },
      err))
  {
    printf("can't download <%.64s>, err = %d, %s\n", task.encoded_sha256.data(), err.c_str());
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
