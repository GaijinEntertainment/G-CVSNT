# decode_stream_blob_data miscomputes header-part size when BlobHeader is split across chunks (OOB write/read)

- **File:** cvsnt/cvsnt-2.5.05.3744/ca_blobs_fs/streaming_blobs.h
- **Line(s):** 55-71 (esp. 57-58)
- **Severity:** high
- **Confidence:** high
- **Category:** memory

## Code
```cpp
  if (info.dataRead < sizeof(BlobHeader))
  {
    size_t hdrPart = (data_length - info.dataRead);            // <-- BUG
    hdrPart = hdrPart < sizeof(BlobHeader) ? hdrPart : sizeof(BlobHeader);
    memcpy((char*)&info.hdr + info.dataRead, data, hdrPart);
    info.dataRead += hdrPart;
    ...
    data += hdrPart;
    data_length -= hdrPart;
```

## Why this is a bug
`data`/`data_length` is the *current chunk*; `info.dataRead` is the *cumulative* number of bytes consumed so far. The number of header bytes still needed is `sizeof(BlobHeader) - info.dataRead`, but the code computes `data_length - info.dataRead` (clamped to `sizeof(BlobHeader)`). This only produces the right value when the whole 16-byte header arrives inside the first chunk (`dataRead == 0`). This function is fed directly by network reads (download_blob_to.cpp:390, keyValueServer/proxy/proxy_file_lib.cpp:384, content_addressed_fs.cpp:161), where TCP can legally deliver any chunk size, including < 16 bytes.

Failure scenarios once the first chunk is shorter than 16 bytes (say 10, so `dataRead == 10`):

1. Second chunk `data_length = 20`: `hdrPart = min(20-10, 16) = 10`, but only 6 bytes are needed. `memcpy((char*)&info.hdr + 10, data, 10)` writes 4 bytes past the 16-byte `info.hdr` (into the adjacent `cctx` field), and 4 bytes of real payload are consumed as "header", so decompression starts 4 bytes into the stream and fails / produces corrupt data.
2. Second chunk `data_length = 3` (< `dataRead`): `hdrPart = 3 - 10` underflows to a huge `size_t`, clamped to 16. `memcpy(&hdr+10, data, 16)` reads 13 bytes past the 3-byte chunk and writes 10 bytes past `info.hdr`. Then `data += 16` points past the buffer and `data_length -= 16` underflows to ~2^64, after which `decompress_stream` walks off the end of the chunk — out-of-bounds read of attacker-influenced length, likely crash.
3. Second chunk `data_length == dataRead` (e.g. two 8-byte chunks): `hdrPart = 0`; no header bytes are consumed, `dataRead` is then bumped by `data_length` past `sizeof(BlobHeader)` without `init_decompress_blob_stream` ever being called, so `decompress_stream`/`finish_decompress_stream` (in the `DownloadBlobInfo` destructor, line 24-28) operate on an *uninitialized* `cctx` — undefined behavior (interpreting stack garbage as a z_stream / ZSTD_DStream pointer, then freeing it).

## Suggested fix
```cpp
size_t hdrPart = sizeof(BlobHeader) - info.dataRead;
hdrPart = hdrPart < data_length ? hdrPart : data_length;
```
(i.e. copy `min(bytes-still-needed, bytes-available-in-this-chunk)`).
