# One-shot decompress() returns Error for Unpacked data even on success; leaks z_stream on ZLIB error

- **File:** cvsnt/cvsnt-2.5.05.3744/ca_blobs_fs/src/streaming_compressors.cpp
- **Line(s):** 12-37
- **Severity:** medium
- **Confidence:** high
- **Category:** logic / error-handling

## Code
```cpp
StreamStatus decompress(const char *src, size_t src_size, char *dest, size_t dest_capacity, StreamType type)
{
  if (type == StreamType::ZLIB)
  {
    z_stream stream = {0};
    inflateInit(&stream);
    ...
    if(inflate(&stream, Z_FINISH)!=Z_STREAM_END)
      return StreamStatus::Error;          // <-- leak: inflateEnd never called on this path
    inflateEnd(&stream);
    return StreamStatus::Finished;
  }
  else if (type == StreamType::ZSTD)
  { ... }
  else if (type== StreamType::Unpacked)
  {
    if (src_size <= dest_capacity)
      memcpy(dest, src, src_size);         // <-- copies, but then...
  }
  return StreamStatus::Error;              // <-- ...always reports Error for Unpacked
}
```

## Why this is a bug
Two defects in the public one-shot `decompress` API (declared in streaming_compressors.h line 13):

1. The `StreamType::Unpacked` branch performs the copy but falls through to `return StreamStatus::Error` — there is no `return StreamStatus::Finished` after the successful memcpy, and the "does not fit" case (`src_size > dest_capacity`) is silently indistinguishable. Any caller handling a `NONE`-magic (uncompressed) blob through this entry point treats a successful copy as a failure.
2. In the ZLIB branch, when `inflate` does not reach `Z_STREAM_END` the function returns without `inflateEnd(&stream)`, leaking the ~10KB inflate state allocated by `inflateInit` on every failed/truncated decompression. In a long-lived multithreaded server, repeated corrupt/truncated zlib blobs leak memory unboundedly.

No in-tree caller of this exact overload was found (streaming paths are used instead), so impact today is latent, but it is exported API.

## Suggested fix
```cpp
  else if (type == StreamType::Unpacked)
  {
    if (src_size > dest_capacity)
      return StreamStatus::Error;
    memcpy(dest, src, src_size);
    return StreamStatus::Finished;
  }
```
and in the ZLIB branch call `inflateEnd(&stream)` before the error return.
