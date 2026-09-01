---
id: BUG-blob-19
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/keyValueServer/clientLib/blob_push_client_cmd.cpp
line: 35
severity: low
category: correctness
verdict: CONFIRMED
fix_size_loc: 6
behavior_change: yes
---

# `blob_push_to_server()` spins forever when the producer callback reports zero progress — `cafs_client push` hangs on every file

## Summary
The push loop advances only by whatever `pull_data()` reports, and treats `data_pulled == 0` with a
non-null pointer as a normal (zero-byte) iteration. Nothing detects lack of progress, so a callback
that returns 0 makes the loop spin forever holding the connection open. The in-tree
`cafs_client` push callback has an off-by-`hdrSize` in its length arithmetic that produces exactly
that, so `cafs_client pushfile` hangs on every file (`pushblob` passes `hdrSize == 0`, where
`data_pulled = fsz - at` still makes progress, so only the nonzero-header `pushfile` variant
reaches the zero-progress arithmetic).

## Code
```cpp
// keyValueServer/clientLib/blob_push_client_cmd.cpp:33-48
  uint64_t from = 0;
  int64_t sizeLeft = blob_sz;
  while (sizeLeft > 0)
  {
    uint64_t data_pulled;
    const char *buf = pull_data(from, data_pulled);
    if (!buf)
    {
      blob_logmessage(LOG_ERROR, "IO error on %s:%s", hash_type, hash_hex_str);
      break;
    }
    from += data_pulled;          // 147: += 0
    sizeLeft -= data_pulled;      // 148: -= 0
    if (!send_exact(sockfd, buf, (int)data_pulled))   // send_exact(.., 0) returns true immediately
      {stop_blob_push_client(sockfd); return -2;}
  }
```
```cpp
// keyValueServer/sample/cafs_client.cpp:131-140   the producer that trips it
      int64_t pushed = blob_push_to_server(client, blob_sz, ht, hash, [&](uint64_t at, uint64_t &data_pulled) {
        if (at < hdrSize)
        {
          data_pulled = hdrSize-at;
          return ((const char*)&hdr) + at;
        }
        data_pulled = fsz - at - hdrSize;      // 137: BUG - must be fsz - (at - hdrSize)
        return ((const char*)data) + (at-hdrSize);
      });
```

## Why it is a bug
The header's contract for the sibling pull API is explicit that zero means error —
`extern const char *blob_pull_data(uintptr_t readBlob, uint64_t from, uint64_t &data_pulled);//data_pulled != 0, unless error`
(`blob_server_func_deps.h:298`) — but `blob_push_to_server` neither documents nor enforces the
symmetric requirement on `pull_data`. `send_exact(socket, buf, 0)` is a successful no-op
(`blob_common_net.h:29-42`: `int64_t len = 0; while (len > 0)` never executes), so a zero-length
iteration is indistinguishable from progress and the loop condition `sizeLeft > 0` never changes.

The `cafs_client` callback is called with `at == from`, the *absolute* offset in the blob stream
(header + body). The body offset is therefore `at - hdrSize`, which the `return` statement gets
right and the `data_pulled` line gets wrong: it subtracts `at` (absolute) *and* `hdrSize` from
`fsz`, double-counting the header.

Trace for a 100-byte file (`hdrSize = 16`, `blob_sz = 116`):

| iter | `at` | `data_pulled` | `from` after | `sizeLeft` after | bytes sent |
|---|---|---|---|---|---|
| 1 | 0  | `16-0 = 16`        | 16 | 100 | 16 |
| 2 | 16 | `100-16-16 = 68`   | 84 | 32  | 68 |
| 3 | 84 | `100-84-16 = 0`    | 84 | 32  | 0  |
| 4 | 84 | 0                  | 84 | 32  | 0  |
| ...| ...| ...              |... | ... | ...|

## Failure scenario
`cafs_client <host> <port> <root> pushfile bigasset.dds` against a running `cafs_server`.

1. The client sends `PUSH blake3:<H> size=116` and then 84 of the promised 116 bytes.
2. It enters the spin above: 100 % CPU on one core, no syscalls, no output, forever.
3. On the server, `handle_push` -> `recv_lambda(socket, 116, ...)` is blocked waiting for the last
   32 bytes. It holds the temp blob file open and one connection slot (one thread with
   `MULTI_THREADED`, one forked process otherwise) until the 30-minute `SO_RCVTIMEO`
   (`blob_raw_sockets.h:39`) expires.
4. The user must `Ctrl-C`. Because the client never reached `blob_end_push_data`, the server's temp
   file is only cleaned up when `blob_destroy_push_data` runs on the receive error.

Scripting `cafs_client push` in a loop (its obvious use as an ingest tool) therefore wedges both
ends. The `pushblob` sub-command has the same defect with `hdrSize = 0`: `data_pulled = fsz - at`,
which is correct — so only `pushfile` hangs, which is why the bug survived.

## Suggested fix
Fix the caller:
```cpp
        data_pulled = fsz - (at - hdrSize);
```
and harden the library so no other producer can hang it:
```cpp
    if (!buf || !data_pulled)
    {
      blob_logmessage(LOG_ERROR, "IO error on %s:%s", hash_type, hash_hex_str);
      break;
    }
```
(the existing `if (sizeLeft > 0)` zero-padding block at `:153-159` already handles the short
transfer correctly once the loop exits).

## Refutation attempt
I checked the other in-tree consumer, `test_client.cpp:75`
(`data_pulled = strlen(text) - at; return text + at;`), which is correct and completes in one
iteration — so the library defect is only observable through `cafs_client`, but it is a defect of
the library nonetheless because nothing in `blob_client_lib.h` forbids a zero-length chunk. I
checked whether `send_exact` might return false for a zero length and break the loop — it returns
`true` (the `while (len > 0)` body is skipped). I checked whether `blob_size_on_server` or some
guard runs first and would abort the push for an already-present blob — `cafs_client` does not call
it before pushing. I re-derived the table above from the actual code rather than by inspection. The
finding stands, with one scoping caveat that keeps the severity at low: `blob_push_client_cmd.cpp`
is part of the shipped `libkv_client_lib` (`keyValueServer/clientLib/Makefile.am`), but
`keyValueServer/sample/cafs_client.cpp` appears in no `Makefile.am` or `.vcxproj`, so the hang is
only reproducible when the sample is built by hand.
