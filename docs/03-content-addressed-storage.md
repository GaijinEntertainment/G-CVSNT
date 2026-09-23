# Content-addressed storage (CAFS)

CAFS is the store that holds the bytes of every `-kB`/`-kBz` file. It is a plain
`hash → immutable byte string` map on a filesystem. It knows nothing about revisions, branches,
paths or users; the RCS `,v` files remain the only place where *history* lives.

Source: `ca_blobs_fs/` (on-disk store) and `keyValueServer/` (network layer).

## The key: BLAKE3

The key of a blob is the **BLAKE3-256 hash of its uncompressed content**, lower-case hex, 64
characters (`src/sha_blob_reference.h:10`).

BLAKE3 was chosen over SHA-256 for two stated reasons (`src/sha_blob_reference.h:7`): it is not
vulnerable to length extension, and it is several times faster — the comment claims 4× in plain C
and 8× with SSE2 alone, with more available from AVX. Hashing throughput matters because every commit and every integrity check re-hashes
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

`BEST_POSSIBLE_COMPRESSION` is set by `repack()` (`ca_blobs_fs/src/content_addressed_fs.cpp:288`).
That is called both by the offline `repack-blobs` tool *and* by `cafs_server` itself for every newly
stored blob, at lowered priority, unless the server was started with `norepack`
(`keyValueServer/server/blob_file_lib.cpp:70`, `cafs_server.cpp:9`). Clients compress on the fly,
favouring speed; the flag records "this one has already been squeezed as hard as we can, don't
bother again".

(The comment at `ca_blob_format.h:11` still says only a maintenance utility sets it. That comment is
stale relative to the code.)

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
`Blob-ref` (`src/server.cpp:4429`); on the commit path the shrink happens in
`RCS_write_binary_rev_data` (`src/rcs_cvt_kB.cpp:91`), while `rcs_checkin` only extracts the
referenced hash to compare revisions (`src/rcs_checkin.cpp:1491`).

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
   one `stat` and no disk I/O — but note the server still reads the whole blob off the socket first
   (`keyValueServer/serverLib/blob_push_proc.cpp:116`). Avoiding the *network* transfer is a
   separate, client-side optimisation: the client issues a `SIZE` query and skips the push
   altogether (`src/blob_kv_processor.cpp`, `upload()`).
2. Otherwise a temp file is opened, ideally in the destination directory.
3. Each `stream_push` writes the *already-compressed* bytes straight through, and — unless the
   client's hash is trusted — simultaneously decompresses them into a BLAKE3 hasher so the true hash
   of the uncompressed content is known by the end.
4. `finish` compares the computed hash with the claimed one (`WRONG_HASH` on mismatch) — but only
   when trust is off or no hash was provided; with default trust and a client hash the claim is
   copied unverified (`content_addressed_fs.cpp:203-211`, see the trust model below). Then it
   `rename`s the temp file into place with `blob_fileio_rename_file_if_nexist`. If someone else won
   the race, the temp file is unlinked and the result is `DEDUPLICATED`.

Results are `OK`, `DEDUPLICATED`, `IO_ERROR`, `WRONG_HASH`, `EMPTY_DATA`; `is_ok()` treats the first
two as success.

### The trust model

`set_allow_trust(bool)` decides whether a client-supplied hash is believed without verification. It
defaults to **on**. The reasoning written out at `ca_blobs_fs/content_addressed_fs.h:27` — an
existing blob is never overwritten, so a lying client cannot corrupt content that is already
stored — undersells the risk: with trust on the data is stored under the client-supplied hash
unverified (`content_addressed_fs.cpp:158,203-211`) and the first writer wins. A client that
pushes junk under a real BLAKE3 key binds that key for good — later honest pushes of the true
content return `DEDUPLICATED` and never overwrite, `gc-blobs` keeps the referenced poison, and
every later download of it fails client-side validation. Working copies are not silently
corrupted, but that store key is dead.
Enabling encryption switches trust off automatically — but only when `allow_trust` was `on`: the
clear sits inside `if (allow)` (`keyValueServer/server/cafs_server.cpp:45-46`), so with
`allow_trust off` (itself a no-op, see below) the never-applied default stays trusting.

The header comment at `ca_blobs_fs/content_addressed_fs.h:41` claims the networking server never
trusts client hashes. **The code does not implement that.**
`keyValueServer/server/blob_file_lib.cpp:18` passes the client-supplied hash straight into
`start_push`, so with the default `allow_trust` an unencrypted `cafs_server` does accept it
unverified. Worse, `cafs_server`'s own `allow_trust(on|off)` argument is parsed but never applied —
see `_reports/BUG-blob-07-cafs-server-allow-trust-off-ignored.md`.

## Read path (`pull`)

```
start_pull(ctx, hash, &blob_sz)  →  pull(pd, from, &data_pulled)*  →  destroy(pd)
```

Reads are memory-mapped. The API shape allows random access, but the implementation returns data
from the start of the mapping no matter what `from` says (`ca_blobs_fs/src/fileio.cpp:305-313`),
so a `PULL` resumed at a nonzero megabyte boundary yields wrong bytes — whole-blob pulls are the
only correct form today (see [04-protocols.md](04-protocols.md)). The body of a `PULL` is streamed
in one run, not in chunks.

## Immutability and its consequences

A blob's *uncompressed content* under a given hash never changes as long as hashes are verified.
The push path's no-overwrite guard is check-then-rename (`blob_fileio_rename_file_if_nexist`,
`ca_blobs_fs/src/fileio.h:19-27`, used at `content_addressed_fs.cpp:219`), and a POSIX `rename`
replaces its target: two concurrent pushers of one hash can both pass the check, and the later
rename silently replaces the earlier file. With verified hashes both hold the same content and the
race is harmless; with `allow_trust` on it is one more way a wrong payload can land under a real
key. The stored *bytes* can still be replaced by a
repack, which rewrites the same path with a better-compressed payload using the unconditional
rename (`ca_blobs_fs/src/content_addressed_fs.cpp:354`); the proxy accounts for this
(`keyValueServer/proxy/proxy_file_lib.cpp:198`).

Content stability gives:

* **Free dedup** — the same asset committed on ten branches is one file.
* **Trivial caching** — a proxy can cache a blob forever; content can never change under a hash.
* **Integrity checking** — decompress the payload, re-hash it, compare to the filename
  (`ca_blobs_fs/src/content_addressed_fs.cpp:161`). Note that you cannot hash the blob file's raw
  bytes: the hash is of the *uncompressed* content, and the file is header-plus-compressed-payload.
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

`cvtblob` and `gc-blobs` take the repository root and coordinate with a running server through the
lock server (`tools/simpleLock.cpp.inc`). `repack-blobs` takes the root but takes **no** lock.
`blake3-calc` takes a single filename — not a repository — and touches neither the repository nor
the lock server.
