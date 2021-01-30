#include <stdio.h>
#include <stdlib.h>
#include "../include/blob_server.h"
#include "../sampleImplementation/def_log_printf.cpp"
#include "../include/blob_sockets.h"//move init to out of line
#include "../../ca_blobs_fs/content_addressed_fs.h"//move init to out of line

bool repack_immediately = true;

int main(int argc, const char **argv)
{
  if (argc < 3)
  {
    printf("Usage is cafs_server dir_for_roots allow_trust(on|off) [norepack] [port](2403) [max_pending_connections](1024)\n");
    return 1;
  }
  bool allow = strcmp(argv[2], "on") == 0;
  printf("Starting content-addressed file server with root=<%s> and %s\n", argv[1], allow ? "trust client" : "don't trust client");
  caddressed_fs::set_dir_for_roots(strcmp(argv[1], "/") == 0 ? "" : argv[1]);
  caddressed_fs::set_allow_trust(allow);
  int pc = 3;
  if (!blob_init_sockets())
  {
    blob_logmessage(LOG_ERROR, "Can't init sockets, %d", blob_get_last_sock_error());
    return 1;
  }
  if (argc > pc && strcmp(argv[pc], "norepack") == 0)
  {
    repack_immediately = false;
    ++pc;
  }
  int port = argc>pc ? atoi(argv[pc]) : 2403;
  int max_pending = argc>pc+1 ? atoi(argv[pc+1]) : 1024;
  volatile bool shouldStop = false;
  printf("Starting server listening at port %d%s\n", port, repack_immediately ? ", with auto repacking" : ", without auto repacking");
  const bool result = start_push_server(port, max_pending, nullptr);
  printf("server quit %s", result ? "with error\n" :"normally\n");
  blob_close_sockets();
  return result ? 0 : 1;
}