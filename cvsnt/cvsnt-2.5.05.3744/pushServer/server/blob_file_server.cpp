#include <stdio.h>
#include <stdlib.h>
#include "../include/blob_server.h"
#include "../sampleImplementation/def_log_printf.cpp"
#include "../include/blob_sockets.h"//move init to out of line

int main(int argc, const char **argv)
{
  if (argc < 2)
    printf("Usage is blob_file_server [<port>] [<max_pending_connections>]\n");
  if (!blob_init_sockets())
  {
    blob_logmessage(LOG_ERROR, "Can't init sockets, %d", blob_get_last_sock_error());
    return 1;
  }
  int port = argc>1 ? atoi(argv[1]) : 2403;
  int max_pending = argc>2 ? atoi(argv[2]) : 1024;
  volatile bool shouldStop = false;
  const bool result = start_push_server(port, max_pending, nullptr);
  printf("server quit %s", result ? "with error\n" :"normally\n");
  blob_close_sockets();
  return result ? 0 : 1;
}