# File size obtained via ftell() truncates/erros for >2GB blobs (esp. 32-bit long / Windows)

- **File:** cvsnt/cvsnt-2.5.05.3744/src/blob_kv_processor.cpp
- **Line(s):** 17-27
- **Severity:** low
- **Confidence:** medium
- **Category:** overflow / portability

## Code
```cpp
FILE* rf = fopen(file, "rb");
fseek(rf, 0, SEEK_END);
const size_t fsz = ftell(rf);          // ftell returns long
fseek(rf, 0, SEEK_SET);
...
BlobHeader hdr = get_header(blob_binary_compressed ? zstd_magic : noarc_magic, fsz, 0);  // stored in uint64 uncompressedLen
```

## Why this is a bug
`ftell` returns `long`. On Windows `long` is 32-bit even in 64-bit builds, and `fseek`/`ftell` are documented not to work beyond 2 GB (they return -1L). For a blob file larger than 2 GB this yields either a negative value or a wrong low-32-bit truncation, which is then widened to `size_t fsz` (becoming a huge value like `SIZE_MAX` when ftell returned -1) and written into `BlobHeader::uncompressedLen` (a `uint64_t` intended to hold the true size).

The actual payload streaming is driven by `fread` until EOF, so the transmitted bytes are still complete; the damage is a corrupted `uncompressedLen` header field for large blobs. Because this module's own comment set and the surrounding subsystem explicitly target LARGE binary files ("allows to transfer up to 4TB"), storing a wrong size for >2GB files is a latent correctness problem for any consumer that trusts `uncompressedLen` (repack bookkeeping, tools, diagnostics).

Note the file-content hashing path uses the mmap-based `blobe_fileio_get_file_size`/`_stat64` (64-bit) correctly; only this streaming-upload helper uses `ftell`.

## Suggested fix
Use a 64-bit size query: `_ftelli64`/`_fseeki64` on MSVC (or `ftello`/`fseeko` with `_FILE_OFFSET_BITS=64` on POSIX), or reuse `blob_fileio_get_file_size(file)`; also check the size for the error sentinel before use.
