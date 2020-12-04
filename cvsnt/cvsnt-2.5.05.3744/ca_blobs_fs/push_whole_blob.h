#pragma once

#include <algorithm>
#include "streaming_compressors.h"
#include "content_addressed_fs.h"
#include "ca_blob_format.h"

namespace caddressed_fs
{

template<typename GetUnpackedData>//GetUnpackedData StreamType(const char *&src, size_t &pos, size_t &sz)
inline PushResult push_whole_blob_lambda(context *ctx, size_t unpacked_sz, char *resulted_hash, bool pack, GetUnpackedData cb)
{
  using namespace caddressed_fs;
  using namespace streaming_compression;
  PushData* pd = start_push(ctx);//pre-packed blobs
  if (!pd)
    return PushResult::IO_ERROR;
  BlobHeader hdr = get_header(pack ? zstd_magic : noarc_magic, unpacked_sz, 0);
  if (!stream_push(pd, &hdr, sizeof(hdr)))
  {
    destroy(pd);
    return PushResult::IO_ERROR;
  }
  char bufOut[32768];
  StreamStatus st = compress_lambda(
    [&](const char *&src, size_t &src_pos, size_t &src_size)
    {
      if (src_pos < src_size)
        return StreamStatus::Continue;
      return cb(src, src_pos, src_size);
    },
    [&](char *&dst, size_t &dst_pos, size_t &dst_capacity)
      {
        if (dst_pos && !stream_push(pd, dst, dst_pos))
          return StreamStatus::Error;
        dst = bufOut; dst_pos = 0; dst_capacity = sizeof(bufOut);
        return StreamStatus::Continue;
      }
   , 6, pack ? StreamType::ZSTD : StreamType::Unpacked);

  if (st != StreamStatus::Finished)
  {
    destroy(pd);
    return PushResult::IO_ERROR;
  }

  return finish(pd, resulted_hash);
}

inline bool push_whole_blob_from_raw_data(context* ctx, const void *data, size_t sz, char *resulted_hash, bool pack)
{
  using namespace streaming_compression;
  return is_ok(push_whole_blob_lambda(ctx, sz, resulted_hash, pack,
      [&](const char *&src, size_t &src_pos, size_t &src_size){
        src = (const char*)data;src_size = sz;
        return src_pos < src_size ? StreamStatus::Continue : StreamStatus::Finished;
    }));
}


}