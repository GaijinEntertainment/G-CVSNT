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
KVRet blob_check_on_server(intptr_t &sockfd, const char *hash_type, const char *hash_hex_str, bool &has)
{
  has = false;
  uint8_t command[chck_command_len + command_len];
  memcpy(command, chck_command, command_len);//copy command
  if (!encode_hash_str_to_blob_hash_s(hash_type, hash_hex_str, command+command_len, hash_len))
    return KVRet::Error;
  if (!send_exact(int(sockfd), command, sizeof(command)))
    {stop_blob_push_client(sockfd); return KVRet::Fatal;}
  char response[response_len+1];
  if (!recv_exact(int(sockfd), response, response_len))
    {stop_blob_push_client(sockfd); return KVRet::Fatal;}
  if (strncmp(response, have_response, response_len) == 0)
  {
    has = true;
    return KVRet::OK;
  }
  if (strncmp(response, none_response, response_len) == 0)
  {
    has = false;
    return KVRet::OK;
  }

  response[response_len] = 0;
  blob_logmessage(LOG_ERROR, "unknown or error response %s on have request for <%s:%s>", response, hash_type, hash_hex_str);
  if (!is_error_response(response))
    stop_blob_push_client(sockfd);
  return KVRet::Fatal;
}

int64_t blob_size_on_server(intptr_t &sockfd, const char *hash_type, const char *hash_hex_str)
{
  uint8_t command[size_command_len + command_len];
  memcpy(command, size_command, command_len);//copy command
  if (!encode_hash_str_to_blob_hash_s(hash_type, hash_hex_str, command+command_len, hash_len))
    return 0;

  if (!send_exact(int(sockfd), command, sizeof(command)))
    {stop_blob_push_client(sockfd); return -1;}
  char response[response_len+1];
  if (!recv_exact(int(sockfd), response, response_len))
    {stop_blob_push_client(sockfd); return -1;}
  response[response_len] = 0;
  if (strncmp(response, size_response, response_len) == 0)
  {
    int64_t blob_sz;
    if (!recv_exact(int(sockfd), (char*)&blob_sz, sizeof(blob_sz)))
      {stop_blob_push_client(sockfd); return -1;}
    return blob_sz;
  }

  if (strncmp(response, none_response, response_len) == 0)
    return 0;

  response[response_len] = 0;
  blob_logmessage(LOG_ERROR, "unknown or error response %s on size request for <%s:%s>", response, hash_type, hash_hex_str);
  if (!is_error_response(response))
    stop_blob_push_client(sockfd);
  return -1;
}
