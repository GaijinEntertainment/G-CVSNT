---
id: BUG-blob-14
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/keyValueServer/proxy/free_disk_space.cpp
line: 17
severity: medium
category: logic
status: open - the sentinel-return fix was applied (e1a4bab) and reverted (535222a) in this slice: the stated consequence was wrong (the cache does not grow without bound) and the line-level fix alone makes the proxy noisier; the narrower defect stands
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# `available_disk_space()` inverts its error check and therefore always returns `UINT64_MAX`

## Summary
`std::filesystem::space()` sets the `error_code` only on failure, but the code returns the
"unknown" sentinel when `ec` is *clear* and returns `si.available` when `ec` is *set* — and on
failure the standard fills every `space_info` member with `(uintmax_t)-1`. Both branches therefore
yield `UINT64_MAX`, so the proxy's disk-pressure detection is entirely dead.

## Code
```cpp
// keyValueServer/proxy/free_disk_space.cpp:13-20
uint64_t available_disk_space(const char *dir)
{
  std::error_code ec;
  const std::filesystem::space_info si = std::filesystem::space(dir, ec);
  if (!bool(ec))                          // 186 <-- BUG: inverted; !ec means SUCCESS
    return uint64_t(~uint64_t(0));
  return si.available;
}
```

## Why it is a bug
[fs.op.space] specifies that the `error_code` overload of `std::filesystem::space` calls
`ec.clear()` on success and, if an error occurs, sets `ec` **and** sets all three members of the
returned `space_info` to `static_cast<uintmax_t>(-1)`. So:

* success -> `ec` is falsy -> `!bool(ec)` is true -> returns `~0ull`, discarding the real value;
* failure -> `ec` is truthy -> returns `si.available`, which is `(uintmax_t)-1` == `~0ull`.

The function has exactly one possible result. The neighbouring functions in the same file use the
opposite (correct) convention — `space_occupied` and `free_space` pass `ec` and *skip* entries whose
`file_size` came back as `-1`.

## Failure scenario
POSIX proxy (`keyValueServer/proxy/Makefile.am` builds `gc_proc_monitor.cpp` + `free_disk_space.cpp`).
The GC child process is the only consumer:

```cpp
// keyValueServer/proxy/gc_proc_monitor.cpp:26-35, 65-73
  int64_t lastAvail = available_disk_space(cache_folder.c_str());        // -1
  while(1) {
    for (int i = 0; i < 10; ++i)
    {
      const int64_t avail = available_disk_space(cache_folder.c_str());  // -1
      if (avail == 0 || (lastAvail + lastOccupied > int64_t(file_cache_size) + avail))
      ...
```
`avail == 0` — the "disk is completely full, free something now" trip-wire — can never be true.
The second half of the condition degenerates to `lastOccupied > file_cache_size` (the two `-1`s
cancel), so the GC still enforces its *own* soft limit but is blind to the actual filesystem.

Concretely: a proxy configured with a 100 GB soft limit on a partition it shares with build
artefacts. Another process fills the partition to 0 bytes free while the blob cache is still at
60 GB. The intended behaviour is for `avail == 0` to fire and evict blobs; instead the GC sees
`lastOccupied (60G) > file_cache_size (100G)` as false and does nothing. Every
`PullThroughTemp::start` then fails to create a temp file (`proxy_file_lib.cpp:335-337`), the proxy
falls back to pure net-proxying, and `perform_immediate_gc(expectedSize*2)` — called from
`proxy_file_lib.cpp:394` when a write fails — cannot help either, because its own
`needed_sz < 0` early-out (`gc_proc_monitor.cpp:67`) is also gated on
`available_disk_space` and never fires.

## Suggested fix
```cpp
  if (bool(ec))
    return uint64_t(~uint64_t(0));
  return si.available;
```

## Refutation attempt
I checked whether `space_info::available` might be meaningful on error in libstdc++/MSVC despite
the standard — both implementations explicitly assign `{-1,-1,-1}` on the error path, so the
failure branch returns `~0ull` as well and the function is genuinely constant. I checked whether the
`-1`s cancelling in `gc_proc_monitor` make the bug harmless — they preserve the *soft-limit* check
but not the `avail == 0` disk-full check nor the `perform_immediate_gc` fast path, both of which
become unreachable. I also confirmed there is no second definition of `available_disk_space` that
the Windows build might pick up instead (`cafs_proxy.vcxproj` links `gc_thread_monitor.cpp`, which
does not call it at all). The finding stands.
