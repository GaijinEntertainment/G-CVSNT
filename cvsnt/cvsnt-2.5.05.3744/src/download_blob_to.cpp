#include <stdio.h>
#include "httplib.h"
#include "sha_blob_format.h"
#include "sha_blob_reference.h"
#include <vector>
#include <string>
#include <mutex>
#include <thread>

//may be better to switch to libcurl https://curl.se/libcurl/c/http2-download.html
static std::unique_ptr<httplib::Client> client;
static std::string base_repo;

//link time resolved dependency
extern void get_download_source(const char *&url, int &port, const char *&auth_user, const char *&auth_passwd, const char *&repo);

void init_download_source()
{
  if (client)
    return;
  const char *url, *repo, *user = nullptr, *passwd = nullptr; int port;
  get_download_source(url, port, user, passwd, repo);
  std::unique_ptr<httplib::Client> cli(new httplib::Client(url, port));
  cli->set_keep_alive(true);
  cli->set_tcp_nodelay(true);
  //cli.enable_server_certificate_verification(true);//if we will switch to https
  if (user && passwd)
    cli->set_basic_auth(user, passwd);//read me from User registry once
  auto initialRes = cli->Get("/");
  if (!cli->is_socket_open() || initialRes->status != 200)
  {
    printf("can't connect <%s:%d>, err = %d, %s\n", url, port, initialRes->status, httplib::detail::status_message(initialRes->status));
    return;
  }
  base_repo = repo;
  if (base_repo[0] != '/')
    base_repo = "/" + base_repo;
  if (base_repo[base_repo.length()] != '/')
    base_repo += "/";
  std::swap(client, cli);
}

static bool download_blob_ref_file(const char *to, const char *encoded_sha256)
{
  char buf[128];
  std::snprintf(buf, sizeof(buf),
     "%s%c%c/%c%c/%.*s",
     base_repo.c_str(),
     encoded_sha256[0], encoded_sha256[1],
     encoded_sha256[2], encoded_sha256[3],
     64-4, encoded_sha256+4);

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
  if (res->status != 200)
  {
    printf("can't download <%s>, err = %d, %s\n", buf, res->status, httplib::detail::status_message(res->status));
    return false;
  }
  void *data = nullptr; bool need_free = false;
  size_t decodedData = decode_binary_blob(to, temp_buf.data(), temp_buf.size(), &data, need_free);
  std::string temp_filename = "_new_";
  temp_filename+=to;
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
  rename_file (temp_filename.c_str(), to);
  return true;
}


#include <sys/utime.h>
#include "error.h"
extern int change_mode(const char *filename, const char *mode_string, int respect_umask);
extern void change_utime(const char* filename, time_t timestamp);

void add_download_queue(const char *filename, const char *encoded_sha256, const char *file_mode, time_t timestamp)
{
  init_download_source();
  if (!download_blob_ref_file(filename, encoded_sha256))
    error (1, errno, "downloading %s", filename);

  change_utime(filename, timestamp);
  {
    int status = change_mode (filename, file_mode, 1);
    if (status != 0)
      error (0, status, "cannot change mode of %s", filename);
  }
}
