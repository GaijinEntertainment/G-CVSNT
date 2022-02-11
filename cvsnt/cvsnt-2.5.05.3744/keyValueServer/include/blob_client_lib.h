#pragma once
#include <stdint.h>
#include <functional>
#include "blobs_encryption.h"

struct BlobSocket;

BlobSocket start_blob_push_client(const char* url, int port, const char* root, int timeout_sec, const uint8_t *otp, uint64_t otp_page, CafsClientAuthentication encryption);
bool stop_blob_push_client(BlobSocket &client);
enum class KVRet{OK, Error, Fatal}; //on fatal protocol error, client will become invalid

KVRet blob_check_on_server(BlobSocket &client, const char *hash_type, const char *hash_hex_str, bool &has);
int64_t blob_size_on_server(BlobSocket &client, const char *hash_type, const char *hash_hex_str); //<-1 if fatal error, -1 if file is missing

int64_t blob_pull_from_server(BlobSocket &client, const char *hash_type, const char *hash_hex_str,
  uint64_t from, uint64_t sz,
  std::function<void(const char *data, uint64_t at, uint64_t size)> cb);//<0 if error

KVRet blob_start_pull_from_server(BlobSocket &sockfd, const char *hash_type, const char *hash_hex_str,
  uint64_t &from, uint64_t &sz);

int64_t blob_push_to_server(BlobSocket &client, uint64_t blob_sz,
  const char *hash_type, const char *hash_hex_str,
  std::function<const char*(uint64_t at, uint64_t &data_pulled)> pull_data);//<0 if error. pull_data returns nullptr on error

KVRet blob_stream_to_server(BlobSocket &client,
  const char *hash_type, const char *hash_hex_str,
  std::function<const char*(uint64_t &data_pulled)> stream_data);//stream_data returns nullptr on error, data_pulled = 0 when finished

class StreamToServerData;
StreamToServerData *start_blob_stream_to_server(BlobSocket &sockfd, const char *hash_type, const char *hash_hex_str);
KVRet blob_stream_to_server(StreamToServerData &strm, const void *data, uint64_t data_size);
KVRet finish_blob_stream_to_server(BlobSocket &sockfd, StreamToServerData *s, bool ok);//will delete the pointer
