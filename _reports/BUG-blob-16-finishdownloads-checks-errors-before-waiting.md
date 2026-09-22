---
id: BUG-blob-16
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/src/download_blob_to.cpp
line: 67
severity: medium
category: concurrency
verdict: CONFIRMED
fix_size_loc: 6
behavior_change: yes
---

# `finishDownloads()` samples `hasErrors` *before* waiting for the workers, so failures during the final drain never affect the exit status

## Summary
`finishDownloads()` reads `hasErrors` once, up front, and only then calls `queue.finishWork()` to
drain and join the worker threads. Any blob download or upload that fails while that drain is in
progress sets `hasErrors` too late to be seen: the flag is never re-read, `wait_threads()` returns
normally, and `cvs` exits 0 even though files are missing or were never uploaded.

## Code
```cpp
// src/download_blob_to.cpp:67-76
  void finishDownloads()
  {
    if (hasErrors.load())            // 69: sampled BEFORE the wait
    {
      queue.cancel();
      cvs_flusherr();
      error_exit();
    }
    queue.finishWork();              // 75: joins the workers; errors raised here are dropped
  }
```
```cpp
// src/download_blob_to.cpp:82-87  (the workers that set the flag)
  static void processor_thread_loop(BackgroundProcessor *processors, BlobNetworkProcessor *download_processor, BlobNetworkProcessor *upload_processor)
  {
    BlobTask task;
    while (hasErrors.load() == 0 && processors->queue.wait_and_pop(task))
      hasErrors.fetch_or(process_blob_task(download_processor, upload_processor, task) ? 0 : 1);
  }
```
```cpp
// src/download_blob_to.cpp:354-359
void wait_threads()
{
  if (processor)
    processor->finishDownloads();
  processor.reset();
}
```

## Why it is a bug
`finishWork()` is precisely the point where the *last* queued tasks are executed — `cancel(Status::Finish)`
lets `wait_and_pop` keep draining (`concurrent_queue.h:91-98` only stops on an empty queue, not on
`Finish`), and `waitAll()` then joins. So the majority of the outstanding work typically completes
*after* line 69 has already been evaluated. The result of that work is written to `hasErrors` and
then thrown away: there is no second `hasErrors.load()` anywhere after `finishWork()`, and
`wait_threads()` has no return value.

The queue also empties itself unconditionally under `Finish`, so a worker that bails out early
because `hasErrors` became non-zero leaves items behind that nobody will ever process — and
`emplace()` has already `unlink_file(task.filename.c_str())`d the user's copy of every download
target (`:109-110`) on the assumption that the worker will replace it.

## Failure scenario
`cvs update` of a module with 400 binary assets, 7 worker threads.

1. `add_download_queue` enqueues all 400 tasks; the workers churn through them.
2. The main thread finishes its own work and calls `wait_threads()` -> `finishDownloads()`.
3. `hasErrors.load()` is 0 (the ~380 tasks completed so far all succeeded), so the branch is skipped.
4. `queue.finishWork()` starts; the workers pick up the last ~20 tasks. The blob server is
   restarted at this moment, so `download_blob_ref_file` for `big_texture.dds` exhausts its retries
   and returns false; `hasErrors` becomes 1.
5. The remaining workers see `hasErrors.load() != 0` at the top of their loop and exit, abandoning
   whatever is still queued. `waitAll()` joins them all.
6. `finishDownloads()` returns, `processor.reset()`, `main` continues to its normal
   `return 0` path.

`cvs` exits with status 0. The `Entries` file lists the new revisions, the working copies of
`big_texture.dds` and every abandoned task were deleted by `emplace()` at step 1, and the only trace
is an `ERROR: can't download <hash>` line on stderr. A CI job or a `cvs update && make` chain
proceeds on a silently incomplete checkout.

## Suggested fix
```cpp
  void finishDownloads()
  {
    if (hasErrors.load())
    {
      queue.cancel();
      cvs_flusherr();
      error_exit();
    }
    queue.finishWork();
    if (hasErrors.load())            // re-check after the drain
    {
      cvs_flusherr();
      error_exit();
    }
  }
```

## Refutation attempt
I checked whether the process exit status is derived from something else that would catch this:
`wait_threads()` is `void` (`:354`), `main.cpp:1717` ignores it, and the only escalation in the
worker path is `emplace()`'s synchronous branch (`:94-98`), which is taken only when
`threads.empty()` (single-threaded mode). I checked whether `cvs_outerr` sets a global error flag —
it writes to the error stream and returns an int that `download_blob_ref_file` discards. I checked
whether the workers might finish before line 69 in practice: they cannot be relied upon to, because
`emplace()` is called from the middle of the update walk and `finishDownloads()` is called at its
end, so a deep tree keeps tasks in flight right up to the join. The finding stands.
