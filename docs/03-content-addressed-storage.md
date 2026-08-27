# Content-addressed storage (CAFS)

CAFS is the store that holds the bytes of every `-kB`/`-kBz` file. It is a plain
`hash → immutable byte string` map on a filesystem. It knows nothing about revisions, branches,
paths or users; the RCS `,v` files remain the only place where *history* lives.

Source: `ca_blobs_fs/` (on-disk store) and `keyValueServer/` (network layer).

## The key: BLAKE3

The key of a blob is the **BLAKE3-256 hash of its uncompressed content**, lower-case hex, 64
characters (`src/sha_blob_reference.h:9`).

BLAKE3 was chosen over SHA-256 for two stated reasons (`src/sha_blob_reference.h:6`): it is not
vulnerable to length extension, and it is several times faster — 4× in plain C, 8× with SSE2, more
with AVX-512. Hashing throughput matters because every commit and every integrity check re-hashes
whole files. The implementation is vendored in `blake3/` with SSE2/SSE4.1/AVX2/AVX-512 kernels
selected at run time.

## On-disk layout

```
<dir_for_roots>/<root>/blobs/<h0><h1>/<h2><h3>/<64-hex-hash>
```

Two levels of 256-way fan-out from the first four hex characters keep directory sizes manageable
(`ca_blobs_fs/src/content_addressed_fs.cpp:66`). Example:

```
/cvs/myrepo/blobs/3f/a9/3fa91c2e...<64 chars total>
```

`set_dir_for_roots()` sets the parent, `set_root()` appends the repository root and the fixed
`blobs` sub-folder. `set_temp_dir()` chooses where partially-written blobs live; when unset, the
temp file is created inside the *destination* directory so the final step is a same-filesystem
rename.

## Blob file format

Every blob file starts with a 16-byte header (`ca_blobs_fs/ca_blob_format.h`):

| Offset | Size | Field | Meaning |
| --- | --- | --- | --- |
| 0 | 4 | `magic` | `ZSTD`, `ZLIB`, or `NONE` |
| 4 | 2 | `headerSize` | `sizeof(BlobHeader)` |
| 6 | 2 | `flags` | bit 0 = `BEST_POSSIBLE_COMPRESSION` |
| 8 | 8 | `uncompressedLen` | size of the original content |

The payload follows, compressed according to `magic`. The *hash is always of the uncompressed
content*, so re-compressing a blob with a different algorithm does not change its key — which is
exactly what makes `repack-blobs` safe.

`BEST_POSSIBLE_COMPRESSION` is set only by the offline `repack-blobs` maintenance tool. Clients and
the server compress on the fly, favouring speed; the flag records "this one has already been
squeezed as hard as we can, don't bother again".

Compression is abstracted over zlib and zstd by `ca_blobs_fs/streaming_compressors.h`
(`StreamType::{Unpacked, ZLIB, ZSTD}`), so a store can hold a mixture and readers cope.

## The blob reference

A `-kB` revision stores, as its RCS revision text, a fixed-size reference:

```
blake3:<64 hex characters>          71 bytes
```

`src/sha_blob_reference.h` defines `HASH_TYPE_REV_STRING "blake3:"`, `hash_encoded_size = 64`,
`blob_reference_size = 71`. `is_blob_reference_data()` /
`get_blob_reference_content_hash()` (`src/blob_operations.cpp:9`) recognise and parse it.

### The session blob reference

There is a second, related form used **only inside the server**: the **session blob reference**,
79 bytes (`session_blob_reference_size = blob_reference_size + 8`). The extra 8 bytes are an FNV-1a
hash of the 71-byte reference mixed with a per-process random salt (`gen_session_crypt()`,
`src/blob_operations.cpp:47`; salt in `src/server.cpp:7217`, seeded from `rdtsc`).

It exists because the server reconstructs a scratch working directory from the client's requests.
When a client sends `Blob-ref-transfer`, the server writes a 79-byte marker file instead of real
content (`src/server.cpp:1926`). Later, on the way out, the server treats any 79-byte file that
carries a valid session MAC as "this is a reference, not content", truncates it to 71 bytes and sends
`Blob-ref` (`src/server.cpp:4429`); `rcs_checkin` does the same on the commit path
(`src/rcs_checkin.cpp:1491`).

The salt is what stops an ordinary versioned file whose content happens to be 79 bytes beginning
with `blake3:` and 64 hex digits — or a deliberately crafted one uploaded by a client — from being
mistaken for a server-side blob marker. It is a disambiguation tag scoped to one server process, not
an access-control mechanism.

## Write path (`push`)

`ca_blobs_fs/src/content_addressed_fs.cpp`, API in `ca_blobs_fs/content_addressed_fs.h`:

```
start_push(ctx, hash)  →  stream_push(pd, data, len)*  →  finish(pd, &actual_hash)
```

1. **Early dedup.** If `hash` is supplied and that file already exists, `start_push` returns a
   sentinel handle and all subsequent writes are no-ops; `finish` reports `DEDUPLICATED`. This costs
   one `stat` and no data transfer at all.
2. Otherwise a temp file is opened, ideally in the destination directory.
3. Each `stream_push` writes the *already-compressed* bytes straight through, and — unless the
   client's hash is trusted — simultaneously decompresses them into a BLAKE3 hasher so the true hash
   of the uncompressed content is known by the end.
4. `finish` compares the computed hash with the claimed one (`WRONG_HASH` on mismatch), then
   `rename`s the temp file into place with `blob_fileio_rename_file_if_nexist`. If someone else won
   the race, the temp file is unlinked and the result is `DEDUPLICATED`.

Results are `OK`, `DEDUPLICATED`, `IO_ERROR`, `WRONG_HASH`, `EMPTY_DATA`; `is_ok()` treats the first
two as success.

### The trust model

`set_allow_trust(bool)` decides whether a client-supplied hash is believed without verification. It
defaults to **on**, and the reasoning is written out at `ca_blobs_fs/content_addressed_fs.h:27`:
an existing blob is never overwritten, so a lying client cannot corrupt content that is already
stored — the worst outcome is an unreferenced junk blob that garbage collection later removes.
Trust is switched off automatically when the server runs with encryption enabled
(`keyValueServer/server/cafs_server.cpp:44`), and the network server never trusts clients on the
verification path.

## Read path (`pull`)

```
start_pull(ctx, hash, &blob_sz)  →  pull(pd, from, &data_pulled)*  →  destroy(pd)
```

Reads are memory-mapped and support random access from an arbitrary offset, which is what lets the
network layer serve a blob in 1 MB chunks (`blob_push_proto::pull_chunk_size`) and lets a client
resume.

## Immutability and its consequences

Blobs are never modified in place and never overwritten. That gives:

* **Free dedup** — the same asset committed on ten branches is one file.
* **Trivial caching** — a proxy can cache a blob forever; content can never change under a hash.
* **Cheap integrity checking** — re-hash the file, compare to the filename.
* **No reference counting** — nothing tracks how many `,v` files point at a blob. Reclaiming space
  therefore requires a *mark-and-sweep* pass, which is what `gc-blobs` does: scan every `,v` file
  in the repository for references, then delete unreferenced blobs. See
  [06-server-operations.md](06-server-operations.md).

## Related tools

| Tool | Source | Purpose |
| --- | --- | --- |
| `cvtblob` | `tools/convert_to_blob.cpp` | Rewrites existing `,v` files, moving inline binary revisions into the blob store and replacing them with references |
| `gc-blobs` | `tools/gc-blobs.cpp` | Mark-and-sweep collection of unreferenced blobs |
| `repack-blobs` | `tools/repack-blobs.cpp` | Recompresses blobs to the best available ratio and sets `BEST_POSSIBLE_COMPRESSION` |
| `blake3-calc` | `tools/blake3-calc.cpp` | Prints a file's blob key |

All four take the repository root and operate directly on the filesystem; they coordinate with the
running server through `tools/simpleLock.cpp.inc`.
