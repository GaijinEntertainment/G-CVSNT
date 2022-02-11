#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <thread>
#include <algorithm>
#include <time.h>

#include "../include/blob_sockets.h"
#include "../include/blob_raw_sockets.h"
#include "../include/blob_push_protocol.h"
#include "../include/blob_push_proto_ver.h"
#include "../include/blob_common_net.h"
#include "../blob_push_log.h"
#include "../include/blob_client_lib.h"

using namespace blob_push_proto;

int connect_with_timeout(intptr_t raw_sock, const struct sockaddr *addr, size_t addr_len, int timeout_sec)
{
  if (timeout_sec <= 0)
    return connect(raw_sock, addr, (int)addr_len);//no timeout
  timeval timeout;
  timeout.tv_sec = timeout_sec;
  timeout.tv_usec = 0;

  //set the socket in non-blocking
  if (!raw_set_non_blocking(raw_sock, true))
    blob_logmessage(LOG_ERROR, "ioctlsocket failed with error: %d (%d)\n", blob_get_last_sock_error());
  auto connectRet = connect(raw_sock, addr, (int)addr_len);
  if (connectRet == -1 && !raw_socket_would_block(raw_get_last_sock_error()))
  {
    blob_logmessage(LOG_ERROR, "connect failed with error: %d\n", raw_get_last_sock_error());
    return -1;
  }

  // restart the socket mode
  fd_set Write, Err;
  FD_ZERO(&Write);
  FD_ZERO(&Err);
  FD_SET(raw_sock, &Write);
  FD_SET(raw_sock, &Err);

  // check if the socket is ready
  if (select((SOCKET)(raw_sock+1),NULL,&Write,&Err,&timeout) == -1)
  {
    blob_logmessage(LOG_ERROR, "select failed with error: %d\n", raw_get_last_sock_error());
    return -1;
  }
  if (FD_ISSET(raw_sock, &Write))
  {
    if (!raw_set_non_blocking(raw_sock, false))
      blob_logmessage(LOG_ERROR, "ioctlsocket failed with error: %d (%d)\n", raw_get_last_sock_error());
    return 0;
  }
  return -1;
}

BlobSocket start_blob_push_client(const char* url, int port, const char* root, int timeout_sec, const uint8_t *otp, uint64_t otp_page, CafsClientAuthentication encryption)
{
  if (encryption == CafsClientAuthentication::RequiresAuth && !otp)
  {
    blob_logmessage(LOG_ERROR, "Client can't require authentication if it has no OTP itself");
    return BlobSocket();
  }
  const size_t rootLen = root ? strlen(root) : 1;
  if (rootLen>255)
  {
    blob_logmessage(LOG_ERROR, "root len is too big %d", rootLen);
    return BlobSocket();
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
    return BlobSocket();
  }
  intptr_t raw_sockfd = -1;
  uint32_t serverIP = ~0;
  for (struct addrinfo *rp = result; rp != NULL; rp = rp->ai_next)
  {
    intptr_t sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1)
      continue;

    if (connect_with_timeout(sfd, rp->ai_addr, rp->ai_addrlen, timeout_sec) != -1)
    {
      memcpy(&serverIP, &((const sockaddr_in *)rp->ai_addr)->sin_addr.s_addr, sizeof(serverIP));//assume ipv4
      raw_sockfd = sfd;
      break;
    }
    raw_close_socket(sfd);
  }

  freeaddrinfo(result);
  if (raw_sockfd < 0)
  {
    blob_logmessage(LOG_ERROR, "ERROR connecting to <%s:%d>", url, port);
    return BlobSocket();
  }
  raw_set_socket_def_options(raw_sockfd);
  raw_set_socket_no_delay(raw_sockfd, true);

  char greeting[greeting_length+1];
  if (raw_recv_exact(raw_sockfd, greeting, greeting_length) <= 0)
  {
    blob_logmessage(LOG_ERROR, "Can't receive greeting");
    raw_close_socket(raw_sockfd);
    return BlobSocket();
  }

  greeting[greeting_length] = 0;
  if (memcmp(greeting, BLOB_PUSH_GREETING_STR, greeting_prefix_length) != 0)
  {
    blob_logmessage(LOG_ERROR, "Incorrect greeting <%s>, expected <%s>", greeting, BLOB_PUSH_GREETING_STR);
    raw_close_socket(raw_sockfd);
    return BlobSocket();
  }
  const uint32_t receivedVersion = (greeting[greeting_prefix_length]-'0')*100 + (greeting[greeting_prefix_length+1]-'0')*10 + (greeting[greeting_prefix_length+2]-'0');
  const bool isAuthServer = receivedVersion >= BLOB_PUSH_AUTH_VERSIONI;
  blob_logmessage(LOG_NOTIFY, "greetings passed <%.*s>, server version is %d. Ask for version", int(greeting_length), greeting, isAuthServer);
  const bool authServer = otp && isAuthServer;

  if (!authServer)
  {
    if (encryption == CafsClientAuthentication::RequiresAuth || blob_classify_ip(serverIP) == IpType::PUBLIC)
      blob_logmessage(LOG_ERROR, "Server <%s:%d> doesn't support authentication, and client demands it for %s ip, %d",
        url, port, raw_sockfd, blob_classify_ip(serverIP) == IpType::PUBLIC ? "public" : "private");
    raw_close_socket(raw_sockfd);
    return BlobSocket();
  }

  uint8_t authComand[command_len + vers_command_len+1 + sizeof(otp_page)];
  const char* clientVersion = authServer ? BLOB_PUSH_AUTH_VERSION : BLOB_PUSH_PROTO_VERSION;
  size_t authCommandTotalLen = 0;
  {
    uint8_t *authAt = authComand;
    memcpy(authAt, "VERS", command_len); authAt += command_len;
    memcpy(authAt, clientVersion, vers_command_len); authAt += vers_command_len;
    if (authServer)
    {
      memcpy(authAt, &otp_page, sizeof(otp_page)); authAt += sizeof(otp_page);
    }
    authCommandTotalLen = authAt - authComand;
    if ( authCommandTotalLen >= sizeof(authComand))
    {
      blob_logmessage(LOG_ERROR, "INTERNAL ERROR");
      raw_close_socket(raw_sockfd);
      return BlobSocket();
    }
  }
  if (raw_send_exact(raw_sockfd, authComand, (int)authCommandTotalLen) <= 0)
  {
    blob_logmessage(LOG_ERROR, "Server socket error %d", raw_get_last_sock_error());
    raw_close_socket(raw_sockfd);
    return BlobSocket();
  }
  BlobSocket blobSocket;
  if (authServer)
  {
    blobSocket = connect_to_server_blob_socket(raw_sockfd, otp);
    if (!is_valid(blobSocket))
    {
      blob_logmessage(LOG_ERROR, "Can't connect to encrypted server <%s:%d>, connection = %d", url, port, raw_sockfd);
      return blobSocket;
    }
    const uint64_t curTimeStamp = blob_get_timestamp();
    uint8_t paddedTimeStamp[16];
    memset(paddedTimeStamp, 0xFF, sizeof(paddedTimeStamp));
    memcpy(paddedTimeStamp, &curTimeStamp, sizeof(curTimeStamp));
    if (!send_exact(blobSocket, paddedTimeStamp, sizeof(paddedTimeStamp)))
    {
      blob_logmessage(LOG_ERROR, "Can't send timestamp");
      blob_close_socket(blobSocket);
      return BlobSocket();
    }
  } else
  {
    blobSocket = connect_to_server_blob_socket_no_auth(raw_sockfd);
  }

  {
    const uint8_t len = root ? uint8_t(rootLen) : 0;
    uint8_t rootComand[256+1];
    size_t rootComandTotalLen = 0;
    memcpy(rootComand, &len, 1);
    memcpy(rootComand+1, root, len);
    if (!send_exact(blobSocket, rootComand, int(len)+1))
    {
      blob_logmessage(LOG_ERROR, "Can't send root");
      blob_close_socket(blobSocket);
      return BlobSocket();
    }
  }

  char vers_response[response_len+1];
  if (!recv_exact(blobSocket, vers_response, response_len))
  {
    blob_logmessage(LOG_ERROR, "Can't receive auth response");
    blob_close_socket(blobSocket);
    return BlobSocket();
  }
  vers_response[response_len] = 0;
  if (memcmp(vers_response, have_response, response_len) == 0)
  {
    blob_logmessage(LOG_NOTIFY, "Server want to encrypt traffic");
  } else if (memcmp(vers_response, none_response, response_len) == 0)
  {
    blob_logmessage(LOG_NOTIFY, "Server want's to close encryption");
    blob_close_encryption(blobSocket);
  } else if (memcmp(vers_response, err_file_error, response_len) == 0)
  {
    blob_logmessage(LOG_ERROR, "Server doesn't accept our time! Check your clock.");
    blob_close_socket(blobSocket);
    return BlobSocket();
  } else if (memcmp(vers_response, err_bad_command, response_len) == 0)
  {
    blob_logmessage(LOG_ERROR, "Server doesn't accept our version, our version %s", clientVersion);
    blob_close_socket(blobSocket);
    return BlobSocket();
  } else
  {
    blob_logmessage(LOG_ERROR, "Invalid server response %s, our version %s", vers_response, clientVersion);
    blob_close_socket(blobSocket);
    return BlobSocket();
  }

  blob_logmessage(LOG_NOTIFY, "Connected to <%s:%d>, connection = %d", url, port, blobSocket.opaque);

  return blobSocket;
}

bool stop_blob_push_client(BlobSocket &sockfd)
{
  if (!is_valid(sockfd))
    return false;
  blob_close_socket(sockfd);
  sockfd = BlobSocket();
  return true;
}

