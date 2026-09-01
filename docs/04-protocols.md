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
C: Valid-responses ok error Updated Blob-ref Blob-ref-created ...
C: valid-requests
S: Valid-requests Root Directory Entry Modified Blob-transfer ...
S: ok
C: Root /cvs
C: UseUnchanged
C: Argument -d
C: Directory develop/assets
C: /cvs/game/develop/assets
C: Entry /tex.dds/1.14/Wed Jun  4 10:00:00 2025/-kB/
C: Unchanged tex.dds
C: update
S: Blob-ref develop/assets/
S: /cvs/game/develop/assets/tex.dds
S: /tex.dds/1.15//-kB/
S: u=rw,g=rw,o=r
S: 71
S: blake3:3fa91c2e...
S: ok
```

Note the ordering: the client sends `Valid-responses` and `valid-requests` and reads the server's
capability list *before* it sends `Root` (`src/client.cpp:4646`, `:4659`, `:4682`). `Root` is never
the first request.

The client then fetches `3fa91c2e...` over the blob connection and writes `tex.dds`.

### Requests added by this fork

| Request | Handler | Purpose |
| --- | --- | --- |
| `Blob-transfer` | `serve_blob` | Client uploads a *prepared blob* — header plus framed payload — keyed by its content hash; the server streams it into the CAFS as-is (`src/client.cpp:5775`, `src/server.cpp:1836`) |
| `Blob-ref-transfer` | `serve_blob_ref` | Client has already pushed the blob itself and sends only the reference |
| `Binary-transfer` | `serve_binary_transfer` | Binary file transfer that bypasses text/codepage translation |
| `Zstd-stream` | `serve_zstd_stream` | Switch the whole connection to zstd framing instead of gzip |

`Blob-transfer` is marked `GAIJIN_RQ_ESSENTIAL`, and `Blob-ref` is marked `GAIJIN_rs_essential`.
Both are aliases for the ordinary `RQ_ESSENTIAL`/`rs_essential`, with a comment saying to redefine
them to `0`/`rs_optional` if you need to interoperate with a pre-blob peer
(`src/server.cpp:4907`, `src/client.cpp:4020`). As shipped, therefore, **this build does not talk to
an old client or an old server** — the blob extensions are mandatory.

`Blob-ref-transfer` is the fast path: the client pushes the blob directly to the CAFS server, then
tells the CVS server only the 71-byte reference — so the payload does not cross the CVS connection,
*provided* the background pre-upload already placed it in the store. If it did not, the client falls
back to `Blob-transfer` with the full payload (`src/client.cpp:5864`). `update` always takes the
reference path (`src/update.cpp:410`); on commit the fallback decision sits in `send_blob_file`
(`src/client.cpp:5864`).

### Responses added by this fork

| Response | Handler | Purpose |
| --- | --- | --- |
| `Blob-ref` | `handle_updated_blobs_refs` | "This file's content is blob *H*" — replaces `Updated` |
| `Blob-ref-created` | `handle_created_blobs_refs` | Same, for a file being created (replaces `Created`) |
| `Blob-url` | `handle_blob_url` | Where to fetch blobs from; may be repeated to give several proxies |
| `Blob-OTP` | `handle_blob_otp` | Time-based one-time secret plus page number, for authenticating to an encrypted blob server |

`Blob-ref` carries the same envelope as `Updated` — directory, repository path, entry line, mode,
then a decimal byte count — but its "content" is the 71-byte reference rather than the file
(`src/server.cpp:4509`, byte count at `src/server.cpp:4565`).

`Blob-url` values come from the server's own configuration: `cvsnt/PServer/BlobURL` and
`BlobURL0`...`BlobURL31` (`src/server.cpp:3346`). The client appends every value to a list and
round-robins between them — nothing is discarded
(`add_blobs_url`, `src/client.cpp:2138`; `get_round_robin_blob_url`, `src/client.cpp:2183`). The
"overwrite" wording in the server-side comments is stale. When an OTP is configured,
`BlobEncryptedURL0`...`BlobEncryptedURL31` are sent *as well*, after the `Blob-OTP` line
(`src/server.cpp:3406`); the client files those separately as encrypting URLs.

`Blob-OTP` sends a TOTP secret derived from the server-side shared secret `cvsnt/PServer/BlobOTP`
plus the current page number, both hex-encoded (`src/server.cpp:3380`). The client uses it to
authenticate to the CAFS server without ever seeing the shared secret. The response rides the
ordinary CVS stream (`buf_to_net`, `src/server.cpp:3402-3405`), so over a cleartext method such as
`:pserver:` a network observer can read the OTP page and present it to the blob server for the
page window — a `BlobOTP` deployment needs an encrypted CVS transport (`-x`, `sserver`, ssh) or an
equivalently private path for that response.

### Client-side override

`cvs --blob_url <url>` (`src/main.cpp:761`) overrides whatever the server advertises:

```
cvs --blob_url cvs-proxy.lan@2403 up
```

The help text at `src/main.cpp:349` advertises a `|`-separated list with `def` meaning "the master".
**Neither is implemented.** `parse_url_port` (`src/client.cpp:2123`) truncates at the first `@` and
`atoi()`s the rest, so everything after the first entry is silently discarded. No code anywhere
compares against `def`. The parsed URL replaces only the advertised download list: the master URL
is still appended after it (`src/download_blob_to.cpp:258,283`), so downloads fall back to the
master when the override fails, and uploads ignore the override entirely — an upload client always
talks to the master (`getNext` with `id < 0` returns the last URL,
`src/download_blob_to.cpp:216-222`).

Two transport back-ends implement `BlobNetworkProcessor` (`src/blob_network_processor.h`):

* `src/blob_kv_processor.cpp` — the native `blob_push` TCP protocol (`host@port`)
* `src/blob_http_processor.cpp` — HTTP GET only, using the vendored `src/httplib.h`. Uploading is a
  stub (`canUpload()` returns `false`, `src/blob_http_processor.cpp:8`) and the back-end is never
  selected: `BackgroundProcessor::init()` constructs only `get_kv_processor`
  (`src/download_blob_to.cpp:292`). The file compiles but is currently unreachable.

## 2. The `blob_push` protocol

Specified in `keyValueServer/include/blob_push_protocol.h`. Binary, request/response, over one TCP
connection; little-endian lengths.

### Handshake

The server greets with `BLOBPUSH_SERVER_V` plus a three-character version (20 bytes total). The
client replies `VERS` plus its own three-character version, then one of two negotiations:

* **Authenticated (current)** — client sends its 8-byte OTP page number in the clear, then 8 random
  bytes and its Diffie-Hellman parameters encrypted with the OTP (`blob_push_protocol.h:17`). The server answers with its own 8 random
  bytes and DH parameters. Both derive the same session keys from the two nonces, the OTP page and
  the DH exchange. The client then sends its 64-bit timestamp plus 64 padding bits, encrypted with
  the session keys ("Client Ready"). The server echoes 64 bits of ones, which the client verifies —
  an explicit server-authentication / MITM check (`keyValueServer/clientLib/blob_push_pull_client.cpp:192`,
  server side `keyValueServer/serverLib/blob_push_proc.cpp:313`). The client then sends the root, and
  only afterwards reads the 4-byte verdict: `HAVE` (encryption stays on), `NONE` (continue in clear),
  `ERIO` (bad timestamp) or `ERBD` (bad version). The two are pipelined — the server writes its
  verdict before reading the root (`blob_push_proc.cpp:361`). The root is always sent encrypted.
* **Prototype (legacy)** — the root is sent immediately; the server answers `NONE` or `ERBD`.

(The header comment spells the bad-version code `EBRD`; the wire constant is `ERBD`,
`keyValueServer/include/blob_push_protocol.h:73`.)

Encryption is all-or-nothing for the rest of the connection.

### Commands

All commands are 4 ASCII bytes. Hashes travel as a **6-byte** type tag — `blake3`, with the trailing
`:` of `HASH_TYPE_REV_STRING` dropped on the wire (`keyValueServer/include/blob_hash_util.h:87`) —
followed by 32 raw bytes (`bin_hash_len`), for `hash_len = 38` bytes total.

| Command | Payload | Responses |
| --- | --- | --- |
| `VERS` | 3-byte version, then the handshake above | `HAVE` / `NONE` / `ERIO` / `ERBD` |
| `SIZE` | hash | `SIZE` plus 8-byte size, `NONE`, `ERxx` |
| `CHCK` | hash | `HAVE`, `NONE`, `ERxx` |
| `PUSH` | hash plus 8-byte size, then that many bytes of prepared blob | `HAVE`, `ERxx` |
| `STRM` | hash, then chunks until a zero-length chunk | `HAVE`, `ERxx` |
| `PULL` | hash plus 8-byte size plus 4-byte start offset | `TAKE` with hash, 8-byte body length, 4-byte offset, then the bytes; or `NONE` / `ERxx` |

Notes:

* `PULL` with size 0 and offset 0 means "the whole blob" — and that is the only shape that works
  today. The `TAKE` size field is not an echo of the request: the server writes the byte count it
  intends to send (`sizeLeft`, `blob_push_proc.cpp:201-208`). Two implementation defects break the
  general form: a nonzero size is never clamped in the send loop — `blobe_fileio_pull` hands back
  the whole remaining mapping and the server streams all of it (`blob_push_proc.cpp:217-229`),
  misframing the connection; and a nonzero offset (counted in `1 << 20` units,
  `pull_chunk_size`) returns bytes from the *start* of the blob — `ca_blobs_fs/src/fileio.cpp:313`
  ignores `from` in the returned pointer — so a resume reconstructs invalid content. The body is
  not chunked: after the `TAKE` header the server streams in one run.
* `STRM` chunks are `uint16_t` length plus that many bytes; a zero length ends the stream, and
  `0xFFFF` aborts it (the server replies `ERIO`,
  `keyValueServer/serverLib/blob_push_proc.cpp:151`), so the largest data chunk is 65534 bytes. This
  is the path used when the client does not know the final compressed size in advance.
* What travels for `PUSH`/`STRM` is the *prepared blob* — header plus compressed payload, exactly as
  it will land on disk — not the raw file. The hash, however, is of the uncompressed content.
* Any response beginning with `ER` is an error (`is_error_response`).

### Why a separate protocol

* Blob bytes stay off the CVS connection (apart from the commit `Blob-transfer` fallback above),
  so metadata stays responsive on a slow link.
* Content addressing makes the service cacheable by a proxy that understands nothing about CVS
  (`keyValueServer/proxy/`).
* Downloads can be parallelised across worker threads (`cvs -j N`) and across several URLs.
* `SIZE` before `STRM` turns a re-commit of unchanged content into two small round trips
  (`src/blob_kv_processor.cpp:144`). Note that the CVS client uses `SIZE`/`STRM`, not `CHCK`/`PUSH`:
  `CHCK` is used by the proxy (`keyValueServer/proxy/proxy_file_lib.cpp:216`) and `PUSH` only by the
  sample clients.

## 3. Stream compression

The CVS connection itself can be compressed:

| Request | Meaning |
| --- | --- |
| `Gzip-stream <level>` | Classic CVS zlib framing (`src/zlib.cpp`) |
| `Zstd-stream <level>` | zstd framing, added by this fork (`src/zstd_buffer.cpp`) |

Selected with the global `-z <0-9>` option. zstd is preferred when both ends support it: it
compresses metadata-heavy streams faster at comparable ratios. Blob payloads are already compressed
inside the blob format, so this affects mainly protocol chatter and text files.
