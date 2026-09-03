---
id: BUG-server-16
area: import
file: cvsnt/cvsnt-2.5.05.3744/src/import.cpp
line: 1622
severity: medium
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 5
behavior_change: no
---

# `add_rcs_file` write-error path calls `fclose(fpuser)` and `fn_root(userfile)` without the NULL guard the success path has

## Summary
`fpuser` is deliberately left `NULL` whenever `add_rcs_file` is called with no source file or with a checkin callback. The normal exit guards the close (`if (fpuser != NULL)`), but the `write_error` / `write_error_noclose` labels do not — so any I/O failure while writing the new `,v` file turns into `fclose(NULL)` plus `fn_root(NULL)`.

## Code
```cpp
// import.cpp:1257-1262 — fpuser is explicitly allowed to be NULL
	if(callback || !userfile)
	{
		fpuser = NULL;
		memset(&sb,0,sizeof(sb));
		sb.st_mode=0644;
	}
```
```cpp
// import.cpp:1603-1608 — normal exit gets it right
    /* Close fpuser only if we opened it to begin with. */
    if (fpuser != NULL)
    {
		if (fclose (fpuser) < 0)
			error (0, errno, "cannot close %s", userfile);
    }
```
```cpp
// import.cpp:1617-1623 — error exit does not
write_error:
    ierrno = errno;
    if (fclose (fprcs) < 0)
		error (0, errno, "cannot close %s", fn_root(rcs));
write_error_noclose:
    if (fclose (fpuser) < 0)                                   // <-- fpuser may be NULL
		error (0, errno, "cannot close %s", fn_root(userfile));  // <-- userfile may be NULL
```

The comment on the guarded copy ("Close fpuser only if we opened it to begin with") states the invariant that the error path violates.

## Why it is a bug
`fclose(NULL)` is undefined behaviour: glibc dereferences the `FILE *` immediately and faults; the MSVC CRT raises an invalid-parameter assertion (or faults in a release build). There is no path that sets `fpuser` to a valid stream after the `if(callback || !userfile)` branch takes it to NULL — that branch is exclusive with the `fopen` in the `else`.

`write_error_noclose` is also reached from the very first thing that can fail:
```cpp
// import.cpp:1294-1299
    fprcs = fopen (rcs, "w+b");
    if (fprcs == NULL)
    {
	ierrno = errno;
	goto write_error_noclose;
    }
```
so the crash happens on the most likely failure of all — being unable to create the RCS file.

## Failure scenario
`create_mapping_file` (mapping.cpp:1295-1305) calls:

```cpp
		add_rcs_file(message?message:"created", fn, NULL, "1.1", NULL, NULL, NULL, 0, NULL, "mapping file", 12, NULL, NULL);
```

with `userfile == NULL`, so `fpuser` is NULL for the whole call. This runs server-side whenever CVSNT needs to create the per-directory mapping file (`.directory_history,v`) — i.e. on the first rename/add in a directory that does not yet have one.

Now make the write fail — the repository directory is read-only for the CVS user, the filesystem is full, or the volume is mounted read-only. `fopen (rcs, "w+b")` returns NULL, control jumps to `write_error_noclose`, and the server calls `fclose(NULL)`. Instead of the intended `ERROR: cannot write file <path>` message (and the `ENOSPC` cleanup below it, which never runs), the server process dies with SIGSEGV mid-commit, leaving the directory write lock to be cleaned up by the signal handler at best.

The same applies to `checkaddfile`'s call at commit.cpp:2209 whenever a non-NULL `callback` is passed, since `if(callback || ...)` also NULLs `fpuser`.

## Suggested fix
```cpp
write_error_noclose:
    if (fpuser != NULL && fclose (fpuser) < 0)
		error (0, errno, "cannot close %s", fn_root(userfile));
```

## Refutation attempt
* *Could `fpuser` be non-NULL on every path that reaches these labels?* No. `fpuser = NULL` is set unconditionally at import.cpp:1259 when `callback || !userfile`, and every `goto write_error*` below that point is reachable from that state — including the `fopen` failure at import.cpp:1298, four lines after the assignment.
* *Is `fclose(NULL)` benign on some libc?* It is undefined behaviour; glibc, musl and the MSVC CRT all dereference the argument. Nothing in this tree wraps `fclose`.
* *Are the two other `goto read_error` sites affected?* No — import.cpp:1274 and 1289 are inside the `else` branch where `userfile` is non-NULL, and `read_error` does not touch `fpuser` at all. (That does mean `fpuser` leaks if a later `read_error` were ever added, but as written there is none.)
* *Does `error (0, ...)` before the fclose save us?* No; the `fclose` is the first statement at the label.
