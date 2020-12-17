#include <string>
#include <memory>
#include <algorithm>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

//these are consts! initialized once in main
static uint64_t file_cache_size = uint64_t(20*1024)<<uint64_t(20);
static std::string cache_folder;
uint64_t space_occupied(const char *dir);
uint64_t free_space(const char *dir, int64_t max_size);
static pid_t gc_pid = 0;
void init_gc(const char *folder, uint64_t max_size)
{
  cache_folder = folder;
  file_cache_size = max_size;
  if ((gc_pid = fork()) != 0) //parent or error
    return;

  while(1) {//infinite loop
    free_space(cache_folder.c_str(), uint64_t(file_cache_size));
    usleep(600*1000000);//sleep for 600 seconds, 10 minutes.
  }
}

void close_gc()
{
  if (gc_pid > 0)
    kill(gc_pid, SIGTERM);
}

//--
void lazy_report_to_gc(uint64_t )
{
  //we can use pipes to signal to sibling process, but instead we just each 10 minutes free cache
}

bool perform_immediate_gc(int64_t needed_sz)
{
  return free_space(cache_folder.c_str(), -needed_sz) > 0;
}