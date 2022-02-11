#include "../blob_server_func_deps.h"
#include "../../ca_blobs_fs/content_addressed_fs.h"

void finish_hashes(caddressed_fs::context*);
struct NewHash{char hash[64];};
void created_new_hash(const NewHash &);

void *blob_create_ctx(const char *root) {auto ctx = caddressed_fs::create();caddressed_fs::set_root(ctx, root); return (void*)ctx;}
void blob_destroy_ctx(void *ctx) {
  finish_hashes((caddressed_fs::context*)ctx);//has to be called before caddressed_fs::destroy
  return caddressed_fs::destroy((caddressed_fs::context*) ctx);
}

bool blob_is_under_attack(bool , void *) {return false;}
uint64_t blob_get_hash_blob_size(const void *ctx, const char* htype, const char* hhex) {return caddressed_fs::get_size((const caddressed_fs::context*)ctx, hhex);}
bool blob_does_hash_blob_exist(const void *ctx, const char* htype, const char* hhex) {return caddressed_fs::exists((const caddressed_fs::context*)ctx, hhex);}

uintptr_t blob_start_push_data(const void *ctx, const char* htype, const char* hhex, uint64_t size) {return uintptr_t(caddressed_fs::start_push((const caddressed_fs::context*)ctx, hhex));}
bool blob_push_data(const void *data, uint64_t size, uintptr_t up) { return caddressed_fs::stream_push((caddressed_fs::PushData *)up, data, size); }
void blob_destroy_push_data(uintptr_t up) {caddressed_fs::destroy((caddressed_fs::PushData *)up);}

bool blob_end_push_data(uintptr_t up) {
  NewHash h;h.hash[0]=0;
  const caddressed_fs::PushResult cr = caddressed_fs::finish((caddressed_fs::PushData *)up, h.hash);
  if (!caddressed_fs::is_ok(cr))
    return false;
  if (cr == caddressed_fs::PushResult::OK)
    created_new_hash(h);
  return true;
}


uintptr_t blob_start_pull_data(const void *ctx, const char* htype, const char* hhex, uint64_t &sz){return uintptr_t(caddressed_fs::start_pull((const caddressed_fs::context*)ctx, hhex, sz));}
const char *blob_pull_data(uintptr_t up, uint64_t from, uint64_t &read){return caddressed_fs::pull((caddressed_fs::PullData*)up, from, read);}
bool blob_end_pull_data(uintptr_t up){return caddressed_fs::destroy((caddressed_fs::PullData*)up);}

//repacking
#include <vector>
#if !_WIN32
#include <sys/resource.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "../../ca_blobs_fs/src/details.h"
thread_local std::vector<NewHash> new_hashes;

inline void set_priority(bool low)
{
  #if _WIN32
    #if MULTI_THREADED
      SetThreadPriority(GetCurrentThread(), low ? 0 : 10);
    #endif
  #else
    #if !MULTI_THREADED
      setpriority( PRIO_PROCESS, 0, low ? 20 : 0);
    #else
      int policy = 0;
      struct sched_param param;
      if (pthread_getschedparam(pthread_self(), &policy, &param) == 0)
      {
        param.sched_priority += low ? -20 : 20;
        pthread_setschedparam(pthread_self(), policy, &param);
      }
    #endif
  #endif
}

extern bool repack_immediately;
void finish_hashes(caddressed_fs::context* ctx)
{
  if (!new_hashes.size())
    return;
  set_priority(true);
  for (auto &h:new_hashes)
    caddressed_fs::repack(ctx, h.hash, true);
  new_hashes.clear();
  set_priority(false);
}

void created_new_hash(const NewHash &hash)
{
  if (!repack_immediately)
    return;
  new_hashes.push_back(hash);
}
