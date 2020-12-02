#pragma once
#include <stddef.h>

//generalizes and abstracting zlib/zstd

namespace streaming_compression
{

enum class StreamType {Unpacked, ZLIB, ZSTD, Undefined};
enum class StreamStatus {Continue, Finished, Error};
enum {CTX_SIZE = 256};
//this is just generalized zstd/zlib code
StreamStatus decompress(const char *src, size_t src_size, char *dest, size_t dest_capacity, StreamType type);
size_t compress_bound(size_t src_size, StreamType type);

bool init_decompress_stream(char *ctx_, size_t ctx_size, StreamType type);
StreamStatus decompress_stream(char *ctx, const char *src, size_t &src_pos, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity);
void finish_decompress_stream(char *ctx);

bool init_compress_stream(char *ctx_, size_t ctx_size, int compression_level, StreamType type);
size_t compress_bound(char *ctx, size_t src_size);
StreamStatus compress_stream(char *ctx, const char *src, size_t &src_pos, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity);//will free context, no need to call finish_compress_stream
StreamStatus compress_stream_and_finish(char *ctx, const char *src, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity);

//should be called in loop until Finished or Error, while freeing some space in destination
void kill_compress_stream(char *ctx);
StreamStatus finalize_compress_stream(char *ctx, char *dest, size_t &dest_pos, size_t dest_capacity);

//StreamStatus (const char *&src, size_t &src_pos, size_t &src_size)
template <typename Produce, typename Consume>
inline StreamStatus compress_lambda(Produce rcb, Consume wcb, int compression_level, StreamType type = StreamType::ZSTD)
{
  char cctx[CTX_SIZE];
  if (!init_compress_stream(cctx, sizeof(cctx), compression_level, type))
    return StreamStatus::Error;
  size_t src_pos = 0, src_size = 0; const char *src = nullptr;
  size_t dst_pos = 0, dst_size = 0; char *dst = nullptr;
  StreamStatus readStatus, writeStatus, compressStatus = StreamStatus::Continue;
  while ((writeStatus = wcb(dst, dst_pos, dst_size)) == StreamStatus::Continue && (readStatus = rcb(src, src_pos, src_size)) == StreamStatus::Continue)
  {
    compressStatus = compress_stream(cctx, src, src_pos, src_size, dst, dst_pos, dst_size);//will free context, no need to call finish_compress_stream
    if (compressStatus == StreamStatus::Error)
      return StreamStatus::Error;
  }
  if (readStatus == StreamStatus::Error)
    return StreamStatus::Error;

  while ((writeStatus = wcb(dst, dst_pos, dst_size)) == StreamStatus::Continue && compressStatus == StreamStatus::Continue)
  {
    compressStatus = finalize_compress_stream(cctx, dst, dst_pos, dst_size);//will free context, no need to call finish_compress_stream
    if (compressStatus == StreamStatus::Error)
      return StreamStatus::Error;
  }
  return compressStatus == StreamStatus::Finished && writeStatus != StreamStatus::Error ? StreamStatus::Finished : StreamStatus::Error;
}
//sample file compression FILE* rf, *wf;
// char bufIn[32768], bufOut[32768];
//compress_lambda(
// [&](const char *&src, size_t &src_pos, size_t &src_size)
// {if (src_pos < src_size) return StreamStatus::Continue;//previously extracted wasn't consumed
//  src = bufIn; src_pos = 0;
//  src_size = fread(src, 1, sizeof(bufIn), rf);
//  return ferror(rf) ? StreamStatus::Error : src_size == 0 ? StreamStatus::Finished : StreamStatus::Continue;
// },
// [&](const char *&dst, size_t &dst_pos, size_t &dst_size)
// {
//  if (dst_pos && fwrite(dst, 1, dst_pos, wf) != dst_pos) return StreamStatus::Error;
//  dst = bufOut; dst_pos = 0; dst_size = sizeof(bufOut);
//  return StreamStatus::Continue;
// },
// 19, StreamType::ZSTD);

template <typename Produce, typename Consume>
inline StreamStatus decompress_lambda(Produce rcb, Consume wcb, StreamType type)
{
  char cctx[CTX_SIZE];
  if (!init_decompress_stream(cctx, sizeof(cctx), type))
    return StreamStatus::Error;
  size_t src_pos = 0, src_size = 0; const char *src = nullptr;
  size_t dst_pos = 0, dst_size = 0; char *dst = nullptr;
  StreamStatus readStatus, writeStatus, decompressStatus = StreamStatus::Continue;
  while ((writeStatus = wcb(dst, dst_pos, dst_size)) == StreamStatus::Continue && (readStatus = rcb(src, src_pos, src_size)) == StreamStatus::Continue)
  {
    decompressStatus = decompress_stream(cctx, src, src_pos, src_size, dst, dst_pos, dst_size);//will free context, no need to call finish_compress_stream
    if (decompressStatus != StreamStatus::Continue)
      break;
  }
  finish_decompress_stream(cctx);
  if (decompressStatus == StreamStatus::Finished || readStatus == StreamStatus::Finished)
  {
    wcb(dst, dst_pos, dst_size);//consume
    if (type == StreamType::Unpacked || decompressStatus == StreamStatus::Finished)
      return StreamStatus::Finished;
  }
  return StreamStatus::Error;
}
//sample file decompression FILE* rf, *wf;
// char bufIn[32768], bufOut[32768];
//decompress_lambda(
// [&](const char *&src, size_t &src_pos, size_t &src_size)
// {if (src_pos < src_size) return StreamStatus::Continue;//previously extracted wasn't consumed
//  src = bufIn; src_pos = 0;
//  src_size = fread(src, 1, sizeof(bufIn), rf);
//  return ferror(rf) ? StreamStatus::Error : StreamStatus::Continue;
// },
// [&](const char *&dst, size_t &dst_pos, size_t &dst_size)
// {
//  if (dst_pos && fwrite(src, 1, dst_pos, wf) != dst_pos) return StreamStatus::Error;
//  dst = bufOut; dst_pos = 0; dst_size = sizeof(bufOut);
//  return StreamStatus::Continue;
// },
// StreamType::ZSTD);
};