---
id: BUG-update-03
area: client/update core
file: cvsnt/cvsnt-2.5.05.3744/src/update.cpp
line: 2246
severity: medium
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 2
behavior_change: no
---

# `patch_file_write()` reads `buffer[len - 1]` without checking `len != 0` — out-of-bounds read on empty revisions

## Summary
`patch_file_write` is the `RCSCHECKOUTPROC` callback used by the server's `patch_file()`.
It unconditionally dereferences `buffer[len - 1]`. `RCS_checkout()` in this fork calls the
callback even when `len == 0` (passing the empty string literal `""`), so a zero-byte
revision produces `buffer[(size_t)-1]`, i.e. a read one byte *before* a read-only string
literal, and a garbage value for `data->final_nl`.

## Code
```cpp
/* src/update.cpp:2239-2250 */
static void patch_file_write (void *callerdat, const char *buffer, size_t len)
{
    struct patch_file_data *data = (struct patch_file_data *) callerdat;

    if (fwrite (buffer, 1, len, data->fp) != len)
	error (1, errno, "cannot write %s", data->filename);

    data->final_nl = (buffer[len - 1] == '\n');     /* 2246 <-- len==0 => buffer[SIZE_MAX] */

    if (data->compute_checksum)
		data->md5->Update(buffer,len);
}
```

The producer side (`src/rcs_checkin.cpp:230-235`) calls the callback unconditionally:

```cpp
    if (pfn != NULL)
    {
		/* The PFN interface is very simple to implement right now, as
			we always have the entire file in memory.  */
		pfn(callerdat, len?value:"", len);      /* len may be 0; buffer is then "" */
    }
```

## Why it is a bug
`len` is `size_t` (unsigned). `len - 1` for `len == 0` is `SIZE_MAX`, so
`buffer[len - 1]` indexes far past the object; in practice the compiler folds it to
`buffer[-1]` (one byte before the `""` literal in `.rodata`). This is undefined
behaviour, trips ASan/Valgrind, and yields a meaningless `final_nl`.

The two `RCS_checkout()` calls in `patch_file()` (update.cpp:2048-2051 and 2071-2074)
pass `patch_file_write` as `pfn` with `workfile == NULL` and `sout == RUN_TTY`, which is
exactly the `pfn != NULL` branch above. Note the fork's `pfn(callerdat, len?value:"", len)`
deliberately substitutes `""` for a NULL/zero-length value — it *guarantees* the pointer
is valid but it does not guarantee `len > 0`, and the callback assumes it does.

`data->final_nl` drives the `fail` flag:
```cpp
	if (retcode != 0 || ! data.final_nl)
	    fail = 1;
```
so the garbage byte decides whether the server sends a diff/patch or falls back to a
full checkout. When the stale byte happens to be `'\n'`, `final_nl` is wrongly 1 for a
file with no trailing newline, which is precisely the case the check exists to reject.

## Failure scenario
Server-side `cvs update -u` (i.e. `patches != 0`, `T_PATCH` status) on a text file whose
*currently checked-out* revision is zero bytes:

1. `cvs add empty.txt` (0 bytes), `cvs ci` -> rev 1.1 is empty.
2. Someone else commits content -> rev 1.2.
3. The first user runs `cvs update`; `Classify_File` returns `T_PATCH`,
   `update_fileproc` (update.cpp:2868-2876 region) calls `patch_file()`.
4. `patch_file()` checks out `vers_ts->vn_user` (= 1.1) into `file1` via
   `RCS_checkout(..., patch_file_write, &data, &mode)`.
5. `RCS_checkout_raw_value` yields `len == 0`; `rcs_checkin.cpp:234` calls
   `pfn(callerdat, "", 0)`.
6. `patch_file_write` executes `buffer[(size_t)0 - 1]` -> OOB read of `.rodata`
   preceding the `""` literal. Under ASan this aborts the server child handling that
   client; without ASan `data.final_nl` is whatever byte happened to be there.

The same happens whenever any revision reachable through `patch_file` is empty
(e.g. a file truncated to zero bytes and committed).

## Suggested fix
```cpp
static void patch_file_write (void *callerdat, const char *buffer, size_t len)
{
    struct patch_file_data *data = (struct patch_file_data *) callerdat;

    if (len == 0)
	return;			/* nothing written; leave final_nl as the caller set it (0) */

    if (fwrite (buffer, 1, len, data->fp) != len)
	error (1, errno, "cannot write %s", data->filename);

    data->final_nl = (buffer[len - 1] == '\n');

    if (data->compute_checksum)
		data->md5->Update(buffer,len);
}
```
(`patch_file()` already initialises `data.final_nl = 0` before each `RCS_checkout()`
at update.cpp:2043 and 2066, so the early return leaves the correct "no trailing
newline" verdict.)

## Refutation attempt
* Could `RCS_checkout` guard the call with `if (len != 0)` like stock CVS does? Not in
  this tree — `rcs_checkin.cpp:234` is `pfn(callerdat, len?value:"", len);` with no
  length guard. The `len?value:""` ternary shows the author was thinking about the
  zero case for the *pointer* and missed the *index*.
* Could `patch_file()` short-circuit before ever reaching a zero-length revision? The
  early returns cover `noexec || pipeout || joining()`, `KFLAG_BINARY`, a missing
  `vn_user` revision, and `RCS_isdead`. None of them exclude a valid-but-empty revision.
* Is `patch_file` dead code? No — it is reached from `update_fileproc`'s `T_PATCH` case
  whenever `patches` is set, which `update()` sets when the client sends `-u`
  (update.cpp:270-277), and modern clients do send `-u` because the server advertises
  `update-patches`.
* Is the read harmless because `""` is in a page that is always mapped? Not guaranteed —
  a string literal can be the first object in its section, and this is UB regardless.
  It is also the input to a security-relevant decision (`final_nl` -> `fail`).
