---
id: BUG-blob-06
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/ca_blobs_fs/streaming_blobs.h
line: 82
severity: high
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 3
behavior_change: no
---

# `~DownloadBlobInfo` frees a decompressor that was never created when the blob magic is invalid

## Summary
`decode_stream_blob_data()` sets `info.dataRead` to `sizeof(BlobHeader)` *before* trying to
initialise the decompressor, and bails out if the magic is not recognised. The destructor keys off
`dataRead >= sizeof(BlobHeader)` and calls `finish_decompress_stream(cctx)` on a `cctx` that was
never written — indeterminate stack bytes — leading to `ZSTD_freeDStream()` or `inflateEnd()` on a
wild pointer.

## Code
```cpp
// ca_blobs_fs/streaming_blobs.h:75-85
struct DownloadBlobInfo
{
  size_t dataRead = 0, realUncompressedSize = 0;
  BlobHeader hdr = {0};
  char cctx[streaming_compression::CTX_SIZE];    // <-- no initialiser
  ~DownloadBlobInfo()
  {
    if (dataRead >= sizeof(BlobHeader))
      streaming_compression::finish_decompress_stream(cctx);   // 83
  }
};

// ca_blobs_fs/streaming_blobs.h:115-122
    memcpy((char*)&info.hdr + info.dataRead, data, hdrPart);
    info.dataRead += hdrPart;                                  // dataRead now >= sizeof(BlobHeader)

    if (info.dataRead >= sizeof(BlobHeader))
    {
      if (!init_decompress_blob_stream(info.cctx, sizeof(info.cctx), info.hdr))
        return false;                                          // <-- cctx still uninitialised
    }
```

```cpp
// ca_blobs_fs/src/streaming_compressors.cpp:125-134
void finish_decompress_stream(char *ctx)
{
  if (*(StreamType*)ctx == StreamType::ZLIB)
    inflateEnd((z_stream*)(ctx+sizeof(StreamType)));
  else if (*(StreamType*)ctx == StreamType::ZSTD)
    ZSTD_freeDStream(*(ZSTD_DStream**)(ctx+sizeof(StreamType)));   // free() of 8 stack-garbage bytes
```

## Why it is a bug
`init_decompress_blob_stream` (`streaming_blobs.h:87-104`) returns false without touching `ctx`
whenever `is_accepted_magic(hdr.magic)` is false — i.e. whenever the first four bytes of the blob
are not `ZLIB`, `ZSTD` or `NONE`. `init_decompress_stream` is the only thing that ever writes the
`StreamType` tag at `cctx[0..3]` and the `ZSTD_DStream*` at `cctx[4..11]`, so after a rejected
magic the whole `cctx` array is whatever was on the stack.

The struct has no initialiser for `cctx`, and the two network-facing call sites *default*-initialise
it rather than value-initialising it:

```cpp
// src/download_blob_to.cpp:384      (inside the 16-attempt retry loop)
    caddressed_fs::DownloadBlobInfo info;
// keyValueServer/sample/cafs_client.cpp:94
    caddressed_fs::DownloadBlobInfo info;
// ca_blobs_fs/src/content_addressed_fs.cpp:303   (repack)
  DownloadBlobInfo info;
```

so `cctx` is indeterminate. `*(StreamType*)cctx` then reads a random 4-byte value; one time in
~2^31 it equals `StreamType::ZSTD` (2) and `ZSTD_freeDStream` calls `free()` on eight arbitrary
stack bytes, and one time in ~2^31 it equals `ZLIB` (1) and `inflateEnd` dereferences a garbage
`z_stream`. In practice stack slots are highly non-random and frequently hold small integers, so the
odds are far worse than uniform — a leftover `1` or `2` from an earlier frame is a very common
pattern.

## Failure scenario
CVS client checkout of a blob. `download_blob_ref_file` (`download_blob_to.cpp:366-451`) declares
`DownloadBlobInfo info;` *inside* the 16-attempt loop, so the destructor runs once per attempt.

A CAFS server (or, per BUG-blob-01, an in-path attacker on an unencrypted session, or simply a
blob file that got corrupted on disk) returns a body whose first four bytes are, say, `"XXXX"`:

1. `recv_lambda` -> `decode_stream_blob_data(info, data, len, ...)`.
2. `hdrPart = 16`, `memcpy` fills `info.hdr`, `info.dataRead = 16`.
3. `init_decompress_blob_stream` -> `is_accepted_magic("XXXX")` is false -> returns false,
   **`info.cctx` untouched**.
4. `decode_stream_blob_data` returns false -> the download callback returns false -> `ok = false`
   -> `KVNetworkProcessor::download` returns false -> `downloadRet = false`.
5. End of loop iteration: `~DownloadBlobInfo` runs with `dataRead == 16` and an uninitialised
   `cctx` -> `finish_decompress_stream(garbage)` -> `free()` on a wild pointer -> heap corruption
   or `SIGSEGV` in the CVS client.

The same shape is reachable server-side through `stream_push` (`content_addressed_fs.cpp:161`) when
the server is started with `set_allow_trust(false)`, although there `info` lives inside a
heap-allocated `PushData` that is aggregate-initialised, so `cctx` happens to be zeroed there.

## Suggested fix
Track initialisation explicitly instead of inferring it from `dataRead`:
```cpp
  size_t dataRead = 0, realUncompressedSize = 0;
  BlobHeader hdr = {0};
  bool cctxInited = false;
  char cctx[streaming_compression::CTX_SIZE] = {0};
  ~DownloadBlobInfo()
  {
    if (cctxInited)
      streaming_compression::finish_decompress_stream(cctx);
  }
```
and set `info.cctxInited = true` only on a successful `init_decompress_blob_stream`.
(Even the one-line `char cctx[...] = {0};` alone removes the wild free, because
`StreamType(0) == Unpacked` makes `finish_decompress_stream` a no-op.)

## Refutation attempt
I checked whether the two heap call sites save the day. `caddressed_fs::PushData`
(`content_addressed_fs.cpp:113-146`) is created with `new PushData{...}`, so `info` is
*value*-initialised and `cctx` is zeroed — that path is safe. The proxy's
`PullThroughTemp::info` is default-initialised via `new BlobProxyPull` but is explicitly reassigned
`info = caddressed_fs::DownloadBlobInfo{};` at `proxy_file_lib.cpp:346` before use, which zeroes it
— also safe. The remaining sites (`download_blob_to.cpp:384`, `cafs_client.cpp:94`,
`content_addressed_fs.cpp:303`, `src/rcs_cvt_kB.cpp:33`) are plain block-scope declarations with no
initialiser and no assignment, so `cctx` really is indeterminate there. I also verified that
`ZSTD_freeDStream(NULL)` is safe but `ZSTD_freeDStream(garbage)` is not — it dereferences the
context's custom-allocator field before freeing. The finding stands.
