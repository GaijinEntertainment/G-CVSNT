#pragma once
#include <algorithm>
#include "streaming_compressors.h"
#include "ca_blob_format.h"

namespace caddressed_fs
{

struct DownloadBlobInfo;

bool init_decompress_blob_stream(char *ctx, size_t ctx_size, const BlobHeader &hdr);

template<typename ProcessUnpackedCB>
bool decode_stream_blob_data(DownloadBlobInfo &info, const char *data, size_t data_length, ProcessUnpackedCB cb);


//==========

struct DownloadBlobInfo
{
  size_t dataRead = 0, realUncompressedSize = 0;
  BlobHeader hdr = {0};
  char cctx[streaming_compression::CTX_SIZE];
  ~DownloadBlobInfo()
  {
    if (dataRead >= sizeof(BlobHeader))
      streaming_compression::finish_decompress_stream(cctx);
  }
};

inline bool init_decompress_blob_stream(char *ctx, size_t ctx_size, const BlobHeader &hdr)
{
  using namespace streaming_compression;
  if (!is_accepted_magic(hdr.magic))
    return false;
  StreamType type = StreamType::Undefined;
  if (is_zlib_blob(hdr))
    type = StreamType::ZLIB;
  else if (is_zstd_blob(hdr))
    type = StreamType::ZSTD;
  else if (is_noarc_blob(hdr))
    type = StreamType::Unpacked;
  else
    type = StreamType::Undefined;
  if (type == StreamType::Undefined)
    return false;
  return init_decompress_stream(ctx, ctx_size, type);
}

//ProcessUnpackedCB bool (void *unpacked_data, size_t sz)
template<typename ProcessUnpackedCB>
inline bool decode_stream_blob_data(DownloadBlobInfo &info, const char *data, size_t data_length, ProcessUnpackedCB cb)
{
  using namespace streaming_compression;
  if (info.dataRead < sizeof(BlobHeader))
  {
    size_t hdrPart = (data_length - info.dataRead);
    hdrPart = hdrPart < sizeof(BlobHeader) ? hdrPart : sizeof(BlobHeader);
    memcpy((char*)&info.hdr + info.dataRead, data, hdrPart);
    info.dataRead += hdrPart;

    if (info.dataRead >= sizeof(BlobHeader))
    {
      if (!init_decompress_blob_stream(info.cctx, sizeof(info.cctx), info.hdr))
        return false;
    }
    data += hdrPart;
    data_length -= hdrPart;
    if (!data_length)
      return true;
  }

  info.dataRead += data_length;

  if (is_noarc_blob(info.hdr))
  {
    bool ret = cb(data, data_length);
    info.realUncompressedSize += data_length;
    return ret;
  }

  size_t src_pos = 0;
  char buf[65536];
  while (src_pos < data_length)
  {
    size_t dest_pos = 0;
    auto status = decompress_stream(info.cctx, data, src_pos, data_length, buf, dest_pos, sizeof(buf));
    if (status == StreamStatus::Error)
      return false;
    if (!cb(buf, dest_pos))
      return false;
    info.realUncompressedSize += dest_pos;
    if (status == StreamStatus::Finished)
      return true;
  }
  return true;
}

};