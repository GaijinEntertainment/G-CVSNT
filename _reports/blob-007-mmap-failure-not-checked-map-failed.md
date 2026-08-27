# POSIX mmap failure (MAP_FAILED) is not detected; (void*)-1 used as a valid mapping

- **File:** cvsnt/cvsnt-2.5.05.3744/ca_blobs_fs/src/fileio.cpp
- **Line(s):** 222-230 (mmap), 299-302 (caller check)
- **Severity:** high
- **Confidence:** high
- **Category:** error-handling / portability

## Code
```cpp
const void* blob_fileio_os_mmap(const char *filepath, std::uintmax_t flen)
{
  int fd = open(filepath, O_RDONLY);
  if (fd == -1)
    return nullptr;
  void *ret = mmap(NULL, flen, PROT_READ, MMAP_FLAGS, fd, 0);
  close(fd);
  return ret;                    // on failure mmap returns MAP_FAILED == (void*)-1, NOT nullptr
}
...
BlobFileIOPullData* blobe_fileio_start_pull(const char* filepath, uint64_t &blob_sz)
{
  ...
  const char *begin = (const char *) blob_fileio_os_mmap(filepath, blob_sz);
  if (!begin)                    // <-- only catches nullptr, misses MAP_FAILED
    return nullptr;
  return new BlobFileIOPullData{begin, blob_sz};
}
```

## Why this is a bug
On POSIX, `mmap` returns `MAP_FAILED` (defined as `(void *)-1`), never `NULL`, when it fails (e.g. `ENOMEM`/`EAGAIN` when mapping a very large blob under memory pressure, or a hitting an mmap limit). `blob_fileio_os_mmap` returns that `(void*)-1` straight through, and the caller's `if (!begin)` guard does not catch it, so a `BlobFileIOPullData{ (const char*)-1, blob_sz }` is created and treated as a valid mapping.

Subsequent use dereferences the bogus pointer:
- `blobe_fileio_pull` returns `fp->begin` (== `(void*)-1`) with `data_pulled = blob_sz`;
- the KV server's `handle_pull` then does `send_exact(socket, buf, data_pulled)` — reading `blob_sz` bytes starting at address `-1` → out-of-bounds read / SIGSEGV;
- `get_file_content_hash`/`repack` feed it to `blake3_hasher_update` — same crash.

This is a remotely triggerable server crash (DoS): any PULL/SIZE-driven access to a blob whose mapping fails takes down the connection thread, and repeated large-blob pulls under memory pressure can be induced. The comments elsewhere confirm POSIX is the production platform ("We don't use production servers on windows"). The Windows implementation (line 136-154) correctly returns `nullptr` on failure, so only the POSIX path is affected.

Note also `blob_fileio_os_unmap` receives this pointer on cleanup; `munmap((void*)-1, size)` will just fail with EINVAL, but the primary damage is the earlier dereference.

## Suggested fix
```cpp
  void *ret = mmap(NULL, flen, PROT_READ, MMAP_FLAGS, fd, 0);
  close(fd);
  if (ret == MAP_FAILED)
    return nullptr;
  return ret;
```
(and/or check `begin == nullptr || begin == MAP_FAILED` at the call site).
