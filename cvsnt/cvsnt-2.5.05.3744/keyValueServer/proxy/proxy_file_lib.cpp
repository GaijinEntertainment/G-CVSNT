#include <string>
#include <memory>
#include <condition_variable>
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
//--
void init_gc(const char *folder, uint64_t max_size);
void close_gc();
void lazy_report_to_gc(uint64_t sz);
bool perform_immediate_gc(int64_t needed_sz);
void gc_sleep_msec(int msec);

//
void close_proxy(){close_gc();}
void init_proxy(const char *url, int port, const char *cache, uint64_t sz, const char *secret, CafsClientAuthentication auth_on_master)
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
  ~ClientConnection(){kill();}
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
  } else
  {
    fflush(tmpf);
  }
  return tmpf;
}

inline uintptr_t attempt_pull_cache(const char *fn, uint64_t &sz){
  uint64_t cachedSz;
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
  for (int i = 0; i < 100; ++i)//100 attempts to restart socket/free space
  {
    if ((ret = attempt_pull_cache(fn.c_str(), sz)) != 0)//already in cache
      return ret;
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

    if (pulledSz == 0)
      return 0;
    if (pulledSz > 0)
      break;
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

