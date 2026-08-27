# blobe_fileio_pull ignores the `from` offset and returns the start of the file

- **File:** cvsnt/cvsnt-2.5.05.3744/ca_blobs_fs/src/fileio.cpp
- **Line(s):** 305-314
- **Severity:** medium
- **Confidence:** high
- **Category:** logic

## Code
```cpp
const char *blobe_fileio_pull(BlobFileIOPullData* fp, uint64_t from, uint64_t &data_pulled)
{
  if (!fp)
    return nullptr;
  const int64_t left = fp->size - from;
  if (left < 0)
    return nullptr;
  data_pulled = left;
  return fp->begin;      // <-- BUG: returns start of mapping, not fp->begin + from
}
```

## Why this is a bug
This is the random-access read primitive behind `caddressed_fs::pull()` (content_addressed_fs.cpp:226) and is documented in content_addressed_fs.h:48-50 as "pull allows random access ... pull(PullData*, uint64_t from, ...)". It computes `data_pulled = size - from` (the number of bytes remaining after `from`) but returns `fp->begin`, i.e. the mapping origin, **not** `fp->begin + from`. So a caller asking for the region starting at byte `from` instead receives the first `size-from` bytes of the file.

Reachability: the KV server's `handle_pull` (keyValueServer/serverLib/blob_push_proc.cpp:192-234) derives `from = blob_from_chunk * pull_chunk_size` from a **client-controlled** 32-bit chunk index and then loops `buf = blob_pull_data(readBlob, from, data_pulled); send_exact(socket, buf, data_pulled);`. A client that requests any chunk > 0 (the protocol's resume/random-access feature) is served the wrong file region — bytes `[0, size-from)` instead of `[from, size)`. The honest client library currently always passes `from == 0` (all `blob_pull_from_server(...,0,0,...)` call sites), so the whole-file case happens to work because a single mmap slice covers everything; that is exactly why the bug is latent and dangerous — the moment chunked/resumed pulls are used (or a peer requests a non-zero chunk), data is silently corrupted. The proxy write-through path (proxy_file_lib.cpp:531,679) uses the same primitive.

There is no out-of-bounds read (`data_pulled = size - from <= size`, and the mapping is `size` bytes), so this is a data-correctness defect rather than memory corruption; content-addressed hash validation on the client will reject the mismatched data, turning it into a hard failure of any non-zero-offset pull.

## Suggested fix
```cpp
  data_pulled = left;
  return fp->begin + from;
```
