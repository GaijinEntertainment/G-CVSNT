#include "simpleLock.cpp.inc"
static void usage()
{
  printf("Usage: <server> <user> <full_root/> (<full_directory/file,v> | <lock_id))\n");
  printf("example:simplelock simplelock.exe 127.0.0.1 some_user /home/some_user/test/ testDir/a.png,v\n");
}

int main(int ac, const char* argv[])
{
  if (ac < 5)
  {
    usage();
    exit(1);
  }
  tcp_init();
  int lock_server_socket = lock_register_client(argv[2], argv[3], argv[1]);
  if (lock_server_socket == -1)
  {
    printf("Can't connect\n");
    exit(1);
  }
  char fullPath[512];
  snprintf(fullPath, sizeof(fullPath), "%s%s", argv[3], argv[4]);
  size_t lockId = do_lock_file(lock_server_socket, fullPath, 1, 1);
  printf("obtained lock %lld, sleep for 1sec\n", (uint64_t) lockId);
  sleep(1000);
  do_unlock_file(lock_server_socket, lockId);

  cvs_tcp_close(lock_server_socket);
  return 0;
}
