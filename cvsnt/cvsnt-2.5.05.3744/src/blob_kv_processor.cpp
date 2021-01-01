#include <sstream>
#include "blob_network_processor.h"
#include "sha_blob_reference.h"
#include <../keyValueServer/include/blob_client_lib.h>
#include <memory>

void error(int, int, const char*, ...);

inline KVRet send_blob_file_data_net(intptr_t &client, const char *file, const char *hash, bool blob_binary_compressed, std::string &err)
{
  auto output = std::stringstream("");
  using namespace caddressed_fs;
  using namespace streaming_compression;
  FILE* rf = fopen(file, "rb");
  fseek(rf, 0, SEEK_END);
  const size_t fsz = ftell(rf);
  fseek(rf, 0, SEEK_SET);
  StreamToServerData *strm = start_blob_stream_to_server(client, HASH_TYPE_REV_STRING, hash);
  if (!strm)
  {
    output << "Can't start sending binary blob data for " << file; err = output.str();
    return client < 0 ? KVRet::Fatal : KVRet::Error;
  }

  BlobHeader hdr = get_header(blob_binary_compressed ? zstd_magic : noarc_magic, fsz, 0);
  char hctx[HASH_CONTEXT_SIZE];
  init_blob_hash_context(hctx, sizeof(hctx));
  KVRet r = blob_stream_to_server(*strm, &hdr, sizeof(hdr));

  char bufIn[128<<10]; char bufOut[64<<10];
  StreamStatus st = compress_lambda(
    [&](const char *&src, size_t &src_pos, size_t &src_size)
      {if (src_pos < src_size) return StreamStatus::Continue;//previously extracted wasn't consumed
       src = bufIn; src_pos = 0;
       src_size = fread(bufIn, 1, sizeof(bufIn), rf);
       update_blob_hash(hctx, bufIn, src_size);
       return ferror(rf) ? StreamStatus::Error : src_size == 0 ? StreamStatus::Finished : StreamStatus::Continue;
      },
    [&](char *&dst, size_t &dst_pos, size_t &dst_capacity)
      {
        if (dst_pos && r == KVRet::OK)
          r = blob_stream_to_server(*strm, bufOut, dst_pos);
        dst_pos = 0; dst_capacity = sizeof(bufOut); dst = bufOut;
        return StreamStatus::Continue;
      }
   , 6, blob_binary_compressed ? StreamType::ZSTD : StreamType::Unpacked);
  fclose(rf);

  if (st != StreamStatus::Finished)
  {
    output << "Can't compress binary blob for " << file << "\n";
    if (r == KVRet::OK)
      r = KVRet::Error;
  }
  uint8_t digest[32];char real_hash[65];real_hash[0]=real_hash[64]=0;
  if (!finalize_blob_hash(hctx, digest) || !bin_hash_to_hex_string_64(digest, real_hash))
  {
    output << "Can't calc hash for " << file;
    if (r == KVRet::OK)
      r = KVRet::Error;
  }
  if (memcmp(real_hash, hash, 64))
  {
    output << "File " << file << " changed it's hash from " << hash << " to "<<real_hash<<". Failing!\n";
    if (r == KVRet::OK)
      r = KVRet::Error;
  }
  if (r == KVRet::Fatal || (r = finish_blob_stream_to_server(client, strm, r == KVRet::OK)) == KVRet::Fatal)
    stop_blob_push_client(client);
  if (r != KVRet::OK)
    output << "Can't send binary blob data for " << file;
  err = output.str();
  return r;
}

struct KVNetworkProcessor:public BlobNetworkProcessor
{
  KVNetworkProcessor(const char *url_, int port_, const char* root_):url(url_), port(port_), root(root_){}
  ~KVNetworkProcessor() { stop_blob_push_client(client); }
  bool init() { stop_blob_push_client(client); return (client = start_blob_push_client(url.c_str(), port, root.c_str())) >= 0; }
  virtual bool canDownload() {return true;}
  virtual bool canUpload() {return true;}
  virtual bool download(const char *hex_hash, std::function<bool(const char *data, size_t data_length)> cb, std::string &err)
  {
    int64_t pulled = blob_pull_from_server(client, HASH_TYPE_REV_STRING, hex_hash, 0, 0, [&](const char *data, uint64_t , uint64_t size)
    {
      if (data)//that's hint of size
         cb(data, size);
    });
    if (pulled == 0)
    {
      err = "No blob ";
      err += hex_hash;
      return 0;
    }
    if (pulled < 0)
    {
      err = "Error reading data ";
      err += hex_hash;
      init();
      return false;
    }
    return true;
  }
  virtual bool upload(const char *file, bool compress, char *hex_hash, std::string &err) {
    if (!caddressed_fs::get_file_content_hash(file, hex_hash, 64))
      return false;

    int64_t sz = blob_size_on_server(client, HASH_TYPE_REV_STRING, hex_hash); //<0 if error
    if (sz < 0)
    {
      init();
      return false;
    }
    if (sz > 0)//already on server
      return true;
    KVRet r = send_blob_file_data_net(client, file, hex_hash, compress, err);
    if (r == KVRet::Fatal)
      init();
    return r == KVRet::OK;
  }
  intptr_t client;
  std::string url, root; int port;
};

BlobNetworkProcessor *get_kv_processor(const char *url, int port, const char *root, const char *user, const char *passwd)
{
  auto ret =  new KVNetworkProcessor(url, port, root);
  if (!ret->init())
  {
    delete ret;
    return nullptr;
  }
  return ret;
}
#include <../keyValueServer/blob_push_log.h>
#include <stdarg.h>
void blob_logmessage(int log, const char *fmt,...)
{
  if (log < LOG_WARNING)
    return;
  char buf[512];
  va_list va;
  va_start(va, fmt);
  vsnprintf(buf, sizeof(buf), fmt, va);
  va_end(va);
  error(0, log, "%s", buf);
}

