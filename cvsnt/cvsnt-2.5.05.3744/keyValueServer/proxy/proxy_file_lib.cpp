#include <string>
#include <memory>
#include <condition_variable>
#include <string.h>
#include "../blob_server_func_deps.h"
#include "../../ca_blobs_fs/content_addressed_fs.h"
#include "../../ca_blobs_fs/src/fileio.h"
#include "../../ca_blobs_fs/src/details.h"
#include "../include/blob_client_lib.h"
#include "../include/blob_sockets.h"
#include "../blob_push_log.h"

static std::string master_url;
static std::string cache_folder, blobs_folder, encryption_secret;
static CafsClientAuthentication auth_on_master_as_client = CafsClientAuthentication::AllowNoAuthPrivate;
static int master_port = 2403;
static bool should_update_mtimes_on_access = false;
//--
void init_gc(const char *folder, uint64_t max_size);
void close_gc();
void lazy_report_to_gc(uint64_t sz);
bool perform_immediate_gc(int64_t needed_sz);
void gc_sleep_msec(int msec);
void update_write_time_to_current(const char*name);

//
void close_proxy(){close_gc();}
void init_proxy(const char *url, int port, const char *cache, uint64_t sz, const char *secret, CafsClientAuthentication auth_on_master, bool update_mtimes)
{
  if (auth_on_master == CafsClientAuthentication::RequiresAuth && !secret)
  {
    blob_logmessage(LOG_ERROR, "You can't demand auth from server if you don't provide secret");
    return;
  }
  master_url = url;
  master_port = port;
  cache_folder = cache;
  encryption_secret = secret ? secret : "";
  //do not demand encryption on proxy from server, even if it is encrypting as server
  auth_on_master_as_client = auth_on_master;
  if (cache_folder.length()==0)
    cache_folder = ".";
  else
    blob_fileio_ensure_dir(cache_folder.c_str());
  if (cache_folder[cache_folder.length() - 1] != '/')
    cache_folder += "/";

  blobs_folder = cache_folder + "blobs/";
  blob_fileio_ensure_dir(blobs_folder.c_str());
  should_update_mtimes_on_access = update_mtimes;
  init_gc(cache_folder.c_str(), uint64_t(sz)<<uint64_t(20));
}
///

struct ClientConnection
{
  mutable BlobSocket cs;
  mutable uint32_t failedAttempts = 0;
  uint32_t maxFailedAttempts = 100;
  std::string root;
  ClientConnection(const char*root_):root(root_)
  {
    if (should_update_mtimes_on_access)
      accessed_files.reserve(65536);
  }
  bool start() const
  {
    uint64_t otpPage = 0; bool hasOTP = false;
    uint8_t otp[otp_page_size];
    CafsClientAuthentication clientAuth = auth_on_master_as_client;
    if (encryption_secret.length() != 0)
    {
      otpPage = blob_get_otp_page();
      hasOTP = blob_gen_totp_secret(otp, (const unsigned char*)encryption_secret.c_str(), (uint32_t)encryption_secret.length(), otpPage);
      if (!hasOTP)
      {
        if (clientAuth == CafsClientAuthentication::RequiresAuth)
        {
          blob_logmessage(LOG_ERROR, "failed attempt to generate otp, client requires auth from server");
          return false;
        } else
        {
          clientAuth = CafsClientAuthentication::AllowNoAuthPrivate;
          blob_logmessage(LOG_WARNING, "failed attempt to generate otp");
        }
      }
    }
    cs = start_blob_push_client(master_url.c_str(), master_port, root.c_str(), 0,
                                hasOTP ? otp : nullptr,//allow to connect to encrypted proxies
                                otpPage,//get current otp page
                                clientAuth
                                );
    return is_valid(cs);
  }//infinite timeout on proxy
  void kill() const{stop_blob_push_client(cs);}
  void restart() const
  {
    kill();
    if (!start())
      blob_logmessage(LOG_ERROR, "Can't reconnect to %s:%d", master_url.c_str(), master_port);
  }
  mutable std::vector<char> accessed_files;
  void process_atimes() const
  {
    for (const char* at = accessed_files.data(), *e = at + accessed_files.size(); at < e; at += strlen(at)+1)
      update_write_time_to_current(at);
    accessed_files.clear();
  }
  void add_accessed_file(const std::string &fn) const
  {
    if (should_update_mtimes_on_access)//we could just call update_write_time_to_current, but that can increase latency. Instead save till end of session
    {
      if (accessed_files.size() > (2<<20))//let's ensure we never consume too much memory. 2mb is fine limit
        process_atimes();
      accessed_files.insert(accessed_files.end(), fn.c_str(), fn.c_str() + fn.length()+1);
    }
  }
  ~ClientConnection()
  {
    kill();
    process_atimes();
  }
};

extern void blob_sleep_for_msec(unsigned int msec);
bool blob_is_under_attack(bool failed_attempt, void *c) {
  if (!c)
    return false;
  if (failed_attempt)
  {
    ClientConnection *cc = (ClientConnection *)c;
    cc->failedAttempts++;
    if (cc->failedAttempts >= cc->maxFailedAttempts)//attacker!
    {
      blob_logmessage(LOG_ERROR, "Server is probably under attack, %d/%d failed attempts", cc->failedAttempts, cc->maxFailedAttempts);
      return true;
    }
    const double ratio = cc->failedAttempts/cc->maxFailedAttempts;
    const double biggestDelaySeconds = 100;//100 seconds
    //quadratic growth of delay. if we allow 100 attempts prior to ban IP, than first failed attempt 10msec, 10th attempt one second delay, 100th attempt 1000second delay
    const double delaySeconds = ratio*ratio*biggestDelaySeconds;
    blob_sleep_for_msec(uint32_t(delaySeconds*1000));
    blob_logmessage(LOG_WARNING, "failed attempt to get something, delaying %gsec", delaySeconds);
    return false;
  } else
  {
    //we can't decrease number of failed attempts, that allows to write attack
  }
  return false;
}

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
  char sub[7] = {hhex[0], hhex[1], '/', hhex[2], hhex[3], '/', '\0'};
  return (blobs_folder + sub);
}

static void ensure_dir(const char* htype, const char* hhex)
{
  std::string dir = get_hash_file_folder(htype, hhex);
  dir[dir.length()-4] = 0;
  blob_fileio_ensure_dir(dir.c_str());
  dir[dir.length()-4] = '/';
  blob_fileio_ensure_dir(dir.c_str());
}

std::string get_hash_file_name(const char* htype, const char* hhex)
{
  if (!hhex || !hhex[0] || !hhex[1] || !hhex[2] || !hhex[3])
    return std::string();
  return get_hash_file_folder(htype, hhex) + hhex;
}

uint64_t blob_get_hash_blob_size(const void *c, const char* htype, const char* hhex) {
  //we return local file size if exist. If blob was removed from master that would cause ability to checkout non-existent blob (but the one that existed earlier)
  //if blob has changed on server (repacked) we will return cached local size anyway
  const uint64_t cachedSz = blob_fileio_get_file_size(get_hash_file_name(htype, hhex).c_str());
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
bool proxy_allow_push = true;

uintptr_t blob_start_push_data(const void *c, const char* htype, const char* hhex, uint64_t size){
  if (!proxy_allow_push)
    return 0;
  const ClientConnection *cc = (const ClientConnection *)c;
  StreamToServerData *ss = start_blob_stream_to_server(cc->cs, htype, hhex);
  if (!ss || !is_valid(cc->cs))
  {
    cc->restart();
    return 0;
  }
  return uintptr_t(new PushData{cc,ss, true});
}

bool blob_push_data(const void *data, uint64_t data_size, uintptr_t up)
{
  if (!proxy_allow_push)
    return false;
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

#define WRITE_THROUGH_PROXY 1

#if WRITE_THROUGH_PROXY

struct PullThroughTemp
{
  char buf[65536];
  std::string tmpfn;
  FILE *tmpf = nullptr;
  const ClientConnection *cc = nullptr;
  int64_t expectedSize = 0;
  int64_t pulledSz = 0;
  bool tempIsOk = true;
  bool start(const ClientConnection *c, const char* htype, const char* hhex, uint64_t &sz)
  {
    cc = c;
    if (!cc)
    {
      fprintf(stderr, "No client connection\n");
      return false;
    }

    pulledSz = 0;
    for (int i = 0; i < 100; ++i)
    {
      uint64_t from = 0;
      sz = 0;
      KVRet ret = blob_start_pull_from_server(cc->cs, htype, hhex, from, sz);
      if (ret == KVRet::OK)
      {
        expectedSize = sz;
        break;
      }
      if (ret == KVRet::Error)//not an error, blob is probably just missing on server
        return false;
      if (ret == KVRet::Fatal)//we have to restart working with server, network error
      {
        gc_sleep_msec(i*200);//sleep for longer and longer periods of time, up to 20sec
        cc->restart();
      }
    }
    //report_to_gc_needed_space(expectedSize);
    pulledSz = 0;
    tmpf = blob_fileio_get_temp_file(tmpfn, blobs_folder.c_str());
    if (!tmpf)
      fprintf(stderr, "Can't open temp file %s. Will perform net proxying.\n", tmpfn.c_str());
    else
      ensure_dir(htype, hhex);
    tempIsOk = tmpf != nullptr;
    return true;
  }

  template <class Cb>
  bool readChunk(Cb cb)
  {
    const int64_t sizeLeft = expectedSize - pulledSz;
    int l = recv(cc->cs, buf, (int)std::min(sizeLeft, (int64_t)sizeof(buf)));
    if (l == 0)/*gracefully closed*/ {
        blob_logmessage(LOG_NOTIFY, "socket returned 0, err=%d", blob_get_last_sock_error());
        return false;
    }
    if (l < 0)
    {
        blob_logmessage(LOG_ERROR, "can't read socket, err=%d", blob_get_last_sock_error());
        return false;
    }
    cb(buf, l);
    return true;
  }
  const char *pull(uint64_t from, uint64_t &read)
  {
    if (from != pulledSz)
    {
      //we have to finish all downloading to temp, and then provide access to temp file. we can't move cursor in downloading file
      //can be done, but is not used now!
      //todo: fixme
      return nullptr;
    }
    read = 0;
    if (readChunk([&](const char *data, int len)
    {
      if (tempIsOk)
      {
        tempIsOk = (blob_fwrite64(buf, 1, len, tmpf) == len);
        if (!tempIsOk)
        {
          fprintf(stderr, "Couldn't download to temp <%s>, err=%d.\n", tmpfn.c_str(), blob_fileio_get_last_error());
          perform_immediate_gc(expectedSize*2);//there was not enough space?
          closeAndUnlink();//no point in keeping sem-written temp file opened
        }
      }
      read = len;
      pulledSz += len;
    }))
      return buf;

    read = 0;
    return nullptr;
  }
  void closeAndUnlink()
  {
    if (tmpf)
      fclose(tmpf);
    blob_fileio_unlink_file(tmpfn.c_str());
    tmpf = NULL;
  }
  bool finish(const std::string &fn)
  {
    if (!tmpf)
      return false;
    if (!tempIsOk || expectedSize != pulledSz)
    {
      fprintf(stderr, "Couldn't download <%s> from master. pulled = %lld out of %lld\n", fn.c_str(),
        (long long int)pulledSz, (long long int)expectedSize);
      closeAndUnlink();
      return false;
    } else if (fflush(tmpf) != 0)
    {
      fprintf(stderr, "Couldn't flush download <%s> from master, pulled = %lld \n", tmpfn.c_str(), (long long int)pulledSz);
      closeAndUnlink();
      return false;
    }
    const int64_t fileSz = blob_fileio_get_file_size(tmpfn.c_str());
    if (fileSz != pulledSz)
    {
      fprintf(stderr, "Downloaded file %s for %s is of %lld size, while should be %lld\n", tmpfn.c_str(), fn.c_str(),
        (long long int)fileSz, (long long int)pulledSz);
      return false;
    }
    #if _MSC_VER
    fclose(tmpf);
    //we should close file only after we have started pull. That way GC thread won't delete file
    //however, windows isn't capable of that
    tmpf = NULL;
    #endif

    if (!blob_fileio_rename_file_if_nexist(tmpfn.c_str(), fn.c_str()))//can't rename
    {
      const int err = blob_fileio_get_last_error();
      closeAndUnlink();
      fprintf(stderr, "Can't rename file %s to %s, because of %d\n", tmpfn.c_str(), fn.c_str(), err);
      return false;
    }
    if (tmpf)
      fclose(tmpf);//we close file only after we have started pull. That way GC thread won't delete file
    lazy_report_to_gc(fileSz);
    return true;
  }
};

struct BlobProxyPull
{
public:
  const char *pull(uint64_t from, uint64_t &read)
  {
    return isCached() ? pullCached(from, read) : pullWriteThrough(from, read);
  }

  bool start(const ClientConnection *cc, const char* htype, const char* hhex, uint64_t &sz)
  {
    cacheFileName = get_hash_file_name(htype, hhex);
    if (!cacheFileName.length())
    {
      fprintf(stderr, "invalid file name for %s %s\n", htype, hhex);
    } else
    {
      cached = blobe_fileio_start_pull(cacheFileName.c_str(), sz);
      if (cached)
      {
        if (cc)
          cc->add_accessed_file(cacheFileName.c_str());
        return true;
      }
    }
    return writeThrough.start(cc, htype, hhex, sz);
  }

  ~BlobProxyPull()
  {
    const bool wasWriting = !isCached();
    if (cached)
    {
      blobe_fileio_destroy(cached);
      cached = nullptr;
    }
    writeThrough.finish(cacheFileName);
  }

private:
  const char *pullWriteThrough(uint64_t from, uint64_t &read)
  {
    return writeThrough.pull(from, read);
  }

  const char *pullCached(uint64_t from, uint64_t &read)
  {
    if (!cached){read = 0;return nullptr;}
    return blobe_fileio_pull(cached, from, read);
  }
  bool isCached() const {return cached != nullptr;};
  std::string cacheFileName;
  BlobFileIOPullData *cached = nullptr;
  PullThroughTemp writeThrough;
};

uintptr_t blob_start_pull_data(const void *c, const char* htype, const char* hhex, uint64_t &sz)
{
  BlobProxyPull *pullData = new BlobProxyPull;
  if (pullData->start((const ClientConnection *)c, htype, hhex, sz))
    return uintptr_t(pullData);
  delete pullData;
  return 0;
}

const char *blob_pull_data(uintptr_t up, uint64_t from, uint64_t &read)
{
  if (!up){read = 0;return nullptr;}
  return ((BlobProxyPull*)up)->pull(from, read);
}

bool blob_end_pull_data(uintptr_t up)
{
  if (!up)
    return false;
  delete (BlobProxyPull*)up;
  return true;
}

#else

static inline FILE* download_blob(BlobSocket &sock, std::string &tmpfn, const char* htype, const char* hhex, int64_t &pulledSz)
{
  pulledSz = 0;
  FILE* tmpf = blob_fileio_get_temp_file(tmpfn, blobs_folder.c_str());
  if (!tmpf)
  {
    fprintf(stderr, "Can't open temp file\n");
    return NULL;
  }
  bool ok = true;
  int64_t expectedSize = -1;
  pulledSz = blob_pull_from_server(sock, htype, hhex,
    0, 0, [&](const char *data, uint64_t at, uint64_t size)
    {
      if (data)
        ok &= (blob_fwrite64(data, 1, size, tmpf) == size);
      else
        expectedSize = size;
    });
  if (!ok || pulledSz <= 0 || (expectedSize > 0 && expectedSize != pulledSz) )
  {
    fprintf(stderr, "Couldn't download <%.64s> from master. pulled = %lld out of %lld\n", hhex, (long long int)pulledSz, (long long int)expectedSize);
    fclose(tmpf);
    blob_fileio_unlink_file(tmpfn.c_str());
    tmpf = NULL;
  } else if (fflush(tmpf) != 0)
  {
    fprintf(stderr, "Couldn't flush download <%s> from master, pulled = %lld \n", tmpfn.c_str(), (long long int)pulledSz);
    fclose(tmpf);
    blob_fileio_unlink_file(tmpfn.c_str());
    tmpf = NULL;
  }
  return tmpf;
}
inline uintptr_t attempt_pull_cache(const char *fn, uint64_t &sz){
  return (uintptr_t)blobe_fileio_start_pull(fn, sz);
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
  for (int i = 0; i < 100; ++i)//100 attempts to restart socket/free space
  {
    if ((ret = attempt_pull_cache(fn.c_str(), sz)) != 0)//already in cache
    {
      if (cc)
        cc->add_accessed_file(fn.c_str());
      return ret;
    }
    //we have to pull it from server to cache
    int64_t pulledSz;
    while ((tmpf = download_blob(cc->cs, tmpfn, htype, hhex, pulledSz)) == nullptr && pulledSz>0)
    {
      //not enough space?!
      if (!perform_immediate_gc(pulledSz*(i/2+1)))
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

    if (pulledSz > 0)
    {
      const int64_t fileSz = blob_fileio_get_file_size(tmpfn.c_str());
      if (fileSz != pulledSz)
        fprintf(stderr, "Downloaded file %s for %s is of %lld size, while should be %lld\n", tmpfn.c_str(), fn.c_str(),
          (long long int)fileSz, (long long int)pulledSz);
      else
        break;
    }
    gc_sleep_msec(i*200);//sleep for longer and longer periods of time, up to 20sec
    cc->restart();
  }
  ensure_dir(htype, hhex);
  #if _MSC_VER
  fclose(tmpf);
  //we should close file only after we have started pull. That way GC thread won't delete file
  //however, windows isn't capable of that
  tmpf = NULL;
  #endif

  if (!blob_fileio_rename_file_if_nexist(tmpfn.c_str(), fn.c_str()))//can't rename
  {
    const int err = blob_fileio_get_last_error();
    if (tmpf)
      fclose(tmpf);
    blob_fileio_unlink_file(tmpfn.c_str());
    fprintf(stderr, "Can't rename file %s to %s, because of %d\n", tmpfn.c_str(), fn.c_str(), err);
    return 0;
  }
  if ((ret = attempt_pull_cache(fn.c_str(), sz)) == 0)
    fprintf(stderr, "Can't start pull of just downloaded file %s, %d!\n", fn.c_str(), errno);
  if (tmpf)
    fclose(tmpf);//we close file only after we have started pull. That way GC thread won't delete file
  lazy_report_to_gc(sz);
  return ret;
}

const char *blob_pull_data(uintptr_t up, uint64_t from, uint64_t &read){
  if (!up){read = 0;return nullptr;}
  return blobe_fileio_pull((BlobFileIOPullData*)up, from, read);
}

bool blob_end_pull_data(uintptr_t up){
  return blobe_fileio_destroy((BlobFileIOPullData*)up);
}

#endif
