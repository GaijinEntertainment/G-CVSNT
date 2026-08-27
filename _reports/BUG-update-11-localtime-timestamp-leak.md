---
id: BUG-update-11
area: client/update core
file: cvsnt/cvsnt-2.5.05.3744/src/client.cpp
line: 2006
severity: low
category: leak
verdict: CONFIRMED
fix_size_loc: 6
behavior_change: no
---

# `localtime_timestamp` is allocated once per updated file in three functions and never used or freed

## Summary
`update_entries()`, `update_blob_ref_entries()` and `update_meta_entries()` each declare a
local `char *localtime_timestamp`, assign it a freshly `xmalloc`'d string from
`time_stamp (..., 1)`, and then never read it and never free it. Every file the client
receives leaks one timestamp string.

## Code
```cpp
/* src/client.cpp:1993-2010 (update_entries) */
	char *local_timestamp;
	char *localtime_timestamp;      /* 1995 - declared */
	char *file_timestamp;

	(void) time (&last_register_time);

	local_timestamp = data->timestamp;
	if (local_timestamp == NULL || ts[0] == '+')
	{
        if (file_mtime == 0)
            file_mtime = get_file_mtime(filename);
	    file_timestamp = time_stamp (file_mtime,0);
	    localtime_timestamp = time_stamp (file_mtime,1);   /* 2006 - only write, never read */
	}
	else
	    file_timestamp = NULL;
```
The counterpart `file_timestamp` *is* released at the end of the block
(`if (file_timestamp) xfree (file_timestamp);`, client.cpp:2095-2096); `localtime_timestamp`
never is.

The identical pattern repeats at:
* `update_blob_ref_entries()` — declaration client.cpp:2503, allocation client.cpp:2512
* `update_meta_entries()` — declaration client.cpp:2720, allocation client.cpp:2730

## Why it is a bug
`time_stamp (time_t, int)` (client.cpp:2269) returns an `xstrdup`'d/`xmalloc`'d buffer; the
callers own it. `grep -n "localtime_timestamp" client.cpp` returns exactly six lines — three
declarations and three assignments — so the value is written and dropped on the floor in
all three functions. There is no `#ifdef` in which it is consumed.

These three functions are the per-file handlers for every content-carrying server response
(`Updated`, `Created`, `Update-existing`, `Merged`, `Patched`, `Rcs-diff`, and the fork's
blob-ref/meta variants), so the leak scales with the number of files transferred, not with
the number of commands.

## Failure scenario
`cvs checkout` of a large module over the network: `handle_updated` ->
`call_in_directory(..., update_entries, ...)` runs once per file, and each call leaks the
`time_stamp()` result (a `ctime`-style string, ~25-30 bytes plus allocator overhead).
A 500,000-file checkout leaks on the order of 20-30 MB of RSS in the client for no reason;
long-lived client invocations (a full `cvs checkout` of a monorepo, or `cvs update -d` over
a very large tree) grow monotonically. It is not a correctness failure, only unbounded
growth within a single command.

## Suggested fix
Delete the dead variable in all three functions:

```cpp
	char *local_timestamp;
-	char *localtime_timestamp;
	char *file_timestamp;
	...
	    file_timestamp = time_stamp (file_mtime,0);
-	    localtime_timestamp = time_stamp (file_mtime,1);
```
(client.cpp:1995 + 2006, client.cpp:2503 + 2512, client.cpp:2720 + 2730)

## Refutation attempt
* Could `time_stamp` return a static buffer, making the "leak" a non-leak? No —
  `client.cpp:2269 static char *time_stamp (time_t mtime, int local)` builds the string and
  returns `xstrdup (buf)` / an `xmalloc`'d buffer, and its sibling result `file_timestamp`
  is explicitly `xfree`'d at client.cpp:2095-2096, confirming caller ownership.
* Is it consumed through an out-parameter or a macro? No — the six `grep` hits are the only
  mentions of the identifier in the file, and none of them takes its address.
* Is the leak bounded because the process is short-lived? The client process lives for the
  whole command; a single `checkout`/`update` can transfer hundreds of thousands of files
  in one process, so the growth is per-file, not per-process.
* Severity: no corruption, no wrong output — filed `low` accordingly.
