#include "blob_sockets.h"
#include "../blob_push_log.h"


#define HANDLE_SOCKET_ERROR(socket_ret)\
if (socket_ret == 0)/*gracefully closed*/{\
  blob_logmessage(LOG_NOTIFY, "socket returned 0, err=%d", blob_get_last_sock_error());\
  return false;\
}\
if (socket_ret < 0)\
{\
  blob_logmessage(LOG_ERROR, "can't read socket, err=%d", blob_get_last_sock_error());\
  return false;\
}


static inline bool recv_exact(int socket, void *data, unsigned int len, int flags = 0)
{
  while (len > 0)
  {
    int l = recv(socket, (char*)data, len, 0);
    HANDLE_SOCKET_ERROR(l);
    data = (char*)data + l;
    len -= l;
  }
  return true;
}

static inline bool send_exact(int socket, const void *data, int len, int flags = 0)
{
  while (len > 0)
  {
    int l = send(socket, (const char*)data, len, flags);
    HANDLE_SOCKET_ERROR(l)
    data = (const char*)data + l;
    len -= l;
  }
  return true;
}

template<typename Callback>
static inline bool recv_lambda(int socket, uint64_t total, Callback cb,int flags = 0)
{
  int64_t sizeLeft = total;
  char buf[65536];
  while (sizeLeft > 0)
  {
    int l = recv(socket, buf, std::min(sizeLeft, (int64_t)sizeof(buf)), 0);
    HANDLE_SOCKET_ERROR(l)
    cb(buf, l);
    sizeLeft -= l;
  }
  return true;
}
