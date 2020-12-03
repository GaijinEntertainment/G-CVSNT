#include "sha_blob_reference.h"
#include "../ca_blobs_fs/push_whole_blob.h"
#include "../ca_blobs_fs/streaming_blobs.h"

void* blob_alloc(size_t sz);
static void RCS_write_binary_rev_data_blob(const char *fn, char *&data, size_t &len, bool store_packed, bool write_it)
{
  //todo: skip actual write, if !write_it. we then just need hash
  //however, new client should never send data
  char hash[64];
  bool res = caddressed_fs::push_whole_blob_from_raw_data(data, len, hash, store_packed);
  if (!res)
    error(1,errno,"Couldn't write blob of %s", fn);
  data = (char*)xrealloc (data, blob_reference_size+1);
  memcpy(data, HASH_TYPE_REV_STRING, hash_type_magic_len);
  memcpy(data+hash_type_magic_len, hash, sizeof(hash));
  data[len = blob_reference_size] = 0;
}

static bool RCS_read_binary_rev_data_direct(const char *fn, char **out_data, size_t *out_len, int *inout_data_allocated, bool packed);


inline bool pull_at_once(const char* hash_hex_string, size_t &blob_sz, char **decoded)
{
  //this is for compatibility with old clients
  using namespace caddressed_fs;
  using namespace streaming_compression;
  PullData *pd = start_pull(hash_hex_string, blob_sz);
  if (!pd)
    return false;
  size_t at = 0;
  DownloadBlobInfo info;
  char *dest = nullptr;
  while (at < blob_sz)
  {
    size_t data_pulled = 0;
    const char *some = pull(pd, at, data_pulled);//returned data_pulled != 0, unless error
    if (!data_pulled)
      break;
    at += data_pulled;
    if (!decode_stream_blob_data(info, some, data_pulled, [&](const void *unpacked, size_t sz)
      {
        if (!dest)
          dest = (char*)blob_alloc(info.hdr.uncompressedLen);
        if (info.realUncompressedSize + sz > info.hdr.uncompressedLen)
          return false;
        memcpy(dest+info.realUncompressedSize, unpacked, sz);
        return true;
      }
    ))
      break;
  }
  *decoded = dest;
  return destroy(pd);
}

static bool RCS_read_binary_rev_data(const char *fn, char **out_data, size_t *out_len, int *inout_data_allocated, bool supposed_packed, bool *is_ref)
{
  const bool blobRef = is_blob_reference_data(*out_data, *out_len);

  if (!is_ref && blobRef)// is_blob_reference(current_parsed_root->directory, fn, sha_file_name, sizeof(sha_file_name)))
  {
    //as soon as we convert all binary repo, we should only keep that branch
    char hash[64]; memcpy(hash, *out_data + hash_type_magic_len, sizeof(hash));

    if (*inout_data_allocated && *out_data)
      xfree (*out_data);
    *out_data = NULL;
    *out_len = 0;
    *inout_data_allocated = 0;
    pull_at_once(hash, *out_len, out_data);
    if (*out_data)
      *inout_data_allocated = 1;
    return true;
  }
  else
  {
    //as soon as we convert all binary repo, it can't be happening

    bool ret = blobRef ? true : RCS_read_binary_rev_data_direct(fn, out_data, out_len, inout_data_allocated, supposed_packed);
    if (is_ref)
      *is_ref = blobRef;
      //*is_ref = is_blob_reference(current_parsed_root->directory, fn, sha_file_name, sizeof(sha_file_name));
    return ret;
    //read like it used to be
  }

}

//we need reading, untill all converts
static bool RCS_read_binary_rev_data_direct(const char *fn, char **out_data, size_t *out_len, int *inout_data_allocated, bool packed)
{
  char *fnpk = NULL;
  struct stat s;
  int data_sz = 0;
  FILE *fp = NULL;

  if (!packed)
  {
    if (CVS_STAT (fn, &s) >= 0)
      data_sz = s.st_size;
    else
    {
      fnpk = (char*)xmalloc(strlen(fn)+4); strcpy(fnpk, fn); strcat(fnpk, "#z");
      if (CVS_STAT (fnpk, &s) >= 0 && s.st_size >= 4)
        fn = fnpk, packed = true;
      else
      {
        xfree(fnpk); fnpk = NULL;
        s.st_size = 0;
        cvs_output ("warning: missing ", 0);
        cvs_output (fn, 0);
        cvs_output ("\n", 1);
      }
    }
  }
  else
  {
    fnpk = (char*)xmalloc(strlen(fn)+4); strcpy(fnpk, fn); strcat(fnpk, "#z");
    if (CVS_STAT (fnpk, &s) >= 0)
      fn = fnpk;
    else
    {
      xfree(fnpk); fnpk = NULL;
      if (CVS_STAT (fn, &s) >= 0)
        data_sz = s.st_size, packed = false;
      else
      {
        s.st_size = 0;
        cvs_output ("warning: missing ", 0);
        cvs_output (fn, 0);
        cvs_output ("\n", 1);
      }
    }
  }

  if (packed && s.st_size)
  {
    fp = CVS_FOPEN(fn,"rb");
    data_sz = (fread(&data_sz,1,4,fp) == 4) ? ntohl(data_sz) : -1;
  }

  if (*inout_data_allocated && *out_data)
    xfree (*out_data);
  *out_data = NULL;
  *out_len = 0;
  *inout_data_allocated = 0;

  if (!fp)
    fp = CVS_FOPEN(fn,"rb");
  if (fnpk)
    xfree(fnpk);

  if (data_sz)
  {
    *out_data = (char*)xmalloc(data_sz+1);
    *out_len = data_sz;
    *inout_data_allocated = 1;
    out_data[0][data_sz] = 0;

    if (!packed)
    {
      if (fread(*out_data, 1, data_sz, fp) != data_sz)
        error(1,errno,"Couldn't read from to %s", fn);
    }
    else
    {
      int zlen = s.st_size-4;
      void *tmpbuf = xmalloc(zlen);

      if (fread(tmpbuf, 1, zlen, fp) != zlen)
        error(1,errno,"Couldn't read from %s#z", fn);

      z_stream stream = {0};
      inflateInit(&stream);
      stream.avail_in = zlen;
      stream.next_in = (Bytef*)tmpbuf;
      stream.avail_out = data_sz;
      stream.next_out = (Bytef*)*out_data;
      if(inflate(&stream, Z_FINISH)!=Z_STREAM_END)
          error(1,0,"internal error: inflate failed");
      xfree(tmpbuf);
    }
  }
  if (fp)
    fclose(fp);
  return true;
}

void RCS_write_binary_rev_data(const char *rcs_version, const char *rcs_path, const char *workfile, char * &data, size_t &len, bool guessed_compression, bool write_it)
{
  if (is_session_blob_reference_data(data, len))
  {
    // that's the only correct for new clients
    len = blob_reference_size; //shrink data;
    data[len] = 0;
    //write_direct_blob_reference(fn, data, blob_reference_size);
  } else
    RCS_write_binary_rev_data_blob(rcs_version, data, len, guessed_compression, write_it);//we should rely on packed sent by client. it know not only is src_packed but also if it was reasonable to pack
}
