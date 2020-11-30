#include "zlib.h"
#include "zstd.h"
#include "streaming_compressors.h"
#include <string.h>

BlobStreamStatus decompress_blob(const char *src, size_t src_size, char *dest, size_t dest_capacity, BlobStreamType type)
{
  if (type == BlobStreamType::ZLIB)
  {
    z_stream stream = {0};
    inflateInit(&stream);
    stream.avail_in = src_size;
    stream.next_in = (Bytef*)src;
    stream.avail_out = dest_capacity;
    stream.next_out = (Bytef*)dest;
    if(inflate(&stream, Z_FINISH)!=Z_STREAM_END)
      return BlobStreamStatus::Error;
    inflateEnd(&stream);
    return BlobStreamStatus::Finished;
  }
  else if (type == BlobStreamType::ZSTD)
  {
    return ZSTD_isError(ZSTD_decompress(dest, dest_capacity, src, src_size)) ?  BlobStreamStatus::Error : BlobStreamStatus::Finished;
  }
  else if (type== BlobStreamType::Unpacked)
  {
    if (src_size <= dest_capacity)
      memcpy(dest, src, src_size);
  }
  return BlobStreamStatus::Error;
}

bool init_decompress_blob_stream(char *ctx_, size_t ctx_size, BlobStreamType type)
{
  static_assert(sizeof(z_stream) <= BLOB_STREAM_CTX_SIZE, "streaming ctx defined wrong");
  static_assert(sizeof(ZSTD_DStream*) + sizeof(BlobStreamType) <= BLOB_STREAM_CTX_SIZE, "streaming ctx defined wrong");
  if (ctx_size < BLOB_STREAM_CTX_SIZE)
    return false;
  memcpy(ctx_, &type, sizeof(type));
  char *ctx = ctx + sizeof(type);
  if (type == BlobStreamType::ZLIB)
    inflateInit((z_stream*)ctx);
  else if (type == BlobStreamType::ZSTD)
  {
    memcpy(ctx, ZSTD_createDStream(), sizeof(sizeof(ZSTD_DStream*)));
    ZSTD_initDStream((ZSTD_DStream*)ctx);
  } else
    return false;
  return true;
}

static BlobStreamStatus decode_blob_stream_zlib(z_stream* stream, const char *src, size_t &src_pos, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity)
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
      return BlobStreamStatus::Finished;
    if (result != Z_OK)
      return BlobStreamStatus::Error;
  }
  return BlobStreamStatus::Continue;
}

static BlobStreamStatus decode_blob_stream_zstd(ZSTD_DStream* zds, const char *src, size_t &src_pos, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity)
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
      return BlobStreamStatus::Error;
  } while(streamIn.pos < streamIn.size && streamOut.pos < streamOut.size);
  dest_pos = streamOut.pos;
  src_pos = streamIn.pos;
  return sz == 0 ? BlobStreamStatus::Finished : BlobStreamStatus::Continue;
}

BlobStreamStatus decompress_blob_stream(char *ctx, const char *src, size_t &src_pos, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity)
{
  if (src_pos >= src_size || dest_pos >= dest_capacity)
    return BlobStreamStatus::Continue;
  if (*(BlobStreamType*)ctx == BlobStreamType::ZLIB)
    return decode_blob_stream_zlib((z_stream*)(ctx+sizeof(BlobStreamType)), src, src_pos, src_size, dest, dest_pos, dest_capacity);
  else if (*(BlobStreamType*)ctx == BlobStreamType::ZSTD)
    return decode_blob_stream_zstd((ZSTD_DStream*)(ctx+sizeof(BlobStreamType)), src, src_pos, src_size, dest, dest_pos, dest_capacity);
  else if (*(BlobStreamType*)ctx == BlobStreamType::Unpacked)
  {
    size_t copy = src_size-src_pos < dest_capacity-dest_pos ? src_size-src_pos : dest_capacity-dest_pos;
    memcpy(dest + dest_pos, src + src_pos, copy);
    src_pos += copy;
    dest_pos += copy;
    return BlobStreamStatus::Continue;//we never know if data is finished in unpacked file. That is left to application
  } else
    return BlobStreamStatus::Error;
}

void finish_decompress_blob_stream(char *ctx)
{
  if (*(BlobStreamType*)ctx == BlobStreamType::ZLIB)
    inflateEnd((z_stream*)(ctx+sizeof(BlobStreamType)));
  else if (*(BlobStreamType*)ctx == BlobStreamType::ZSTD)
    ZSTD_freeDStream((ZSTD_DStream*)(ctx+sizeof(BlobStreamType)));
  else if (*(BlobStreamType*)ctx == BlobStreamType::Unpacked)
  {}
}


//====compressing
bool init_compress_blob_stream(char *ctx_, size_t ctx_size, int compression, BlobStreamType type)
{
  static_assert(sizeof(z_stream) <= BLOB_STREAM_CTX_SIZE, "streaming ctx defined wrong");
  static_assert(sizeof(ZSTD_CStream*) + sizeof(BlobStreamType) + sizeof(int)<= BLOB_STREAM_CTX_SIZE, "streaming ctx defined wrong");
  if (ctx_size < BLOB_STREAM_CTX_SIZE)
    return false;
  memcpy(ctx_, &type, sizeof(type));
  char *ctx = ctx + sizeof(type);
  if (type == BlobStreamType::ZLIB)
    deflateInit((z_stream*)ctx, compression);
  else if (type == BlobStreamType::ZSTD)
  {
    memcpy(ctx, ZSTD_createCStream(), sizeof(sizeof(ZSTD_CStream*)));
    ZSTD_initCStream((ZSTD_CStream*)ctx, compression);
    memcpy(ctx + sizeof(ZSTD_CStream*), &compression, sizeof(int));
  } else
    return false;
  return true;
}

size_t compress_blob_bound(char *ctx, size_t src_size)
{
  if (*(BlobStreamType*)ctx == BlobStreamType::ZLIB)
    return deflateBound((z_stream*)(ctx+sizeof(BlobStreamType)), src_size);
  else if (*(BlobStreamType*)ctx == BlobStreamType::ZSTD)
    return ZSTD_compressBound(src_size);
  else if (*(BlobStreamType*)ctx == BlobStreamType::Unpacked)
    return src_size;
  return 0;
}

BlobStreamStatus compress_blob(char *ctx_, const char *src, size_t src_size, char *dest, size_t &dest_size)
{
  char *ctx = ctx_ + sizeof(BlobStreamType);
  if (*(BlobStreamType*)ctx_ == BlobStreamType::ZLIB)
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
    *(BlobStreamType*)ctx_ = BlobStreamType::Undefined;
    return error ? BlobStreamStatus::Error : BlobStreamStatus::Finished;
  }
  else if (*(BlobStreamType*)ctx_ == BlobStreamType::ZSTD)
  {
    ZSTD_CStream* zcs = (ZSTD_CStream*)ctx;
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
    *(BlobStreamType*)ctx_ = BlobStreamType::Undefined;
    return sz == 0 ? BlobStreamStatus::Finished : BlobStreamStatus::Error;
  }
  else if (*(BlobStreamType*)ctx == BlobStreamType::Unpacked)
  {
    if (src_size <= dest_size)
      memcpy(dest, src, src_size);
  }
  return BlobStreamStatus::Error;
}


static BlobStreamStatus compress_blob_stream_zlib(z_stream* stream, const char *src, size_t &src_pos, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity)
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
      return BlobStreamStatus::Continue;
    if (result != Z_OK)
      return BlobStreamStatus::Error;
  }
  return BlobStreamStatus::Continue;
}

static BlobStreamStatus compress_blob_stream_zstd(ZSTD_CStream* zcs, const char *src, size_t &src_pos, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity)
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
      return BlobStreamStatus::Error;
    src_pos = streamIn.pos;
    dest_pos = streamOut.pos;
  }
  return BlobStreamStatus::Continue;
}

BlobStreamStatus compress_blob_stream_and_finish(char *ctx, const char *src, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity)
{
  size_t src_pos = 0;
  if (dest_pos >= dest_capacity)
    return BlobStreamStatus::Continue;
  if (*(BlobStreamType*)ctx == BlobStreamType::ZLIB)
    return compress_blob_stream_zlib((z_stream*)(ctx+sizeof(BlobStreamType)), src, src_pos, src_size, dest, dest_pos, dest_capacity);
  else if (*(BlobStreamType*)ctx == BlobStreamType::ZSTD)
    return compress_blob_stream_zstd((ZSTD_CStream*)(ctx+sizeof(BlobStreamType)), src, src_pos, src_size, dest, dest_pos, dest_capacity);
  else if (*(BlobStreamType*)ctx == BlobStreamType::Unpacked)
  {
    size_t copy = src_size-src_pos < dest_capacity-dest_pos ? src_size-src_pos : dest_capacity-dest_pos;
    memcpy(dest + dest_pos, src + src_pos, copy);
    src_pos += copy;
    dest_pos += copy;
    return BlobStreamStatus::Continue;//we never know if data is finished in unpacked file. That is left to application
  } else
    return BlobStreamStatus::Error;
}

BlobStreamStatus finalize_compress_blob_stream(char *ctx, char *dest, size_t &dest_pos, size_t dest_capacity)
{
  if (*(BlobStreamType*)ctx == BlobStreamType::ZLIB)
  {
    z_stream* stream = (z_stream*)(ctx+sizeof(BlobStreamType));
	stream->avail_out = dest_capacity - dest_pos;
	stream->next_out = (unsigned char *)(dest + dest_pos);

	const int result = deflate (stream, Z_FINISH);
    dest_pos = dest_capacity - stream->avail_out;
	if (result != Z_OK && result != Z_STREAM_END)
	{
      deflateEnd(stream);
      *(BlobStreamType*)ctx = BlobStreamType::Undefined;
	}
    if (result == Z_STREAM_END)
      return BlobStreamStatus::Finished;
    return result == Z_OK ? BlobStreamStatus::Continue : BlobStreamStatus::Error;
  } else if (*(BlobStreamType*)ctx == BlobStreamType::ZSTD)
  {
    ZSTD_CStream* zcs = (ZSTD_CStream*)(ctx+sizeof(BlobStreamType));
    ZSTD_outBuffer_s streamOut;
    streamOut.dst = dest;
    streamOut.size = dest_capacity;
    streamOut.pos = dest_pos;
    const size_t sz = ZSTD_endStream(zcs, &streamOut);
    dest_pos = streamOut.pos;
    if (sz == 0 || ZSTD_isError(sz))
    {
      *(BlobStreamType*)ctx = BlobStreamType::Undefined;
      ZSTD_freeCStream(zcs);
    }
    if (sz == 0)
      return BlobStreamStatus::Finished;
    return ZSTD_isError(sz) ? BlobStreamStatus::Error : BlobStreamStatus::Continue;
  }
  return BlobStreamStatus::Finished;
}
