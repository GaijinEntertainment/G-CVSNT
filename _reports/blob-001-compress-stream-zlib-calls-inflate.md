# Streaming ZLIB compressor calls inflate() instead of deflate()

- **File:** cvsnt/cvsnt-2.5.05.3744/ca_blobs_fs/src/streaming_compressors.cpp
- **Line(s):** 218-235 (esp. 226)
- **Severity:** high
- **Confidence:** high
- **Category:** typo

## Code
```cpp
static StreamStatus compress_stream_zlib(z_stream* stream, const char *src, size_t &src_pos, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity)
{
  while (src_pos < src_size && dest_pos < dest_capacity)
  {
    stream->avail_in = (uint32_t)(src_size - src_pos);
    stream->next_in = (Bytef*)(src + src_pos);
    stream->avail_out = (uint32_t)(dest_capacity - dest_pos);
    stream->next_out = (Bytef*)(dest + dest_pos);
    int result = inflate (stream, Z_NO_FLUSH);   // <-- BUG: this is the COMPRESS path
    ...
```

## Why this is a bug
`compress_stream_zlib` is the ZLIB branch of the public streaming `compress_stream()` (line 259-291, dispatched when `*(StreamType*)ctx == StreamType::ZLIB`). The z_stream held in the ctx was initialized with `deflateInit` in `init_compress_stream` (line 147). Calling `inflate()` on a deflate-initialized stream is invalid: modern zlib's `inflateStateCheck` returns `Z_STREAM_ERROR` (so every streaming ZLIB compression fails), and older zlibs interpret deflate state as inflate state — undefined behavior/possible memory corruption. This is an obvious copy-paste from `decode_stream_zlib` (line 58-75), which is byte-for-byte identical except the error handling.

Note also the paired error-handling logic (lines 229-232) is the copy-pasted inflate logic: `Z_BUF_ERROR` -> Continue, `!= Z_OK` -> Error, which happens to be tolerable for deflate but was never written for it.

Currently nothing in the tree calls streaming compression with `StreamType::ZLIB` (all users pass ZSTD or Unpacked), so the bug is latent — but the function is a public API (`streaming_compressors.h` line 22) and will break the moment anyone compresses a legacy ZLIB blob through it.

## Suggested fix
```cpp
int result = deflate(stream, Z_NO_FLUSH);
```
and review the result handling for deflate semantics (deflate with Z_NO_FLUSH returns Z_OK on success; Z_BUF_ERROR when no progress is possible, which is a valid Continue).
