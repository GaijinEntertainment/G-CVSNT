#include <stdio.h>
#include <stdlib.h>
#include "../include/blob_server.h"
#include "../include/blob_sockets.h"//move init to out of line
#define BLOB_LOG_LEVEL LOG_WARNING
#include "../sampleImplementation/def_log_printf.cpp"

void init_proxy(const char *url, int port, const char *cache, size_t sz);

int main(int argc, const char **argv)
{
  if (argc < 3)
  {
    printf("Usage is cafs_proxy_server master_url cache_folder [cache_soft_limit_size (mb). default is 20480 == 20Gb]\n");
    return 1;
  }
  if (!blob_init_sockets())
  {
    blob_logmessage(LOG_ERROR, "Can't init sockets, %d", blob_get_last_sock_error());
    return 1;
  }
  const int master_port = 2403;
  init_proxy(argv[1], master_port, argv[2], argc>3 ? atoi(argv[3]) : 20*1024);
  printf("Starting server listening at port %d\n", master_port);
  const bool result = start_push_server(master_port, 1024, nullptr);
  printf("server quit %s", result ? "with error\n" :"normally\n");
  blob_close_sockets();
  return result ? 0 : 1;
}