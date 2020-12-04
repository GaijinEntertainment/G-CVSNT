#define _CRT_SECURE_NO_DEPRECATE
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "../include/blob_hash_util.h"
#include "../include/blob_client_lib.h"
#include "../sampleImplementation/def_log_printf.cpp"
#include "../include/blob_sockets.h"//move init to out of line
#include "../../ca_blobs_fs/ca_blob_format.h"//move init to out of line
#include "../../blake3/blake3.h"//move init to out of line

bool calc_file_hash(const char *f, char *hash);

int main(int argc, const char **argv)
{
  if (argc < 6)
  {
    printf("Usage is blob_file_client [url] [port] [root] command (file|arg). Command list: has hash; size hash; pushblob file; streamfile file; pushfile file; pull hash\n");
    return 1;
  }
  if (!blob_init_sockets())
  {
    blob_logmessage(LOG_ERROR, "Can't init sockets, %d", blob_get_last_sock_error());
    return 1;
  }
  const char *cmdname = argv[4];
  const char *argname = argv[5];
  enum {HAS, SIZE, PULL, STREAMFILE, PUSHFILE, PUSHBLOB, UNKNOWN} cmd = UNKNOWN;
  if (strcmp(cmdname, "has") == 0)
    cmd = HAS;
  else if (strcmp(cmdname, "size") == 0)
    cmd = SIZE;
  else if (strcmp(cmdname, "pull") == 0)
    cmd = PULL;
  else if (strcmp(cmdname, "pushfile") == 0)
    cmd = PUSHFILE;
  else if (strcmp(cmdname, "streamfile") == 0)
    cmd = STREAMFILE;
  else if (strcmp(cmdname, "pushblob") == 0)
    cmd = PUSHBLOB;
  if (cmd == UNKNOWN)
  {
    printf("unknown command %s\n", cmdname);
    return 1;
  }

  intptr_t client = start_blob_push_client(argv[1], atoi(argv[2]), argv[3]);
  if (client == -1)
  {
    blob_logmessage(LOG_ERROR, "Can't connect client %d", blob_get_last_sock_error());
    return 1;
  }
  const char* ht = "blake3";
  if (cmd == HAS)
  {
    bool has = false;
    if (blob_check_on_server(client, ht, argname, has) != KVRet::OK)
      printf("error in check %s\n", argname);
    else
      printf("%s is %son server\n", argname, has ? "" : "not");
  }
  if (cmd == SIZE)
    printf("%s size on server is %lld\n", argname, blob_size_on_server(client, ht, argname));
  if (cmd == PULL)
  {
    FILE *f=fopen(argname, "wb");
    int64_t pulled = blob_pull_from_server(client, ht, argname, 0, 0, [&](const char *data, uint64_t at, uint64_t size){
    //store
      return data ? fwrite(data, 1, size, f) == size : true;//at is ignored!
    });
    fclose(f);
    printf("pulled %lld of %s\n", pulled, argname);
  }
  if (cmd == PUSHBLOB || cmd == PUSHFILE)
  {
    char hash[65];hash[0] = hash[64] = 0;
    if (cmd == PUSHFILE && !calc_file_hash(argname, hash))
      printf("Cant open calc hash for file %s\n", argname);
    else if (cmd == PUSHBLOB)
    {
      if (strlen(argname) != 64)
        printf("arg <%s> is not a hash string\n", argname);
      else
        strncpy(hash, argname, sizeof(hash));
    }
    FILE *f = fopen(argname, "rb");
    if (!f)
      printf("Cant open file %s for reading\n", argname);
    else
    {
      fseek(f, 0, SEEK_END);
      const size_t fsz = ftell(f);
      fseek(f, 0, SEEK_SET);

      char buf[65536];
      const size_t hdrSize = cmd == PUSHBLOB ? 0 : sizeof(caddressed_fs::BlobHeader);
      const size_t blob_sz = fsz + hdrSize;
      caddressed_fs::BlobHeader hdr = caddressed_fs::get_header(caddressed_fs::noarc_magic, fsz, 0);
      int64_t pushed = blob_push_to_server(client, blob_sz, ht, hash, [&](uint64_t at, size_t &data_pulled) {
        if (at < hdrSize)
        {
          data_pulled = hdrSize-at;
          return ((const char*)&hdr) + at;
        }
        size_t was = ftell(f)+hdrSize;
        if (at != was)
          fseek(f, int(int64_t(at)-hdrSize-int64_t(was)), SEEK_CUR);
        data_pulled = fread(buf, 1, sizeof(buf), f);
        printf("pushing %lld at %lld\n", data_pulled, at);
        return (const char*)(ferror(f) ? nullptr : buf);
      //store
      });
      printf("pushed %lld of %s hash = %s\n", pushed, argname, hash);
      fclose(f);
    }
  } else if (cmd == STREAMFILE)
  {
    char hash[65];hash[0] = hash[64] = 0;
    if (!calc_file_hash(argname, hash))
      printf("Cant open calc hash for file %s\n", argname);
    FILE *f = fopen(argname, "rb");
    if (!f)
      printf("Cant open file %s for reading\n", argname);
    else
    {
      fseek(f, 0, SEEK_END);
      const size_t fsz = ftell(f);
      fseek(f, 0, SEEK_SET);
      char buf[65536];
      const size_t hdrSize = sizeof(caddressed_fs::BlobHeader);
      caddressed_fs::BlobHeader hdr = caddressed_fs::get_header(caddressed_fs::noarc_magic, fsz, 0);
      bool headerSend = false;
      int64_t sent = 0;
      KVRet ret = blob_stream_to_server(client, ht, hash, [&](size_t &data_pulled) {
        if (!headerSend)
        {
          data_pulled = sizeof(hdr);
          headerSend= true;
          return ((const char*)&hdr);
        }
        data_pulled = fread(buf, 1, sizeof(buf), f);
        printf("stream pushing %lld\n", data_pulled);
        sent += data_pulled;
        return (const char*)(ferror(f) ? nullptr : buf);
      //store
      });
      printf("pushed %lld of %s hash = %s, %s\n", sent, argname, hash, ret == KVRet::OK ? "ok" : "error");
      fclose(f);
    }
  }
  stop_blob_push_client(client);
  blob_close_sockets();
}

bool calc_file_hash(const char *fn, char *hash)
{
  FILE *f = fopen(fn, "rb");
  if (!f)
    return false;

  blake3_hasher hasher;
  blake3_hasher_init(&hasher);
  char buf[8192];
  for(;;){
    size_t sz = fread(buf, 1, sizeof(buf), f);
    blake3_hasher_update(&hasher, buf, sz);
    if (sz != sizeof(buf))
      break;
  }
  unsigned char digest[32];
  blake3_hasher_finalize(&hasher, digest, 32);
  bin_hash_to_hex_string(digest, hash);
  fclose(f);
  return true;
}
