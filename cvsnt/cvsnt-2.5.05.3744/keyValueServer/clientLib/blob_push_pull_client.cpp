#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <thread>
#include <algorithm>
#include <time.h>

#include "../include/blob_sockets.h"
#include "../include/blob_push_protocol.h"
#include "../include/blob_push_proto_ver.h"
#include "../include/blob_common_net.h"
#include "../blob_push_log.h"
#include "../include/blob_client_lib.h"

using namespace blob_push_proto;

int connect_with_timeout(SOCKET sock, const struct sockaddr *addr, size_t addr_len, int timeout_sec)
{
  if (timeout_sec <= 0)
    return connect(sock, addr, addr_len);//no timeout
  timeval timeout;
  timeout.tv_sec = timeout_sec;
  timeout.tv_usec = 0;

  //set the socket in non-blocking
  if (!blob_set_non_blocking(sock, true))
    blob_logmessage(LOG_ERROR, "ioctlsocket failed with error: %d (%d)\n", blob_get_last_sock_error());
  auto connectRet = connect(sock,addr,addr_len);
  if (connectRet == -1 && !blob_socket_would_block(blob_get_last_sock_error()))
  {
    blob_logmessage(LOG_ERROR, "connect failed with error: %d\n", blob_get_last_sock_error());
    return -1;
  }

  // restart the socket mode
  fd_set Write, Err;
  FD_ZERO(&Write);
  FD_ZERO(&Err);
  FD_SET(sock, &Write);
  FD_SET(sock, &Err);

  // check if the socket is ready
  if (select(0,NULL,&Write,&Err,&timeout) == -1)
  {
    blob_logmessage(LOG_ERROR, "select failed with error: %d\n", blob_get_last_sock_error());
    return -1;
  }
  if (FD_ISSET(sock, &Write))
  {
    if (!blob_set_non_blocking(sock, false))
      blob_logmessage(LOG_ERROR, "ioctlsocket failed with error: %d (%d)\n", blob_get_last_sock_error());
    return 0;
  }
  return -1;
}

intptr_t start_blob_push_client(const char *url, int port, const char *root, int timeout_sec)
{
  const size_t rootLen = root ? strlen(root) : 1;
  if (rootLen>255)
  {
    blob_logmessage(LOG_ERROR, "root len is too big %d", rootLen);
    return -1;
  }

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;
  struct addrinfo *result;
  char portS[32]; std::snprintf(portS, sizeof(portS), "%d", port);
  int err = 0;
  if ((err = getaddrinfo(url, portS, &hints, &result)) != 0)
  {
    blob_logmessage(LOG_ERROR, "can't resolve URL <%s> %s", url, gai_strerror(err));
    return -1;
  }
  intptr_t sockfd = -1;
  for (struct addrinfo *rp = result; rp != NULL; rp = rp->ai_next)
  {
    auto sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1)
      continue;

    if (connect_with_timeout(sfd, rp->ai_addr, rp->ai_addrlen, timeout_sec) != -1)
    {
      sockfd = sfd;
      break;
    }
    blob_close_socket((int)sfd);
  }

  freeaddrinfo(result);
  if (sockfd < 0)
  {
    blob_logmessage(LOG_ERROR, "ERROR connecting to <%s:%d>", url, port);
    return -1;
  }
  blob_set_socket_def_options((int)sockfd);
  blob_logmessage(LOG_NOTIFY, "Connected to <%s:%d>, connection = %d", url, port, sockfd);
  char greeting[greeting_length+1];
  if (!recv_exact(int(sockfd), greeting, greeting_length))
  {
    blob_logmessage(LOG_ERROR, "Can't receive greeting");
    blob_close_socket(int(sockfd));
    return -1;
  }

  if (memcmp(greeting, BLOB_PUSH_GREETING_STR, greeting_prefix_length) != 0)
  {
    greeting[greeting_length] = 0;
    blob_logmessage(LOG_ERROR, "Incorrect greeting <%s>, expected <%s>", greeting, BLOB_PUSH_GREETING_STR);
    blob_close_socket(int(sockfd));
    return -1;
  }
  blob_logmessage(LOG_NOTIFY, "greetings passed <%.*s>. Ask for version", int(greeting_length), greeting);

  char vers_response[response_len+1];
  const uint8_t len = uint8_t(rootLen);
  if (!send_exact(int(sockfd), "VERS" BLOB_PUSH_PROTO_VERSION, command_len + vers_command_len)
     || !send_exact(int(sockfd), &len, 1)
     || !send_exact(int(sockfd), root, len)
     || !recv_exact(int(sockfd), vers_response, response_len))
  {
    blob_close_socket(int(sockfd));
    blob_logmessage(LOG_ERROR, "Server socket error %d", blob_get_last_sock_error());
    return -1;
  }
  if (memcmp(vers_response, none_response, response_len) != 0)
  {
    vers_response[response_len] = 0;
    blob_logmessage(LOG_ERROR, "Server doesn't accept %s, our version " BLOB_PUSH_PROTO_VERSION, vers_response);
    blob_close_socket(int(sockfd));
    return -1;
  }
  blob_set_socket_no_delay(int(sockfd), true);
  return sockfd;
}

bool stop_blob_push_client(intptr_t &sockfd)
{
  if (sockfd < 0)
    return false;
  blob_close_socket(int(sockfd));
  sockfd = -1;
  return true;
}

