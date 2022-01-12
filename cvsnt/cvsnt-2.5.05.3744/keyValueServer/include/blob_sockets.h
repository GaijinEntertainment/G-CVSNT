#pragma once
#define NOMINMAX
#include <stdio.h>
#if _WIN32
  #pragma comment(lib,"Ws2_32.lib")
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <stdlib.h>
  #include <fcntl.h>
  inline bool blob_init_sockets()
  {
    WSADATA wsaData = {0};
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
  }
  inline void blob_close_sockets(){ WSACleanup(); }
  inline void blob_close_socket(int socket) { closesocket(socket); }
  inline int blob_get_last_sock_error() { return WSAGetLastError(); }
  typedef int socklen_t;
  enum {MSG_NOSIGNAL = 0};
  #define SOCKET_EWOULDBLOCK WSAEWOULDBLOCK
  inline bool blob_set_non_blocking(SOCKET sock, bool on)
  {
    unsigned long iMode = on ? 1 : 0;
    return ioctlsocket(sock, FIONBIO, &iMode) == 0;
  }
  inline bool blob_socket_would_block(int err)
  {
    return err == WSAEWOULDBLOCK;
  }
#else
  #include <sys/socket.h>
  #include <sys/time.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <stdlib.h>
  #include <string.h>
  #include <unistd.h>
  #include <errno.h>
  #include <netdb.h>
  #include <fcntl.h>

  inline void blob_close_socket(int socket) { close(socket); }
  inline bool blob_init_sockets(){return true;}
  inline void blob_close_sockets(){}
  inline int blob_get_last_sock_error() { return errno; }
  #if defined(__APPLE__) && !defined(MSG_NOSIGNAL)
  enum {MSG_NOSIGNAL = 0};
  #endif
  #define SOCKET_EWOULDBLOCK EWOULDBLOCK
  typedef int SOCKET;
  inline bool blob_set_non_blocking(SOCKET sock, bool on)
  {
    int arg = fcntl(sock, F_GETFL, NULL);
    if (on)
      arg |= O_NONBLOCK;
    else
      arg &= ~O_NONBLOCK;
    return fcntl(sock, F_SETFL, arg) != -1;
  }
  inline bool blob_socket_would_block(int err)
  {
    return err == EWOULDBLOCK || err==EINPROGRESS;
  }

#if defined(__APPLE__) && !defined(SOL_TCP)
  #define SOL_TCP IPPROTO_TCP
#endif

#endif

inline void blob_set_socket_def_options(int socket)
{
  (void)socket;
  #ifdef __APPLE__
  int set = 1;
  setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
  #endif
}

inline void blob_set_socket_no_delay(int socket, bool no_delay)
{
  int v = no_delay ? 1 : 0;
  setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char *) &v, sizeof(v));
}

inline bool blob_set_socket_reuse_addr(int socket, bool reuse_addr)
{
  int v = reuse_addr ? 1 : 0;
  //todo: may be add SO_EXCLUSIVEADDRUSE on Windows? otherwise there is a possibility of malicious
  return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *) &v, sizeof(v)) == 0;
}

inline void enable_keepalive(int sock, bool keep_alive) {
  int v = keep_alive ? 1 : 0;
  setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&v, sizeof(int));
}

inline void blob_send_recieve_sock_timeout(int sock, int timeout_sec) {
  #if !_WIN32
  struct timeval timeout; memset(&timeout, 0, sizeof(timeout)); timeout.tv_sec = timeout_sec;
  #else
  DWORD timeout = timeout_sec* 1000;
  #endif
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
}

inline void set_keepalive_tcp(int sock, int keep_cnt=5, int keep_interval=30, int keep_idle=60) {
  (void)keep_idle;
  (void)keep_cnt;
  (void)keep_interval;

  #if !_WIN32
  #ifndef __APPLE__
  setsockopt(sock, SOL_TCP, TCP_KEEPIDLE, (char *)&keep_idle, sizeof(keep_idle));
  #endif
  setsockopt(sock, SOL_TCP, TCP_KEEPCNT, (char *)&keep_cnt, sizeof(keep_cnt));
  setsockopt(sock, SOL_TCP, TCP_KEEPINTVL, (char *)&keep_interval, sizeof(keep_interval));
  #endif
  enable_keepalive(sock, true);
}