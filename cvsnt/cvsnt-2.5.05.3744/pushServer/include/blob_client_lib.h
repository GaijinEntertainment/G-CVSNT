#pragma once
#include <stdint.h>
#include <functional>

intptr_t start_blob_push_client(const char *url, int port);
bool stop_blob_push_client(intptr_t &client);

//on protocol error, client will become invalid
bool blob_check_on_server(intptr_t &client, const char *hash_type, const char *hash_hex_str, bool &has);
int64_t blob_size_on_server(intptr_t &client, const char *hash_type, const char *hash_hex_str); //<0 if error

int64_t blob_pull_from_server(intptr_t &client, const char *hash_type, const char *hash_hex_str,
  uint64_t from, uint64_t sz,
  std::function<bool(const char *data, uint64_t at, uint64_t size)> cb);//<0 if error

int64_t blob_push_to_server(intptr_t &client, size_t blob_sz,
  const char *hash_type, const char *hash_hex_str,
  std::function<const char*(uint64_t at, size_t &data_pulled)> pull_data);//<0 if error
