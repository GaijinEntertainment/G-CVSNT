#pragma once
#include <stddef.h>
#include <functional>
#include <string>

struct BlobNetworkProcessor
{
  virtual ~BlobNetworkProcessor(){}
  virtual bool canDownload() = 0;
  virtual bool canUpload() = 0;
  virtual bool download(const char *repo, const char *hex_hash, std::function<bool(const char *data, size_t data_length)>, std::string &err) = 0;
  virtual bool upload(const char *repo, const char *hex_hash, std::function<const char*(size_t from, size_t &data_provided)>, std::string &err) = 0;
};

