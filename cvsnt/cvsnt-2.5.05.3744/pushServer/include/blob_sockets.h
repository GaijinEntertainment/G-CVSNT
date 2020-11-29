#pragma once
#define NOMINMAX
#include <stdio.h>
#if _WIN32
  #pragma comment(lib,"Ws2_32.lib")
  #include <winsock.h>
  #include <stdlib.h>
  inline bool blob_init_sockets()
  {
    WSADATA wsaData = {0};
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
  }
  inline void blob_close_sockets(){ WSACleanup(); }
  inline void blob_close_socket(int socket) { closesocket(socket); }
  inline int blob_get_last_sock_error() { return WSAGetLastError(); }
  typedef int socklen_t;
#else
  #include <sys/socket.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <stdlib.h>
  #include <string.h>
  #include <unistd.h>
  #include <errno.h>
  #include <netdb.h>

  inline void blob_close_socket(int socket) { close(socket); }
  inline bool blob_init_sockets(){return true;}
  inline void blob_close_sockets(){}
  inline int blob_get_last_sock_error() { return errno; }
#endif

inline void blob_set_socket_no_delay(int socket, bool no_delay)
{
  int v = no_delay ? 1 : 0;
  setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char *) &v, sizeof(v));
}
