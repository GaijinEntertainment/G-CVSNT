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

#include <chrono>
#include <thread>
void blob_sleep_for_msec(unsigned int msec)
{
  std::this_thread::sleep_for(std::chrono::microseconds(uint32_t(msec*1000)));
}


using namespace blob_push_proto;
enum class ServerRet {OK, ATTACK, ERR};

static ServerRet handle_chck(void *, BlobSocket socket);
static ServerRet handle_size(void *, BlobSocket socket);
static ServerRet handle_pull(void *, BlobSocket socket);
static ServerRet handle_strm(void *, BlobSocket socket);
static ServerRet handle_push(void *, BlobSocket socket);
static bool send_simple_response(BlobSocket socket, const char *resp, int len = response_len) { return send_exact(socket, resp, len); }

static ServerRet blob_push_thread_proc_int(void *ctx, BlobSocket socket)
{
  char commandBuf[command_len + 1];
  if (!recv_exact(socket, commandBuf, command_len))
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

static ServerRet handle_chck(void *ctx, BlobSocket socket)
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

static ServerRet handle_size(void *ctx, BlobSocket socket)
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


static ServerRet handle_push(void *ctx, BlobSocket socket)
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
  if (!recv_lambda(socket, blob_sz, [&](const char *buf, intptr_t len){ok &= blob_push_data(buf, len, tempBlob);return true;}))
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

static ServerRet handle_strm(void *ctx, BlobSocket socket)
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

static ServerRet handle_pull(void *ctx, BlobSocket socket)
{
  uint8_t commandBuf[pull_command_len];
  if (!recv_exact(socket, commandBuf, pull_command_len))
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

#include "../include/blob_raw_sockets.h"

static ServerRet authenticate_client(BlobSocket &blob_socket, void * &result, bool is_public_client_ip,
                                     intptr_t raw_socket, const char *encryption_secret, CafsServerEncryption encryption)
{
  const char *greeting = encryption_secret ? (BLOB_PUSH_GREETING_STR BLOB_PUSH_AUTH_VERSION) : (BLOB_PUSH_GREETING_STR BLOB_PUSH_PROTO_VERSION);
  if (raw_send_exact(raw_socket, greeting, greeting_length) != greeting_length)
    return ServerRet::ERR;

  char authCommand[8];
  if (raw_recv_exact(raw_socket, authCommand, command_len + vers_command_len) != command_len + vers_command_len)
    return ServerRet::ERR;
  authCommand[7] = 0;
  if (memcmp(authCommand, vers_command, command_len) != 0)
  {
    blob_logmessage(LOG_ERROR, "first command should be VERS, not %.4s", authCommand);
    return ServerRet::ATTACK;
  }
  const uint32_t receivedVersion = (authCommand[command_len]-'0')*100 + (authCommand[command_len+1]-'0')*10 + (authCommand[command_len+2]-'0');
  const bool performAuth = receivedVersion >= BLOB_PUSH_AUTH_VERSIONI;
  const char *auth_response = NULL;
  bool removeEncryption = false;
  bool badVersion = false;
  if (receivedVersion != BLOB_PUSH_PROTO_VERSIONI && receivedVersion != BLOB_PUSH_AUTH_VERSIONI)
  {
    blob_logmessage(LOG_WARNING, "Unknown version %d", receivedVersion);
    badVersion = true;
  }

  if (performAuth)
  {
    if (!encryption_secret)
    {
      blob_logmessage(LOG_ERROR, "We can't authenticate ourselves, as no encryption_secret was provided");
      return ServerRet::ERR;
    }
    uint64_t oneTimePadPageNo = 0;
    if (raw_recv_exact(raw_socket, &oneTimePadPageNo, sizeof(oneTimePadPageNo)) != sizeof(oneTimePadPageNo))
      return ServerRet::ERR;
    if (!blob_is_valid_otp_page(oneTimePadPageNo))
    {
      blob_logmessage(LOG_ERROR, "Invalid OTP pageNo %lld", (long long int)oneTimePadPageNo);
      return ServerRet::ATTACK;
    }
    uint8_t otp_page[key_plus_iv_size];
    memset(otp_page, 0, sizeof(otp_page));
    if (!blob_gen_totp_secret(otp_page, (const uint8_t*) encryption_secret, (uint32_t)strlen(encryption_secret), oneTimePadPageNo))
    {
      blob_logmessage(LOG_ERROR, "can't generate OTP secret for %d", raw_socket);
      return ServerRet::ATTACK;
    }
    BlobSocket blobSocket = connect_to_client_blob_socket(raw_socket, otp_page);
    if (!is_valid(blobSocket))
    {
      //blob_logmessage(LOG_ERROR, "Can't receive encryption");
      blob_close_encryption(blobSocket);
      return ServerRet::ERR;
    }

    uint8_t paddedTimeStamp[16]; memset(paddedTimeStamp, 0, sizeof(paddedTimeStamp));
    if (!recv_exact(blobSocket, paddedTimeStamp, sizeof(paddedTimeStamp)))
    {
      //blob_logmessage(LOG_ERROR, "Can't receive timestamp");
      blob_close_encryption(blobSocket);
      return ServerRet::ERR;
    }
    uint64_t clientTimeStamp, padding;
    memcpy(&clientTimeStamp, paddedTimeStamp, sizeof(clientTimeStamp));
    memcpy(&padding, paddedTimeStamp+sizeof(clientTimeStamp), sizeof(padding));
    if (padding != uint64_t(~uint64_t(0)) || (clientTimeStamp>>40ULL) != uint64_t(0))//we couldn't receive expected magic. Encryption is broken, auth not passed
    {
      //blob_logmessage(LOG_ERROR, "Incorrect timestamp, or broken encoding. We under attack");
      blob_close_encryption(blobSocket);
      return ServerRet::ATTACK;
    }
    if (!blob_is_valid_timestamp(clientTimeStamp))//time stamp is invalid. we stop working to prevent traffic replication attack
    {
      send_exact(blobSocket, err_file_error, response_len);
      blob_close_encryption(blobSocket);
      return ServerRet::ERR;
    }
    blob_socket = blobSocket;
    auth_response = have_response;
    if (encryption == CafsServerEncryption::Public)
    {
      //if it is private address, removes authentication
      if (!is_public_client_ip)
      {
        blob_logmessage(LOG_NOTIFY, "Client is in private network, and encryption is optional, so remove encryption.");
        auth_response = none_response;
        removeEncryption = true;
      }
    }
  } else
  {
    blob_socket = connect_to_client_blob_socket_no_auth(raw_socket);
    if (encryption == CafsServerEncryption::All)
    {
      blob_logmessage(LOG_WARNING, "Server is only working with authenticating clients");
      badVersion = true;
    } else if (encryption == CafsServerEncryption::Public)
    {
      if (is_public_client_ip)
      {
        blob_logmessage(LOG_WARNING, "Server in public newtork is only working with authenticating clients");
        badVersion = true;
      }
    }
    auth_response = none_response;
  }

  if (badVersion)//no need to read anything else
  {
    send_exact(blob_socket, err_bad_command, response_len);
    blob_close_encryption(blob_socket);
    return ServerRet::ERR;
  }

  send_simple_response(blob_socket, auth_response);

  uint8_t len;
  if (!recv_exact(blob_socket, &len, sizeof(len)))
    return ServerRet::ERR;
  char rootBuf[256];
  if (!recv_exact(blob_socket, rootBuf, len))
    return ServerRet::ERR;
  rootBuf[len] = 0;
  blob_logmessage(LOG_NOTIFY, "CVS root %s", rootBuf);
  result = blob_create_ctx(rootBuf);
  if (!result)
  {
    blob_logmessage(LOG_ERROR, "can't initiate ctx for %d", raw_socket);
    return ServerRet::ERR;
  }
  if (removeEncryption)
  {
    blob_close_encryption(blob_socket);
  }
  return ServerRet::OK;
}

static inline ServerRet sleep_on_attack(ServerRet ret)
{
  if (ret == ServerRet::ATTACK)
    blob_sleep_for_msec(2000);//sleep for 2 seconds before closing socket. Doesn't make sense to sleep more than that, attacker will just close connection anyway
  return ret;
}

void blob_syslog(ServerRet err, uint32_t client_ip);
static ServerRet syslog_on_authentication(ServerRet ret, uint32_t client_ip)//we need up
{
  blob_syslog(ret, client_ip);
  return sleep_on_attack(ret);
}


bool blob_push_thread_proc(intptr_t raw_socket, uint32_t client_ip, volatile bool *should_stop, const char *encryption_secret, CafsServerEncryption encryption)//return false on attack
{
  if (!encryption_secret && encryption != CafsServerEncryption::Local)
    return false;
  const bool is_public_client_ip = blob_classify_ip(client_ip) == IpType::PUBLIC;

  raw_set_socket_def_options(raw_socket);
  raw_set_socket_no_delay(raw_socket, true);
  raw_send_recieve_sock_timeout(raw_socket, default_socket_timeout);//set timeout to 10 minutes
  raw_set_keepalive_tcp(raw_socket);
  BlobSocket socket;
  void* ctx  = 0;
  ServerRet authRet = syslog_on_authentication(authenticate_client(socket, ctx, is_public_client_ip, raw_socket, encryption_secret, encryption), client_ip);
  if (authRet != ServerRet::OK)
  {
    blob_logmessage(LOG_ERROR, "can't send greeting for %d", raw_socket);
    raw_close_socket(raw_socket);
    return true;
  }
  ServerRet ret = ServerRet::OK;
  while (!(should_stop && *should_stop))//command is processed
  {
    ret = sleep_on_attack(blob_push_thread_proc_int(ctx, socket));
    if (ret != ServerRet::OK)
      break;
  }
  blob_logmessage(LOG_NOTIFY, "close connection %d", raw_socket);
  blob_close_socket(socket);
  blob_destroy_ctx(ctx);
  return false;
}
#if _WIN32
  //do nothing. we don't support windows server completely, so no reason for fail2ban
  void blob_syslog(ServerRet err, uint32_t client_ip){}
#else
  #include <syslog.h>
  #include <time.h>
  void blob_syslog(ServerRet err, uint32_t client_ip)
  {
    time_t t = time(NULL);
    struct tm ctm = *localtime(&t);
    openlog("CAFS:", LOG_NDELAY|LOG_NOWAIT, LOG_LOCAL1);
    syslog(LOG_WARNING, "%lld|%d-%02d-%02d %02d:%02d:%02d|%d.%d.%d.%d|%s",
      (long long int)t,
      ctm.tm_year + 1900, ctm.tm_mon + 1, ctm.tm_mday, ctm.tm_hour, ctm.tm_min, ctm.tm_sec,
      client_ip&0xFF, (client_ip>>8)&0xFF, (client_ip>>16)&0xFF, (client_ip>>24)&0xFF,
      err == ServerRet::OK ? "OK" : (err == ServerRet::ERR ? "ERR" : "FAIL"));
    closelog();
  }
#endif