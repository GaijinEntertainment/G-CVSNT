---
id: BUG-blob-11
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/src/download_blob_to.cpp
line: 119
severity: medium
category: concurrency
verdict: CONFIRMED
fix_size_loc: 4
behavior_change: no
---

# `BackgroundProcessor` declares `queue` before `threads`, so teardown destroys the thread vector first — `std::terminate()` on any early exit, dangling `waiting_threads` otherwise

## Summary
`concurrent_queue` keeps a raw `std::vector<std::thread>*` and joins through it in its destructor,
but the vector it points at is declared *after* the queue, so it is destroyed *before* the queue.
On the normal path `finishWork()` empties `threads` first and this is invisible; on any path that
skips `wait_threads()` — every `error(1, ...)` / `error_exit()` — the static `processor` is
destroyed with joinable threads still in the vector, which calls `std::terminate()`.

## Code
```cpp
// src/download_blob_to.cpp:63, 119-122, 148
  BackgroundProcessor():queue(&threads){}
  ...
  concurrent_queue<BlobTask> queue;                                     // 119  destroyed LAST
  std::vector<std::unique_ptr<BlobNetworkProcessor>> download_clients;  // 120
  std::vector<std::unique_ptr<BlobNetworkProcessor>> upload_clients;    // 121
  std::vector<std::thread> threads;//soa                                // 122  destroyed FIRST
  ...
static std::unique_ptr<BackgroundProcessor> processor;                  // 148  static storage duration
```
```cpp
// src/concurrent_queue.h:18, 22-41
  std::vector<std::thread> * waiting_threads = nullptr;
  ...
  ~concurrent_queue() {finishWork();}
  void finishWork() { cancel(Status::Finish); waitAll(); }
  void waitAll()
  {
    if (!waiting_threads)
      return;
    for (auto &e:*waiting_threads)      // <-- iterates a vector that is already destroyed
      e.join();
    std::unique_lock<std::mutex> lock(the_mutex);
    waiting_threads->clear();
  }
```

## Why it is a bug
Non-static data members are destroyed in reverse declaration order, so `~BackgroundProcessor`
runs `~vector<std::thread>` (line 122) first, then the two client vectors, then
`~concurrent_queue` (line 119). Two consequences:

1. **`std::terminate()`.** `std::thread::~thread()` calls `std::terminate()` if the thread is
   still `joinable()`. The workers spawned in `init()` (`:297-298`) block forever in
   `wait_and_pop()`'s `the_condition.wait(lock)` until somebody sets the queue status, which only
   `finishWork()`/`cancel()` do. If teardown happens without those, every worker is joinable and
   the vector's destructor aborts the process.
2. **Dangling pointer.** Even when `threads` is empty, `~concurrent_queue` still dereferences
   `waiting_threads` — a pointer to storage whose object lifetime has ended — to iterate it and to
   call `clear()` on it. That is UB regardless of whether the loop body executes.

The ordering also makes the two client vectors die before the queue is told to stop: if a worker
were still running it would be using `BlobNetworkProcessor` objects that were just freed.

The only place that saves the normal path is `wait_threads()`:
```cpp
// src/download_blob_to.cpp:354-359
void wait_threads()
{
  if (processor)
    processor->finishDownloads();   // -> queue.finishWork() -> joins AND clears `threads`
  processor.reset();
}
```
and it is called from exactly two places — `src/main.cpp:1717` and `src/client.cpp:5897` — both on
the *successful* end of a command.

## Failure scenario
`cvs update` of a module with binary blobs. `add_download_queue` creates the `BackgroundProcessor`
and 7 worker threads; the workers block in `wait_and_pop`.

Any subsequent fatal error in the main thread — a corrupt `Entries` line, a failed `rename_file`
(`filesubr.cpp:766` calls `error(1, ...)`), "cannot open CVS/Root", anything that reaches
`error_exit()` — leads to `exit(EXIT_FAILURE)` (`src/error.cpp`, end of `error_exit`).
`exit()` runs static destructors, which destroy the file-scope `processor`:

1. `~BackgroundProcessor` (empty body).
2. `~std::vector<std::thread>` for `threads` — 7 joinable threads -> `std::terminate()` -> `abort()`.

The user sees a crash / core dump instead of the intended one-line error message, and the CVS
sandbox is left half-updated with no cleanup (`error_exit`'s `Lock_Cleanup()` did run, but the
abort happens after it).

The same abort is guaranteed — not merely possible — when the fatal error originates *inside a
worker*, e.g. the `rename_file` at `download_blob_to.cpp:458`: that thread calls `exit()`, static
destruction runs on that thread, and `~thread` for the thread's own entry cannot be joined.

## Suggested fix
Declare the queue after everything it refers to, so it is destroyed first:
```cpp
  std::vector<std::unique_ptr<BlobNetworkProcessor>> download_clients;//soa
  std::vector<std::unique_ptr<BlobNetworkProcessor>> upload_clients;//soa
  std::vector<std::thread> threads;//soa
  concurrent_queue<BlobTask> queue;//must outlive nothing; must be destroyed before `threads`
```
and give `~BackgroundProcessor` an explicit `queue.finishWork();` so teardown is correct even when
`wait_threads()` was never reached.

## Refutation attempt
I checked whether the constructor's `queue(&threads)` is itself ill-formed — it is not; taking the
address of a not-yet-constructed member is allowed and `concurrent_queue`'s constructor only stores
the pointer. I checked whether `waitAll()` being called twice would double-join — it will not,
because it clears the vector, but that only holds on the path where the queue is destroyed *before*
`threads`, which is exactly the order this class does not have. I checked whether `error_exit()`
might use `_exit()`/`abort()` and thereby skip static destructors — it ends with
`exit (EXIT_FAILURE)` (`src/error.cpp`), which runs them. Finally I confirmed
`BackgroundProcessor::wait()` (`:301-305`) joins `threads` without clearing it, so calling it before
`finishWork()` would additionally produce a `std::system_error` from joining non-joinable threads.
The finding stands.
