---
id: BUG-blob-05
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/ca_blobs_fs/src/fileio.cpp
line: 313
severity: high
category: protocol
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# `blobe_fileio_pull()` ignores the `from` offset — ranged `PULL` returns the wrong bytes and over-sends past the announced length

## Summary
`blobe_fileio_pull()` computes `data_pulled = size - from` but returns `fp->begin`, the base of the
mapping, instead of `fp->begin + from`. Every ranged `PULL` therefore serves the *start* of the blob
and serves `blob_sz - from` bytes even though the `TAKE` header just promised `min(request_sz,
blob_sz - from)`.

## Code
```cpp
// ca_blobs_fs/src/fileio.cpp:305-314
const char *blobe_fileio_pull(BlobFileIOPullData* fp, uint64_t from, uint64_t &data_pulled)
{
  if (!fp)
    return nullptr;
  const int64_t left = fp->size - from;
  if (left < 0)
    return nullptr;
  data_pulled = left;
  return fp->begin;          // <-- BUG: must be fp->begin + from
}
```

## Why it is a bug
The `from` parameter is plainly part of the contract. `fileio.h:145` declares
`const char *blobe_fileio_pull(BlobFileIOPullData* fp, uint64_t from, uint64_t &data_pulled);`, the
public wrapper `caddressed_fs::pull` documents "pull allows random access"
(`content_addressed_fs.h:47-50`), and the wire protocol advertises the feature explicitly
(`blob_push_protocol.h:155-162`: *"You can ask for 0:0 if you want whole file"*, *"server will
immediately write full bytes_size data (starting from bytes_from<<20 of blob). Allows pull by 1mb
chunks"*). The proxy's own pull implementation honours the offset — it refuses the request rather
than lie about it:
```cpp
// keyValueServer/proxy/proxy_file_lib.cpp:370-378
  const char *pull(uint64_t from, uint64_t &read)
  {
    if (from != pulledSz)
      return nullptr;      // "we can't move cursor in downloading file ... todo: fixme"
```
and so does the reference in-memory backend shipped as the sample:
```cpp
// keyValueServer/sample/stub_file_lib.cpp:83-90
const char *blob_pull_data(uintptr_t up, uint64_t from, uint64_t &read){
  if (!up){read = 0;return nullptr;}
  auto &v = *(const std::vector<uint8_t>*)up;
  if (v.size() < from)
    {read = 0;return nullptr;}
  read = v.size() - from;
  return (const char*)(v.data()+from);      // <-- offsets the pointer, as it must
}
```
so the local-filesystem backend is the only one of the three that drops the offset.

The server loop that consumes it trusts both return values:
```cpp
// keyValueServer/serverLib/blob_push_proc.cpp:201-234
  int64_t sizeLeft = blob_sz - from;
  sizeLeft = request_sz == 0 ? sizeLeft : std::min((int64_t)request_sz, sizeLeft);
  ...
  memcpy_to(to, &sizeLeft, sizeof(sizeLeft));      // TAKE announces sizeLeft
  ...
  while (sizeLeft > 0)
  {
    uint64_t data_pulled;
    const char *buf = blob_pull_data(readBlob, from, data_pulled);
    ...
    from += data_pulled;
    sizeLeft -= data_pulled;
    if (!send_exact(socket, buf, data_pulled))     // sends data_pulled, not sizeLeft
```
`data_pulled` comes back as `blob_sz - from`, which is larger than the announced `sizeLeft`
whenever `request_sz` is a non-zero value smaller than the remaining blob. The extra bytes are
written onto the wire *after* the framed response, so the peer reads them as the next response.

## Failure scenario
A 3 MiB blob `H` is present in the store. A client issues the documented chunked request
`PULL <H> request_sz = 0x100000 (1 MiB) chunk = 0`:

1. `handle_pull`: `from = 0`, check `1 MiB + 0 > 3 MiB` is false, `sizeLeft = min(1 MiB, 3 MiB) = 1 MiB`.
2. Server sends `TAKE <hash> sizeLeft=1048576 chunk=0`.
3. Loop: `blob_pull_data(readBlob, 0, data_pulled)` returns `begin`, `data_pulled = 3 MiB`.
   `send_exact(socket, buf, 3 MiB)` pushes **3 MiB** onto a connection that was told to expect 1 MiB.
4. The client consumes 1 MiB and then calls `recv_exact(response, 4)` for its next command — and
   reads blob payload as a response code. The connection is desynchronised for the rest of its
   life; `blob_check_on_server`/`blob_size_on_server` will return arbitrary answers derived from
   blob bytes.

With `chunk = 1` (`from = 1 MiB`) the same call additionally returns the first megabyte of the blob
where megabyte number two was requested, so a client that reassembles chunks writes a corrupt file.

Both the local-FS server (`blob_file_lib.cpp:228`) and the proxy's *cached* path
(`proxy_file_lib.cpp:531`) route through this function, so both are affected.

## Suggested fix
```cpp
  return fp->begin + from;
```
(and, for defence in depth, clamp the amount actually written in `handle_pull`:
`if (data_pulled > (uint64_t)sizeLeft) data_pulled = sizeLeft;`)

## Refutation attempt
I checked whether the in-tree clients ever send a ranged request that would expose this today:
`blob_kv_processor.cpp:113` calls `blob_pull_from_server(client, ..., 0, 0, ...)` and
`blob_start_pull_from_server` then derives `chunk = from/pull_chunk_size = 0` and sends `sz = 0`, so
the shipped CVS client always asks for the whole blob and happens to get correct behaviour. That
makes the bug latent for *this* client but not benign: the server accepts and mis-answers ranged
requests from any conforming peer (the format is documented in the protocol header and the field is
parsed at `blob_push_proc.cpp:180-181`), and `caddressed_fs::get_file_content_hash`
(`content_addressed_fs.cpp:244-254`) already contains a `while (at < blob_sz)` loop that only works
by accident because mmap returns everything in one shot. I also checked whether `blob_fileio_os_mmap`
maps from a non-zero file offset, which would make returning `begin` correct — it does not
(`mmap(NULL, flen, ..., fd, 0)` / `MapViewOfFileEx(hmap, FILE_MAP_READ, 0, 0, flen, NULL)`, both at
offset 0). The finding stands.
