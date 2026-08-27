# available_disk_space inverts the error check and always returns ~0 (disk-full GC never triggers)

- **File:** cvsnt/cvsnt-2.5.05.3744/keyValueServer/proxy/free_disk_space.cpp
- **Line(s):** 13-20
- **Severity:** medium
- **Confidence:** high
- **Category:** logic

## Code
```cpp
uint64_t available_disk_space(const char *dir)
{
  std::error_code ec;
  const std::filesystem::space_info si = std::filesystem::space(dir, ec);
  if (!bool(ec))                       // <-- inverted: this is the SUCCESS case
    return uint64_t(~uint64_t(0));
  return si.available;
}
```

## Why this is a bug
`std::filesystem::space(dir, ec)` clears `ec` on success and sets it on failure (and on failure sets every `space_info` member to `static_cast<uintmax_t>(-1)`). The guard is backwards:

- On **success**, `ec` is false, `!bool(ec)` is true, so the function returns `~0` (UINT64_MAX) instead of the real `si.available`.
- On **failure**, `ec` is true, `!bool(ec)` is false, so it returns `si.available`, which the standard already set to `-1` (== `~0`).

Either way the function returns `UINT64_MAX`; it never reports the actual free space.

Impact in the proxy GC (gc_proc_monitor.cpp):
- Line 35: the emergency trigger `if (avail == 0 || ...)` can never see `avail == 0` (it is always `-1` after the int64 cast), so the "disk is full, free cache now" path is dead. A proxy whose real disk is smaller than the configured cache size can fill the disk with no disk-space-driven GC.
- Lines 69-71 in `perform_immediate_gc`: `if (space > 0 && space > -needed_sz) return true;` never returns early (`space` is `-1`), so it always runs `free_space` even when the disk has ample room — needless cache eviction.

## Suggested fix
```cpp
  if (bool(ec))                        // on error, report "unknown / max"
    return uint64_t(~uint64_t(0));
  return si.available;
```
