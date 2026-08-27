# `update -C` silently disables the "in the way" guard for the whole run (client_overwrite_existing never reset)

- **File:** cvsnt/cvsnt-2.5.05.3744/src/client.cpp
- **Line(s):** 37, 5434-5448, 1544, 2406 (and update.cpp:376)
- **Severity:** medium
- **Confidence:** high
- **Category:** logic

## Code
```cpp
// client.cpp:37 — process-global, one instance
int client_overwrite_existing;

// update.cpp:376 — initialized once before the send phase
client_overwrite_existing = case_sensitive;

// client.cpp:5434-5448 — send_fileproc(), for a locally MODIFIED tracked file under -C
if (args->backup_modified)
{
    if (backup_local_files)
    {
        char *bakname = backup_file (filename, vers->vn_user);
        if (! really_quiet)
            printf ("(Locally modified %s moved to %s)\n", filename, bakname);
        xfree (bakname);
    }
    client_overwrite_existing = 1;      // <-- set to 1, never reset anywhere
}

// client.cpp:1544 (and identical at 2406 for the blob path) — receive phase guard
if (data->existp == UPDATE_ENTRIES_NEW && !client_overwrite_existing && isfile (filename))
{
    error (0, 0, "move away %s; it is in the way", short_pathname);
    ... discard downloaded file, set failure_exit = 1 ...
}
```

## Why this is a bug
`client_overwrite_existing` is a single process-global (client.cpp:37). Its
only assignments are the one-time init from `case_sensitive` (update.cpp:376,
normally 0) and the set-to-1 at client.cpp:5447. **Nothing ever sets it back
to 0.**

During `cvs update -C`, the entire client send-phase recursion
(`send_files`) completes before any server response is processed. In that send
phase, `send_fileproc` runs the block above for every *locally modified tracked*
file and sets `client_overwrite_existing = 1`. So as soon as **one** tracked
file in the tree is locally modified, the flag latches to 1 for the rest of the
invocation.

Then in the receive phase, the guard at client.cpp:1544 (inline content) and
client.cpp:2406 (blob-ref) — which is meant to refuse to overwrite an
*untracked* local file that happens to sit where a newly-added repository file
will be created (`Created` / `Blob-ref-created`, `existp == UPDATE_ENTRIES_NEW`)
— is `!client_overwrite_existing`, now false. The guard is skipped, the
"move away X; it is in the way" protection is gone, and the untracked local
file is silently overwritten with the repository copy.

Failure scenario: a developer runs `cvs update -C` in a tree where (a) they
have one locally-modified tracked file (correctly reverted by `-C`), and (b) an
unrelated, never-added local file `foo.bin` happens to share a name with a file
just added to the repository in that directory. `foo.bin` is silently replaced.
Without the modified tracked file, the same `foo.bin` would instead be preserved
and reported as "move away foo.bin; it is in the way".

Two things make this clearly unintended rather than a deliberate `-C` semantic:
1. It is triggered as a **side effect of an unrelated file** being modified —
   the untracked-file guard's behavior depends on whether some *other* file in
   the run happened to be dirty.
2. It is **inconsistent with the server/local code path**, where `-C`
   (`toss_local_changes`) is not consulted in the `T_CONFLICT`/"in the way"
   branch at all (classify.cpp:107-114, update.cpp T_CONFLICT case), so a local
   or direct-repository `update -C` does *not* overwrite untracked in-the-way
   files. Only the networked client does, and only by accident.

The documented meaning of `-C` (update.cpp:137: "Overwrite locally modified
files with clean repository copies") is about modified *tracked* files, not
untracked ones.

## Suggested fix
Make the overwrite decision per-file instead of via a latched global. Either
carry an "overwrite this file" bit through `struct send_data` / the response
handler for the specific files being reverted, or, at minimum, reset
`client_overwrite_existing` back to its initial value after the send phase so
the receive-phase guard is not disabled by an unrelated modified file:

```cpp
// after send_files() returns in update.cpp (remote branch), before reading responses:
client_overwrite_existing = case_sensitive;
```

Note this interacts with the case-insensitive branch at client.cpp:1539/2402,
which *relies* on `client_overwrite_existing` being set to allow a
case-differing overwrite; a correct fix should preserve that path (e.g. a
separate flag for the case-fold overwrite vs. the untracked-in-the-way guard).
