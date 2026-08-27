---
id: BUG-update-08
area: client/update core
file: cvsnt/cvsnt-2.5.05.3744/src/entries.cpp
line: 257
severity: high
category: correctness
verdict: CONFIRMED
fix_size_loc: 4
behavior_change: yes
---

# Every `Entries.Log` record is written twice; the unprefixed second copy replays as an implicit `A`, resurrecting scratched/renamed entries

## Summary
`Scratch_Entry`, `Rename_Entry` and `Register` call **both** `write_ent_proc()` and
`write_ent_ex_proc()`, but `write_ent_ex_proc()` already writes the `Entries` line itself.
Each `R `/`A ` command in `CVS/Entries.Log` is therefore followed by a second, *unprefixed*
copy of the same entry line, and `fgetentent()` interprets an unprefixed line as an
**add**. A logged removal is immediately undone when the log is replayed.

## Code
```cpp
/* src/entries.cpp:102-134 — write_ent_ex_proc writes BOTH files */
static int write_ent_proc (Node *node, void *closure)
{
    ...
    if (fputentent(entfile, entnode))
		error (1, errno, "cannot write %s", entfilename);
    return (0);
}

static int write_ent_ex_proc (Node *node, void *closure)
{
    ...
    if (fputentent(entfile, entnode))          /* 129 — same write as write_ent_proc */
		error (1, errno, "cannot write %s", entfilename);
    if (fputententex(entexfile, entnode))      /* 131 */
		error (1, errno, "cannot write %s", entexfilename);
    return (0);
}

/* src/entries.cpp:252-258 — Scratch_Entry */
			if (fprintf (entfile, "R ") < 0)
				error (1, errno, "cannot write %s", entfilename);
			if (fprintf (entexfile, "R ") < 0)
				error (1, errno, "cannot write %s", entexfilename);

			write_ent_proc (node, NULL);       /* 257 -> "R /foo/1.5/...//\n"   */
			write_ent_ex_proc (node, NULL);    /* 258 -> "/foo/1.5/...//\n" AGAIN */
```
The same duplicated pair appears at entries.cpp:299-300 and 310-311 (`Rename_Entry`) and
entries.cpp:422-423 (`Register`).

## Why it is a bug
`write_entries()` — the function that rewrites the whole `Entries` file — walks the list
with **only** `write_ent_ex_proc` (entries.cpp:193):
```cpp
	(void) walklist (list, write_ent_ex_proc, (void *) &sawdir);
```
which is correct precisely because `write_ent_ex_proc` emits both the `Entries` line and the
`Entries.Extra` line. The three command-log sites then add a *redundant* `write_ent_proc`
call on top of it.

`subdir_record()` (entries.cpp:1366-1370) shows the intended log format unambiguously — one
command prefix, one entry line:
```cpp
	if (fprintf (entfile, "%c ", cmd) < 0)
	    error (1, errno, "cannot write %s", entfilename);
	if (fputentent (entfile, entnode) != 0)
	    error (1, errno, "cannot write %s", entfilename);
```

`fputentent()` terminates every record with `\n` (entries.cpp:729-745), so the two calls
produce two separate lines. When `Entries_Open()` replays the log (entries.cpp:884-909) it
uses `fgetentent (fpin, &cmd, &sawdir)`, and `fgetentent` assigns the command like this
(entries.cpp:611-620):
```cpp
	if (cmd != NULL)
	{
	    if (l[1] != ' ')
		*cmd = 'A';          /* <-- an unprefixed line is an ADD */
	    else
	    {
		*cmd = l[0];
		l += 2;
	    }
	}
```
The duplicate line starts `/foo/...`, so `l[1]` is `'f'`, not `' '` — it is read as an
`A` command and `AddEntryNode()` puts the entry straight back.

## Failure scenario
Client-side working directory. A file is deleted in the repository, so `cvs update` calls
`scratch_file()` -> `Scratch_Entry()` (update.cpp:1697).

`CVS/Entries.Log` receives:
```
R /gone.c/1.5/Mon Jan  1 10:00:00 2024//
/gone.c/1.5/Mon Jan  1 10:00:00 2024//
```

1. The user interrupts the update (Ctrl-C), the connection drops, or the disk fills before
   `Entries_Close()` runs — so `write_entries()` never collapses the log and
   `CVS/Entries.Log` survives.
2. The next `cvs` command in that directory calls `Entries_Open()`, which replays the log:
   line 1 `cmd == 'R'` deletes the `gone.c` node; line 2 `cmd == 'A'` **re-adds it**.
3. `do_rewrite` is set, so `write_entries()` immediately persists the resurrected entry
   into `CVS/Entries`. `gone.c` is now permanently back in `Entries` with no working file,
   and every subsequent `cvs update` reports it as needing checkout / "no longer pertinent"
   noise, or (worse) `cvs commit` treats the stale entry as live.

`Rename_Entry` is corrupted even without an interruption in the *log's own* semantics: it
writes
```
R /old.c/...       <- remove old
/old.c/...         <- implicit ADD, puts old.c straight back
A /new.c/...       <- add new
/new.c/...         <- redundant duplicate add
```
so a replayed rename leaves **both** names registered.

For `Register` the duplicate is benign (adding the same entry twice is idempotent), which is
why the bug is easy to miss — only the `R` commands are damaged.

## Suggested fix
Drop the redundant `write_ent_proc()` calls; `write_ent_ex_proc()` already covers both
files, exactly as `write_entries()` assumes.

```cpp
/* entries.cpp:257-258 (Scratch_Entry), 299-300 and 310-311 (Rename_Entry),
   422-423 (Register) — in each place delete the write_ent_proc line: */
-			write_ent_proc (node, NULL);
			write_ent_ex_proc (node, NULL);
```

## Refutation attempt
* Could `fputentent` omit the trailing newline so the two calls merge into one line? No —
  all three tails of `fputentent` (entries.cpp:729-745) print `"T%s\n"`, `"D%s\n"` or
  `"\n"`, so each call ends a line.
* Could `fgetentent` skip a line that has no command prefix? No — the only `continue`s are
  for lines that do not start with `/` after optional `D`, and the duplicate does start
  with `/`. The `if (l[1] != ' ') *cmd = 'A';` branch exists specifically to accept such
  lines ("For backward compatibility, the absence of a space indicates an add command").
* Is `Entries.Log` ever actually replayed, or always collapsed first? `Entries_Open()`
  unconditionally opens `CVSADM_ENTLOG` and replays it whenever it exists
  (entries.cpp:883-909); `Entries_Close()` only collapses it at the *end* of the run
  (entries.cpp:962-970). Any abnormal termination between the two — and the log is
  explicitly designed to survive one, that is its whole purpose — leaves the poisoned log
  for the next command.
* Could `write_ent_ex_proc`'s `fputentent(entfile,...)` be the accidental line instead?
  Removing it would break `write_entries()` (entries.cpp:193), which relies on that single
  walk to produce `CVS/Entries`. The four `write_ent_proc` call sites are the odd ones out.
* Does `AddEntryNode` reject duplicates so the second `A` is ignored? It is reached for the
  duplicate line with a fresh `Entnode` and replaces/keeps the entry either way — the
  removal performed by the preceding `R` is undone regardless.
