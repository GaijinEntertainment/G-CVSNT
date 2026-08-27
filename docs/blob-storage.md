# Blob storage: content-addressed store, blob server, proxies

Paths relative to `cvsnt/cvsnt-2.5.05.3744/`.

## Reference format

A blob reference is exactly 71 bytes:

```
blake3:<64 hex chars of the BLAKE3-256 hash>
```

(`src/sha_blob_reference.h`: `HASH_TYPE_REV_STRING`, `blob_reference_size`).
Only the `:` at offset 6 is structurally required by the key-value protocol,
so other 6-letter hash names (historically `sha256:`) fit the same framing
(`keyValueServer/readme.md`). A *session* blob reference (used on commit)
appends an 8-byte crypt magic (`session_blob_reference_size`).

## On-disk store (`ca_blobs_fs/`)

* Layout: `<root>/blobs/<xx>/<yy>/<64-hex-hash>` where `xx`,`yy` are the
  first two hash bytes in hex (`content_addressed_fs.h`).
* Each blob file starts with a small header naming the packing (none / zlib /
  zstd), followed by the (possibly compressed) content
  (`ca_blobs_fs/ca_blob_format.h`, `streaming_compressors.h/.cpp`).
* Push is streaming with hash verification (`start_push`/`stream_push`/
  `finish`); trusting the client-provided hash is allowed by default (worst
  case is a garbage blob that GC removes — overwriting an existing blob is
  not possible, so history cannot be corrupted this way).
* Pull is mmap-based with random access (`start_pull`/`pull`).
* Blobs are immutable; garbage collection and repacking are offline tools
  (`tools/gc-blobs.cpp`, `tools/repack-blobs.cpp`).

## The key-value server (`keyValueServer/`)

A deliberately tiny TCP protocol (default port **2403**) with commands for
check/size/push/pull of hash-keyed blobs. Properties:

* `serverLib/` spawns a thread per connection in a loop
  (`blob_push_server.cpp`); storage behind it is 8 link-time functions
  (`blob_server_func_deps.h`) — the production implementation maps them to
  `ca_blobs_fs`, the sample one is `sample/`.
* `clientLib/` is what `cvs` links (via `src/blob_kv_processor.cpp`).
* `proxy/` (`cafs_proxy`) is a **write-through caching proxy**: reads are
  served from its local cache, writes go through to the master; the cache is
  not populated by writes. Deploy proxies close to the users (per-office),
  keep one master next to the repository.
* There is also an HTTP pull transport (`src/blob_http_processor.cpp`,
  bundled `src/httplib.h`) so blobs can be served by a plain HTTP server /
  CDN; URLs starting with `http://` select it.

## How clients learn the URLs

Order of precedence (`src/client.cpp: get_download_source`,
`src/download_blob_to.cpp: BackgroundProcessor::init`):

1. `--blob_url url` on the cvs command line (also multi-URL
   `url1|url2|...`, `host@port` syntax; `def` selects the master sent by the
   server).
2. `Blob-url` / `Blob-OTP` response lines sent by the CVS server during
   handshake, configured server-side in the `PServer` settings:
   `BlobURL`, `BlobURL0..31` (plain), `BlobEncryptedURL0..31` (requires OTP
   encryption), `BlobOTP` (shared secret for time-based OTP pages)
   (`src/server.cpp: send_blob_private_blob_urls_to_client`,
   `send_blob_otp_to_client`).

At run time the client builds a URL list: *public* proxies, then *private*
proxies, then the master (always last). Up to 8 worker threads
(`min(8, cores-1)`, overridable with `-j N`) each hold their own connection
and pick URLs round-robin with a random per-run shuffle; a failing server is
marked dead and the next one is tried, falling back to the master
(`src/download_blob_to.cpp: RoundRobin, fail()`).

## Operational notes

* Downloads happen *during* update in the background; before overwriting a
  working file the old one is deleted so an aborted update shows the file as
  modified rather than silently stale (`download_blob_to.cpp:100`).
* The OTP mechanism derives a rotating secret from `BlobOTP` + time page
  (`blob_gen_totp_secret`), so proxies can authenticate/encrypt blob traffic
  without CVS accounts.
* Migration of an existing repository: run `cvtblob`
  (`tools/convert_to_blob.cpp`) over the `,v` tree to move `-kB` revision
  data into the store and rewrite the `,v` files as references; `,v` backups
  and idempotency are handled by the tool. `rcs_cvt_kB.cpp` implements the
  in-server conversion path.
