---
id: BUG-update-09
area: client/update core
file: cvsnt/cvsnt-2.5.05.3744/src/entries.cpp
line: 252
severity: medium
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 16
behavior_change: yes
---

# `Scratch_Entry()` and `Rename_Entry()` use the result of `CVS_FOPEN()` without a NULL check — `fprintf(NULL, ...)` crash on a read-only working directory

## Summary
`Scratch_Entry` and `Rename_Entry` open `CVS/Entries.Log` and `CVS/Entries.Extra.Log` in
append mode and pass the returned `FILE *` straight to `fprintf()`. Every other function in
this file (`write_entries`, `Register`, `subdir_record`) tests the handle for NULL first and
degrades to a warning, with an explicit comment explaining that a non-writable working
directory is a supported situation. These two do not, so a failed open is a NULL
dereference.

## Code
```cpp
/* src/entries.cpp:246-258 — Scratch_Entry */
		if (!noexec)
		{
			entfilename = CVSADM_ENTLOG;
			entexfilename = CVSADM_ENTEXTLOG;
			entfile = CVS_FOPEN (entfilename, "a");        /* 249 — may return NULL */
			entexfile = CVS_FOPEN (entexfilename, "a");    /* 250 — may return NULL */

			if (fprintf (entfile, "R ") < 0)               /* 252 — NULL deref */
				error (1, errno, "cannot write %s", entfilename);
			if (fprintf (entexfile, "R ") < 0)             /* 254 — NULL deref */
				error (1, errno, "cannot write %s", entexfilename);
```

```cpp
/* src/entries.cpp:287-297 — Rename_Entry, identical omission */
			entfile = CVS_FOPEN (entfilename, "a");        /* 289 */
			entexfile = CVS_FOPEN (entexfilename, "a");    /* 290 */

			ent = (Entnode*)node->data;

			if (fprintf (entfile, "R ") < 0)               /* 294 */
```

Contrast `Register()`, twelve lines further down, which does exactly the same opens and
*does* check:

```cpp
/* src/entries.cpp:399-415 */
		entfile = CVS_FOPEN (entfilename, "a");
		entexfile = CVS_FOPEN (entexfilename, "a");

		if (entfile == NULL)
		{
			/* Warning, not error, as in write_entries.  */
			/* FIXME-update-dir: should be including update_dir in message.  */
			error (0, errno, "cannot open %s", entfilename);
			return;
		}
		if (entexfile == NULL)
		{
			error (0, errno, "cannot open %s", entexfilename);
			return;
		}
```

and `write_entries()` (entries.cpp:151-168), whose comment spells the scenario out:
*"one user might have checked out a working directory ... A second user, without write
access to that working directory, might want to do a cvs log"*.

## Why it is a bug
`CVS_FOPEN` is a thin wrapper over `fopen` and returns NULL on failure (EACCES on a
read-only `CVS/` directory, EMFILE/ENFILE on descriptor exhaustion, EROFS on a read-only
mount, ENOSPC on some platforms). `fprintf(NULL, "R ")` dereferences the `FILE` object
inside the C library and segfaults; there is no defined behaviour and no `errno` path that
the following `error (1, errno, ...)` could ever report.

The codebase clearly regards a non-writable working directory as a legitimate state — that
is why the two sibling functions warn instead of aborting. `Scratch_Entry` is called on the
normal `cvs update` path for any file that disappeared from the repository
(`scratch_file()` at update.cpp:1545, `checkout_file()` at update.cpp:1783, and the client-side `Removed`/`Remove-entry` handlers at client.cpp:2995 and client.cpp:3005), so it is not a
rare corner.

The `!noexec` guard does not help: `noexec` is only set by `-n` / `cvs update -p`.

## Failure scenario
```
cvs -d :pserver:... checkout proj
chmod -R a-w proj          # read-only reference/build tree, or a tree on a read-only NFS
                           # export, or one owned by a different user in a shared checkout
cvs -d :pserver:... update proj
```
Someone has deleted `proj/obsolete.c` in the repository.

1. `update_fileproc` classifies `obsolete.c` as `T_REMOVE_ENTRY` and calls
   `scratch_file()` -> `Scratch_Entry(finfo->entries, "obsolete.c")` (update.cpp:1545).
2. `noexec` is 0, so entries.cpp:249 runs
   `entfile = CVS_FOPEN ("CVS/Entries.Log", "a")` -> **NULL** (EACCES).
3. entries.cpp:252 `fprintf (entfile, "R ")` -> **segmentation fault**.

The expected behaviour, matching `Register()` and `write_entries()`, is
`cvs update: cannot open CVS/Entries.Log: Permission denied` and continuing.

`Rename_Entry` reaches the same crash on the client side when the server sends a rename
response (client.cpp:3021), and locally from rename.cpp:183.

## Suggested fix
Mirror the guards already present in `Register()`:

```cpp
			entfile = CVS_FOPEN (entfilename, "a");
			entexfile = CVS_FOPEN (entexfilename, "a");

			if (entfile == NULL)
			{
				/* Warning, not error, as in write_entries.  */
				error (0, errno, "cannot open %s", entfilename);
				if (entexfile != NULL)
					fclose (entexfile);
				return;
			}
			if (entexfile == NULL)
			{
				error (0, errno, "cannot open %s", entexfilename);
				fclose (entfile);
				return;
			}
```
(the same block is needed in `Rename_Entry`; note `Register()` at entries.cpp:409-414 also
leaks `entfile` when only `entexfile` fails, and `write_entries()` leaks it at
entries.cpp:189 — the `fclose` above is the fix for that too).

## Refutation attempt
* Could `CVS_FOPEN` be a macro that aborts on failure? No — every other call site in this
  file tests the result against NULL (entries.cpp:151, 172, 402, 409, 1339), which would be
  pointless if the wrapper could not return NULL.
* Is the `CVS` directory guaranteed writable because we got this far? No. `Entries_Open()`
  only *reads* `CVS/Entries` (entries.cpp:843, `"r"`), and it explicitly tolerates the file
  being unopenable. Nothing between `Entries_Open()` and `Scratch_Entry()` requires write
  access.
* Is `Scratch_Entry` reachable in this state? Yes — `update_fileproc`'s `T_REMOVE_ENTRY`
  case (update.cpp:926-929) calls `scratch_file()`, which calls `Scratch_Entry()`
  unconditionally at update.cpp:1545, before any write-permission test.
* Would the client abort earlier with a clearer error? On the client side a read-only
  working directory produces warnings ("cannot rewrite CVS/Entries") but no fatal error —
  that is the documented design of `write_entries()`. So control does reach entries.cpp:252.
