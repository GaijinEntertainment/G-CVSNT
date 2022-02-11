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

int64_t blob_push_to_server(BlobSocket &sockfd, uint64_t blob_sz,
  const char *hash_type, const char *hash_hex_str,
  std::function<const char*(uint64_t at, uint64_t &data_pulled)> pull_data)
{
  if (blob_sz == ~uint64_t(0))
    return -1;
  unsigned char blob_hash[hash_len];
  if (!encode_hash_str_to_blob_hash_s(hash_type, hash_hex_str, blob_hash, sizeof(blob_hash)))
    return -1;
  char command[push_command_len + command_len];
  void *to = command;
  memcpy_to(to, push_command, command_len);//copy command
  memcpy_to(to, blob_hash, hash_len);//copy hash
  memcpy_to(to, &blob_sz, sizeof(blob_sz));
  if (!send_exact(sockfd, command, sizeof(command)))
    {stop_blob_push_client(sockfd); return -2;}

  uint64_t from = 0;
  int64_t sizeLeft = blob_sz;
  while (sizeLeft > 0)
  {
    uint64_t data_pulled;
    const char *buf = pull_data(from, data_pulled);
    if (!buf)
    {
      blob_logmessage(LOG_ERROR, "IO error on %s:%s", hash_type, hash_hex_str);//we cant continue. we need to read size, and there is no guarantee it is correct
      break;
    }
    from += data_pulled;
    sizeLeft -= data_pulled;
    if (!send_exact(sockfd, buf, (int)data_pulled))
      {stop_blob_push_client(sockfd); return -2;}
  }
  const int64_t sent = blob_sz-sizeLeft;
  if (sizeLeft > 0)//if there was io error, we still have to finish send. Server will handle bad data gracefully (ignoring)
  {
    char buf[256]; memset(buf, 0, sizeof(buf));
    for (;sizeLeft > 0; sizeLeft -= sizeof(buf))
      if (!send_exact(sockfd, buf, (int)std::min(sizeLeft, (int64_t)sizeof(buf))))
        {stop_blob_push_client(sockfd); return -2;}
  }

  char response[response_len+1];
  if (!recv_exact(sockfd, response, (int)response_len))
  {
    stop_blob_push_client(sockfd);
    return -2;
  }
  if (strncmp(response, have_response, response_len) == 0)
    return sent;//successfully sent

  response[response_len] = 0;
  blob_logmessage(LOG_ERROR, "%s response %s on push request for <%s:%s>", is_error_response(response) ? "Error" : "Unknown",
    response, hash_type, hash_hex_str);
  if (!is_error_response(response))
    stop_blob_push_client(sockfd);
  return 0;
}
