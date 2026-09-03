---
id: BUG-blob-08
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/src/blob_kv_processor.cpp
line: 70
severity: medium
category: leak
verdict: CONFIRMED
fix_size_loc: 3
behavior_change: no
---

# Short-circuit in `send_blob_file_data_net()` leaks the 64 KiB `StreamToServerData` on every fatal upload error

## Summary
`if (r == KVRet::Fatal || (r = finish_blob_stream_to_server(...)) == KVRet::Fatal)` never evaluates
the right-hand side when `r` is already `Fatal`. `finish_blob_stream_to_server()` is the only thing
that deletes the `StreamToServerData` returned by `start_blob_stream_to_server()`, so the object —
which embeds a `BufferedSocketOutput<65536>` — is leaked whenever the upload hits a socket error.

## Code
```cpp
// src/blob_kv_processor.cpp:20-24, 30, 43-44, 70-71
  StreamToServerData *strm = start_blob_stream_to_server(client, HASH_TYPE_REV_STRING, hash);
  if (!strm) { ... }
  ...
  KVRet r = blob_stream_to_server(*strm, &hdr, sizeof(hdr));      // can return Fatal
  ...
        if (dst_pos && r == KVRet::OK)
          r = blob_stream_to_server(*strm, bufOut, dst_pos);      // can return Fatal
  ...
  if (r == KVRet::Fatal || (r = finish_blob_stream_to_server(client, strm, r == KVRet::OK)) == KVRet::Fatal)
    stop_blob_push_client(client);
```
```cpp
// keyValueServer/clientLib/blob_strm_client_cmd.cpp:322-331
KVRet finish_blob_stream_to_server(BlobSocket &sockfd, StreamToServerData *s, bool ok)
{
  if (!s)
    return KVRet::Error;
  KVRet ret = end_blob_stream_to_server(*s, ok);
  delete s;                                   // <-- the only delete
  ...
}
```

## Why it is a bug
`start_blob_stream_to_server(BlobSocket&, ...)` heap-allocates with `new StreamToServerData(sockfd)`
(`blob_strm_client_cmd.cpp:312`) and the class holds
`BufferedSocketOutput<65536> wr;` (`:268`), i.e. a 64 KiB buffer plus the socket handle. Ownership
is documented on the declaration: `KVRet finish_blob_stream_to_server(BlobSocket &sockfd,
StreamToServerData *s, bool ok);//will delete the pointer` (`blob_client_lib.h:302`). Nothing else
in `send_blob_file_data_net` owns or frees `strm`; it is a raw pointer in an automatic variable.

`r` becomes `Fatal` from either `blob_stream_to_server` call — those return `KVRet::Fatal` whenever
`strm.wr.send()` fails (`blob_strm_client_cmd.cpp:302-303`), i.e. on any write error to the CAFS
server. So the leak is on the ordinary "network died mid-upload" path, not an exotic one.

## Failure scenario
`cvs commit` of a directory of large binaries against a CAFS server that goes away (restart,
firewall drop, load-balancer reset) partway through.

For each file, `KVNetworkProcessor::upload` -> `send_blob_file_data_net`:
1. `start_blob_stream_to_server` allocates a `StreamToServerData` (~64 KiB).
2. `blob_stream_to_server(*strm, bufOut, dst_pos)` inside the compression consumer lambda fails ->
   `r = KVRet::Fatal`.
3. Line 70 short-circuits, `stop_blob_push_client(client)` closes the socket, `strm` is abandoned.
4. `upload()` sees `r != OK`, returns false; `upload_blob_ref_file` reports the error but the CVS
   client keeps going through the rest of the commit set, and `KVNetworkProcessor::init()`
   reconnects to the next mirror.

Every subsequent file in the commit repeats the cycle, leaking another 64 KiB. A commit of a few
thousand assets against a flapping server leaks hundreds of megabytes in a single `cvs` invocation.

## Suggested fix
```cpp
  const KVRet finishRet = finish_blob_stream_to_server(client, strm, r == KVRet::OK);
  if (r != KVRet::Fatal)
    r = finishRet;
  if (r == KVRet::Fatal)
    stop_blob_push_client(client);
```
(`finish_blob_stream_to_server` already tolerates a dead socket: `end_blob_stream_to_server`'s
`send`/`recv` simply fail and it returns `Fatal`.)

## Refutation attempt
I checked whether `stop_blob_push_client(client)` frees the stream — it does not; it only calls
`blob_close_socket` and resets the `BlobSocket` (`blob_push_pull_client.cpp:261-268`). I checked
whether `StreamToServerData` is reference-counted or registered anywhere — it is a plain
heap object with no owner other than the caller. I checked the sibling code path
`blob_stream_to_server(BlobSocket&, ..., pull_data)` (`blob_strm_client_cmd.cpp:333`) which uses a
*stack* `StreamToServerData strm(sockfd);` and therefore does not leak — confirming that only the
explicit start/finish pairing is affected. The finding stands.
