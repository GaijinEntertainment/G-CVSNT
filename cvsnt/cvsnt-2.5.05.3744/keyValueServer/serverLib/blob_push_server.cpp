#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include "../include/blob_sockets.h"
#include "../blob_push_log.h"

void blob_push_thread_proc(int socket, volatile bool *should_stop);

bool start_push_server(int portno, int max_connections, volatile bool *should_stop)
{
  if (should_stop && *should_stop)
    return true;
// create a socket
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    blob_logmessage(LOG_ERROR, "can't open socket %d", blob_get_last_sock_error());
    return false;
  }
  blob_set_socket_no_delay(sockfd, true);
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
    blob_close_socket(sockfd);
    return false;
  }

  listen(sockfd, max_connections);
  struct sockaddr_in client; socklen_t clientSz = sizeof(client);
  int client_sock;
  while(!should_stop && (client_sock = accept(sockfd, (struct sockaddr *)&client, (socklen_t*)&clientSz)) >= 0)
  {
    blob_set_socket_no_delay(client_sock, true);
    blob_logmessage(LOG_NOTIFY, "got connection %d from %s port %d",
        client_sock, inet_ntoa(client.sin_addr), ntohs(client.sin_port));

    std::thread client_thread(blob_push_thread_proc, client_sock, should_stop);
    client_thread.detach();
  }

  blob_logmessage(LOG_ERROR, "can't bind accept connection %d", blob_get_last_sock_error());
  blob_close_socket(sockfd);
  return true;
}