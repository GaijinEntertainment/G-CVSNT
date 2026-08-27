# Protocols

G-CVSNT speaks two protocols on two separate connections:

1. The **CVS client/server protocol** — inherited from CVS/CVSNT, extended with a handful of
   requests and responses for blobs and zstd.
2. The **`blob_push` protocol** — new in this fork; a small binary protocol that moves blob bytes.

## 1. The CVS client/server protocol

Line-oriented and text-based. The client sends *requests*; the server replies with *responses*
terminated by `ok` or `error`. Both sides negotiate capabilities up front: the client asks
`valid-requests`, the server answers with the list it supports; the client announces
`Valid-responses` with the list *it* supports. Anything not in the other side's list is simply not
used, which is how new features stay backward compatible.

* Request table: `src/server.cpp:4908` (`struct request requests[]`)
* Response table: `src/client.cpp:4022` (`struct response responses[]`)
* Reference for the base protocol: `doc/cvsclient.dbk`

### Typical `update` exchange

```
C: Root /cvs
C: Valid-responses ok error Updated Blob-ref Blob-ref-created ...
C: valid-requests
S: Valid-requests Root Directory Entry Modified Blob-transfer ...
S: ok
C: UseUnchanged
C: Argument -d
C: Directory develop/assets
C: /cvs/game/develop/assets
C: Entry /tex.dds/1.14/Wed Jun 4 10:00:00 2025//
C: Unchanged tex.dds
C: update
S: Blob-ref develop/assets/
S: /cvs/game/develop/assets/tex.dds
S: /tex.dds/1.15///
S: u=rw,g=rw,o=r
S: blake3:3fa91c2e...
S: ok
```

The client then fetches `3fa91c2e...` over the blob connection and writes `tex.dds`.

### Requests added by this fork

| Request | Handler | Purpose |
| --- | --- | --- |
| `Blob-transfer` | `serve_blob` | Client uploads file content that the server should turn into a blob |
| `Blob-ref-transfer` | `serve_blob_ref` | Client has already pushed the blob itself and sends only the reference |
| `Binary-transfer` | `serve_binary_transfer` | Binary file transfer that bypasses text/codepage translation |
| `Zstd-stream` | `serve_zstd_stream` | Switch the whole connection to zstd framing instead of gzip |

`Blob-transfer` is marked `GAIJIN_RQ_ESSENTIAL`, and `Blob-ref` is marked `GAIJIN_rs_essential`.
Both are aliases for the ordinary `RQ_ESSENTIAL`/`rs_essential`, with a comment saying to redefine
them to `0`/`rs_optional` if you need to interoperate with a pre-blob peer
(`src/server.cpp:4907`, `src/client.cpp:4020`). As shipped, therefore, **this build does not talk to
an old client or an old server** — the blob extensions are mandatory.

`Blob-ref-transfer` is the fast path: the client pushes the blob directly to the CAFS server, then
tells the CVS server only the 71-byte reference, so the payload never crosses the CVS connection at
all.

### Responses added by this fork

| Response | Handler | Purpose |
| --- | --- | --- |
| `Blob-ref` | `handle_updated_blobs_refs` | "This file's content is blob *H*" — replaces `Updated` |
| `Blob-ref-created` | `handle_created_blobs_refs` | Same, for a file being created (replaces `Created`) |
| `Blob-url` | `handle_blob_url` | Where to fetch blobs from; may be repeated to give several proxies |
| `Blob-OTP` | `handle_blob_otp` | Time-based one-time secret plus page number, for authenticating to an encrypted blob server |

`Blob-ref` carries the same envelope as `Updated` (directory, repository path, entry line, mode) but
its "content" is the 71-byte reference rather than the file (`src/server.cpp:4509`).

`Blob-url` values come from the server's own configuration: `cvsnt/PServer/BlobURL` and
`BlobURL0`...`BlobURL31` (`src/server.cpp:3346`). Later values *override* earlier ones, and the
client can round-robin between them. When an OTP is configured,
`BlobEncryptedURL0`...`BlobEncryptedURL31` are sent instead (`src/server.cpp:3379`).

`Blob-OTP` sends a TOTP secret derived from the server-side shared secret `cvsnt/PServer/BlobOTP`
plus the current page number, both hex-encoded (`src/server.cpp:3380`). The client uses it to
authenticate to the CAFS server without ever seeing the shared secret.

### Client-side override

`cvs --blob_url <url>` (`src/main.cpp:761`) overrides whatever the server advertises. The value is a
`|`-separated list, each entry `host[/path][@port]`, with `def` meaning "the master". Examples:

```
cvs --blob_url http://cvs-proxy.lan@8080 up
cvs --blob_url "localhost@2403|cvs-master.lan@2403" up
```

Two transport back-ends implement `BlobNetworkProcessor` (`src/blob_network_processor.h`):

* `src/blob_kv_processor.cpp` — the native `blob_push` TCP protocol (`host@port`)
* `src/blob_http_processor.cpp` — plain HTTP GET/PUT (`http://...`), using the vendored
  `src/httplib.h`; useful when a CDN or ordinary web cache sits in front of the store

## 2. The `blob_push` protocol

Specified in `keyValueServer/include/blob_push_protocol.h`. Binary, request/response, over one TCP
connection; little-endian lengths.

### Handshake

The server greets with `BLOBPUSH_SERVER_V` plus a three-character version (20 bytes total). The
client replies `VERS` plus its own three-character version, then one of two negotiations:

* **Authenticated (current)** — client sends an 8-byte OTP page number, then 8 random bytes and its
  Diffie-Hellman parameters, all encrypted with the OTP. The server answers with its own 8 random
  bytes and DH parameters. Both derive the same session keys from the two nonces, the OTP page and
  the DH exchange. The client then sends its 64-bit timestamp plus 64 padding bits, encrypted with
  the session keys ("Client Ready"); the server answers `HAVE` (encryption stays on), `NONE`
  (continue in clear), `ERIO` (bad timestamp) or `EBRD` (bad version). The root is always sent
  encrypted.
* **Prototype (legacy)** — the root is sent immediately; the server answers `NONE` or `EBRD`.

Encryption is all-or-nothing for the rest of the connection.

### Commands

All commands are 4 ASCII bytes. Hashes travel as 32 raw bytes (`bin_hash_len`), prefixed by the
`blake3:` tag, for `hash_len = 38` bytes total.

| Command | Payload | Responses |
| --- | --- | --- |
| `VERS` | 3-byte version, then the handshake above | `HAVE` / `NONE` / `ERIO` / `EBRD` |
| `SIZE` | hash | `SIZE` plus 8-byte size, `NONE`, `ERxx` |
| `CHCK` | hash | `HAVE`, `NONE`, `ERxx` |
| `PUSH` | hash plus 8-byte size, then that many bytes of prepared blob | `HAVE`, `ERxx` |
| `STRM` | hash, then chunks until a zero-length chunk | `HAVE`, `ERxx` |
| `PULL` | hash plus 8-byte size plus 4-byte start offset | `TAKE` plus echo of the request, then the bytes; or `NONE` / `ERxx` |

Notes:

* `PULL` with size 0 and offset 0 means "the whole blob". The offset is counted in units of
  `1 << 20`, so transfers are naturally chunked at 1 MB (`pull_chunk_size`) and a client can resume
  from an arbitrary megabyte boundary.
* `STRM` chunks are `uint16_t` length plus that many bytes; a zero length ends the stream. This is
  the path used when the client does not know the final compressed size in advance.
* What travels for `PUSH`/`STRM` is the *prepared blob* — header plus compressed payload, exactly as
  it will land on disk — not the raw file. The hash, however, is of the uncompressed content.
* Any response beginning with `ER` is an error (`is_error_response`).

### Why a separate protocol

* Blob bytes never occupy the CVS connection, so metadata stays responsive on a slow link.
* Content addressing makes the service cacheable by a proxy that understands nothing about CVS
  (`keyValueServer/proxy/`).
* Downloads can be parallelised across worker threads (`cvs -j N`) and across several URLs.
* `CHCK` before `PUSH` turns a re-commit of unchanged content into two small round trips.

## 3. Stream compression

The CVS connection itself can be compressed:

| Request | Meaning |
| --- | --- |
| `Gzip-stream <level>` | Classic CVS zlib framing (`src/zlib.cpp`) |
| `Zstd-stream <level>` | zstd framing, added by this fork (`src/zstd_buffer.cpp`) |

Selected with the global `-z <0-9>` option. zstd is preferred when both ends support it: it
compresses metadata-heavy streams faster at comparable ratios. Blob payloads are already compressed
inside the blob format, so this affects mainly protocol chatter and text files.
