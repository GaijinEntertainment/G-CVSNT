#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "../include/blob_hash_util.h"
#include "../include/blob_client_lib.h"
#include "../sampleImplementation/def_log_printf.cpp"
#include "../include/blob_sockets.h"//move init to out of line

int main(int argc, const char **argv)
{
  if (argc < 3)
    printf("Usage is blob_file_client root [url] [port]\n");
  if (!blob_init_sockets())
  {
    blob_logmessage(LOG_ERROR, "Can't init sockets, %d", blob_get_last_sock_error());
    return 1;
  }
  const char* text = "Some test Text";
  const char *hash_type = "sha256";
  const char* hash = "C38DD6C8F7F1BB198A754532B284B951BDF59C220F2FA48F24051B474812FDC5";//sha256, hash of text
  //const char* hash = "D38DD6C8F7F1BB198A754532B284B951BDF59C220F2FA48F24051B474812FDC5";//spoiled
  const char *root = argv[1];
  uint8_t binhash[32];
  if (!hex_string_to_bin_hash(hash, binhash))
    blob_logmessage(LOG_ERROR, "Can't cvt hash %s", hash);
  char hash2[65] = {0};
  bin_hash_to_hex_string(binhash, hash2);
  printf("hash %s->%s\n", hash, hash2);

  const char *url = argc>2 ? argv[2] : "127.0.0.1";
  int port = argc>3 ? atoi(argv[3]) : 2403;
  intptr_t client = start_blob_push_client(url, port, root);
  if (client == -1)
  {
    blob_logmessage(LOG_ERROR, "Can't connect client %d", blob_get_last_sock_error());
    return 1;
  }
  bool has = false;
  if (blob_check_on_server(client, hash_type, hash, has) != KVRet::OK)
  {
    blob_logmessage(LOG_ERROR, "Can't check %s", blob_get_last_sock_error());
    return 1;
  }
  printf("<%s> %s on server\n", text, has ? "is" : "not");
  int64_t blob_sz = blob_size_on_server(client, hash_type, hash);
  printf("<%s> size on server is %d vs %d\n", hash, int(blob_sz), (int)strlen(text));
  if (blob_sz < 0)
  {
    blob_logmessage(LOG_ERROR, "Can't get size %s", blob_get_last_sock_error());
    return 1;
  }

  blob_logmessage(LOG_NOTIFY, "Pull back <%s>", hash);
  std::vector<char> pulled; int64_t pulledData=-1;
  auto pull = [&]()
  {
    pulled.clear();
    pulledData = blob_pull_from_server(client, hash_type, hash, 0, 0,//pull all
      [&](const char *data, uint64_t at, uint64_t size)
      {
        if (pulled.size() < at + size)
          pulled.resize(at + size);
        if (data)
          memcpy(pulled.data()+at, data, size);
      });
    pulled.push_back('\0');
    printf("<%s> %d %d pulled from server<%s>, eq=%d\n", hash, int(pulledData), int(pulled.size()), pulled.data(), strcmp(pulled.data(), text) == 0);
  };
  pull();
  int64_t pushedData=-1;
  auto push = [&]()
  {
    int64_t pushedData = blob_push_to_server(client, strlen(text), hash_type, hash,
      [&](uint64_t at, uint64_t &data_pulled) {data_pulled = strlen(text) - at; return text + at;});
    printf("<%s>, we pushed %d, size on server now is %d vs %d\n", hash, (int)pushedData, (int)blob_size_on_server(client, hash_type, hash), (int)strlen(text));
    if (pushedData < 0)
      blob_logmessage(LOG_ERROR, "Can't push data %s", blob_get_last_sock_error());
  };
  push();
  pull();
  push();
  stop_blob_push_client(client);
  blob_close_sockets();
}