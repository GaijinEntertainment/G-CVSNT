---
id: BUG-blob-17
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/keyValueServer/proxy/gc_thread_monitor.cpp
line: 51
severity: low
category: concurrency
verdict: CONFIRMED
fix_size_loc: 6
behavior_change: no
---

# Proxy GC thread waits on a condition variable with no predicate, and the notifier does not hold the mutex — wakeups are lost and the cache overruns its limit

## Summary
`gc_thread_proc` calls `wakeup_gc_cond.wait(lock)` with no predicate and no loop, while
`lazy_report_to_gc` evaluates the predicate and calls `notify_one()` without ever taking
`gc_mutex`. A notification issued between the GC thread's release of the lock and its next `wait()`
is lost, so a cache that crosses its soft limit at the end of a burst stays over the limit until the
next pull happens to notify again.

## Code
```cpp
// keyValueServer/proxy/gc_thread_monitor.cpp:15-62
static std::atomic<int64_t> cache_occupied_size;
static std::mutex gc_mutex;
static std::condition_variable wakeup_gc_cond;
...
static inline bool should_do_gc() { return (cache_occupied_size.load() > (int64_t)file_cache_size); }
static void do_gc()
{
  if (!should_do_gc())
    return;
  cache_occupied_size -= free_space(cache_folder.c_str(), uint64_t(file_cache_size));
}

static void gc_thread_proc()
{
  do_gc();
  while(1)
  {
    std::unique_lock<std::mutex> lock(gc_mutex);
    wakeup_gc_cond.wait(lock);          // 51: no predicate, no loop
    do_gc();
  }
}

void lazy_report_to_gc(uint64_t sz)
{
  //wake up GC thread
  cache_occupied_size += sz;            // 59: predicate mutated without gc_mutex
  if (should_do_gc())
    wakeup_gc_cond.notify_one();        // 61: notify without gc_mutex
}
```

## Why it is a bug
`gc_mutex` guards nothing: the predicate lives entirely in the atomic `cache_occupied_size`, which
`lazy_report_to_gc` updates without the lock. The standard requirement for a race-free
condition-variable handshake is that the predicate be modified while holding the same mutex the
waiter uses (or at minimum that the waiter re-tests the predicate in a loop). Neither holds here, so
there is a plain lost-wakeup window:

1. GC thread finishes `do_gc()`; the `unique_lock` at the top of the loop body is destroyed and
   `gc_mutex` is released.
2. A connection thread runs `lazy_report_to_gc(sz)`: `cache_occupied_size` crosses
   `file_cache_size`, `should_do_gc()` is true, `notify_one()` fires — with no thread inside
   `wait()`, the notification is discarded.
3. GC thread re-enters the loop, takes `gc_mutex`, calls `wait(lock)` and blocks.

`wait()` without a predicate is also formally wrong on its own: a spurious wakeup makes the GC run
`do_gc()` unnecessarily (harmless here, since `do_gc` re-tests `should_do_gc`), but the missing
predicate is what makes step 3 unrecoverable — there is no re-test of the already-true condition
before blocking.

`gc_thread_monitor.cpp` is the Windows proxy's GC (`keyValueServer/proxy/cafs_proxy.vcxproj:325`);
the POSIX build uses the fork-based `gc_proc_monitor.cpp` instead
(`keyValueServer/proxy/Makefile.am:6`).

## Failure scenario
A Windows `cafs_proxy_server` with a 100 GB soft limit sits at 99.9 GB. A developer checks out a
branch containing one 500 MB blob and then stops working for the night.

1. `PullThroughTemp::finish` succeeds and calls `lazy_report_to_gc(fileSz)`
   (`proxy_file_lib.cpp:479`). `cache_occupied_size` becomes 100.4 GB.
2. `should_do_gc()` is true, `notify_one()` fires. The GC thread happens to be between iterations
   (it just returned from `do_gc()` for the previous blob), so nothing is waiting and the
   notification is lost.
3. The GC thread blocks in `wait()`. No further pulls arrive.
4. The cache stays 400 MB over its configured limit indefinitely. If the operator sized the limit
   against the partition, the overshoot accumulates across every such window until a pull happens to
   arrive at the right moment.

## Suggested fix
```cpp
static void gc_thread_proc()
{
  do_gc();
  while(1)
  {
    std::unique_lock<std::mutex> lock(gc_mutex);
    wakeup_gc_cond.wait(lock, []{ return should_do_gc(); });   // predicate loop
    lock.unlock();
    do_gc();
  }
}

void lazy_report_to_gc(uint64_t sz)
{
  cache_occupied_size += sz;
  if (should_do_gc())
  {
    std::lock_guard<std::mutex> lock(gc_mutex);   // publish before notifying
    wakeup_gc_cond.notify_one();
  }
}
```

## Refutation attempt
I checked whether `cache_occupied_size` being `std::atomic` removes the need for the mutex — it
removes the data race on the variable but not the lost-wakeup race, which is about the ordering of
"predicate became true" against "waiter entered `wait()`"; only a predicate re-test inside the
waiter (or the mutex held across the mutation) closes it. I checked whether some other path wakes
the GC: `perform_immediate_gc` (`:158`) calls `free_space` directly from the connection thread and
never notifies, and `do_gc()` is otherwise only reached from `gc_thread_proc`. I checked whether
overshoot is self-correcting: it is, but only on the *next* `lazy_report_to_gc` that finds the
predicate true, i.e. only if more traffic arrives. The finding stands; severity is low because the
consequence is a soft-limit overshoot rather than corruption.
