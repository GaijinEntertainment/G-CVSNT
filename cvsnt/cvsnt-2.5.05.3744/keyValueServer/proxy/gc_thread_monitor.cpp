#include <string>
#include <memory>
#include <algorithm>

//gc thread functions
//these are consts! initialized once in main
static uint64_t file_cache_size = uint64_t(20*1024)<<uint64_t(20);
static std::string cache_folder;
uint64_t space_occupied(const char *dir);
uint64_t free_space(const char *dir, int64_t max_size);

#include <thread>
#include <atomic>
#include <mutex>
static std::atomic<int64_t> cache_occupied_size;
static std::mutex gc_mutex;
static std::condition_variable wakeup_gc_cond;

static void gc_thread_proc();

void init_gc(const char *folder, uint64_t max_size)
{
  cache_folder = folder;
  cache_occupied_size = space_occupied(cache_folder.c_str());
  file_cache_size = max_size;
  printf("Current space occupied by cache folder %dmb\n", (int)(cache_occupied_size.load()>>uint64_t(20)));
  if (cache_occupied_size > file_cache_size)
    printf("It is more than limit, bug GC will be only called on next pull to server\n");
  std::thread gc(gc_thread_proc);
  gc.detach();
}
void close_gc(){}//no bother, threads die with parent

//--
static inline bool should_do_gc() { return (cache_occupied_size.load() > file_cache_size); }
static void do_gc()
{
  if (!should_do_gc())
    return;
  //we have to do gc as occupied space is more than limit
  //recurse over all files in cache_folder, and delete all old files until occupied size is less then file_cache_size
  cache_occupied_size -= free_space(cache_folder.c_str(), uint64_t(file_cache_size));
}

static void gc_thread_proc()
{
  do_gc();
  while(1)
  {
    std::unique_lock<std::mutex> lock(gc_mutex);
    wakeup_gc_cond.wait(lock);
    do_gc();
  }
}

void lazy_report_to_gc(uint64_t sz)
{
  //wake up GC thread
  cache_occupied_size += sz;
  if (should_do_gc())
    wakeup_gc_cond.notify_one();
}

bool perform_immediate_gc(int64_t needed_sz)
{
  if (cache_occupied_size.load() <= 0)//there was occupied space, explicitly free it now and try again
    return false;
  int64_t freedSz = free_space(cache_folder.c_str(), std::max(int64_t(0), int64_t(cache_occupied_size.load()) - needed_sz));
  cache_occupied_size -= freedSz;
  return freedSz > 0;
}

void gc_sleep_msec(int msec)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(msec));
}