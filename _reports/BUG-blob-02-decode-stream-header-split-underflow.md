---
id: BUG-blob-02
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/ca_blobs_fs/streaming_blobs.h
line: 57
severity: critical
category: memory-safety
status: fixed in the previous slice (audit/01)
verdict: CONFIRMED
fix_size_loc: 2
behavior_change: no
---

# `decode_stream_blob_data()` computes the header remainder from the wrong operand — `size_t` underflow gives a ~2^64 length to the decompressor

## Summary
When the 16-byte `BlobHeader` spans more than one network chunk, the number of header bytes still
needed is computed as `data_length - info.dataRead` instead of `sizeof(BlobHeader) - info.dataRead`.
If the follow-up chunk is *smaller* than the number of header bytes already consumed, the
subtraction wraps around, `hdrPart` is clamped to 16 (over-reading and over-writing), and
`data_length` becomes ~2^64, which is then handed straight to `memcpy`/`ZSTD_decompressStream`.

## Code
```cpp
// ca_blobs_fs/streaming_blobs.h:52-73
inline bool decode_stream_blob_data(DownloadBlobInfo &info, const char *data, size_t data_length, ProcessUnpackedCB cb)
{
  using namespace streaming_compression;
  if (info.dataRead < sizeof(BlobHeader))
  {
    size_t hdrPart = (data_length - info.dataRead);              // <-- BUG: should be sizeof(BlobHeader) - info.dataRead
    hdrPart = hdrPart < sizeof(BlobHeader) ? hdrPart : sizeof(BlobHeader);
    memcpy((char*)&info.hdr + info.dataRead, data, hdrPart);     // <-- writes past &info.hdr
    info.dataRead += hdrPart;

    if (info.dataRead >= sizeof(BlobHeader))
    {
      if (!init_decompress_blob_stream(info.cctx, sizeof(info.cctx), info.hdr))
        return false;
    }
    data += hdrPart;
    data_length -= hdrPart;                                      // <-- wraps to ~2^64
    if (!data_length)
      return true;
  }

  info.dataRead += data_length;
  ...
```

## Why it is a bug
`info.dataRead` counts *header bytes consumed so far* while this branch is active (0..15).
`data_length` is the size of the *current* chunk. Subtracting one from the other is meaningless.
Three distinct defects follow:

1. **`size_t` underflow.** With `info.dataRead = 8` and a follow-up chunk of `data_length = 4`,
   `hdrPart = 4 - 8 = SIZE_MAX - 3`. The clamp `hdrPart < sizeof(BlobHeader)` is false, so
   `hdrPart` becomes 16.
2. **Out-of-bounds write.** `memcpy((char*)&info.hdr + 8, data, 16)` writes 16 bytes into the last
   8 bytes of the 16-byte `BlobHeader` — 8 bytes past the end of `info.hdr`, into `info.cctx`.
   It also reads 16 bytes from a chunk that only holds 4.
3. **Catastrophic length.** `data_length -= hdrPart` → `4 - 16 = SIZE_MAX - 11`. `!data_length` is
   false, so execution falls through with `data_length ≈ 1.8e19` and a `data` pointer 16 bytes into
   a 4-byte-valid region. For a `NONE` blob that goes straight to `cb(data, data_length)`; for
   `ZSTD`/`ZLIB` it goes to `decompress_stream(..., src_size = data_length, ...)`, i.e.
   `ZSTD_inBuffer{.size = ~2^64}`.

Even without underflow the arithmetic is wrong: `dataRead = 4`, `data_length = 100` gives
`hdrPart = min(96,16) = 16`, so 16 bytes are copied into `hdr[4..19]` (4 past the end) and 16
payload bytes are consumed where only 12 header bytes were outstanding — 4 bytes of the compressed
stream are silently dropped.

The chunk boundaries are entirely attacker/network controlled. `blob_common_net.h:51`
(`recv_lambda`) passes whatever the raw `recv()` returned:
```cpp
int l = recv(socket, buf, (int)std::min(sizeLeft, (int64_t)sizeof(buf)));
HANDLE_SOCKET_ERROR(l)
cb(buf, l);
```
On an unencrypted `BlobSocket`, `recv()` (`blob_sockets.cpp:141`) is the bare socket call and may
return any number of bytes.

## Failure scenario
CVS client checking out a large binary. `download_blob_to.cpp:388-397` installs
`decode_stream_blob_data(info, data, data_length, ...)` as the pull callback, and
`blob_pull_client_cmd.cpp:96` drives it from `recv_lambda`.

A CAFS server (or, given BUG-blob-01, an in-path attacker on an unencrypted session) sends the
blob body split so that the first TCP read yields 8 bytes and the second yields 4:

1. call 1: `data_length = 8`, `dataRead = 0` → `hdrPart = 8`, `hdr[0..7]` filled, `dataRead = 8`,
   `data_length` becomes 0 → returns true.
2. call 2: `data_length = 4`, `dataRead = 8` → `hdrPart = 4 - 8` wraps → clamped to 16.
   `memcpy(&info.hdr + 8, data, 16)` writes 8 bytes past `info.hdr`; `dataRead = 24`;
   `data_length = 4 - 16 = SIZE_MAX - 11`.
3. The assembled `hdr.magic` is whatever the attacker put in bytes 0..3 — choose `"NONE"`, so
   `is_noarc_blob(info.hdr)` is true and the code calls
   `cb(data, SIZE_MAX-11)` → `fwrite(data, 1, SIZE_MAX-11, tmp)` in
   `download_blob_to.cpp:392`, walking off the end of `recv_lambda`'s 64 KiB stack buffer until it
   faults. Choosing `"ZSTD"` instead hands `streamIn.size = SIZE_MAX-11` to
   `ZSTD_decompressStream` (`streaming_compressors.cpp:81`) with the same result.

The same code path is reached by the proxy (`proxy_file_lib.cpp:384`, when `should_validate_blobs`)
against data from the master, and by the server (`content_addressed_fs.cpp:161` via
`stream_push`) when it is started with `set_allow_trust(false)` (`cafs_server.cpp:46`).

## Suggested fix
```cpp
    size_t hdrPart = sizeof(BlobHeader) - info.dataRead;
    hdrPart = hdrPart < data_length ? hdrPart : data_length;
```

## Refutation attempt
I checked whether some caller guarantees the first chunk is at least `sizeof(BlobHeader)`.
It does not: `recv_lambda` forwards the raw `recv()` return value, and `PullThroughTemp::readChunk`
(`proxy_file_lib.cpp:357`) does the same. I also checked whether the encrypted path saves it —
it does, because `blob_sockets.cpp:149` uses `raw_recv_exact` and always fills the full request —
but the unencrypted path (`Local`/`Public` server encryption, and the whole
`CafsServerEncryption::Public`-on-private-network mode) does not. Finally I verified the arithmetic
in the non-underflow case is also wrong (bytes silently skipped), so the finding is not merely a
theoretical edge case. The finding stands.
