---
id: BUG-update-10
area: client/update core
file: cvsnt/cvsnt-2.5.05.3744/src/client.cpp
line: 1938
severity: high
category: logic
verdict: CONFIRMED
fix_size_loc: 2
behavior_change: yes
---

# `update_entries()` applies the wire `mode_string` only when a `Mode` response was *also* received, so checked-out file permissions are never set

## Summary
In `update_entries()` the block that chmods a freshly written file to the mode the server
sent inline with the `Updated`/`Created`/`Merged`/`Rcs-diff` response is wrapped in
`if (stored_mode != NULL)`. `stored_mode` comes from a *separate* `Mode` response, which the
server only emits before `Checked-in` — a response type that never reaches this code path.
The inline mode is therefore either skipped entirely, or (when `stored_mode` does exist)
applied and then immediately overwritten by `stored_mode` twelve lines later. Either way
`change_mode (filename, mode_string, 1)` can never have an observable effect, and updated
files keep the `0777 & ~umask` mode of the temp file they were renamed from.

## Code
```cpp
/* src/client.cpp:1938-1955 */
    if (stored_mode != NULL)                                  /* 1938 <-- inverted guard */
    {
	    int status = change_mode (filename, mode_string, 1);  /* 1940 */
	    if (status != 0)
			error (0, status, "cannot change mode of %s", short_pathname);
	}

	xfree (mode_string);
	if (buf)
	    xfree (buf);
    }

    if (stored_mode != NULL)
    {
	change_mode (filename, stored_mode, 1);                   /* 1952 — overrides 1940 */
	xfree (stored_mode);
	stored_mode = NULL;
    }
```

The mode is read off the wire unconditionally for every content-carrying response
(client.cpp:1512-1519):
```cpp
	char *mode_string;
	...
	read_line (&mode_string);
```
and the file itself is created with a wide-open mode (client.cpp:1688-1692):
```cpp
	    fd = CVS_OPEN (temp_filename,
			   (O_WRONLY | O_CREAT | O_TRUNC
			    | (open_binary ? OPEN_BINARY : 0)),
			   0777);
```

## Why it is a bug
`stored_mode` is set only by `handle_mode()` (client.cpp:1351-1356), which handles the
`Mode` response. The server emits `Mode` from exactly one place —
`checked_in_response()` (server.cpp:3667-3685) — immediately before a `Checked-in`
response. `Checked-in` maps to `UPDATE_ENTRIES_CHECKIN` (client.cpp:2826-2833), and the
entire block containing line 1938 is inside

```cpp
    if (data->contents == UPDATE_ENTRIES_UPDATE
	|| data->contents == UPDATE_ENTRIES_PATCH
	|| data->contents == UPDATE_ENTRIES_RCS_DIFF)
```
(client.cpp:1505-1508) — i.e. it never runs for `Checked-in`. So on the `Updated` path
`stored_mode` is NULL and line 1940 is dead; on the (unreachable-here) path where it is
non-NULL, line 1952 immediately re-chmods with the other value. The statement is
unconditionally useless.

The fork's own newer code shows the intended precedence — `update_blob_ref_entries()`
(client.cpp:2467-2470) does:
```cpp
    add_download_queue(short_pathname, filename, blob_ref+hash_type_magic_len,
      stored_mode ? stored_mode : mode_string,
      file_mtime,
      blob_downloaded_no_write);
```
`stored_mode` if present, **otherwise `mode_string`** — which is precisely what line 1938
inverts.

Nothing else restores the mode afterwards: after `rename_file (temp_filename, filename)`
(client.cpp:1806) the file carries the `0777`-minus-umask mode from `CVS_OPEN`, and the
next chmod-ish call in `update_entries` is the (skipped) line 1940.

## Failure scenario
POSIX client, default `umask 022`, ordinary remote checkout:

```
cvs -d :pserver:host:/repo checkout proj
ls -l proj/README.txt
```

1. The server sends `Updated proj/README.txt\n/repo/proj\n/README.txt/1.1/...\nu=rw,g=r,o=r\n1234\n<data>`.
2. `update_entries()` reads `mode_string = "u=rw,g=r,o=r"` (client.cpp:1519) and creates
   the temp file with `CVS_OPEN(..., 0777)` -> mode `0755` after umask.
3. `rename_file` moves it into place as `README.txt`, mode `0755`.
4. `stored_mode` is NULL (no `Mode` response accompanies `Updated`), so line 1940 is
   skipped and line 1952 is skipped.
5. Result: `-rwxr-xr-x README.txt` — a plain text file checked out **executable**, instead
   of the `-rw-r--r--` the repository records.

Every non-executable file in every remote checkout/update on a POSIX client is affected;
conversely a repository file that should be `u=rwx` and a file that should be `u=rw` become
indistinguishable, so the executable bit stops round-tripping through
checkout -> commit (`send_modified` re-derives the mode from the working file at
client.cpp:5060, `mode_string = mode_to_string (sb.st_mode)`), silently flipping the
recorded permissions in the repository on the next commit.

## Suggested fix
Restore the unconditional application (this is the shape stock CVS uses, and matches the
`stored_mode ? stored_mode : mode_string` precedence used by `update_blob_ref_entries`):

```cpp
    {
	    int status = change_mode (filename, mode_string, 1);
	    if (status != 0)
			error (0, status, "cannot change mode of %s", short_pathname);
	}
```
(the later `if (stored_mode != NULL) change_mode (filename, stored_mode, 1);` at
client.cpp:1950-1955 then correctly takes precedence when a `Mode` response was sent.)

## Refutation attempt
* Could `stored_mode` be set by something other than the `Mode` response? `grep -n
  "stored_mode" client.cpp` shows assignments only in `handle_mode` (client.cpp:1355) and
  the various `stored_mode = NULL` resets. `handle_mode` is bound to `"Mode"` in the
  response table at client.cpp:4048.
* Could the server send `Mode` before `Updated`? `grep -n 'Mode ' server.cpp` finds a
  single emitter, `checked_in_response()` at server.cpp:3681, reached only from
  `server_checked_in()`. `server_updated()` does not emit it.
* Is `change_mode` a no-op so it would not matter? Only under `CHMOD_BROKEN` (Windows),
  where it degrades to `xchmod(filename, writeable)`. On the `#else` branch
  (client.cpp:329-397) it does a real `chmod (filename, mode & 0777)`.
* Could the temp file already have the right mode? No — it is opened with a literal `0777`
  and never chmod'd before the rename, so it lands at `0777 & ~umask` regardless of what
  the server said.
* Is the whole `if (contents == UPDATE|PATCH|RCS_DIFF)` block perhaps also entered for
  `Checked-in`? No: `handle_checked_in` (client.cpp:2826) builds
  `dat.contents = UPDATE_ENTRIES_CHECKIN`, which fails the three-way test at
  client.cpp:1505.
