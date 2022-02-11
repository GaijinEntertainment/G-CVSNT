#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <algorithm>
#include "../include/blob_sockets.h"
#include "../include/blob_push_protocol.h"
#include "../include/blob_common_net.h"
#include "../include/blob_hash_util.h"
#include "../blob_push_log.h"
#include "../include/blob_client_lib.h"

using namespace blob_push_proto;
class StreamToServerData
{
public:
  BufferedSocketOutput<65536> wr;
  StreamToServerData(BlobSocket &sock):wr(sock) {}
  void finish();
};

inline KVRet start_blob_stream_to_server(StreamToServerData &strm, const char *hash_type, const char *hash_hex_str)
{
  unsigned char blob_hash[hash_len];
  if (!encode_hash_str_to_blob_hash_s(hash_type, hash_hex_str, blob_hash, sizeof(blob_hash)))
    return KVRet::Error;
  return strm.wr.send(strm_command, command_len) && strm.wr.send(blob_hash, hash_len) ? KVRet::OK : KVRet::Fatal;
}

inline KVRet end_blob_stream_to_server(StreamToServerData &strm, bool ok)
{
  uint16_t send = ok ? 0 : uint16_t(~uint16_t(0));
  if (!strm.wr.send(&send, 2))
    return KVRet::Fatal;
  if (!strm.wr.finish())
    return KVRet::Fatal;

  char response[response_len+1];
  if (!recv_exact(strm.wr.sock, response, (int)response_len))
    return KVRet::Fatal;
  return strncmp(response, have_response, response_len) == 0 ? KVRet::OK : KVRet::Error;
}

KVRet blob_stream_to_server(StreamToServerData &strm, const void *data, uint64_t data_size)
{
  uint16_t send = 0;
  const char* buf = (const char*)data;
  while (data_size > 0)
  {
    uint16_t send = (int)std::min((uint64_t)65534, data_size);
    if (!strm.wr.send(&send, 2) || !strm.wr.send(buf, send))
      return KVRet::Fatal;
    buf += send;
    data_size -= send;
  }
  return KVRet::OK;
}

StreamToServerData *start_blob_stream_to_server(BlobSocket &sockfd, const char *hash_type, const char *hash_hex_str)
{
  StreamToServerData *s = new StreamToServerData(sockfd);
  KVRet ret = start_blob_stream_to_server(*s, hash_type, hash_hex_str);
  if (ret == KVRet::OK)
    return s;
  delete s;
  if (ret == KVRet::Fatal)
    stop_blob_push_client(sockfd);
  return nullptr;
}

KVRet finish_blob_stream_to_server(BlobSocket &sockfd, StreamToServerData *s, bool ok)
{
  if (!s)
    return KVRet::Error;
  KVRet ret = end_blob_stream_to_server(*s, ok);
  delete s;
  if (ret == KVRet::Fatal)
    stop_blob_push_client(sockfd);
  return ret;
}

KVRet blob_stream_to_server(BlobSocket &sockfd,
  const char *hash_type, const char *hash_hex_str,
  std::function<const char*(uint64_t &data_pulled)> pull_data)
{
  StreamToServerData strm(sockfd);
  KVRet rs = start_blob_stream_to_server(strm, hash_type, hash_hex_str);
  if (rs != KVRet::OK)
  {
    if (rs == KVRet::Fatal)
      stop_blob_push_client(sockfd);
    return rs;
  }
  bool ok = false;
  KVRet r = KVRet::Fatal;
  while (1)
  {
    uint64_t data_pulled;
    const char *buf = pull_data(data_pulled);
    if (!buf)
    {
      blob_logmessage(LOG_ERROR, "IO errors on %s:%s", hash_type, hash_hex_str);//we cant continue. we need to read size, and there is no guarantee it is correct
      r = end_blob_stream_to_server(strm, false);
      break;
    }
    if (!data_pulled)//we have finished transfer
    {
      r = end_blob_stream_to_server(strm, true);
      break;
    }
    r = blob_stream_to_server(strm, buf, data_pulled);
    if (r != KVRet::OK)
      break;
  }
  if (r == KVRet::Fatal)
    stop_blob_push_client(sockfd);
  return r;
}
