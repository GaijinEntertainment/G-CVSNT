#pragma once
#include <stddef.h>
#include <functional>
#include <string>

struct BlobNetworkProcessor
{
  virtual ~BlobNetworkProcessor(){}
  virtual bool reinit(const char *url, int port, const char *user, const char *passwd) = 0;
  virtual bool canDownload() = 0;
  virtual bool canUpload() = 0;
  virtual bool download(const char *hex_hash, std::function<bool(const char *data, size_t data_length)>, std::string &err) = 0;
  virtual bool upload(const char *file, bool compress, char *hex_hash, std::string &err) = 0;
};

