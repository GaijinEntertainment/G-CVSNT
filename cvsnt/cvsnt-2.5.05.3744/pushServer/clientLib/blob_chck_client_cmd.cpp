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
bool blob_check_on_server(intptr_t &sockfd, const char *hash_type, const char *hash_hex_str, bool &has)
{
  has = false;
  uint8_t command[chck_command_len + command_len];
  memcpy(command, chck_command, command_len);//copy command
  if (!encode_hash_str_to_blob_hash_s(hash_type, hash_hex_str, command+command_len, hash_len))
    return false;
  if (!send_exact(sockfd, command, sizeof(command)))
    {stop_blob_push_client(sockfd); return false;}
  char responce[responce_len+1];
  if (!recv_exact(sockfd, responce, responce_len))
    {stop_blob_push_client(sockfd); return false;}
  if (strncmp(responce, have_responce, responce_len) == 0)
  {
    has = true;
    return true;
  }
  if (strncmp(responce, none_responce, responce_len) == 0)
  {
    has = false;
    return true;
  }

  responce[responce_len] = 0;
  blob_logmessage(LOG_ERROR, "unknown or error responce %s on have request for <%s:%s>", responce, hash_type, hash_hex_str);
  if (!is_error_responce(responce))
    stop_blob_push_client(sockfd);
  return false;
}

int64_t blob_size_on_server(intptr_t &sockfd, const char *hash_type, const char *hash_hex_str)
{
  uint8_t command[size_command_len + command_len];
  memcpy(command, size_command, command_len);//copy command
  if (!encode_hash_str_to_blob_hash_s(hash_type, hash_hex_str, command+command_len, hash_len))
    return -1;

  if (!send_exact(sockfd, command, sizeof(command)))
    {stop_blob_push_client(sockfd); return -1;}
  char responce[responce_len+1];
  if (!recv_exact(sockfd, responce, responce_len))
    {stop_blob_push_client(sockfd); return -1;}
  responce[responce_len] = 0;
  if (strncmp(responce, size_responce, responce_len) == 0)
  {
    int64_t blob_sz;
    if (!recv_exact(sockfd, (char*)&blob_sz, sizeof(blob_sz)))
      {stop_blob_push_client(sockfd); return -1;}
    return blob_sz;
  }

  if (strncmp(responce, none_responce, responce_len) == 0)
    return 0;

  responce[responce_len] = 0;
  blob_logmessage(LOG_ERROR, "unknown or error responce %s on size request for <%s:%s>", responce, hash_type, hash_hex_str);
  if (!is_error_responce(responce))
    stop_blob_push_client(sockfd);
  return -1;
}
