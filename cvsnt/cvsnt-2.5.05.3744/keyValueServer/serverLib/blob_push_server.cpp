#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/blob_raw_sockets.h"
#include "../include/blob_sockets.h"
#include "../blob_push_log.h"

#if MULTI_THREADED
#include <thread>
//while multi-threaded should be fine for Linux also (as far as I know, there are no leaks), but why bother?
#else
#include <signal.h>
#endif

bool blob_push_thread_proc(intptr_t raw_socket, uint32_t client_ip, volatile bool *should_stop, const char *encryption_file, CafsServerEncryption encryption);//returns false on attach

bool start_push_server(int portno, int max_connections, volatile bool* should_stop, const char *encryption_secret, CafsServerEncryption encryption)
{
  if (should_stop && *should_stop)
    return true;
  if (!encryption_secret && encryption != CafsServerEncryption::Local)
  {
    blob_logmessage(LOG_ERROR, "only local server can be run without encryption secret provided!");
    return true;
  }
// create a socket
  intptr_t sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    blob_logmessage(LOG_ERROR, "can't open socket %d", raw_get_last_sock_error());
    return false;
  }
  raw_set_socket_no_delay(sockfd, true);
  raw_set_socket_reuse_addr(sockfd, true);
  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;

  // automatically be filled with current host's IP address
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  // convert short integer value for port must be converted into network byte order
  serv_addr.sin_port = htons(portno);
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
  {
    blob_logmessage(LOG_ERROR, "can't bind socket %d", blob_get_last_sock_error());
    raw_close_socket(sockfd);
    return false;
  }
  raw_set_socket_def_options(sockfd);
  #if !MULTI_THREADED
    signal(SIGCHLD,SIG_IGN);
  #endif

  listen(sockfd, max_connections);
  struct sockaddr_in client; socklen_t clientSz = sizeof(client);
  while (!should_stop)
  {
    intptr_t client_raw_sock = (client_raw_sock = accept(sockfd, (struct sockaddr *)&client, (socklen_t*)&clientSz));
    if (client_raw_sock >= 0)
    {
      uint32_t ip; memcpy(&ip, &client.sin_addr.s_addr, sizeof(ip));
      blob_logmessage(LOG_NOTIFY, "got connection %d from %s port %d",
          client_raw_sock, inet_ntoa(client.sin_addr), ntohs(client.sin_port));
      #if MULTI_THREADED
      std::thread(blob_push_thread_proc, client_raw_sock, ip, should_stop, encryption_secret, encryption).detach();
      #else
      const pid_t pid = fork();
      if (pid == 0)
      {
        //we are in child process
        raw_close_socket(sockfd);//doesn't affect parent
        blob_push_thread_proc(client_raw_sock, ip, should_stop, encryption_secret, encryption);
        exit(0);
      } else
      {
        raw_close_socket(client_raw_sock);//doesn't affect child
        if (pid < 0)//wtf
          blob_logmessage(LOG_ERROR, "can't start child process %d", errno);
      }
      #endif
    } else
    {
      auto ret = raw_get_last_sock_error();
      if (ret != EAGAIN)
      {
        blob_logmessage(LOG_ERROR, "can't bind accept connection %d", blob_get_last_sock_error());
        break;
      }
    }
  }

  raw_close_socket(sockfd);
  return true;
}