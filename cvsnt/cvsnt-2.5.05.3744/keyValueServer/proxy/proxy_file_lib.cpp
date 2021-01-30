#include <string>
#include <memory>
#include <condition_variable>
#include "../blob_server_func_deps.h"
#include "../../ca_blobs_fs/content_addressed_fs.h"
#include "../../ca_blobs_fs/src/fileio.h"
#include "../../ca_blobs_fs/src/details.h"
#include "../include/blob_client_lib.h"
#include "../blob_push_log.h"

static std::string master_url;
static std::string cache_folder;
static int master_port = 2403;
//--
void init_gc(const char *folder, uint64_t max_size);
void close_gc();
void lazy_report_to_gc(uint64_t sz);
bool perform_immediate_gc(int64_t needed_sz);

//
void close_proxy(){close_gc();}
void init_proxy(const char *url, int port, const char *cache, size_t sz)
{
  master_url = url;
  master_port = port;
  cache_folder = cache;
  blob_fileio_ensure_dir(cache_folder.c_str());
  if (cache_folder.length()==0)
    cache_folder = ".";
  if (cache_folder[cache_folder.length() - 1] != '/')
    cache_folder += "/";
  init_gc(cache_folder.c_str(), uint64_t(sz)<<uint64_t(20));
}
///

struct ClientConnection
{
  mutable intptr_t cs = intptr_t(-1);
  std::string root;
  ClientConnection(const char*root_):root(root_)
  {
  }
  bool start()const{cs = start_blob_push_client(master_url.c_str(), master_port, root.c_str());return cs >= 0;}
  void kill() const{stop_blob_push_client(cs);}
  void restart() const{kill();start();}
  ~ClientConnection(){kill();}
};

void *blob_create_ctx(const char *root) {
  ClientConnection *cc = new ClientConnection(root);
  if (!cc->start())
  {
    blob_logmessage(LOG_ERROR, "can't connect to root url %s:%d root <%s>", master_url.c_str(), master_port, root);
    delete cc;
    return nullptr;
  }
  return (void*)cc;
}

void blob_destroy_ctx(void *ctx) {
  delete (ClientConnection*)ctx;
}

std::string get_hash_file_folder(const char* /*htype*/, const char* hhex)
{
  char sub[4] = {hhex[0], hhex[1], '/', '\0'};
  return (cache_folder + sub);
}

std::string get_hash_file_name(const char* htype, const char* hhex)
{
  if (!hhex || !hhex[0] || !hhex[1])
    return std::string();
  return get_hash_file_folder(htype, hhex) + hhex;
}

size_t blob_get_hash_blob_size(const void *c, const char* htype, const char* hhex) {
  //we return local file size if exist. If blob was removed from master that would cause ability to checkout non-existent blob (but the one that existed earlier)
  //if blob has changed on server (repacked) we will return cached local size anyway
  const size_t cachedSz = blob_fileio_get_file_size(get_hash_file_name(htype, hhex).c_str());
  if (cachedSz != invalid_blob_file_size)
    return cachedSz;
  const ClientConnection *cc = (const ClientConnection *)c;
  const int64_t sz = blob_size_on_server(cc->cs, htype, hhex);
  if (sz >= 0)
    return (size_t)sz;
  if (size_t(sz) == invalid_blob_file_size)
    return invalid_blob_file_size;
  if (sz < -1)
    cc->restart();
  return invalid_blob_file_size;
}

bool blob_does_hash_blob_exist(const void *c, const char* htype, const char* hhex) {
  const ClientConnection *cc = (const ClientConnection *)c;
  bool has = false;
  if (blob_check_on_server(cc->cs, htype, hhex, has) != KVRet::Fatal)
    return has;
  cc->restart();
  return false;
}

struct PushData
{
  const ClientConnection *cc;
  StreamToServerData *ss;
  bool ok;
};

uintptr_t blob_start_push_data(const void *c, const char* htype, const char* hhex, uint64_t size){
  const ClientConnection *cc = (const ClientConnection *)c;
  StreamToServerData *ss = start_blob_stream_to_server(cc->cs, htype, hhex);
  if (!ss || cc->cs < 0)
  {
    cc->restart();
    return 0;
  }
  return uintptr_t(new PushData{cc,ss, true});
}

bool blob_push_data(const void *data, size_t data_size, uintptr_t up)
{
  if (!up)
    return false;
  if (!data_size)
    return true;
  if (!data)
    return false;
  PushData *pd = (PushData*)up;
  if (!pd->ok)//why bother stream data after error
    return false;
  auto ret = blob_stream_to_server(*pd->ss, data, data_size);
  if (ret == KVRet::OK)
    return true;
  pd->ok = false;
  if (ret == KVRet::Fatal)
    pd->cc->restart();
  return false;
}

bool blob_end_push_data(uintptr_t up){
  if (up == uintptr_t(0))
    return false;
  std::unique_ptr<PushData> pd((PushData*)up);
  auto ret = finish_blob_stream_to_server(pd->cc->cs, pd->ss, pd->ok);
  if (ret == KVRet::Fatal)
    pd->cc->restart();
  return (ret == KVRet::OK);
}

void blob_destroy_push_data(uintptr_t up)
{
  if (up == uintptr_t(0))
    return;
  std::unique_ptr<PushData> pd((PushData*)up);
  auto ret = finish_blob_stream_to_server(pd->cc->cs, pd->ss, false);
  if (ret == KVRet::Fatal)
    pd->cc->restart();
}

static inline FILE* download_blob(intptr_t &sock, std::string &tmpfn, const char* htype, const char* hhex, int64_t &pulledSz)
{
  pulledSz = 0;
  FILE* tmpf = blob_fileio_get_temp_file(tmpfn, cache_folder.c_str());
  if (!tmpf)
  {
    fprintf(stderr, "Can't open temp file\n");
    return NULL;
  }
  bool ok = true;
  pulledSz = blob_pull_from_server(sock, htype, hhex,
    0, 0, [&](const char *data, uint64_t at, uint64_t size)
    {
      if (data)
        ok &= (fwrite(data, 1, size, tmpf) == size);
    });
  if (!ok || pulledSz <= 0)
  {
    fclose(tmpf);
    blob_fileio_unlink_file(tmpfn.c_str());
    tmpf = NULL;
  }
  return tmpf;
}

inline uintptr_t attempt_pull_cache(const char *fn, uint64_t &sz){
  size_t cachedSz;
  uintptr_t ret = (uintptr_t)blobe_fileio_start_pull(fn, cachedSz);
  sz = cachedSz;
  return ret;
}

uintptr_t blob_start_pull_data(const void *c, const char* htype, const char* hhex, uint64_t &sz)
{
  std::string fn = get_hash_file_name(htype, hhex);
  if (!fn.length())
  {
    fprintf(stderr, "invalid file name for %s %s\n", htype, hhex);
    return 0;
  }
  const ClientConnection *cc = (const ClientConnection *)c;
  std::string tmpfn;
  FILE *tmpf = nullptr;
  uintptr_t ret = 0;
  for (int i = 0; i < 100; ++i)//100 attempts to restart socket
  {
    if ((ret = attempt_pull_cache(fn.c_str(), sz)) != 0)//already in cache
      return ret;
    //we have to pull it from server to cache
    int64_t pulledSz;
    while ((tmpf = download_blob(cc->cs, tmpfn, htype, hhex, pulledSz)) == nullptr && pulledSz>0)
    {
      //not enough space!
      if (!perform_immediate_gc(pulledSz))
      {
        fprintf(stderr, "There is no enough space on proxy cache folder to even download one file of %lld size\n", (long long int)pulledSz);
        return 0;
      }
    }
    if (pulledSz <= 0 && tmpf)
    {
      fprintf(stderr, "ASSERT!");
      fclose(tmpf);
      return 0;
    }

    if (pulledSz == 0)
      return 0;
    if (pulledSz > 0)
      break;
    cc->restart();
  }

  blob_fileio_ensure_dir(get_hash_file_folder(htype, hhex).c_str());
  if (!blob_fileio_rename_file(tmpfn.c_str(), fn.c_str()))//can't rename
  {
    fclose(tmpf);
    blob_fileio_unlink_file(tmpfn.c_str());
    fprintf(stderr, "Can't rename file %s to %s\n", tmpfn.c_str(), fn.c_str());
    return 0;
  }
  fflush(tmpf);
  if ((ret = attempt_pull_cache(fn.c_str(), sz)) == 0)
    fprintf(stderr, "Can't start pull of just downloaded file %s, %d!\n", fn.c_str(), errno);
  fclose(tmpf);//we close file only after we have started pull. That way GC thread won't delete file
  lazy_report_to_gc(sz);
  return ret;
}

const char *blob_pull_data(uintptr_t up, uint64_t from, size_t &read){
  if (!up){read = 0;return nullptr;}
  return blobe_fileio_pull((BlobFileIOPullData*)up, from, read);
}

bool blob_end_pull_data(uintptr_t up){
  return blobe_fileio_destroy((BlobFileIOPullData*)up);
}

