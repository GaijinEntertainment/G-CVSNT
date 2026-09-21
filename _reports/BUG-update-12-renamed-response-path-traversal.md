---
id: BUG-update-12
area: client/update core
file: cvsnt/cvsnt-2.5.05.3744/src/client.cpp
line: 3015
severity: high
category: correctness
verdict: CONFIRMED
fix_size_loc: 4
behavior_change: yes
---

# `rename_entry_and_file()` accepts an unvalidated destination path from the server — the `Renamed` response can write outside the working copy

## Summary
The handler for the server's `Renamed` response reads the new name straight off the wire and
passes it to `CVS_RENAME()` and `Rename_Entry()` with no check that it is a bare filename.
Its sibling handler `copy_a_file()` (the `Copy-file` response) performs exactly that check,
with a comment explaining why. A malicious or compromised server can therefore move a file
it just placed in the working copy to any path reachable by relative traversal.

## Code
```cpp
/* src/client.cpp:3015-3032 — no validation of `renamed_to` */
static void rename_entry_and_file (char *data, List *ent_list, char *short_pathname, char *filename)
{
	char *renamed_to;

    read_line (&renamed_to);

    Rename_Entry (ent_list, filename, renamed_to);

    /* Note that we don't ignore existence_error's here. ... */
    if (CVS_RENAME(filename,renamed_to) < 0)
		error (0, errno, "unable to rename %s", short_pathname);

	xfree(renamed_to);
}
```

The check that is missing is spelled out 1800 lines earlier for the analogous response
(client.cpp:1190-1201):

```cpp
static void copy_a_file (char *data, List *ent_list, char *short_pathname, char *filename)
{
    char *newname;

    read_line (&newname);

    /* cvsclient.texi has said for a long time that newname must be in the
       same directory.  Wouldn't want a malicious or buggy server overwriting
       ~/.profile, /etc/passwd, or anything like that.  */
    if (last_component (newname) != newname)
	error (1, 0, "protocol error: Copy-file tried to specify directory");

    copy_file (filename, newname, 1, 1);
    xfree (newname);
}
```

## Why it is a bug
`renamed_to` is attacker-controlled protocol data: `handle_renamed()` (client.cpp:3039) is
the dispatcher for the `"Renamed"` response (registered at client.cpp:4051), and the
destination is read by `read_line (&renamed_to)` *inside* the callback — after
`call_in_directory()` has already `chdir`'d into the target working directory.

`call_in_directory()` does validate its own argument (client.cpp:875-881):
```cpp
	if(isabsolute(pathname) || pathname_levels(pathname)>client_max_dotdot)
    {
		error (0, 0,
               "Server attempted to update a file via an invalid pathname:");
        error (1, 0, "'%s'.", pathname);
    }
```
but that guard applies to the `Renamed` response's *pathname* argument, not to the
second line carrying `renamed_to`. Nothing between `read_line` and `CVS_RENAME` inspects it.

The platform wrappers do not help either:
* POSIX: `lib/system.h:427` `#define CVS_RENAME rename` — a raw `rename(2)`.
* Windows: `windows-NT/config.h:284` `#define CVS_RENAME wnt_rename`, which calls
  `validate_filename()` (windows-NT/filesubr.cpp:671-673). `validate_filename()`
  (windows-NT/win32.cpp:464-497) only rejects DOS device names (`CON`, `AUX`, `COM1`, …)
  and the characters `"<>|`. It explicitly skips past directory separators before checking,
  so `..\..\..\file` passes.

The fact that the CVS client treats the server as untrusted for exactly this class of
attack is established policy in this file — see the `copy_a_file` comment above, the
`pathname_levels`/`client_max_dotdot` guard, and `is_cvsroot_level`.

## Failure scenario
A user runs `cvs update` (or `cvs checkout`) against a hostile or compromised `:pserver:` /
`:ext:` server — for example a mirror, a shared build server, or a server whose repository
an attacker can write to and which supports the `Renamed` response.

The server answers with two responses:

1. `Updated ./`, repository line, entries line, mode, size, contents — content chosen by
   the attacker, written as `payload` in the user's working directory. (This part is
   entirely legitimate CVS traffic.)
2. `Renamed ./`
   `<repos>/payload`
   `payload`
   `../../../../../../home/victim/.bashrc`

`handle_renamed` -> `call_in_directory("./", rename_entry_and_file, NULL)` -> the callback
reads `renamed_to = "../../../../../../home/victim/.bashrc"` and executes
`CVS_RENAME("payload", "../../../../../../home/victim/.bashrc")`.

Result: an arbitrary file outside the checkout is overwritten with attacker-chosen content;
with `.bashrc`/`.profile`/`~/.ssh/authorized_keys` as the target this is client-side code
execution. `Rename_Entry (ent_list, filename, renamed_to)` additionally writes the traversal
string into `CVS/Entries` as an entry name.

Even without an attacker, a buggy server that sends a path in `renamed_to` silently
corrupts the working copy instead of producing the "protocol error" that `Copy-file`
produces.

## Suggested fix
```cpp
static void rename_entry_and_file (char *data, List *ent_list, char *short_pathname, char *filename)
{
	char *renamed_to;

    read_line (&renamed_to);

    /* As for Copy-file: the new name must be in the same directory, so that a
       malicious or buggy server cannot overwrite ~/.profile or the like.  */
    if (last_component (renamed_to) != renamed_to
        || isabsolute (renamed_to)
        || strcmp (renamed_to, "..") == 0)
	error (1, 0, "protocol error: Renamed tried to specify directory");

    Rename_Entry (ent_list, filename, renamed_to);
    ...
```
(`last_component()` in `filesubr.cpp` only splits on `/`; the Windows build's
`windows-NT/filesubr.cpp` version handles `\` as well, so the check is correct on both.)

## Refutation attempt
* Is the destination validated somewhere upstream? `handle_renamed` (client.cpp:3039) does
  nothing but `call_in_directory (args, rename_entry_and_file, (char *)NULL);`, and
  `call_in_directory` reads only the repository line and validates only `pathname`
  (client.cpp:844-881). `renamed_to` is read later, at client.cpp:3019.
* Could `CVS_RENAME` refuse a traversing path? `rename(2)` happily follows `..`. The
  Windows `wnt_rename` -> `validate_filename` path checks only reserved names and
  `"<>|` and deliberately skips leading directory components
  (`for(p=path+strlen(path)-1;p>=path;--p) if(ISDIRSEP(*p)) break; p++;`).
* Is `Renamed` reachable in a normal session? It is `rs_optional` in the response table
  (client.cpp:4051), so the client advertises it in `Valid-responses` and any server may
  use it. CVSNT's own server emits it via `server_rename_file` (see
  `update.cpp:1150-1153`, `send_client_rename`).
* Does `Rename_Entry` sanity-check the name? No — entries.cpp:274-330 only does
  `findnode_fn`, `xstrdup(to)` and list surgery.
* Could `client_max_dotdot` already forbid `..`? That variable gates
  `call_in_directory`'s `pathname` only (client.cpp:875); `renamed_to` never reaches
  `pathname_levels()`.
