# Gaijin modifications vs. stock CVSNT 2.5.05

The fork identifies itself as `CVSNT 3.5.x (Gan + [Gaijin -kB/-kBz patch])`
(`version_no.h`, `version_fu.h`). This page lists what was changed/added on
top of the March Hare CVSNT 2.5.05.3744 code base. Paths relative to
`cvsnt/cvsnt-2.5.05.3744/`.

## 1. Content-addressed blob storage for binary files (the big one)

For files committed with `-kB` (binary, no keyword expansion, binary deltas)
the RCS `,v` files no longer contain revision data. Instead every revision's
"text" is a fixed-size reference:

```
blake3:<64 hex characters of the BLAKE3-256 hash of the content>
```

(`src/sha_blob_reference.h`; deltas between revisions degenerate to
`d1 1\na1 1\nblake3:<hash>` — replace the single "line", see
`src/rcs_checkin.cpp`). The actual content lives in a content-addressed store
(`ca_blobs_fs/`), where each blob is stored (zlib- or zstd-compressed, with a
small header) under `blobs/<xx>/<yy>/<64-hex-hash>` — `xx`/`yy` being the
first two bytes of the hash. Consequences:

* `,v` files of huge binary assets are a few hundred bytes; server-side
  operations that parse or rewrite `,v` (tag, branch, status) no longer read
  gigabytes.
* Identical content is stored once, repository-wide (dedup), and can be
  garbage-collected / repacked offline (`tools/gc-blobs.cpp`,
  `tools/repack-blobs.cpp`).
* Content transfer is offloaded from the CVS protocol connection to the blob
  server (parallel, resumable-by-retry, cacheable by proxies).
* `blobs de-facto immutable` — overwriting an existing blob is not allowed.

Components:

* `ca_blobs_fs/` — the store library (push/pull streaming, mmap pulls,
  streaming zlib/zstd compressors, BLAKE3 hashing via `blake3/`)
* `keyValueServer/` — a small TCP key-value protocol (default port **2403**)
  with `serverLib` (thread per connection), `clientLib` (used by cvs),
  `blob_sockets`, a **caching proxy** (`proxy/cafs_proxy.vcxproj`,
  write-through cache) and sample server implementations
* `src/blob_operations.cpp`, `src/download_blob_to.cpp` — cvs-side plumbing:
  a background pool of up to 8 threads pushes/pulls blobs while the main
  protocol loop keeps running; URLs are tried round-robin (public proxies →
  private proxies → master)
* `src/blob_kv_processor.cpp` (native kv protocol) and
  `src/blob_http_processor.cpp` (HTTP pull via `src/httplib.h`) — two
  transports behind one `BlobNetworkProcessor` interface
* `src/rcs_cvt_kB.cpp`, `tools/convert_to_blob.cpp` (`cvtblob`) — migration
  of existing `,v` files to blob references

### Protocol extensions (all negotiated, old clients keep working)

* Server→client responses: `Blob-ref` (send a reference instead of file
  content; client queues a background download), `Blob-ref-created`,
  `Blob-url` (tell the client where the blob servers/proxies are, from the
  server's `PServer/BlobURL`, `BlobURL0..31` settings), `Blob-OTP`
  (time-based one-time secret for blob-server authentication/encryption,
  `BlobEncryptedURL0..31`) — see `src/server.cpp`
  (`send_blob_url_to_client`, `send_blob_otp_to_client`) and the response
  table in `src/client.cpp`.
* Client→server: `Blob-ref-transfer` / `Blob-transfer` valid-requests; on
  commit the client uploads the blob to the blob server and sends only the
  reference to the CVS server (`src/client.cpp: send_modified`).
* Client options: `--blob_url url[|url2|...]` override, `-j N` download
  concurrency (`blob_concurrency_download_level`).

## 2. `-kB` / `-kBz` commit and update flow

The `ver` banner's "`-kB/-kBz patch`" refers to this flow (flags parsed in
`src/rcs.cpp: RCS_get_kflags` — `B` = binary with binary deltas, `z` =
compress):

* **commit** (`src/client.cpp: send_modified/send_blob_files`): when the
  server advertises `Blob-ref-transfer`/`Blob-transfer`, the client does NOT
  send the binary content over the CVS connection. It hashes the file,
  uploads the blob to the blob server through the background pool
  (compressed when the file has the `z` modifier, see `client.cpp:5889`),
  and sends only the session blob reference to the CVS server.
* **update/checkout**: the server answers with `Blob-ref` instead of file
  data; the client registers the new revision in `CVS/Entries` and queues a
  background download of the blob (`src/client.cpp:
  update_blob_ref_entries`, `src/download_blob_to.cpp`).
* Modified-file detection for binary files stays timestamp-based via
  `CVS/Entries` (plus CVSNT's `Entries.Extra` per-file metadata inherited
  from upstream: edit baseline MD5, RCS timestamp, merge tags).

## 3. Modern compression and hashing everywhere

* **zstd** (vendored `zstd/`, used for wire compression as an alternative to
  zlib — `src/zstd_buffer.cpp` — and for blob packing in `ca_blobs_fs`)
* **BLAKE3** (vendored `blake3/` with SSE2/SSE4.1/AVX2/AVX-512 asm) — content
  hashing; `src/sha256/` retained for the older sha256-based blob format
  (`sha256:` prefix still parses, see `keyValueServer/readme.md`)

## 4. Miscellaneous fork changes

* `src/RecurseRepository.cpp` — server-side repository recursion helper
  (faster directory walking for module expansion)
* `src/concurrent_queue.h` — the work queue used by the blob thread pool
* `-F file` global option — read command arguments from a file (long file
  lists exceed OS command-line limits in gamedev-sized checkouts)
* Windows x64 build modernized (VS2019/v142 solution, `build-windows.py`
  added by this repo), macOS packaging in `osx/` (`build-macosx`,
  x86_64+arm64), Linux server build script (`build-linux-server`)
* `mkmodules`/trigger execution order fix: premodule triggers run before the
  update operation (see git history: `c82d510`)
* Various hardening/large-file fixes throughout `src/` (the fork exists to
  survive checkouts with 10^5–10^6 files and multi-GB binaries)

## 5. Repository administration additions

Server-side settings live in the `PServer` configuration section (registry
`HKLM\SOFTWARE\CVS\PServer` on Windows, `/etc/cvsnt/PServer` on Unix):

| Setting | Meaning |
|---------|---------|
| `BlobURL`, `BlobURL0`..`BlobURL31` | blob server / proxy URLs handed to clients (`Blob-url` response) |
| `BlobEncryptedURL0`..`31` | URLs that demand OTP-encrypted blob traffic |
| `BlobOTP` | secret used to derive time-based OTP pages for blob auth |

Offline maintenance tools (`tools/`, built by `tools/build_tools`):
`cvtblob` (convert an RCS repo to blob references), `gc-blobs` (find/remove
unreferenced blobs), `repack-blobs` (recompress), `blake3-calc`/`sha256_calc`,
`simplelock`/`unlock` (lock DB maintenance).
