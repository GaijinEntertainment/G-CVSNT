#pragma once

#include <algorithm>
#include "streaming_compressors.h"
#include "content_addressed_fs.h"
#include "ca_blob_format.h"

inline bool push_whole_blob_from_raw_data(const void *data, size_t sz, char *resulted_hash, bool pack)
{
  using namespace caddressed_fs;
  using namespace streaming_compression;
  PushData* pd = start_push();//pre-packed blobs
  if (!pd)
    return false;
  BlobHeader hdr = get_header(pack ? zstd_magic : noarc_magic, sz, 0);
  if (!stream_push(pd, &hdr, sizeof(hdr)))
  {
    destroy(pd);
    return false;
  }
  char bufOut[32768];
  StreamStatus st = compress_lambda(
    [&](const char *&src, size_t &src_pos, size_t &src_size)
      {src = (const char*)data; src_size = sz;return src_pos < src_size ? StreamStatus::Continue : StreamStatus::Finished;},
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
    return false;
  }

  return finish(pd, resulted_hash) == PushResult::OK;
}
