# One-shot compress_stream reads StreamType from wrong pointer in Unpacked branch

- **File:** cvsnt/cvsnt-2.5.05.3744/ca_blobs_fs/src/streaming_compressors.cpp
- **Line(s):** 169-215 (esp. 208)
- **Severity:** low
- **Confidence:** high
- **Category:** typo

## Code
```cpp
StreamStatus compress_stream(char *ctx_, const char *src, size_t src_size, char *dest, size_t &dest_size)
{
  char *ctx = ctx_ + sizeof(StreamType);      // ctx = payload area, ctx_ = where the type tag lives
  if (*(StreamType*)ctx_ == StreamType::ZLIB)
  { ... }
  else if (*(StreamType*)ctx_ == StreamType::ZSTD)
  { ... }
  else if (*(StreamType*)ctx == StreamType::Unpacked)   // <-- BUG: reads ctx, not ctx_
  {
    if (src_size <= dest_size)
      memcpy(dest, src, src_size);
    return StreamStatus::Finished;
  }
  return StreamStatus::Error;
}
```

## Why this is a bug
The first two branches correctly test the type tag at `ctx_` (offset 0), but the Unpacked branch tests `ctx` (= `ctx_ + sizeof(StreamType)`), i.e. it interprets the 4 bytes where the z_stream/ZSTD pointer payload would live — which for an Unpacked context are uninitialized stack garbage (init_compress_stream stores nothing there for Unpacked). So a context initialized with `StreamType::Unpacked` only takes the Unpacked branch if that garbage happens to equal 0; otherwise the function returns `StreamStatus::Error` for a perfectly valid context. Conversely, a ZLIB/ZSTD ctx whose payload bytes happen to be 0 can never reach this branch (types already matched earlier), so the failure mode is "valid Unpacked compression randomly fails".

Secondary defect in the same branch: on success `dest_size` is not updated to `src_size`, so the caller cannot learn the output length, and the `src_size > dest_size` overflow case still returns `Finished` without copying anything.

Mitigating factor: this 5-argument overload is not declared in streaming_compressors.h and has no callers in the tree (the 7-argument streaming overload is the used one) — dead code today, but a landmine.

## Suggested fix
Use `*(StreamType*)ctx_` like the other branches, set `dest_size = src_size` on success, return Error when `src_size > dest_size` — or simply delete this unused overload.
