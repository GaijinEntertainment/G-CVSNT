---
id: BUG-update-02
area: client/update core
file: cvsnt/cvsnt-2.5.05.3744/src/update.cpp
line: 1899
severity: low
category: logic
verdict: CONFIRMED
fix_size_loc: 3
behavior_change: yes
---

# Inserted `TRACE()` statement stole the body of `if (!existence_error (errno))`, making the error unconditional

## Summary
In `checkout_file()`'s backup cleanup, a debug `TRACE()` line was inserted directly
under `if (!existence_error (errno))`. Because neither the `if` nor the following
`error()` call is braced, the guard now controls only the trace statement and
`error (0, errno, "error removing %s", backup)` is emitted unconditionally — including
for the benign ENOENT case the guard exists to suppress.

## Code
```cpp
/* src/update.cpp:1888-1902 */
	if (backup != NULL)
	{
		TRACE(3,"checkout_file: there was a backup...");
		/* If -f/-t wrappers are being used to wrap up a directory,
		then backup might be a directory instead of just a file.  */
		if (unlink_file_dir (backup) < 0)
		{
			TRACE(3,"checkout_file: If -f/-t wrappers are being used to wrap up a directory,");
			TRACE(3,"               then backup might be a directory instead of just a file.  ");
			/* Not sure if the existence_error check is needed here.  */
			if (!existence_error (errno))
			TRACE(3,"checkout_file: Not sure if the existence_error check is needed here.  ");   /* 1899 */
			/* FIXME: should include update_dir in message.  */
			error (0, errno, "error removing %s", backup);                                       /* 1901 <-- now unguarded */
		}
		TRACE(3,"checkout_file: free the backup.");
		xfree (backup);
	}
```

The *same* idiom 280 lines earlier in the same function is still intact and shows the
intended shape:

```cpp
/* src/update.cpp:1618-1624 */
	    if (unlink_file_dir (backup) < 0)
	    {
		/* Not sure if the existence_error check is needed here.  */
		if (!existence_error (errno))
		    /* FIXME: should include update_dir in message.  */
		    error (0, errno, "error removing %s", backup);
	    }
```

## Why it is a bug
`TRACE` is not a macro that swallows the following statement — `cvs.h:1103` defines
`#define TRACE cvs_trace`, i.e. a plain function call. So the `if` at 1898 binds to the
single statement at 1899 (the trace), and 1901 executes on every `unlink_file_dir()`
failure regardless of `errno`.

The comment block `/* FIXME: should include update_dir in message. */` that used to sit
between the guard and the `error()` call is still there, confirming the `error()` was
meant to be the guarded statement.

## Failure scenario
`cvs update` in a non-server, non-pipeout, non-`-rcs` working directory (the normal
local-repository client path). The `CVS/.#file` backup is removed by some other agent
(another `cvs` process cleaning `CVS/`, an editor/AV scanner, or a user `rm`) between
the `rename_file()` at line 1612 and the `unlink_file_dir()` at line 1893. `unlink_file_dir`
returns < 0 with `errno == ENOENT`; the guard was supposed to swallow that, but instead
CVS prints
`cvs update: error removing CVS/.#foo.c: No such file or directory`
for a condition that is explicitly documented as harmless. Under `-t`/`-f` wrappers
where `backup` is a directory, the same spurious message appears for an empty/absent
backup directory.

## Suggested fix
```cpp
		if (unlink_file_dir (backup) < 0)
		{
			TRACE(3,"checkout_file: If -f/-t wrappers are being used to wrap up a directory,");
			TRACE(3,"               then backup might be a directory instead of just a file.  ");
			/* Not sure if the existence_error check is needed here.  */
			if (!existence_error (errno))
			{
				TRACE(3,"checkout_file: Not sure if the existence_error check is needed here.  ");
				/* FIXME: should include update_dir in message.  */
				error (0, errno, "error removing %s", backup);
			}
		}
```

## Refutation attempt
* Could `TRACE` be a macro that consumes the trailing statement (e.g.
  `#define TRACE(...) if (0) something; else`)? No — `cvs.h:1103` is simply
  `#define TRACE cvs_trace`, so `TRACE(3,"...")` is one complete expression statement.
* Is the path unreachable? No: `backup` is non-NULL exactly when
  `!is_rcs && !pipeout && !server_active` and `isfile(xfile)` held at line 1611, which is
  the ordinary local-repository `cvs update` of an existing working file. The failure
  branch only needs `unlink_file_dir()` to fail, which is what `existence_error()` is
  there to filter.
* Does `error(0, ...)` at least not change the exit code? Correct, it only prints —
  hence severity `low`. It is still a user-visible false error and, on the server side,
  an extra `E` protocol line.
