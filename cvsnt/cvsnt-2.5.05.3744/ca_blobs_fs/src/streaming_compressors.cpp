#include <zlib.h>
#include <zstd.h>
#include "../streaming_compressors.h"
#include <string.h>
#include <new>
using namespace streaming_compression;

namespace streaming_compression
{

StreamStatus decompress(const char *src, size_t src_size, char *dest, size_t dest_capacity, StreamType type)
{
  if (type == StreamType::ZLIB)
  {
    z_stream stream = {0};
    inflateInit(&stream);
    stream.avail_in = src_size;
    stream.next_in = (Bytef*)src;
    stream.avail_out = dest_capacity;
    stream.next_out = (Bytef*)dest;
    if(inflate(&stream, Z_FINISH)!=Z_STREAM_END)
      return StreamStatus::Error;
    inflateEnd(&stream);
    return StreamStatus::Finished;
  }
  else if (type == StreamType::ZSTD)
  {
    return ZSTD_isError(ZSTD_decompress(dest, dest_capacity, src, src_size)) ?  StreamStatus::Error : StreamStatus::Finished;
  }
  else if (type== StreamType::Unpacked)
  {
    if (src_size <= dest_capacity)
      memcpy(dest, src, src_size);
  }
  return StreamStatus::Error;
}

bool init_decompress_stream(char *ctx_, size_t ctx_size, StreamType type)
{
  static_assert(sizeof(z_stream) <= CTX_SIZE, "streaming ctx defined wrong");
  static_assert(sizeof(ZSTD_DStream*) + sizeof(StreamType) <= CTX_SIZE, "streaming ctx defined wrong");
  if (ctx_size < CTX_SIZE)
    return false;
  memcpy(ctx_, &type, sizeof(type));
  char *ctx = ctx_ + sizeof(type);
  if (type == StreamType::ZLIB)
    inflateInit(new (ctx) z_stream {0});
  else if (type == StreamType::ZSTD)
  {
	*(ZSTD_DStream**)ctx = ZSTD_createDStream();
    ZSTD_initDStream(*(ZSTD_DStream**)ctx);
  } else if (type != StreamType::Unpacked)
    return false;
  return true;
}

static StreamStatus decode_stream_zlib(z_stream* stream, const char *src, size_t &src_pos, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity)
{
  while (src_pos < src_size && dest_pos < dest_capacity)
  {
    stream->avail_in = src_size - src_pos;
    stream->next_in = (Bytef*)(src + src_pos);
    stream->avail_out = dest_capacity - dest_pos;
    stream->next_out = (Bytef*)(dest + dest_pos);
    int result = inflate(stream, Z_NO_FLUSH);
    dest_pos = dest_capacity - stream->avail_out;
    src_pos = src_size - stream->avail_in;
    if (result == Z_STREAM_END)
      return StreamStatus::Finished;
    if (result != Z_OK)
      return StreamStatus::Error;
  }
  return StreamStatus::Continue;
}

static StreamStatus decode_stream_zstd(ZSTD_DStream* zds, const char *src, size_t &src_pos, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity)
{
  ZSTD_inBuffer_s streamIn;
  streamIn.src = src;
  streamIn.size = src_size;
  streamIn.pos = src_pos;

  ZSTD_outBuffer_s streamOut;
  streamOut.dst = dest;
  streamOut.size = dest_capacity;
  streamOut.pos = dest_pos;
  size_t sz;
  do {
    sz = ZSTD_decompressStream(zds, &streamOut, &streamIn);

    if (sz == 0)
      break;
    if (streamOut.pos == streamOut.size && !ZSTD_isError(sz))
      sz = ZSTD_decompressStream(zds, &streamOut, &streamIn);
    if (sz == 0)
      break;
    if (ZSTD_isError(sz))
      return StreamStatus::Error;
  } while(streamIn.pos < streamIn.size && streamOut.pos < streamOut.size);
  dest_pos = streamOut.pos;
  src_pos = streamIn.pos;
  return sz == 0 ? StreamStatus::Finished : StreamStatus::Continue;
}

StreamStatus decompress_stream(char *ctx, const char *src, size_t &src_pos, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity)
{
  if (src_pos >= src_size || dest_pos >= dest_capacity)
    return StreamStatus::Continue;
  if (*(StreamType*)ctx == StreamType::ZLIB)
    return decode_stream_zlib((z_stream*)(ctx+sizeof(StreamType)), src, src_pos, src_size, dest, dest_pos, dest_capacity);
  else if (*(StreamType*)ctx == StreamType::ZSTD)
    return decode_stream_zstd(*(ZSTD_DStream**)(ctx+sizeof(StreamType)), src, src_pos, src_size, dest, dest_pos, dest_capacity);
  else if (*(StreamType*)ctx == StreamType::Unpacked)
  {
    size_t copy = src_size-src_pos < dest_capacity-dest_pos ? src_size-src_pos : dest_capacity-dest_pos;
    memcpy(dest + dest_pos, src + src_pos, copy);
    src_pos += copy;
    dest_pos += copy;
    return StreamStatus::Continue;//we never know if data is finished in unpacked file. That is left to application
  } else
    return StreamStatus::Error;
}

void finish_decompress_stream(char *ctx)
{
  if (*(StreamType*)ctx == StreamType::ZLIB)
    inflateEnd((z_stream*)(ctx+sizeof(StreamType)));
  else if (*(StreamType*)ctx == StreamType::ZSTD)
    ZSTD_freeDStream(*(ZSTD_DStream**)(ctx+sizeof(StreamType)));
  else if (*(StreamType*)ctx == StreamType::Unpacked)
  {}
  *(StreamType*)ctx = StreamType::Undefined;
}


//====compressing
bool init_compress_stream(char *ctx_, size_t ctx_size, int compression, StreamType type)
{
  static_assert(sizeof(z_stream) <= CTX_SIZE, "streaming ctx defined wrong");
  static_assert(sizeof(ZSTD_CStream*) + sizeof(StreamType) + sizeof(int)<= CTX_SIZE, "streaming ctx defined wrong");
  if (ctx_size < CTX_SIZE)
    return false;
  memcpy(ctx_, &type, sizeof(type));
  char *ctx = ctx_ + sizeof(type);
  if (type == StreamType::ZLIB)
    deflateInit(new(ctx) z_stream {0}, compression);
  else if (type == StreamType::ZSTD)
  {
	*(ZSTD_CStream**)ctx = ZSTD_createCStream();
    ZSTD_initCStream(*(ZSTD_CStream**)ctx, compression);
    memcpy(ctx + sizeof(ZSTD_CStream*), &compression, sizeof(int));
  } else if (type != StreamType::Unpacked)
    return false;
  return true;
}

size_t compress_bound(char *ctx, size_t src_size)
{
  if (*(StreamType*)ctx == StreamType::ZLIB)
    return deflateBound((z_stream*)(ctx+sizeof(StreamType)), src_size);
  else if (*(StreamType*)ctx == StreamType::ZSTD)
    return ZSTD_compressBound(src_size);
  else if (*(StreamType*)ctx == StreamType::Unpacked)
    return src_size;
  return 0;
}

StreamStatus compress_stream(char *ctx_, const char *src, size_t src_size, char *dest, size_t &dest_size)
{
  char *ctx = ctx_ + sizeof(StreamType);
  if (*(StreamType*)ctx_ == StreamType::ZLIB)
  {
    z_stream &stream = *(z_stream *)ctx;
    stream.avail_in = src_size;
    stream.next_in = (Bytef*)src;
    stream.avail_out = dest_size;
    stream.next_out = (Bytef*)dest;
    const bool error = deflate(&stream, Z_FINISH) != Z_STREAM_END;
    deflateEnd(&stream);
    if (!error)
      dest_size = dest_size - stream.avail_out;
    *(StreamType*)ctx_ = StreamType::Undefined;
    return error ? StreamStatus::Error : StreamStatus::Finished;
  }
  else if (*(StreamType*)ctx_ == StreamType::ZSTD)
  {
    ZSTD_CStream* zcs = *(ZSTD_CStream**)ctx;
    ZSTD_inBuffer_s streamIn;
    streamIn.src = src;
    streamIn.size = src_size;
    streamIn.pos = 0;

    ZSTD_outBuffer_s streamOut;
    streamOut.dst = dest;
    streamOut.size = dest_size;
    streamOut.pos = 0;

    size_t sz = ZSTD_compressStream(zcs, &streamOut, &streamIn);
    if (!ZSTD_isError(sz))
      sz = ZSTD_endStream(zcs, &streamOut);
    ZSTD_freeCStream(zcs);
    if (sz == 0)
      dest_size = streamOut.pos;
    *(StreamType*)ctx_ = StreamType::Undefined;
    return sz == 0 ? StreamStatus::Finished : StreamStatus::Error;
  }
  else if (*(StreamType*)ctx == StreamType::Unpacked)
  {
    if (src_size <= dest_size)
      memcpy(dest, src, src_size);
    return StreamStatus::Finished;
  }
  return StreamStatus::Error;
}


static StreamStatus compress_stream_zlib(z_stream* stream, const char *src, size_t &src_pos, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity)
{
  while (src_pos < src_size && dest_pos < dest_capacity)
  {
    stream->avail_in = src_size - src_pos;
    stream->next_in = (Bytef*)(src + src_pos);
    stream->avail_out = dest_capacity - dest_pos;
    stream->next_out = (Bytef*)(dest + dest_pos);
    int result = inflate (stream, Z_NO_FLUSH);
    dest_pos = dest_capacity - stream->avail_out;
    src_pos = src_size - stream->avail_in;
    if (result == Z_BUF_ERROR)
      return StreamStatus::Continue;
    if (result != Z_OK)
      return StreamStatus::Error;
  }
  return StreamStatus::Continue;
}

static StreamStatus compress_stream_zstd(ZSTD_CStream* zcs, const char *src, size_t &src_pos, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity)
{
  while (src_pos < src_size && dest_pos < dest_capacity)
  {
    ZSTD_inBuffer_s streamIn;
    streamIn.src = src;
    streamIn.size = src_size;
    streamIn.pos = src_pos;

    ZSTD_outBuffer_s streamOut;
    streamOut.dst = dest;
    streamOut.size = dest_capacity;
    streamOut.pos = dest_pos;
    size_t sz = ZSTD_compressStream(zcs, &streamOut, &streamIn);
    if (ZSTD_isError(sz))
      return StreamStatus::Error;
    src_pos = streamIn.pos;
    dest_pos = streamOut.pos;
  }
  return StreamStatus::Continue;
}

StreamStatus compress_stream(char *ctx, const char *src, size_t &src_pos, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity)
{
  if (dest_pos >= dest_capacity)
    return StreamStatus::Continue;
  if (*(StreamType*)ctx == StreamType::ZLIB)
  {
    z_stream* stream = (z_stream*)(ctx+sizeof(StreamType));
    if (compress_stream_zlib(stream, src, src_pos, src_size, dest, dest_pos, dest_capacity) == StreamStatus::Error)
    {
      deflateEnd(stream);
      *(StreamType*)ctx = StreamType::Undefined;
      return StreamStatus::Error;
    }
  } else if (*(StreamType*)ctx == StreamType::ZSTD)
  {
    ZSTD_CStream* zcs = *(ZSTD_CStream**)(ctx+sizeof(StreamType));
    if (compress_stream_zstd(zcs, src, src_pos, src_size, dest, dest_pos, dest_capacity) == StreamStatus::Error)
    {
      *(StreamType*)ctx = StreamType::Undefined;
      ZSTD_freeCStream(zcs);
      return StreamStatus::Error;
    }
  } else if (*(StreamType*)ctx == StreamType::Unpacked)
  {
    size_t copy = src_size-src_pos < dest_capacity-dest_pos ? src_size-src_pos : dest_capacity-dest_pos;
    memcpy(dest + dest_pos, src + src_pos, copy);
    src_pos += copy;
    dest_pos += copy;
    return StreamStatus::Continue;
  } else
    return StreamStatus::Error;
  return StreamStatus::Continue;
}

StreamStatus compress_stream_and_finish(char *ctx, const char *src, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity)
{
  size_t src_pos = 0;
  StreamStatus st;
  if ((st = compress_stream(ctx, src, src_pos, src_size, dest, dest_pos, dest_capacity)) == StreamStatus::Continue)
    st = finalize_compress_stream(ctx, dest, dest_pos, dest_capacity);
  kill_compress_stream(ctx);
  return st;
}

void kill_compress_stream(char *ctx)
{
  if (*(StreamType*)ctx == StreamType::ZLIB)
  {
    z_stream* stream = (z_stream*)(ctx+sizeof(StreamType));
    deflateEnd(stream);
  } else if (*(StreamType*)ctx == StreamType::ZSTD)
  {
    ZSTD_CStream* zcs = *(ZSTD_CStream**)(ctx+sizeof(StreamType));
    ZSTD_freeCStream(zcs);
  }
  *(StreamType*)ctx = StreamType::Undefined;
}

StreamStatus finalize_compress_stream(char *ctx, char *dest, size_t &dest_pos, size_t dest_capacity)
{
  if (*(StreamType*)ctx == StreamType::ZLIB)
  {
    z_stream* stream = (z_stream*)(ctx+sizeof(StreamType));
	stream->avail_out = dest_capacity - dest_pos;
	stream->next_out = (unsigned char *)(dest + dest_pos);

	const int result = deflate (stream, Z_FINISH);
    dest_pos = dest_capacity - stream->avail_out;
    if (result != Z_OK)
    {
      deflateEnd(stream);
      *(StreamType*)ctx = StreamType::Undefined;
    }
    if (result == Z_STREAM_END)
      return StreamStatus::Finished;
    return result == Z_OK ? StreamStatus::Continue : StreamStatus::Error;
  } else if (*(StreamType*)ctx == StreamType::ZSTD)
  {
    ZSTD_CStream* zcs = *(ZSTD_CStream**)(ctx+sizeof(StreamType));
    ZSTD_outBuffer_s streamOut;
    streamOut.dst = dest;
    streamOut.size = dest_capacity;
    streamOut.pos = dest_pos;
    const size_t sz = ZSTD_endStream(zcs, &streamOut);
    dest_pos = streamOut.pos;
    if (sz == 0 || ZSTD_isError(sz))
    {
      *(StreamType*)ctx = StreamType::Undefined;
      ZSTD_freeCStream(zcs);
    }
    if (sz == 0)
      return StreamStatus::Finished;
    return ZSTD_isError(sz) ? StreamStatus::Error : StreamStatus::Continue;
  }
  return StreamStatus::Finished;
}

};