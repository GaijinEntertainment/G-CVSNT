---
id: BUG-blob-12
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/src/download_blob_to.cpp
line: 451
severity: medium
category: logic
verdict: CONFIRMED
fix_size_loc: 5
behavior_change: yes
---

# `download_blob_ref_file()` falls out of its 16-attempt retry loop into the success path and fatally renames a temp file it just deleted

## Summary
The retry loop has no "all attempts exhausted" branch. When all 16 attempts fail but each
`reconnect()` succeeds, control simply reaches the code after the loop, which `change_mode`s and
then `rename_file`s a temp file that the last iteration unlinked. `rename_file` is the fatal
overload, so a "cannot reach the blob server" condition is reported as
"cannot rename file .../_new_x to .../x" and kills the process.

## Code
```cpp
// src/download_blob_to.cpp:371-473
  size_t readUncompressedSz = ~size_t(0);
  for (int i = 0; i < 16; ++i)//make 16 attempts
  {
    ...
    if (!downloadRet || !validated)
    {
      unlink_file(temp_filename.c_str());
      if (!processor->reconnect())
      {
        cvs_outerr(buf, 0);
        return false;
      } else
        error(0,0, "%s Reconnecting!\n", buf);
    }
    else
    {
      readUncompressedSz = info.realUncompressedSize;
      break;
    }
  }                                                     // 451 <-- no failure branch here
  std::string fullPath = (task.dirpath+"/")+task.filename;
  {
    int status = change_mode (temp_filename.c_str(), task.file_mode.c_str(), 1);
    if (status != 0)
      error (0, status, "cannot change mode of %s", task.filename.c_str());
  }
  rename_file (temp_filename.c_str(), fullPath.c_str());   // 458
  change_utime(fullPath.c_str(), task.timestamp);
```
```cpp
// src/filesubr.cpp:758-775
bool rename_file (const char *from, const char *to, bool fail_on_error)
{
  ...
  if (rename (from, to) < 0)
  {
    error (fail_on_error ? 1 : 0, errno, "cannot rename file %s to %s", fn_root(from), fn_root(to));
    return false;
  }
  ...
}
void rename_file (const char *from, const char *to) { rename_file(from, to, true); }
```

## Why it is a bug
The loop has exactly two exits: `return false` (when `reconnect()` also fails) and `break` (on
success). Falling off the end after `i == 16` is a third, unhandled exit, and the code after the
loop unconditionally assumes success:

* `readUncompressedSz` is still `~size_t(0)`, its "never set" sentinel.
* `temp_filename` was `unlink_file`d by the last failing iteration (`:438`).
* `change_mode` on the missing path fails and emits a red-herring "cannot change mode of x".
* `rename_file` is the `fail_on_error = true` overload, so `ENOENT` becomes `error(1, ...)` ->
  `error_exit()` -> `exit(EXIT_FAILURE)`.

The retry itself is reachable in ordinary operation. `!downloadRet` covers every network error
(`KVNetworkProcessor::download` returns false on `pulled <= 0` or a write failure), and
`!validated` covers a blake3 mismatch or a size mismatch (`:413`, `:427`). `reconnect()` returns
true for as long as `attemptsCount(id)` has entries left, and after those are exhausted it returns
false — but `KVNetworkProcessor::reconnect()` also succeeds whenever the *next* mirror in the round
robin accepts the TCP connection, which a mirror serving stale/garbage data will.

## Failure scenario
A CAFS mirror is up but its blob store was restored from a bad backup, so it serves a body whose
blake3 does not match the requested hash.

1. Attempt 1..16: `downloadRet` is true, `validated` is false (`memcmp(task.encoded_hash.data(),
   recievedHash, 64) != 0`). Each iteration renames the bad temp to a `cvs_temp_name()`, unlinks
   `temp_filename`, and calls `processor->reconnect()`, which connects to the next mirror and
   returns true.
2. `i` reaches 16, the loop ends.
3. `change_mode("<dir>/_new_foo.bin", ...)` fails -> "cannot change mode of foo.bin".
4. `rename_file("<dir>/_new_foo.bin", "<dir>/foo.bin")` -> `ENOENT` -> `error(1, ...)` ->
   `exit(EXIT_FAILURE)`.

Because `download_blob_ref_file` runs inside a `processor_thread_loop` worker
(`download_blob_to.cpp:82-87`), that `exit()` executes static destructors on the worker thread and
(per BUG-blob-11) hits `std::terminate()` in `~std::vector<std::thread>` — the user gets an abort,
not the 16 "Reconnecting!" messages plus a clean diagnostic.

Note also that `emplace()` already deleted the user's existing copy of the file
(`unlink_file(task.filename.c_str())`, `:110`) before queueing the task, so the working file is gone
either way.

## Suggested fix
```cpp
  bool downloaded = false;
  for (int i = 0; i < 16; ++i)
  {
    ...
    else { readUncompressedSz = info.realUncompressedSize; downloaded = true; break; }
  }
  if (!downloaded)
  {
    cvs_outerr(buf, 0);
    return false;
  }
```

## Refutation attempt
I checked whether `noexec` might make `rename_file` benign — it returns true early only when
`noexec` is set, which is not the case for a real `cvs update`. I checked whether the trailing
`if (validateHash)` size check would catch the situation first — it runs *after* `rename_file`, so
it is unreachable. I checked whether `reconnect()` is guaranteed to eventually return false and take
the clean `return false` path: `KVNetworkProcessor::reconnect()` (`blob_kv_processor.cpp:100`) does
`++attempt; return init();` and `init()` succeeds for any mirror whose TCP connect and handshake
work, so a set of reachable-but-wrong mirrors keeps it returning true. The finding stands.
