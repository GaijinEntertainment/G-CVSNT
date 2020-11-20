#include <stdio.h>
#include "httplib.h"
#include "sha_blob_format.h"
#include <vector>
#include <string>
void rename_file (const char *from, const char *to);

size_t decode_binary_blob(const char *context, const void *data, size_t fileLen, void **out_data, bool &need_free);

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
  void *data = nullptr; bool need_free = false;
  size_t decodedData = decode_binary_blob(to, temp_buf.data(), temp_buf.size(), &data, need_free);
  std::string temp_filename = "_new_";
  temp_filename+=to;
  FILE *tmp = fopen(temp_filename.c_str(), "wb");
  if (fwrite(data, 1, decodedData, tmp) != decodedData)
  {
    fclose(tmp);
    return false;
  }
  fclose(tmp);
  if (need_free)
    free(data);
  rename_file (temp_filename.c_str(), to);
  return true;
}
