---
id: BUG-blob-09
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/ca_blobs_fs/streaming_compressors.h
line: 46
severity: medium
category: leak
verdict: CONFIRMED
fix_size_loc: 4
behavior_change: no
---

# `compress_lambda()` returns without killing the compression stream when the producer or consumer reports an error

## Summary
`compress_lambda` owns a `char cctx[CTX_SIZE]` holding a live `ZSTD_CStream*` (or `z_stream`). It
correctly relies on `compress_stream`/`finalize_compress_stream` to free the stream on *their* own
errors, but on the two paths where the caller's read/write lambda returns `StreamStatus::Error` it
simply `return`s, leaving the `ZSTD_CStream` allocated and unreachable.

## Code
```cpp
// ca_blobs_fs/streaming_compressors.h:30-55
template <typename Produce, typename Consume>
inline StreamStatus compress_lambda(Produce rcb, Consume wcb, int compression_level, StreamType type = StreamType::ZSTD)
{
  char cctx[CTX_SIZE];
  if (!init_compress_stream(cctx, sizeof(cctx), compression_level, type))
    return StreamStatus::Error;
  ...
  while ((writeStatus = wcb(dst, dst_pos, dst_size)) == StreamStatus::Continue && (readStatus = rcb(src, src_pos, src_size)) == StreamStatus::Continue)
  {
    compressStatus = compress_stream(cctx, src, src_pos, src_size, dst, dst_pos, dst_size);
    if (compressStatus == StreamStatus::Error)
      return StreamStatus::Error;           // ok: compress_stream already freed it
  }
  if (readStatus == StreamStatus::Error)
    return StreamStatus::Error;             // 46 <-- LEAK: cctx still owns a ZSTD_CStream

  while ((writeStatus = wcb(dst, dst_pos, dst_size)) == StreamStatus::Continue && compressStatus == StreamStatus::Continue)
  {
    compressStatus = finalize_compress_stream(cctx, dst, dst_pos, dst_size);
    if (compressStatus == StreamStatus::Error)
      return StreamStatus::Error;           // ok: finalize freed it
  }
  return compressStatus == StreamStatus::Finished && writeStatus != StreamStatus::Error ? StreamStatus::Finished : StreamStatus::Error;   // 54 <-- LEAK when writeStatus == Error
}
```

## Why it is a bug
`init_compress_stream` stores a heap `ZSTD_CStream*` inside `cctx`
(`streaming_compressors.cpp:150-151`). The header documents the ownership rule:
*"will kill stream. Only need to be called if you changed your mind, on Finished/Error it will be
auto killed"* (`streaming_compressors.h:27`). That auto-kill only happens inside `compress_stream`
and `finalize_compress_stream`. When the *caller's* lambda aborts the pipeline, neither of those
runs on the final iteration, `cctx` goes out of scope as a plain stack array, and the
`ZSTD_CStream` (which for level 6 reserves a multi-hundred-kilobyte window and match tables) is
orphaned.

Both abort paths are ordinary error handling, not corner cases:

* `readStatus == Error` — `blob_kv_processor.cpp:39` returns `StreamStatus::Error` on `ferror(rf)`,
  i.e. any read error on the file being uploaded.
* `writeStatus == Error` — `push_whole_blob.h:203-204` returns `StreamStatus::Error` when
  `stream_push` fails, i.e. any write error on the temp blob file (disk full, quota, EIO).

## Failure scenario
Server-side `cvs commit` conversion path. `push_whole_blob_from_raw_data` (`push_whole_blob.h:165`)
is called per revision from `src/rcs_cvt_kB.cpp:11`. The blob temp directory fills up, so
`stream_push` -> `blob_fwrite64` returns short -> `stream_push` returns false ->
the consumer lambda returns `StreamStatus::Error` -> the second `while` condition is false ->
line 54 returns `StreamStatus::Error` with `cctx` still holding a live `ZSTD_CStream`.
`push_whole_blob_from_raw_data` destroys the `PushData` and returns false; the caller reports the
error and continues with the next file. Every subsequent revision repeats the leak until the
conversion run is over — for a bulk `rcs_cvt_kB` conversion of a large repository that is one
`ZSTD_CStream` per file.

The client-side equivalent is `send_blob_file_data_net` (`blob_kv_processor.cpp:33-48`) with a
file that becomes unreadable mid-upload (`ferror(rf)`).

## Suggested fix
```cpp
  if (readStatus == StreamStatus::Error)
  {
    kill_compress_stream(cctx);
    return StreamStatus::Error;
  }
  ...
  if (compressStatus == StreamStatus::Finished && writeStatus != StreamStatus::Error)
    return StreamStatus::Finished;
  kill_compress_stream(cctx);
  return StreamStatus::Error;
```
(`kill_compress_stream` is documented as safe to call twice — `streaming_compressors.h:27` — and
sets the type to `Undefined`, so adding it on these paths cannot double-free.)

## Refutation attempt
I checked whether `cctx` has a destructor or RAII wrapper — it is a bare `char cctx[CTX_SIZE]`
local. I checked whether `kill_compress_stream` is called by any caller of `compress_lambda` —
neither `blob_kv_processor.cpp:33` nor `push_whole_blob.h:194` has access to `cctx`, which is
private to the template. I checked whether the two abort paths are actually reachable: both lambdas
in the tree can return `StreamStatus::Error` (`ferror(rf)` and `!stream_push(...)` respectively).
I also confirmed the *other* early return (line 43) is not a leak, because `compress_stream` frees
the ZSTD/zlib stream itself before returning `Error` (`streaming_compressors.cpp:266-280`). The
finding stands.
