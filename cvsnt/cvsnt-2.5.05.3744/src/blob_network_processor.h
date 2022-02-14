#pragma once
#include <stddef.h>
#include <functional>
#include <string>

struct BlobNetworkProcessor
{
  virtual ~BlobNetworkProcessor(){}
  virtual bool reconnect() = 0;
  virtual bool canDownload() = 0;
  virtual bool canUpload() = 0;
  virtual bool download(const char *hex_hash, std::function<bool(const char *data, size_t data_length)>, std::string &err) = 0;
  virtual bool upload(const char *file, bool compress, char *hex_hash, std::string &err) = 0;
};

struct UrlProvider
{
  virtual bool demandAuth() const = 0;
  virtual bool getOTP(uint8_t *otp, size_t otp_size, uint64_t &otp_page) const = 0;
  virtual const char* getRoot() const = 0;
  virtual int attemptsCount(int id) const = 0;
  virtual bool getNext(int attempt, int id, std::string &url, int &port) const = 0;//shuffled with id
  virtual void fail(int attempt, int id) const = 0;
};
