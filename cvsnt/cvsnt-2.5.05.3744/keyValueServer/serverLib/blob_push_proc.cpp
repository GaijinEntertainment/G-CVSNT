#define NOMINMAX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "../include/blob_sockets.h"
#include "../include/blob_push_protocol.h"
#include "../include/blob_push_proto_ver.h"
#include "../include/blob_common_net.h"
#include "../blob_push_log.h"
#include "../blob_server_func_deps.h"
#include "../include/blob_hash_util.h"

using namespace blob_push_proto;
enum class ServerRet {OK, ATTACK, ERR};

static ServerRet handle_chck(void *, int socket);
static ServerRet handle_size(void *, int socket);
static ServerRet handle_pull(void *, int socket);
static ServerRet handle_strm(void *, int socket);
static ServerRet handle_push(void *, int socket);
static bool send_simple_response(int socket, const char *resp, int len = response_len) { return send_exact(socket, resp, len); }

static ServerRet initiate(int socket, void * &result)
{
  result = nullptr;
  send_exact(socket, BLOB_PUSH_GREETING_STR BLOB_PUSH_PROTO_VERSION, greeting_length);
  char commandBuf[command_len + 1];
  if (!recv_exact(socket, commandBuf, command_len, 0))
    return ServerRet::ERR;
  if (strncmp(commandBuf, vers_command, command_len) != 0)
  {
    blob_logmessage(LOG_ERROR, "first command should be VERS, not %.4s", commandBuf);
    return ServerRet::ATTACK;
  }
  char vers[3];
  if (!recv_exact(socket, vers, 3))
    return ServerRet::ERR;
  const bool good = memcmp(vers, BLOB_PUSH_PROTO_VERSION, 3) == 0;
  blob_logmessage(LOG_NOTIFY, "%s version, connection=%d", good ? "good" : "bad", socket);
  if (!good)
    return ServerRet::ATTACK;
  uint8_t len;
  if (!recv_exact(socket, &len, sizeof(len)))
    return ServerRet::ERR;
  char buf[256];
  if (!recv_exact(socket, buf, len))
    return ServerRet::ERR;
  send_simple_response(socket, none_response);
  buf[len] = 0;
  blob_logmessage(LOG_NOTIFY, "CVS root %s", buf);
  result = blob_create_ctx(buf);
  return result ?  ServerRet::OK : ServerRet::ERR;
}

static ServerRet blob_push_thread_proc_int(void *ctx, int socket)
{
  char commandBuf[command_len + 1];
  if (!recv_exact(socket, commandBuf, command_len, 0))
    return ServerRet::ERR;
  commandBuf[command_len] = 0;
  blob_logmessage(LOG_NOTIFY, "got command %.*s", command_len, commandBuf);
  char *hash_str = commandBuf + command_len + 1;
  if (strncmp(commandBuf, chck_command, command_len) == 0)
  {
    return handle_chck(ctx, socket);
  } else if (strncmp(commandBuf, size_command, command_len) == 0)
  {
    return handle_size(ctx, socket);
  } else if (strncmp(commandBuf, push_command, command_len) == 0)
  {
    return handle_push(ctx, socket);
  } else if (strncmp(commandBuf, strm_command, command_len) == 0)
  {
    return handle_strm(ctx, socket);
  } else if (strncmp(commandBuf, pull_command, command_len) == 0)
  {
    return handle_pull(ctx, socket);
  } else
  {
    blob_logmessage(LOG_ERROR, "unknown command <%s>", commandBuf);
    return ServerRet::ATTACK;
  }
  return ServerRet::ATTACK;
}

static ServerRet handle_chck(void *ctx, int socket)
{
  uint8_t commandBuf[chck_command_len];
  if (!recv_exact(socket, commandBuf, chck_command_len))
    return ServerRet::ERR;
  char hash_type[7], hash_hex_str[65];
  decode_blob_hash_to_hex_hash(commandBuf, hash_type, hash_hex_str);
  const bool exist = blob_does_hash_blob_exist(ctx, hash_type, hash_hex_str);
  if (!send_simple_response(socket, exist ? have_response : none_response))
    return ServerRet::ERR;
  return blob_is_under_attack(exist, ctx) ? ServerRet::ATTACK : ServerRet::OK;
}

static ServerRet handle_size(void *ctx, int socket)
{
  uint8_t commandBuf[size_command_len];
  if (!recv_exact(socket, commandBuf, size_command_len))
    return ServerRet::ERR;
  char hash_type[7], hash_hex_str[65];
  decode_blob_hash_to_hex_hash(commandBuf, hash_type, hash_hex_str);
  const uint64_t sz = blob_get_hash_blob_size(ctx, hash_type, hash_hex_str);
  if (sz == uint64_t(~uint64_t(0)))
  {
    if (!send_simple_response(socket, none_response))
      return ServerRet::ERR;
    return blob_is_under_attack(false, ctx) ? ServerRet::ATTACK : ServerRet::OK;
  }

  uint64_t size = sz;
  //maybe check if exist, and send none if not ?
  // but currently client returns 0 in both case
  char response[size_response_len];
  void *to = response;
  memcpy_to(to, size_response, response_len);
  memcpy_to(to, &size, sizeof(size));
  return send_simple_response(socket, response, size_response_len) ? ServerRet::OK : ServerRet::ERR;
}


static ServerRet handle_push(void *ctx, int socket)
{
  uint8_t commandBuf[push_command_len];
  if (!recv_exact(socket, commandBuf, push_command_len))
    return ServerRet::ERR;

  char hash_type[7], hash_hex_str[65];
  decode_blob_hash_to_hex_hash(commandBuf, hash_type, hash_hex_str);

  uint64_t blob_sz; memcpy(&blob_sz, commandBuf + hash_len, sizeof(blob_sz));//start lifetime as
  blob_logmessage(LOG_NOTIFY, "client %d pushed %s:%s of %d sz", socket, hash_type, hash_hex_str, (int)blob_sz);

  uintptr_t tempBlob = blob_start_push_data(ctx, hash_type, hash_hex_str, blob_sz);
  bool ok = true;
  if (!recv_lambda(socket, blob_sz, [&](const char *buf, int len){ok &= blob_push_data(buf, len, tempBlob);return true;}))
  {
    blob_destroy_push_data(tempBlob);
    return ServerRet::ERR;
  }
  if (!ok)
  {
    blob_destroy_push_data(tempBlob);
    return send_simple_response(socket, err_file_error) ? ServerRet::OK : ServerRet::ERR;
  }
  return send_simple_response(socket, blob_end_push_data(tempBlob) ? have_response : err_file_error) ? ServerRet::OK : ServerRet::ERR;
}

static ServerRet handle_strm(void *ctx, int socket)
{
  uint8_t commandBuf[push_command_len];
  if (!recv_exact(socket, commandBuf, strm_command_len))
    return ServerRet::ERR;

  char hash_type[7], hash_hex_str[65];
  decode_blob_hash_to_hex_hash(commandBuf, hash_type, hash_hex_str);

  blob_logmessage(LOG_NOTIFY, "client %d stream pushed %s:%s", socket, hash_type, hash_hex_str);

  uintptr_t tempBlob = blob_start_push_data(ctx, hash_type, hash_hex_str, 0);
  uint16_t chunkSz = 0;
  bool ok = true;
  while (1){
    if (!recv_exact(socket, &chunkSz, 2))
    {
      blob_destroy_push_data(tempBlob);
      return ServerRet::ERR;
    }
    if (!chunkSz)
      break;
    if (chunkSz == uint16_t(~uint16_t(0)))
    {
      ok = false;
      break;
    }
    if (!recv_lambda(socket, chunkSz, [&](const char *buf, int len){ok &= blob_push_data(buf, len, tempBlob);return true;}))
    {
      blob_destroy_push_data(tempBlob);
      return ServerRet::ERR;
    }
  }
  if (!ok)
  {
    blob_destroy_push_data(tempBlob);
    return send_simple_response(socket, err_file_error) ? ServerRet::OK : ServerRet::ERR;
  }
  return send_simple_response(socket, blob_end_push_data(tempBlob) ? have_response : err_file_error) ? ServerRet::OK : ServerRet::ERR;
}

static ServerRet handle_pull(void *ctx, int socket)
{
  uint8_t commandBuf[pull_command_len];
  if (!recv_exact(socket, commandBuf, pull_command_len, 0))
    return ServerRet::ERR;

  char hash_type[7], hash_hex_str[65];
  decode_blob_hash_to_hex_hash(commandBuf, hash_type, hash_hex_str);

  const void *from_cmd = commandBuf + hash_len;
  uint64_t request_sz; memcpy_from(&request_sz, from_cmd, sizeof(request_sz));//start lifetime as
  uint32_t blob_from_chunk; memcpy_from(&blob_from_chunk, from_cmd, sizeof(blob_from_chunk));//start lifetime as
  uint64_t blob_sz = 0;

  uintptr_t readBlob = blob_start_pull_data(ctx, hash_type, hash_hex_str, blob_sz);//allows memory mapping
  if (readBlob == 0)
  {
    if (!send_simple_response(socket, none_response))
      return ServerRet::ERR;
    return blob_is_under_attack(false, ctx) ? ServerRet::ATTACK : ServerRet::OK;
  }

  uint64_t from = uint64_t(blob_from_chunk)*pull_chunk_size;
  if (request_sz + from > blob_sz)
  {
    blob_end_pull_data(readBlob);
    if (!send_simple_response(socket, err_file_error))
      return ServerRet::ERR;
    return blob_is_under_attack(false, ctx) ? ServerRet::ATTACK : ServerRet::OK;
  }

  int64_t sizeLeft = blob_sz - from;
  sizeLeft = request_sz == 0 ? sizeLeft : std::min((int64_t)request_sz, sizeLeft);

  char take[take_response_len];
  void *to = take;
  memcpy_to(to, take_response, response_len);
  memcpy_to(to, commandBuf, hash_len);//copy hash
  memcpy_to(to, &sizeLeft, sizeof(sizeLeft));
  memcpy_to(to, &blob_from_chunk, sizeof(blob_from_chunk));

  if (!send_exact(socket, take, take_response_len))
  {
    blob_end_pull_data(readBlob);
    return ServerRet::ERR;
  }

  while (sizeLeft > 0)
  {
    uint64_t data_pulled;
    const char *buf = blob_pull_data(readBlob, from, data_pulled);
    if (!buf)
    {
      blob_logmessage(LOG_ERROR, "IO error on %s:%s", hash_type, hash_hex_str);//we cant continue. we need to read size, and there is no guarantee it is correct
      blob_end_pull_data(readBlob);
      return ServerRet::ERR;
    }
    from += data_pulled;
    sizeLeft -= data_pulled;
    if (!send_exact(socket, buf, data_pulled))
    {
      blob_end_pull_data(readBlob);
      return ServerRet::ERR;
    }
  }
  blob_end_pull_data(readBlob);
  return ServerRet::OK;
}

bool blob_push_thread_proc(int socket, volatile bool *should_stop)//return false on attack
{
  void *ctx = nullptr;
  ServerRet ret = initiate(socket, ctx);
  if (!ctx || ret != ServerRet::OK)
  {
    blob_logmessage(LOG_ERROR, "can't initiate ctx for %d", socket);
    blob_close_socket(socket);
    return ret == ServerRet::ATTACK ? false : true;
  }
  while (!(should_stop && *should_stop))//command is processed
  {
    ret = blob_push_thread_proc_int(ctx, socket);
    if (ret != ServerRet::OK)
      break;
  }
  blob_logmessage(LOG_NOTIFY, "close connection %d", socket);
  blob_close_socket(socket);
  blob_destroy_ctx(ctx);
  return ret == ServerRet::ATTACK ? false : true;
}