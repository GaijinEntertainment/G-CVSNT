#pragma once
#include <stdint.h>
#include <functional>

intptr_t start_blob_push_client(const char *url, int port, const char *root);
bool stop_blob_push_client(intptr_t &client);
enum class KVRet{OK, Error, Fatal}; //on fatal protocol error, client will become invalid

KVRet blob_check_on_server(intptr_t &client, const char *hash_type, const char *hash_hex_str, bool &has);
int64_t blob_size_on_server(intptr_t &client, const char *hash_type, const char *hash_hex_str); //<-1 if fatal error, -1 if file is missing

int64_t blob_pull_from_server(intptr_t &client, const char *hash_type, const char *hash_hex_str,
  uint64_t from, uint64_t sz,
  std::function<void(const char *data, uint64_t at, uint64_t size)> cb);//<0 if error

KVRet blob_start_pull_from_server(intptr_t &sockfd, const char *hash_type, const char *hash_hex_str,
  uint64_t &from, uint64_t &sz);
int64_t blob_pull_some_from_server(intptr_t &sockfd, char *data, size_t data_capacity, int64_t &szLeft);//if <= 0, socket closed. Should not be called when szLeft = 0

int64_t blob_push_to_server(intptr_t &client, size_t blob_sz,
  const char *hash_type, const char *hash_hex_str,
  std::function<const char*(uint64_t at, size_t &data_pulled)> pull_data);//<0 if error. pull_data returns nullptr on error

KVRet blob_stream_to_server(intptr_t &client,
  const char *hash_type, const char *hash_hex_str,
  std::function<const char*(size_t &data_pulled)> stream_data);//stream_data returns nullptr on error, data_pulled = 0 when finished

class StreamToServerData;
StreamToServerData *start_blob_stream_to_server(intptr_t &sockfd, const char *hash_type, const char *hash_hex_str);
KVRet blob_stream_to_server(StreamToServerData &strm, const void *data, size_t data_size);
KVRet finish_blob_stream_to_server(intptr_t &sockfd, StreamToServerData *s, bool ok);//will delete the pointer
