# pull_at_once ignores decode errors and returns header-claimed length, exposing uninitialized tail

- **File:** cvsnt/cvsnt-2.5.05.3744/src/rcs_cvt_kB.cpp
- **Line(s):** 23-57 (esp. 42-52, 55)
- **Severity:** low
- **Confidence:** medium
- **Category:** error-handling / memory

## Code
```cpp
char *dest = nullptr;
while (at < blob_sz)
{
    ...
    if (!decode_stream_blob_data(info, some, data_pulled, [&](const void *unpacked, size_t sz)
      {
        if (!dest)
          dest = (char*)blob_alloc(info.hdr.uncompressedLen);   // sized from header
        if (info.realUncompressedSize + sz > info.hdr.uncompressedLen)
          return false;
        memcpy(dest+info.realUncompressedSize, unpacked, sz);
        return true;
      }
    ))
      break;                          // decode failure just breaks
}
*decoded = dest;
sz = info.hdr.uncompressedLen;        // returns CLAIMED size, not bytes actually decoded
return destroy(pd);                   // returns true regardless of decode success
```

## Why this is a bug
`dest` is allocated to `info.hdr.uncompressedLen` (uninitialized `xmalloc`), the loop fills only `info.realUncompressedSize` bytes, and the function then reports `sz = info.hdr.uncompressedLen` as the output length while returning success (`destroy(pd)` is a bool unrelated to decode success). Two consequences:

1. If `decode_stream_blob_data` fails part-way (decompression/format error, truncated blob) the `break` is silent — `pull_at_once` still returns `true` with a partially-filled buffer. The caller `RCS_read_binary_rev_data` (line 73-76) then sets `*inout_data_allocated = 1` and returns success, so corrupt/short data is treated as valid.
2. Whenever `realUncompressedSize < uncompressedLen` (short or failed decode, or a blob whose header over-states the length), the caller consumes bytes `[realUncompressedSize, uncompressedLen)` of `dest` which were never written — an uninitialized-heap read; on the old-client compatibility path this content is handed back to the client (potential heap-content disclosure).

In normal operation the server writes correct headers (`get_header` uses the true size) so `uncompressedLen == realUncompressedSize`; the defect surfaces on any corrupt/crafted stored blob or mid-stream decode failure, which this "compatibility with old clients" path does not otherwise validate here.

## Suggested fix
Track and honor the actual decoded size, and propagate failure:
```cpp
bool ok = true;
... if (!decode_stream_blob_data(...)) { ok = false; break; }
*decoded = dest;
sz = info.realUncompressedSize;      // actual bytes produced
bool d = destroy(pd);
return ok && d && info.realUncompressedSize == info.hdr.uncompressedLen;
```
