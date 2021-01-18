#include "sha_blob_reference.h"
#include "../ca_blobs_fs/push_whole_blob.h"
#include "../ca_blobs_fs/streaming_blobs.h"

void* blob_alloc(size_t sz);
static void RCS_write_binary_rev_data_blob(const char *fn_context, char *&data, size_t &len, bool store_packed, bool write_it)
{
  //todo: skip actual write, if !write_it. we then just need hash
  //however, new client should never send data
  char hash[64];
  bool res = caddressed_fs::push_whole_blob_from_raw_data(caddressed_fs::get_default_ctx(), data, len, hash, store_packed);
  if (!res)
    error(1,errno,"Couldn't write blob of %s", fn_context);
  data = (char*)xrealloc (data, blob_reference_size+1);
  memcpy(data, HASH_TYPE_REV_STRING, hash_type_magic_len);
  memcpy(data+hash_type_magic_len, hash, sizeof(hash));
  data[len = blob_reference_size] = 0;
}

static bool RCS_read_binary_rev_data_direct(const char *fn, char **out_data, size_t *out_len, int *inout_data_allocated, bool packed);


inline bool pull_at_once(const char* hash_hex_string, size_t &sz, char **decoded)
{
  //this is for compatibility with old clients
  using namespace caddressed_fs;
  using namespace streaming_compression;
  size_t blob_sz;
  PullData *pd = start_pull(get_default_ctx(), hash_hex_string, blob_sz);
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
  sz = info.hdr.uncompressedLen;
  return destroy(pd);
}

static bool RCS_read_binary_rev_data(char **out_data, size_t *out_len, int *inout_data_allocated, bool supposed_packed, bool *is_ref)
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
    if (is_ref)
      *is_ref = blobRef;
      //*is_ref = is_blob_reference(current_parsed_root->directory, fn, sha_file_name, sizeof(sha_file_name));
    return true;
    //read like it used to be
  }

}

//we need reading, untill all converts
void RCS_write_binary_rev_data(const char *fn_context, char * &data, size_t &len, bool guessed_compression, bool write_it)
{
  if (is_session_blob_reference_data(data, len))
  {
    // that's the only correct for new clients
    len = blob_reference_size; //shrink data;
    data[len] = 0;
    if (!caddressed_fs::exists(caddressed_fs::get_default_ctx(), data + hash_type_magic_len))
    {
      char hashZ[65];memcpy(hashZ, data + hash_type_magic_len, 64); hashZ[64]=0;
      error(1,0,"hash %s referenced by client doesn't yet exist!", hashZ);
    }
    //write_direct_blob_reference(fn, data, blob_reference_size);
  } else
    RCS_write_binary_rev_data_blob(fn_context, data, len, guessed_compression, write_it);//we should rely on packed sent by client. it know not only is src_packed but also if it was reasonable to pack
}
