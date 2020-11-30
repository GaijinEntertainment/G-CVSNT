#include <stddef.h>
enum class BlobStreamType {Unpacked, ZLIB, ZSTD, Undefined};
enum class BlobStreamStatus {Continue, Finished, Error};
enum {BLOB_STREAM_CTX_SIZE = 256};
//this is just generalized zstd/zlib code
BlobStreamStatus decompress_blob(const char *src, size_t src_size, char *dest, size_t dest_capacity, BlobStreamType type);
size_t compress_blob_bound(size_t src_size, BlobStreamType type);

bool init_decompress_blob_stream(char *ctx_, size_t ctx_size, BlobStreamType type);
BlobStreamStatus decompress_blob_stream(char *ctx, const char *src, size_t &src_pos, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity);
void finish_decompress_blob_stream(char *ctx);

bool init_compress_blob_stream(char *ctx_, size_t ctx_size, int compression_level, BlobStreamType type);
size_t compress_blob_bound(char *ctx, size_t src_size);
BlobStreamStatus compress_blob_stream(char *ctx, const char *src, size_t &src_pos, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity);//will free context, no need to call finish_compress_blob_stream
BlobStreamStatus compress_blob_stream_and_finish(char *ctx, const char *src, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity);

//should be called in loop until Finished or Error, while freeing some space in destination
BlobStreamStatus finalize_compress_blob_stream(char *ctx, char *dest, size_t &dest_pos, size_t dest_capacity);

//BlobStreamStatus (const char *&src, size_t &src_pos, size_t &src_size)
template <typename Produce, typename Consume>
inline BlobStreamStatus compress_lambda(Produce rcb, Consume wcb, int compression_level, BlobStreamType type = BlobStreamType::ZSTD)
{
  char cctx[BLOB_STREAM_CTX_SIZE];
  if (!init_compress_blob_stream(cctx, sizeof(cctx), compression_level, type))
    return BlobStreamStatus::Error;
  size_t src_pos = 0, src_size = 0; const char *src = nullptr;
  size_t dst_pos = 0, dst_size = 0; char *dst = nullptr;
  BlobStreamStatus readStatus, writeStatus, compressStatus = BlobStreamStatus::Continue;
  while ((writeStatus = wcb(dst, dst_pos, dst_size)) == BlobStreamStatus::Continue && (readStatus = rcb(src, src_pos, src_size)) == BlobStreamStatus::Continue)
  {
    compressStatus = compress_blob_stream(cctx, src, src_pos, src_size, dst, dst_pos, dst_size);//will free context, no need to call finish_compress_blob_stream
    if (compressStatus == BlobStreamStatus::Error)
      return BlobStreamStatus::Error;
  }
  if (readStatus == BlobStreamStatus::Error)
    return BlobStreamStatus::Error;

  while ((writeStatus = wcb(dst, dst_pos, dst_size)) == BlobStreamStatus::Continue && compressStatus == BlobStreamStatus::Continue)
  {
    compressStatus = finalize_compress_blob_stream(cctx, dst, dst_pos, dst_size);//will free context, no need to call finish_compress_blob_stream
    if (compressStatus == BlobStreamStatus::Error)
      return BlobStreamStatus::Error;
  }
  return compressStatus == BlobStreamStatus::Finished && writeStatus == BlobStreamStatus::Finished ? BlobStreamStatus::Finished : BlobStreamStatus::Error;
}
//sample file compression FILE* rf, *wf;
// char bufIn[32768], bufOut[32768];
//compress_lambda(
// [&](const char *&src, size_t &src_pos, size_t &src_size)
// {if (src_pos < src_size) return BlobStreamStatus::Continue;//previously extracted wasn't consumed
//  src = bufIn; src_pos = 0;
//  src_size = fread(src, 1, sizeof(bufIn), rf);
//  return ferror(rf) ? BlobStreamStatus::Error : src_size == 0 ? BlobStreamStatus::Finished : BlobStreamStatus::Continue;
// },
// [&](const char *&dst, size_t &dst_pos, size_t &dst_size)
// {
//  if (dst_pos && fwrite(src, 1, dst_pos, wf) != dst_pos) return BlobStreamStatus::Error;
//  dst = bufOut; dst_pos = 0; dst_size = sizeof(bufOut);
//  return BlobStreamStatus::Continue;
// },
// 19, BlobStreamType::ZSTD);

template <typename Produce, typename Consume>
inline BlobStreamStatus decompress_lambda(Produce rcb, Consume wcb, BlobStreamType type)
{
  char cctx[BLOB_STREAM_CTX_SIZE];
  if (!init_decompress_blob_stream(cctx, sizeof(cctx), type))
    return BlobStreamStatus::Error;
  size_t src_pos = 0, src_size = 0; const char *src = nullptr;
  size_t dst_pos = 0, dst_size = 0; char *dst = nullptr;
  BlobStreamStatus readStatus, writeStatus, decompressStatus = BlobStreamStatus::Continue;
  while ((writeStatus = wcb(dst, dst_pos, dst_size)) == BlobStreamStatus::Continue && (readStatus = rcb(src, src_pos, src_size)) == BlobStreamStatus::Continue)
  {
    decompressStatus = decompress_blob_stream(cctx, src, src_pos, src_size, dst, dst_pos, dst_size);//will free context, no need to call finish_compress_blob_stream
    if (decompressStatus != BlobStreamStatus::Continue)
      break;
  }
  finish_decompress_blob_stream(cctx);
  if (decompressStatus == BlobStreamStatus::Finished)
  {
    wcb(dst, dst_pos, dst_size);//consume
    return BlobStreamStatus::Finished;
  }
  return BlobStreamStatus::Error;
}
//sample file decompression FILE* rf, *wf;
// char bufIn[32768], bufOut[32768];
//decompress_lambda(
// [&](const char *&src, size_t &src_pos, size_t &src_size)
// {if (src_pos < src_size) return BlobStreamStatus::Continue;//previously extracted wasn't consumed
//  src = bufIn; src_pos = 0;
//  src_size = fread(src, 1, sizeof(bufIn), rf);
//  return ferror(rf) ? BlobStreamStatus::Error : BlobStreamStatus::Continue;
// },
// [&](const char *&dst, size_t &dst_pos, size_t &dst_size)
// {
//  if (dst_pos && fwrite(src, 1, dst_pos, wf) != dst_pos) return BlobStreamStatus::Error;
//  dst = bufOut; dst_pos = 0; dst_size = sizeof(bufOut);
//  return BlobStreamStatus::Continue;
// },
// BlobStreamType::ZSTD);