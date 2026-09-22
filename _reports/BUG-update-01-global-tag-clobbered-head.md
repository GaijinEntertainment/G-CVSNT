---
id: BUG-update-01
area: client/update core
file: cvsnt/cvsnt-2.5.05.3744/src/update.cpp
line: 1372
severity: high
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 6
behavior_change: yes
---

# `update_dirent_proc` overwrites the file-static `tag` with the string literal `"HEAD"`, which is later passed to `xfree()`

## Summary
On the "cannot merge branch" ACL failure path, `update_dirent_proc` assigns the string
literal `"HEAD"` to the *file-static* `tag` variable purely so the following `error()`
call has something to print. That assignment is never undone. It (a) silently changes
the sticky tag used for every remaining directory of the recursion and (b) can reach
`xfree (tag)` in `update_predirent_proc`, i.e. `free()` on a string literal.

## Code
```cpp
/* src/update.cpp:1366-1386  (inside update_dirent_proc) */
		if (join_rev1 && !verify_merge(repository,NULL,dirtag,join_rev1,&msg))
		{
			if(!tag) tag="HEAD";                       /* 1372 <-- clobbers the static */
			error (0, 0, "User '%s' cannot merge branch %s with branch %s", CVS_Username, tag, join_rev1);
			if(msg)
				error (0, 0, "%s", msg);
			return R_SKIP_ALL;                          /* 1376: dirtag/dirdate leaked too */
		}

		if (join_rev2 && !verify_merge(repository,NULL,dirtag,join_rev2,&msg))
		{
			if(!tag) tag="HEAD";                       /* 1381 <-- same */
			error (0, 0, "User '%s' cannot merge branch %s with branch %s", CVS_Username, tag, join_rev2);
			...
			return R_SKIP_ALL;
		}
```

```cpp
/* src/update.cpp:83-101 - these are file-static, shared by the whole recursion */
static const char *tag;
static const char *tag_update_dir;

/* src/update.cpp:1071-1077  (update_predirent_proc, run once per directory) */
    if (tag_update_dir != NULL /*&& strcmp (update_dir, tag_update_dir) == 0*/)
    {
	    xfree (tag);          /* 1073 <-- free() of "HEAD" */
	    xfree (date);
		nonbranch = 0;
		xfree (tag_update_dir);
    }
```

## Why it is a bug
`tag` is not a local. It is the static that every callback in update.cpp reads:
`update_fileproc` passes it to `Classify_File()`, `update_dirent_proc` copies it into
`dirtag` (`dirtag=xstrdup(tag)`) and `WriteTag()`s it into `CVS/Tag`, and
`update_filesdone_proc` writes it again when `rewrite_tag` is set. Assigning `"HEAD"`
to it as a display convenience corrupts all of that for every directory processed after
the failing one — `R_SKIP_ALL` only skips the *current* subtree (recurse.cpp:1298),
siblings keep being walked.

Worse, `update.cpp:1073` does `xfree (tag)` — `xfree` expands to
`xfree_s((void**)&_p)` (lib/system.h:556) which calls `free()`. Passing a string
literal to `free()` is undefined behaviour (typically an immediate heap-corruption
abort, since `"HEAD"` lives in `.rodata`).

The two `xfree(tag)` sites are reachable with `tag == "HEAD"` because `tag_update_dir`
becomes non-NULL whenever `ParseTag()` yields a sticky **date** with no tag
(update.cpp:1103-1106: `if (tag != NULL || date != NULL) tag_update_dir = xstrdup(update_dir);`)
— in that case `tag` stays NULL, so the `if(!tag)` guard at 1372 fires.
Note also that the `strcmp (update_dir, tag_update_dir) == 0` guard at line 1071 has
been *commented out* in this fork, so `update_predirent_proc` frees `tag`
unconditionally on the very next directory. `update_dirleave_proc` (line 1424) is not
reached for the failing directory because `do_dir_proc` skips `dirleaveproc` when
`dir_return == R_SKIP_ALL` (recurse.cpp:1357 is inside the
`dir_return != R_SKIP_ALL` block).

## Failure scenario
Server-side (`!current_parsed_root->isremote`), repository with CVSNT ACLs:

1. Working directory's `CVS/Tag` contains a sticky **date** (`D2020.01.01.00.00.00`),
   no tag; repository has at least two sibling subdirectories `a/` and `b/`, and `a/`
   is not yet checked out.
2. `cvs update -d -j BRANCH` with an ACL that denies `write` on `a/` for `BRANCH`
   (`verify_merge` -> `verify_perm(..., "write", tag, merge, ...)`, perms.cpp:599).
3. `update_predirent_proc` for `a/`: `isdir(a)` is false, `update_build_dirs` is set,
   `tag==NULL && date==NULL && !aflag` -> `ParseTag()` fills `date`, leaves `tag` NULL,
   so `tag_update_dir = xstrdup("a")` and the directory is created.
4. `update_dirent_proc` for `a/`: `isdir(a)` is now true, so the
   `else if (!pipeout && !noexec)` branch runs; `date` is non-NULL so
   `dirtag = xstrdup(tag) == NULL`; `verify_merge()` denies ->
   **`tag = "HEAD"`** -> `return R_SKIP_ALL` (leaking `dirdate`).
5. Recursion moves to sibling `b/`: `update_predirent_proc` sees `tag_update_dir != NULL`
   and executes `xfree (tag)` -> `free("HEAD")` -> heap abort / crash of the server
   process handling that request.

Even without step 5 (e.g. `-A` variants where `tag_update_dir` stays NULL), the milder
but still wrong outcome is that every subsequent directory gets `WriteTag(dir,"HEAD",...)`
written into its `CVS/Tag` and every file is classified against a tag literally named
`HEAD`, which does not exist in RCS — with `force_tag_match` on, files resolve to
"no such revision" and get scheduled for removal.

## Suggested fix
Do not touch the static; use a local for the message, and free the two strings before
returning.

```cpp
		if (join_rev1 && !verify_merge(repository,NULL,dirtag,join_rev1,&msg))
		{
			error (0, 0, "User '%s' cannot merge branch %s with branch %s",
			       CVS_Username, tag ? tag : "HEAD", join_rev1);
			if(msg)
				error (0, 0, "%s", msg);
			xfree(dirtag);
			xfree(dirdate);
			return R_SKIP_ALL;
		}

		if (join_rev2 && !verify_merge(repository,NULL,dirtag,join_rev2,&msg))
		{
			error (0, 0, "User '%s' cannot merge branch %s with branch %s",
			       CVS_Username, tag ? tag : "HEAD", join_rev2);
			if(msg)
				error (0, 0, "%s", msg);
			xfree(dirtag);
			xfree(dirdate);
			return R_SKIP_ALL;
		}
```

## Refutation attempt
* Could `xfree` be a no-op on non-heap pointers? No — `lib/system.h:556` defines
  `#define xfree(_p) xfree_s((void**)(&_p))` and `subr.cpp:134-143` `xfree_s` calls
  plain `free()` for any non-NULL pointer.
* Could `tag` never be NULL here, making the assignment dead? No: `update()` sets
  `tag = NULL` when the user passes `-r HEAD` (update.cpp:302-306) and leaves it NULL
  when `-r` is absent, which is the common case for `cvs update -j BRANCH`.
* Could `update_dirleave_proc` (line 1424, which *does* keep the `strcmp` guard) clean
  up first and make the free harmless? No: `recurse.cpp:1298` gates the whole block that
  eventually calls `dirleaveproc` on `dir_return != R_SKIP_ALL`, and this path returns
  `R_SKIP_ALL`.
* Could `verify_merge` be a stub that always succeeds? No — `perms.cpp:599` forwards to
  `verify_perm()`, the same routine used for all other ACL checks in this tree, and the
  surrounding code prints a real denial message.
* Is `R_SKIP_ALL` aborting the entire run so the clobbered `tag` never matters? No —
  `do_dir_proc` returns normally and `walklist` continues to the next sibling directory;
  only the current subtree is skipped.
