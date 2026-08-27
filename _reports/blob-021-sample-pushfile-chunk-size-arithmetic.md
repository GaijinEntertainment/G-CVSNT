# Sample cafs_client PUSHFILE computes wrong chunk size (fsz - at - hdrSize), breaking file push

- **File:** cvsnt/cvsnt-2.5.05.3744/keyValueServer/sample/cafs_client.cpp
- **Line(s):** 131-138
- **Severity:** low
- **Confidence:** high
- **Category:** logic

## Code
```cpp
const size_t blob_sz = fsz + hdrSize;
...
int64_t pushed = blob_push_to_server(client, blob_sz, ht, hash, [&](uint64_t at, uint64_t &data_pulled) {
    if (at < hdrSize)
    {
      data_pulled = hdrSize-at;
      return ((const char*)&hdr) + at;
    }
    data_pulled = fsz - at - hdrSize;             // <-- wrong
    return ((const char*)data) + (at-hdrSize);
});
```

## Why this is a bug
The producer callback must, for a stream offset `at` into the `blob_sz = fsz + hdrSize` byte stream, return the number of contiguous bytes available from `at`. For `at >= hdrSize` that count is `blob_sz - at = fsz + hdrSize - at = fsz - (at - hdrSize)`. The code instead computes `fsz - at - hdrSize`, which is smaller by `2*hdrSize`.

Walking the push: after the header (`at = hdrSize`), it reports `data_pulled = fsz - 2*hdrSize` instead of `fsz`, so the file body is chopped short and the offsets desync; the total bytes fed to `blob_push_to_server` never reach `blob_sz`. The server-side hash of the (truncated/misframed) data will not match the declared hash, so `pushfile` is rejected.

Note the `pushblob` path uses `hdrSize == 0`, for which `fsz - at - 0 == fsz - at` is correct — so only `pushfile` is broken, which is why the error is easy to miss. This is demonstration/sample code (built by build_client_cafs.cmd), so production impact is limited, but it is a genuine off-by arithmetic defect and a misleading example.

## Suggested fix
```cpp
data_pulled = fsz - (at - hdrSize);   // == blob_sz - at
return ((const char*)data) + (at - hdrSize);
```
