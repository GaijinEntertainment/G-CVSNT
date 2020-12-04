#define NOMINMAX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <algorithm>
#include "../include/blob_sockets.h"
#include "../include/blob_push_protocol.h"
#include "../include/blob_push_proto_ver.h"
#include "../include/blob_common_net.h"
#include "../blob_push_log.h"
#include "../blob_server_func_deps.h"
#include "../include/blob_hash_util.h"

using namespace blob_push_proto;

static bool handle_chck(void *, int socket);
static bool handle_size(void *, int socket);
static bool handle_pull(void *, int socket);
static bool handle_strm(void *, int socket);
static bool handle_push(void *, int socket);
static bool send_simple_response(int socket, const char *resp, int len = response_len) { return send_exact(socket, resp, len); }

static void *initiate(int socket)
{
  send_exact(socket, BLOB_PUSH_GREETING_STR BLOB_PUSH_PROTO_VERSION, greeting_length);
  char commandBuf[command_len + 1];
  if (!recv_exact(socket, commandBuf, command_len, 0))
    return nullptr;
  if (strncmp(commandBuf, vers_command, command_len) != 0)
  {
    blob_logmessage(LOG_ERROR, "first command should be VERS, not %.4s", commandBuf);
    return nullptr;
  }
  char vers[3];
  if (!recv_exact(socket, vers, 3))
    return nullptr;
  const bool good = memcmp(vers, BLOB_PUSH_PROTO_VERSION, 3) == 0;
  blob_logmessage(LOG_NOTIFY, "%s version, connection=%d", good ? "good" : "bad", socket);
  if (!good)
    return nullptr;
  uint8_t len;
  if (!recv_exact(socket, &len, sizeof(len)))
    return nullptr;
  char buf[256];
  if (!recv_exact(socket, buf, len))
    return nullptr;
  send_simple_response(socket, none_response);
  return blob_create_ctx(buf);
}

static bool blob_push_thread_proc_int(void *ctx, int socket)
{
  char commandBuf[command_len + 1];
  if (!recv_exact(socket, commandBuf, command_len, 0))
    return false;
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
    return false;
  }
  return true;
}

static bool handle_chck(void *ctx, int socket)
{
  uint8_t commandBuf[chck_command_len];
  if (!recv_exact(socket, commandBuf, chck_command_len))
    return false;
  char hash_type[7], hash_hex_str[65];
  decode_blob_hash_to_hex_hash(commandBuf, hash_type, hash_hex_str);
  return send_simple_response(socket, blob_does_hash_blob_exist(ctx, hash_type, hash_hex_str) ? have_response : none_response);
}

static bool handle_size(void *ctx, int socket)
{
  uint8_t commandBuf[size_command_len];
  if (!recv_exact(socket, commandBuf, size_command_len))
    return false;
  char hash_type[7], hash_hex_str[65];
  decode_blob_hash_to_hex_hash(commandBuf, hash_type, hash_hex_str);
  uint64_t size = blob_get_hash_blob_size(ctx, hash_type, hash_hex_str);
  //maybe check if exist, and send none if not ?
  // but currently client returns 0 in both case
  char response[size_response_len];
  void *to = response;
  memcpy_to(to, size_response, response_len);
  memcpy_to(to, &size, sizeof(size));
  return send_simple_response(socket, response, size_response_len);
}


static bool handle_push(void *ctx, int socket)
{
  uint8_t commandBuf[push_command_len];
  if (!recv_exact(socket, commandBuf, push_command_len))
    return false;

  char hash_type[7], hash_hex_str[65];
  decode_blob_hash_to_hex_hash(commandBuf, hash_type, hash_hex_str);

  uint64_t blob_sz; memcpy(&blob_sz, commandBuf + hash_len, sizeof(blob_sz));//start lifetime as
  blob_logmessage(LOG_NOTIFY, "client %d pushed %s:%s of %d sz", socket, hash_type, hash_hex_str, (int)blob_sz);

  uintptr_t tempBlob = blob_start_push_data(ctx, hash_type, hash_hex_str, blob_sz);
  bool ok = true;
  ok &= recv_lambda(socket, blob_sz, [&](const char *buf, int len){ok &= blob_push_data(buf, len, tempBlob);return true;});
  if (!ok)
  {
    blob_destroy_push_data(tempBlob);
    return send_simple_response(socket, err_file_error);
  }
  return send_simple_response(socket, blob_end_push_data(tempBlob) ? have_response : err_file_error);
}

static bool handle_strm(void *ctx, int socket)
{
  uint8_t commandBuf[push_command_len];
  if (!recv_exact(socket, commandBuf, strm_command_len))
    return false;

  char hash_type[7], hash_hex_str[65];
  decode_blob_hash_to_hex_hash(commandBuf, hash_type, hash_hex_str);

  blob_logmessage(LOG_NOTIFY, "client %d stream pushed %s:%s", socket, hash_type, hash_hex_str);

  uintptr_t tempBlob = blob_start_push_data(ctx, hash_type, hash_hex_str, 0);
  uint16_t chunkSz = 0;
  bool ok = true;
  while (1){
    if (!recv_exact(socket, &chunkSz, 2))
    {
      ok = false;
      break;
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
      ok = false;
      break;
    }
  }
  if (!ok)
  {
    blob_destroy_push_data(tempBlob);
    return send_simple_response(socket, err_file_error);
  }
  return send_simple_response(socket, blob_end_push_data(tempBlob) ? have_response : err_file_error);
}

static bool handle_pull(void *ctx, int socket)
{
  uint8_t commandBuf[pull_command_len];
  if (!recv_exact(socket, commandBuf, pull_command_len, 0))
    return false;

  char hash_type[7], hash_hex_str[65];
  decode_blob_hash_to_hex_hash(commandBuf, hash_type, hash_hex_str);

  const void *from_cmd = commandBuf + hash_len;
  uint64_t request_sz; memcpy_from(&request_sz, from_cmd, sizeof(request_sz));//start lifetime as
  uint32_t blob_from_chunk; memcpy_from(&blob_from_chunk, from_cmd, sizeof(blob_from_chunk));//start lifetime as
  uint64_t blob_sz = 0;

  uintptr_t readBlob = blob_start_pull_data(ctx, hash_type, hash_hex_str, blob_sz);//allows memory mapping
  if (readBlob == 0)
    return send_simple_response(socket, none_response);

  uint64_t from = uint64_t(blob_from_chunk)*pull_chunk_size;
  if (request_sz + from > blob_sz)
  {
    blob_end_pull_data(readBlob);
    return send_simple_response(socket, err_file_error);
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
    return false;
  }

  while (sizeLeft > 0)
  {
    size_t data_pulled;
    const char *buf = blob_pull_data(readBlob, from, data_pulled);
    if (!buf)
    {
      blob_logmessage(LOG_ERROR, "IO error on %s:%s", hash_type, hash_hex_str);//we cant continue. we need to read size, and there is no guarantee it is correct
      blob_end_pull_data(readBlob);
      return false;
    }
    from += data_pulled;
    sizeLeft -= data_pulled;
    if (!send_exact(socket, buf, data_pulled))
      return false;
  }
  blob_end_pull_data(readBlob);
  return true;
}

void blob_push_thread_proc(int socket, volatile bool *should_stop)
{
  void *ctx = initiate(socket);
  if (!ctx)
  {
    blob_logmessage(LOG_ERROR, "can't initiate ctx for %d", socket);
    blob_close_socket(socket);
    return;
  }
  while (!(should_stop && *should_stop) && blob_push_thread_proc_int(ctx, socket))//command is processed
  {
  }
  blob_logmessage(LOG_NOTIFY, "close connection %d", socket);
  blob_close_socket(socket);
}