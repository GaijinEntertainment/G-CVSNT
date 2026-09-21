---
id: BUG-blob-10
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/ca_blobs_fs/src/streaming_compressors.cpp
line: 226
severity: medium
category: typo
status: partially fixed in this slice (the inflate->deflate call at :226); the Unpacked-branch ctx cast and the missing inflateEnd remain open
verdict: CONFIRMED
fix_size_loc: 3
behavior_change: yes
---

# `compress_stream_zlib()` calls `inflate()` instead of `deflate()` — streaming zlib compression can never succeed

## Summary
`compress_stream_zlib` is the streaming *compressor* for `StreamType::ZLIB`; its `z_stream` was set
up by `deflateInit`. The loop body calls `inflate()`, a verbatim copy of the neighbouring
`decode_stream_zlib`. zlib's `inflateStateCheck` rejects a deflate state, so the call always returns
`Z_STREAM_ERROR` and the compressor always reports `StreamStatus::Error`. Two smaller copy-paste
defects sit next to it.

## Code
```cpp
// ca_blobs_fs/src/streaming_compressors.cpp:218-235
static StreamStatus compress_stream_zlib(z_stream* stream, const char *src, size_t &src_pos, size_t src_size, char *dest, size_t &dest_pos, size_t dest_capacity)
{
  while (src_pos < src_size && dest_pos < dest_capacity)
  {
    stream->avail_in = (uint32_t)(src_size - src_pos);
    stream->next_in = (Bytef*)(src + src_pos);
    stream->avail_out = (uint32_t)(dest_capacity - dest_pos);
    stream->next_out = (Bytef*)(dest + dest_pos);
    int result = inflate (stream, Z_NO_FLUSH);      // <-- BUG: must be deflate()
    dest_pos = dest_capacity - stream->avail_out;
    src_pos = src_size - stream->avail_in;
    if (result == Z_BUF_ERROR)
      return StreamStatus::Continue;
    if (result != Z_OK)
      return StreamStatus::Error;
  }
  return StreamStatus::Continue;
}
```
Compare the decompressor it was copied from, `decode_stream_zlib` at `:58-75`, which is identical
except for the `Z_STREAM_END` handling.

Two companion copy-paste defects in the same file:

```cpp
// :169-171, 208   the (undeclared, currently unreachable) 5-argument compress_stream
StreamStatus compress_stream(char *ctx_, const char *src, size_t src_size, char *dest, size_t &dest_size)
{
  char *ctx = ctx_ + sizeof(StreamType);
  if (*(StreamType*)ctx_ == StreamType::ZLIB)      { ... }      // reads type from ctx_  (correct)
  else if (*(StreamType*)ctx_ == StreamType::ZSTD) { ... }      // reads type from ctx_  (correct)
  else if (*(StreamType*)ctx == StreamType::Unpacked)           // 208 <-- reads from ctx_+4
```

```cpp
// :12-37   decompress()
  else if (type== StreamType::Unpacked)
  {
    if (src_size <= dest_capacity)
      memcpy(dest, src, src_size);
  }
  return StreamStatus::Error;      // 36 <-- Unpacked success falls through to Error
```
and the ZLIB branch of the same function (`:22-23`) returns `StreamStatus::Error` without calling
`inflateEnd`, leaking the inflate window on every failed decompression.

## Why it is a bug
zlib's `inflate()` begins with `if (inflateStateCheck(strm) ...) return Z_STREAM_ERROR;`.
`inflateStateCheck` accepts the `state->strm == strm` back-pointer (deflate_state and inflate_state
both start with `z_streamp strm;`) but then tests `state->mode < HEAD || state->mode > SYNC`,
i.e. `mode` in `[16180, 16211]`. At the same offset a `deflate_state` holds `int status`, whose
values are `INIT_STATE = 42`, `BUSY_STATE = 113`, `FINISH_STATE = 666` — all far below 16180.
So `inflateStateCheck` returns 1 and `inflate()` returns `Z_STREAM_ERROR` without touching
`avail_in`/`avail_out`.

`Z_STREAM_ERROR` is neither `Z_BUF_ERROR` nor `Z_OK`, so `compress_stream_zlib` returns
`StreamStatus::Error` on the very first call. Its caller then does the right thing for the wrong
reason:
```cpp
// :263-271
  if (*(StreamType*)ctx == StreamType::ZLIB)
  {
    z_stream* stream = (z_stream*)(ctx+sizeof(StreamType));
    if (compress_stream_zlib(stream, src, src_pos, src_size, dest, dest_pos, dest_capacity) == StreamStatus::Error)
    {
      deflateEnd(stream);
      *(StreamType*)ctx = StreamType::Undefined;
      return StreamStatus::Error;
```
so `StreamType::ZLIB` compression is 100 % broken: `compress_lambda(..., StreamType::ZLIB)` would
fail on its first chunk, and `push_whole_blob_from_raw_data`/`send_blob_file_data_net` would report
"Can't compress binary blob".

## Failure scenario
Today the tree only ever asks for `StreamType::ZSTD` or `StreamType::Unpacked` when compressing
(`content_addressed_fs.cpp:294`, `blob_kv_processor.cpp:48`, `push_whole_blob.h:208`), so the broken
path is not currently exercised — `ZLIB` is only used for *decompressing* legacy blobs
(`streaming_blobs.h:93-94`). The moment anyone selects zlib compression — e.g. to make
`repack()` produce zlib blobs for compatibility, or because `ca_blobs_fs` is reused as the
standalone library its public headers advertise — every compression call returns `Error`, the temp
blob is unlinked (`content_addressed_fs.cpp:296-298` / `push_whole_blob.h:210-213`), and the push is
reported as an I/O failure with no diagnostic pointing at zlib.

## Suggested fix
```cpp
    int result = deflate (stream, Z_NO_FLUSH);
```
plus, for the two companion defects:
```cpp
  else if (*(StreamType*)ctx_ == StreamType::Unpacked)   // :208
```
```cpp
  else if (type == StreamType::Unpacked)                 // :31-35
  {
    if (src_size > dest_capacity)
      return StreamStatus::Error;
    memcpy(dest, src, src_size);
    return StreamStatus::Finished;
  }
```

## Refutation attempt
I checked whether `compress_stream_zlib` might be fed an inflate-initialised stream after all —
`compress_stream` (`:259`) reaches it only via a context built by `init_compress_stream`
(`:146-147`), which calls `deflateInit`, so the state is unambiguously a deflate state. I checked
whether `inflate()` on a deflate state might "accidentally work": it cannot, because
`inflateStateCheck` short-circuits before any decoding. I verified the `Unpacked` type value is
`0` (`streaming_compressors.h:9`) and that `init_compress_stream` writes nothing beyond the first
4 bytes for that type, so the `*(StreamType*)ctx` read at `:208` inspects uninitialised stack —
which is why I flag it, even though that overload is currently undeclared in the header and
unreachable. The finding stands.
