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

static inline bool recv_exact(BlobSocket socket, void *data, uint64_t len)
{
  while (len > 0)
  {
    int l = recv(socket, (char*)data, (int)std::min(len, uint64_t(1<<30)));
    HANDLE_SOCKET_ERROR(l);
    data = (char*)data + l;
    len -= l;
  }
  return true;
}

static inline bool send_exact(BlobSocket socket, const void *data, uint64_t len)
{
  while (len > 0)
  {
    constexpr uint64_t quant = (1<<30);//1Gb of data
    uint64_t sendQuant = std::min(quant, len);
    int l = send_msg_no_signal(socket, (const char*)data, (int)sendQuant);
    HANDLE_SOCKET_ERROR(l)
    data = (const char*)data + l;
    len -= l;
  }
  return true;
}

template<typename Callback>
static inline bool recv_lambda(BlobSocket socket, uint64_t total, Callback cb)
{
  uint64_t sizeLeft = total;
  char buf[65536];
  while (sizeLeft > 0)
  {
    int l = recv(socket, buf, (int)std::min(sizeLeft, (uint64_t)sizeof(buf)));
    HANDLE_SOCKET_ERROR(l)
    cb(buf, l);
    sizeLeft -= l;
  }
  return true;
}

//instead of Naggle algoritm. Naggle doesn't know structire of app data. It introduces constant delay for small packets
//in the same time it optimizes for MTU size utilization
//by buffering data we also optimize for MTU, but we won't wait for 100ms for last packet to be send
template<int bufSize>
struct BufferedSocketOutput
{
  BufferedSocketOutput(BlobSocket sockfd): sock(sockfd) {}
  ~BufferedSocketOutput(){sendBuf();}
  BufferedSocketOutput(const BufferedSocketOutput&) = delete;
  BufferedSocketOutput &operator =(const BufferedSocketOutput&) = delete;

  bool send(const void *data_, int data_size)
  {
    const char *data = (const char *)data_;
    if (bufOccupied)
    {
      const int bufLeft = sizeof(buf) - bufOccupied;
      const int copyToBuf = data_size < bufLeft ? data_size : bufLeft;
      memcpy(buf + bufOccupied, data, copyToBuf);
      bufOccupied += copyToBuf;
      data += copyToBuf;data_size -= copyToBuf;
      if (bufOccupied < sizeof(buf))
        return true;
      if (!sendBuf())
        return false;
    }
    return send_and_bufferize_reminder(data, data_size);
  }
  bool finish(){return sendBuf();}
  BlobSocket sock;
protected:
  bool sendBuf()
  {
    if (!send_exact(sock, buf, bufOccupied))
      return false;
    bufOccupied = 0;
    return true;
  }

  bool send_and_bufferize_reminder(const char *data, int data_size)
  {
    if (!data_size)
      return true;
    //assert(bufOccupied == 0);
    if (data_size >= sizeof(buf))
    {
      int data_send = (data_size/sizeof(buf))*sizeof(buf);
      if (!send_exact(sock, data, data_send))
        return false;
      data += data_send;data_size -= data_send;
    }
    memcpy(buf, data, bufOccupied = data_size);
    return true;
  }
  int bufOccupied = 0;
  char buf[bufSize];
};
