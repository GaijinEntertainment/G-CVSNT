#include <string>
#include <memory>
#include <algorithm>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

//these are consts! initialized once in main
static uint64_t file_cache_size = uint64_t(20*1024)<<uint64_t(20);
static std::string cache_folder;
uint64_t available_disk_space(const char *dir);
uint64_t space_occupied(const char *dir);
uint64_t free_space(const char *dir, int64_t max_size, int64_t &currentOccupied);
uint64_t free_space(const char *dir, int64_t max_size);

static pid_t gc_pid = 0;
void init_gc(const char *folder, uint64_t max_size)
{
  cache_folder = folder;
  file_cache_size = max_size;
  if ((gc_pid = fork()) != 0) //parent or error
    return;

  int64_t lastOccupied = 0;
  free_space(cache_folder.c_str(), uint64_t(file_cache_size), lastOccupied);//free space to required
  int64_t lastAvail = available_disk_space(cache_folder.c_str());
  while(1) {//infinite loop
    for (int i = 0; i < 10; ++i)
    {
      const int64_t avail = available_disk_space(cache_folder.c_str());//current available space
      //lastAvail - avail = <approximately additionally added data>
      // so, (lastAvail - avail + lastOccupied) = <approximate current cache size>
      // so, if (lastAvail - avail + lastOccupied) = <approximate current cache size> > file_cache_size then we are above the limit
      // of course, someone else could allocate data as well
      if (avail == 0 || (lastAvail + lastOccupied > int64_t(file_cache_size) + avail))
      {
        //if there is
        if (free_space(cache_folder.c_str(), uint64_t(file_cache_size), lastOccupied) > 0)// if we have freed some data, change availble amount
        {
          lastAvail = available_disk_space(cache_folder.c_str());
          i = 0;
        }
      }
      usleep(60*1000000);//sleep for 60 seconds, 1 minute.
    }
    free_space(cache_folder.c_str(), uint64_t(file_cache_size), lastOccupied);
    lastAvail = available_disk_space(cache_folder.c_str());
    usleep(60*1000000);//sleep for 60 seconds, 1 minute.
    //usleep(600*1000000);//sleep for 600 seconds, 10 minutes.
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
  if (needed_sz < 0)
  {
    const int64_t space = available_disk_space(cache_folder.c_str());
    if (space > 0 && space > -needed_sz)
      return true;
  }
  return free_space(cache_folder.c_str(), -needed_sz) > 0;
}

void gc_sleep_msec(int msec)
{
  usleep(int64_t(msec)*1000);//sleep for 60 seconds, 1 minute.
}