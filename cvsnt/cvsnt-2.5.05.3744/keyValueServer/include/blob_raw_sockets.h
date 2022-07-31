#pragma once
#define NOMINMAX
#include <stdio.h>
#include <stdint.h>

static constexpr size_t default_socket_timeout = 30*60;//30 minutes
#if _WIN32
  #pragma comment(lib,"Ws2_32.lib")
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <stdlib.h>
  #include <fcntl.h>
  inline bool raw_init_sockets()
  {
    WSADATA wsaData = {0};
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
  }
  inline void raw_close_sockets(){ WSACleanup(); }
  inline void raw_close_socket(intptr_t &raw_socket) { closesocket(raw_socket); raw_socket = intptr_t(-1);}
  inline int raw_get_last_sock_error() { return WSAGetLastError(); }
  typedef int socklen_t;
  enum {MSG_NOSIGNAL = 0};
  #define SOCKET_EWOULDBLOCK WSAEWOULDBLOCK
  inline bool raw_set_non_blocking(intptr_t raw_socket, bool on)
  {
    SOCKET sock = (SOCKET)raw_socket;
    unsigned long iMode = on ? 1 : 0;
    return ioctlsocket(sock, FIONBIO, &iMode) == 0;
  }
  inline bool raw_socket_would_block(int err)
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

  inline void raw_close_socket(intptr_t &raw_socket) { close(raw_socket); raw_socket = intptr_t(-1);}
  inline bool raw_init_sockets(){return true;}
  inline void raw_close_sockets(){}
  inline int raw_get_last_sock_error() { return errno; }
  #if defined(__APPLE__) && !defined(MSG_NOSIGNAL)
  enum {MSG_NOSIGNAL = 0};
  #endif
  #define SOCKET_EWOULDBLOCK EWOULDBLOCK
  typedef int SOCKET;
  inline bool raw_set_non_blocking(intptr_t raw_socket, bool on)
  {
    SOCKET sock = (SOCKET)raw_socket;
    int arg = fcntl(sock, F_GETFL, NULL);
    if (on)
      arg |= O_NONBLOCK;
    else
      arg &= ~O_NONBLOCK;
    return fcntl(sock, F_SETFL, arg) != -1;
  }
  inline bool raw_socket_would_block(int err)
  {
    return err == EWOULDBLOCK || err==EINPROGRESS;
  }

#if defined(__APPLE__) && !defined(SOL_TCP)
  #define SOL_TCP IPPROTO_TCP
#endif

#endif

inline bool raw_set_socket_reuse_addr(intptr_t socket, bool reuse_addr)
{
  SOCKET sock = (SOCKET)socket;
  int v = reuse_addr ? 1 : 0;
  //todo: may be add SO_EXCLUSIVEADDRUSE on Windows? otherwise there is a possibility of malicious
  return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &v, sizeof(v)) == 0;
}

inline void raw_set_socket_no_delay(intptr_t socket, bool no_delay)
{
  SOCKET sock = (SOCKET)socket;
  int v = no_delay ? 1 : 0;
  setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *) &v, sizeof(v));
}

inline void raw_send_recieve_sock_timeout(intptr_t socket, int timeout_sec)
{
  SOCKET sock = (SOCKET)socket;
  #if !_WIN32
  struct timeval timeout; memset(&timeout, 0, sizeof(timeout)); timeout.tv_sec = timeout_sec;
  #else
  DWORD timeout = timeout_sec* 1000;
  #endif
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
}

inline void raw_set_socket_def_options(intptr_t socket)
{
  #ifdef __APPLE__
  SOCKET sock = (SOCKET)socket;
  int set = 1;
  setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
  #endif
  raw_send_recieve_sock_timeout(socket, default_socket_timeout);//set timeout to default
}


inline void raw_enable_keepalive(intptr_t sock, bool keep_alive) {
  int v = keep_alive ? 1 : 0;
  setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&v, sizeof(int));
}

inline void raw_set_keepalive_tcp(intptr_t sock, int keep_cnt=5, int keep_interval=30, int keep_idle=60) {
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
  raw_enable_keepalive(sock, true);
}


static inline int raw_recv_exact(intptr_t socket, void *data, int len_)
{
  for (int len = len_; len > 0;)
  {
    int l = (int)recv(socket, (char*)data, len, 0);
    if (l <= 0)
      return l;
    data = (char*)data + l;
    len -= l;
  }
  return len_;
}

static inline int raw_send_exact(intptr_t socket, const void *data, int len_)
{
  for (int len = len_; len > 0;)
  {
    constexpr int quant = (1<<30);//1Gb of data
    int sendQuant = quant < len ? quant : len;
    int l = (int)send(socket, (const char*)data, sendQuant, MSG_NOSIGNAL);
    if (l <= 0)
      return l;
    data = (const char*)data + l;
    len -= l;
  }
  return len_;
}
