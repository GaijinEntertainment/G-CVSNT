---
id: BUG-blob-03
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/keyValueServer/blob_sockets/blob_sockets.cpp
line: 178
severity: high
category: protocol
verdict: CONFIRMED
fix_size_loc: 8
behavior_change: no
---

# Encrypted `send()` silently discards ciphertext on a short write and still reports full success

## Summary
The encrypted branch of `send(BlobSocket&,...)` calls the raw socket `send()` once per 32 KiB
ciphertext block and only checks for `ret <= 0`. A short write (`0 < ret < curEncrypted`) drops the
tail of that block, yet the function returns `len` — the full plaintext length — so the
`send_exact()` retry loop above it never notices. Because the channel is AES-CTR, losing ciphertext
bytes permanently desynchronises the peer keystream.

## Code
```cpp
// keyValueServer/blob_sockets/blob_sockets.cpp:161-185
static int send(BlobSocket &socket, const void *buf_, int len, int flags)
{
  if (!socket.encrypt)
    return send((SOCKET)socket.opaque, (const char*)buf_, len, flags);

  const unsigned char* buf = (const unsigned char*)buf_;
  unsigned char encryptedBuf[32768];//encode by 32768
  int encrypted = 0;
  for (int lenLeft = len; lenLeft > 0;)
  {
    const int curLen = size_t(lenLeft) < sizeof(encryptedBuf) ? lenLeft : (int)sizeof(encryptedBuf);
    const int curEncrypted = encrypt_and_finalize(socket.encrypt, encryptedBuf, sizeof(encryptedBuf), buf, curLen);
    if (curEncrypted < 0)
      return -1;
    encrypted += curEncrypted;
    buf += curLen;
    lenLeft -= curLen;
    const int ret = send((SOCKET)socket.opaque, (const char*)encryptedBuf, curEncrypted, flags);
    if (ret <= 0)
      return ret;                       // <-- BUG: 0 < ret < curEncrypted treated as full success
  }
  if (encrypted != len)
    return -1;
  return len;
}
```

## Why it is a bug
Everywhere else in this subsystem short writes are handled correctly:
`raw_send_exact()` (`blob_raw_sockets.h:181-194`) advances by `l` and loops, and
`send_exact()` (`blob_common_net.h:29-42`) does the same on top of `send_msg_no_signal`.
The encrypted `send()` is the one layer that breaks that contract: it advances `encrypted` and
`buf` by the *requested* amount regardless of what the kernel accepted, then reports `len` because
`encrypted == len` always holds for a stream cipher (AES-CTR output length equals input length).
`send_exact()` therefore computes `len -= l` with `l == len` and exits satisfied.

Short writes on a blocking socket are not hypothetical here. `raw_set_socket_def_options()` is
called on every connection (`blob_push_proc.cpp:407`, `blob_push_pull_client.cpp:114`) and it calls
`raw_send_recieve_sock_timeout()`, which sets `SO_SNDTIMEO`. A blocking `send()` that hits
`SO_SNDTIMEO` after partially filling the socket buffer returns the number of bytes transferred,
not `-1`. `EINTR` after a partial transfer behaves the same way.

The consequence is worse than a truncated message: AES-CTR keeps a running counter inside
`socket.encrypt`, so the peer `EVP_DecryptUpdate` stream is offset by the number of dropped bytes
and *every* subsequent byte on that connection decrypts to garbage.

## Failure scenario
An encrypted `PULL` of a 200 MB blob to a client on a slow link. `handle_pull`
(`blob_push_proc.cpp:229`) calls `send_exact(socket, buf, data_pulled)` with `data_pulled` equal to
the whole blob; `send_exact` chunks it to 1 GiB and calls `send_msg_no_signal` -> the encrypted
`send()` above. The client stalls (full receive window) long enough for the `SO_SNDTIMEO` on one
32 KiB block to expire after, say, 9000 of 32768 bytes were queued. `send()` returns 9000; the code
adds 32768 to `encrypted`, moves on to the next block, and eventually returns `len`. `send_exact`
declares the whole transfer complete. The client `recv()` decrypts the stream 23768 bytes out of
phase: the blob body is garbage, the blake3 check in `download_blob_ref_file`
(`download_blob_to.cpp:413`) fails, and the *next* command/response framing on that connection is
also garbage, so the client mis-parses arbitrary plaintext as a 4-byte response code.

## Suggested fix
```cpp
    int sentInBlock = 0;
    while (sentInBlock < curEncrypted)
    {
      const int ret = send((SOCKET)socket.opaque, (const char*)encryptedBuf + sentInBlock,
                           curEncrypted - sentInBlock, flags);
      if (ret <= 0)
        return ret;
      sentInBlock += ret;
    }
```

## Refutation attempt
I checked whether a wrapper above compensates: `send_exact` only sees the return value of this
function, which is unconditionally `len` on the success path, so it cannot. I checked whether the
socket is non-blocking (which would make a short write expected and handled elsewhere):
`raw_set_non_blocking(.., false)` is restored after `connect_with_timeout`
(`blob_sockets.cpp:53`), so the socket is blocking with `SO_SNDTIMEO` set — exactly the
configuration in which POSIX allows a partial `send()`. I checked the unencrypted branch: it
returns the raw result and is handled correctly by `send_exact`, so the bug is confined to the
encrypted branch. The finding stands.
