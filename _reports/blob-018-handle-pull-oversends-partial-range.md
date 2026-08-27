# handle_pull sends the whole remaining blob for a partial range request (protocol desync / over-send)

- **File:** cvsnt/cvsnt-2.5.05.3744/keyValueServer/serverLib/blob_push_proc.cpp
- **Line(s):** 217-234 (esp. 220, 229)
- **Severity:** medium
- **Confidence:** high
- **Category:** logic

## Code
```cpp
int64_t sizeLeft = blob_sz - from;
sizeLeft = request_sz == 0 ? sizeLeft : std::min((int64_t)request_sz, sizeLeft);   // announced count
...
memcpy_to(to, &sizeLeft, sizeof(sizeLeft));   // TAKE header tells client sizeLeft bytes
...
while (sizeLeft > 0)
{
    uint64_t data_pulled;
    const char *buf = blob_pull_data(readBlob, from, data_pulled);   // data_pulled = blob_sz - from (whole remaining)
    ...
    from += data_pulled;
    sizeLeft -= data_pulled;
    if (!send_exact(socket, buf, data_pulled))                       // sends data_pulled, NOT capped to sizeLeft
    ...
}
```

## Why this is a bug
For a bounded pull (`request_sz != 0`, the protocol's "pull by 1mb chunks" feature — see blob_push_protocol.h:40,47), the server correctly announces `sizeLeft = min(request_sz, blob_sz-from)` in the TAKE response, but then reads with `blob_pull_data`, whose mmap backend (`blobe_fileio_pull`) always returns `data_pulled = blob_sz - from` (the entire remainder). The `send_exact(socket, buf, data_pulled)` sends that entire remainder, which is larger than the `sizeLeft` the client was told to read.

The client (`blob_pull_from_server`) reads exactly the announced `sz` via `recv_lambda(sockfd, sz, ...)`, leaving the surplus bytes in the socket stream. The next command the client sends is then parsed against leftover blob bytes — the encrypted/framed protocol desynchronizes, effectively corrupting the connection.

Reachability: all in-tree callers currently pass `request_sz == 0` (whole-file), so `sizeLeft == blob_sz-from == data_pulled` and the surplus is zero — the bug is latent. But the server processes an untrusted, client-supplied `request_sz` and the protocol explicitly advertises partial pulls, so any client using a non-zero size (a resumed/1MB-chunked pull, a third-party client) triggers the over-send. This compounds with the `blobe_fileio_pull` offset bug (blob-006): partial/random-access pulls are wholesale broken.

## Suggested fix
Cap each send to the outstanding `sizeLeft`:
```cpp
uint64_t toSend = (uint64_t)std::min<int64_t>(sizeLeft, (int64_t)data_pulled);
from += toSend;
sizeLeft -= toSend;
if (!send_exact(socket, buf, toSend)) { ... }
```
(and fix `blobe_fileio_pull` to honor `from`).
