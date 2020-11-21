#include <stdio.h>
#include "httplib.h"
#include "sha_blob_format.h"
#include "sha_blob_reference.h"
#include <vector>
#include <string>
#if !_MSC_VER
#define _snprintf snprintf
#endif

bool download_blob_ref_file(const char *url, int port, const char *repo, const char *to, const char *encoded_sha256)
{
  char buf[128];
  _snprintf(buf, sizeof(buf),
     "/%s/%c%c/%c%c/%.*s",
     repo,
     encoded_sha256[0], encoded_sha256[1],
     encoded_sha256[2], encoded_sha256[3],
     64-4, encoded_sha256+4);

  std::vector<char> temp_buf;
  httplib::Client cli(url, port);
  cli.set_keep_alive(true);
  cli.set_tcp_nodelay(true);
  auto initialRes = cli.Get("/");
  if (!cli.is_socket_open() || initialRes->status != 200)
  {
    printf("can't connect <%s:%d>, err = %d, %s\n", url, port, initialRes->status, httplib::detail::status_message(initialRes->status));
    return false;
  }
  //cli.enable_server_certificate_verification(true);//if we will switch to https
  //cli.set_basic_auth("user", "passwd");//read me from User registry once
  auto res = cli.Get(buf,
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
