---
id: BUG-server-12
area: rcs
file: cvsnt/cvsnt-2.5.05.3744/src/rcs_cvt_kB.cpp
line: 73
severity: high
category: correctness
verdict: CONFIRMED
fix_size_loc: 15
behavior_change: yes
---

# A failed or truncated blob pull is reported as success: `-kB` checkout silently yields an empty file, a NULL deref, or a tail of uninitialized heap

## Summary
`RCS_read_binary_rev_data` calls `pull_at_once()` and discards its return value, then unconditionally returns `true`. `pull_at_once()` itself does not report pull success — it returns `destroy(pd)`, i.e. whether the *handle teardown* worked — and it assigns `sz = info.hdr.uncompressedLen` even when decoding aborted part-way. Every blob-store failure therefore reaches `RCS_checkout` disguised as a successful read.

## Code
```cpp
// rcs_cvt_kB.cpp:23-57
inline bool pull_at_once(const char* hash_hex_string, size_t &sz, char **decoded)
{
  ...
  PullData *pd = start_pull(get_default_ctx(), hash_hex_string, blob_sz);
  if (!pd)
    return false;                       // <-- sz and *decoded left untouched
  size_t at = 0;
  DownloadBlobInfo info;
  char *dest = nullptr;
  while (at < blob_sz)
  {
    uint64_t data_pulled = 0;
    const char *some = pull(pd, at, data_pulled);
    if (!data_pulled)
      break;                            // <-- truncated blob: silent break
    at += data_pulled;
    if (!decode_stream_blob_data(info, some, data_pulled, [&](const void *unpacked, size_t sz)
      {
        if (!dest)
          dest = (char*)blob_alloc(info.hdr.uncompressedLen);
        ...
      }
    ))
      break;                            // <-- decode error: silent break
  }
  *decoded = dest;
  sz = info.hdr.uncompressedLen;        // <-- full size claimed even on partial decode
  return destroy(pd);                   // <-- reports teardown, not pull success
}
```

```cpp
// rcs_cvt_kB.cpp:59-88
static bool RCS_read_binary_rev_data(char **out_data, size_t *out_len, int *inout_data_allocated, bool supposed_packed, bool *is_ref)
{
  const bool blobRef = is_blob_reference_data(*out_data, *out_len);
  if (!is_ref && blobRef)
  {
    char hash[64]; memcpy(hash, *out_data + hash_type_magic_len, sizeof(hash));
    if (*inout_data_allocated && *out_data)
      xfree (*out_data);
    *out_data = NULL;
    *out_len = 0;
    *inout_data_allocated = 0;
    pull_at_once(hash, *out_len, out_data);   // <-- result discarded
    if (*out_data)
      *inout_data_allocated = 1;
    return true;                              // <-- always success
  }
```

## Why it is a bug
Three distinct bad states leave the function claiming success:

1. **Blob absent** (`start_pull` returns `nullptr`). `pull_at_once` returns false without touching its out-params, so `*out_data` stays `NULL` and `*out_len` stays `0` (set at rcs_cvt_kB.cpp:70-71). `RCS_read_binary_rev_data` returns `true`, and `RCS_checkout` proceeds to write a **zero-byte working file** with no diagnostic.
2. **Blob truncated** (`pull` returns `data_pulled == 0` after the header was consumed). `dest` is still `nullptr` because the unpack callback never ran, but `sz = info.hdr.uncompressedLen` is the real size N. The caller then has `value == NULL` with `len == N > 0`, which `RCS_checkout` feeds to `expand_keywords`, `cvs_output_binary (value, len)` (rcs_checkin.cpp:281), `memchr(value,'\0',len)` (rcs_checkin.cpp:296) and `fwrite` — a **NULL dereference**.
3. **Blob partially decodable** (`decode_stream_blob_data` returns false mid-stream). `dest` was allocated with `blob_alloc`, which is `xmalloc` (client.cpp:6260) and therefore does *not* zero. Only `info.realUncompressedSize` bytes were written, but `sz` is set to the full `uncompressedLen`, so the checked-out file has the right length with a tail of **uninitialized server heap**.

`RCS_checkout` has an error path for this (rcs_checkin.cpp:133-141 frees `log`/`rev` and returns 0 when `RCS_read_binary_rev_data` fails) — it simply can never be taken.

## Failure scenario
The content-addressed store lives outside the RCS files, so it can diverge from them: a partially-replicated blob proxy, an interrupted `push_whole_blob`, an admin pruning `blobs/`, or a truncated file after an unclean shutdown.

Concretely: take a `-kB` file whose head revision's deltatext is `blake3:<64 hex>`, and truncate the corresponding blob in the CAS to fewer bytes than its header advertises. Then:

```
cvs checkout -r 1.3 mymodule/bigasset.dds
```

`RCS_checkout` → `RCS_checkout_raw_value` returns the 71-byte reference → `RCS_read_binary_rev_data` → `pull_at_once`. `start_pull` succeeds (the file exists), the first `pull` returns the header, the next returns 0 → `break`. `dest == nullptr`, `sz == hdr.uncompressedLen`. Back in `RCS_read_binary_rev_data`, `*out_data == NULL` so `*inout_data_allocated` stays 0, and the function returns `true`. `RCS_checkout` then runs `expand_keywords(..., value=NULL, len=N, ...)` and writes the file — crash, or an N-byte file of heap garbage.

Delete the blob entirely instead, and the same command silently writes a **0-byte** `bigasset.dds`. If the user then commits that file, the empty content becomes a real revision — irreversible data loss on the fork's primary storage path.

## Suggested fix
Make both layers report failure:
```cpp
inline bool pull_at_once(const char* hash_hex_string, size_t &sz, char **decoded)
{
  ...
  bool ok = true;
  while (at < blob_sz)
  {
    uint64_t data_pulled = 0;
    const char *some = pull(pd, at, data_pulled);
    if (!data_pulled) { ok = false; break; }
    at += data_pulled;
    if (!decode_stream_blob_data(info, some, data_pulled, cb)) { ok = false; break; }
  }
  ok = ok && info.realUncompressedSize == info.hdr.uncompressedLen;
  *decoded = dest;
  sz = ok ? info.hdr.uncompressedLen : 0;
  if (!destroy(pd)) ok = false;
  return ok;
}
```
and in `RCS_read_binary_rev_data`:
```cpp
    if (!pull_at_once(hash, *out_len, out_data))
    {
      error (0, 0, "blob %.64s referenced by the repository could not be read", hash);
      return false;
    }
    if (*out_data)
      *inout_data_allocated = 1;
    return true;
```

## Refutation attempt
* *Does `RCS_checkout` guard against `value == NULL` with `len > 0`?* No. rcs_checkin.cpp:229 (`pfn(callerdat, len?value:"", len)`) is the only place that special-cases it, and it passes `value` whenever `len` is non-zero. The `sout`/`workfile` paths at rcs_checkin.cpp:281-330 pass `value` straight to `cvs_output_binary` / `memchr` / `fwrite`.
* *Is `info.hdr.uncompressedLen` uninitialized garbage rather than the real size?* No — `DownloadBlobInfo` declares `BlobHeader hdr = {0};` (ca_blobs_fs/streaming_blobs.h), so it is 0 until the header is decoded and the real value after. That is what makes case 2 (`dest == nullptr`, `sz == N`) reachable rather than merely theoretical.
* *Does `blob_alloc` zero the buffer, making case 3 harmless?* No — `void *blob_alloc(size_t sz) {return xmalloc(sz);}` (client.cpp:6260), and `xmalloc` is a checked `malloc`, not `calloc`.
* *Would the CAS ever be inconsistent with the RCS files in practice?* The blobs are a separate store reachable over the network (`BlobURL`/`BlobEncryptedURL` proxies, `push_whole_blob`), replicated and pruned independently of the `,v` files. There is no transaction spanning both, so divergence is exactly the case this error path exists for.
* *Is `char hash[64]` (unterminated) a second bug here?* It is consumed only by `get_file_path`, which formats it with `"%.2s/%.2s/%.64s"` (ca_blobs_fs/src/content_addressed_fs.cpp:72); the precision bounds every read to at most 64 bytes, so it is safe. Compare rcs_cvt_kB.cpp:100, which does terminate its copy (`char hashZ[65]; ...; hashZ[64]=0;`) because it passes the string to a function without such a bound.
