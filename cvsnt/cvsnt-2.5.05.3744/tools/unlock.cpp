#include "simpleLock.cpp.inc"
static void usage()
{
  printf("Usage: lockId <server, default 127.0.0.1>\n");
  printf("Or: list <server, default 127.0.0.1>\n");
}

int main(int ac, const char* argv[])
{
  if (ac < 2)
  {
    usage();
    exit(1);
  }
  tcp_init();
  int lock_server_socket = -1;
  if (strcmp(argv[1], "list") == 0)
  {
    printf("monitoring\n");
    print_status(ac > 2 ? argv[2] : "127.0.0.1");
  } else
  {
    uint32_t lockId = 0;
    sscanf(argv[1],"%u",&lockId);
    force_unlock(ac > 2 ? argv[2] : "127.0.0.1", lockId);
  }

  cvs_tcp_close(lock_server_socket);
  return 0;
}
