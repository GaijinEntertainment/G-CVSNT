---
id: BUG-update-05
area: client/update core
file: cvsnt/cvsnt-2.5.05.3744/src/update.cpp
line: 3370
severity: medium
category: typo
status: open - the one-line fix below was applied (f791743) and reverted (9376253) in this slice: the working-file mode is umask-reduced on checkout while the repository records the unreduced mode, so on a umask 027 client every executable-file merge became a conflict; a correct fix must compare umask-masked modes
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# `special_file_mismatch()` sets `result = 0` on the mismatch branch, so it can never report a mismatch

## Summary
`special_file_mismatch()` is documented (and used) as "return 1 if the two revisions differ
in permissions/linkage". The only assignment inside the mismatch branch is `result = 0`,
which is the value `result` already holds. The function prints the "permission mismatch"
diagnostic and then reports *no* mismatch, so both callers take the wrong branch.

## Code
```cpp
/* src/update.cpp:3280-3296 — the contract */
/*
 * Report whether revisions REV1 and REV2 of FINFO agree on:
 *   . file ownership
 *   . permissions
 *   ...
 * Return 1 if the files differ on these data.
 */
int special_file_mismatch (struct file_info *finfo, const char *rev1, const char *rev2)

/* src/update.cpp:3353-3372 */
    /* Check the file permissions, printing
       an error for each mismatch found.  Return 0 if all characteristics
       matched, and 1 otherwise. */

    result = 0;                                       /* 3357 */
  
	/* Only check the execute bit as that's the only sane thing to check */
	/* Win32 doesn't understand permissions in any meaningful sense, so 
	   we just skip the check there */
#ifndef _WIN32
	if (check_modes &&
	    (rev1_mode & 0111) != (rev2_mode & 0111))
	{
	    error (0, 0, "%s: permission mismatch between %s and %s",
		   fn_root(finfo->file),
		   (rev1 == NULL ? "working file" : rev1),
		   (rev2 == NULL ? "working file" : rev2));
	    result = 0;                                   /* 3370 <-- should be result = 1; */
	}
#endif

    return result;
```

## Why it is a bug
`result` is initialised to 0 at line 3357 and the branch at 3364-3371 is the *only* other
write to it. `special_file_mismatch()` therefore returns 0 unconditionally on every
platform; on `_WIN32` the whole check is `#ifdef`'d out anyway, so the branch exists only
to be taken on POSIX — and it does nothing but print.

The two call sites both rely on a non-zero return to force the "cannot auto-merge, hand the
user both files" path:

```cpp
/* src/update.cpp:2334-2337, merge_file() */
    if ((kf.flags&KFLAG_BINARY)
		|| wrap_merge_is_copy (finfo->file)
		|| special_file_mismatch (finfo, NULL, vers->vn_rcs))
    {
	/* For binary files, a merge is always a conflict.  Same for
	   files whose permissions or linkage do not match. ... */

/* src/update.cpp:3145-3147, join_file() */
    else if ((kf.flags&KFLAG_BINARY)
	     || wrap_merge_is_copy (finfo->file)
	     || special_file_mismatch (finfo, rev1, rev2))
```

The surrounding comments ("Only if the working file, the RCS file, and A all disagree
should this be considered a conflict... in the meantime it is safe to treat any such
mismatch as an automatic conflict. -twp") make the intended return value unambiguous.

The inconsistency is self-evident inside the branch itself: it calls
`error (0, 0, "%s: permission mismatch between %s and %s", ...)` — an error is reported to
the user, then the function tells its caller everything matched.

## Failure scenario
POSIX server, a file whose executable bit differs between the working revision and the
merge target (a very common case: `chmod +x build.sh; cvs ci` on a branch, then
`cvs update -j THATBRANCH` on the trunk where the file is non-executable):

1. `join_file()` reaches update.cpp:3145. `kf.flags & KFLAG_BINARY` is false (shell script),
   `wrap_merge_is_copy()` is false.
2. `special_file_mismatch(finfo, rev1, rev2)` finds `(rev1_mode & 0111) != (rev2_mode & 0111)`,
   prints `build.sh: permission mismatch between 1.4 and 1.6.2.2`, and returns **0**.
3. The `else if` is therefore false, so control falls to
   `status = RCS_merge (finfo->rcs, ..., rev1, rev2, conflict_3way, &mode);` (update.cpp:3191).
4. The content merge silently succeeds and the file is registered as merged. The user sees
   a stray "permission mismatch" line among the `U`/`M` output but **no `C`**, so nothing
   flags the file for manual resolution; the permission difference is dropped on the floor
   and the merge is recorded as complete.

The same happens in `merge_file()` for `T_NEEDS_MERGE`.

## Suggested fix
As written this was tried and reverted (see status); flip the return value only once both
sides are compared under the effective umask.
```cpp
	    error (0, 0, "%s: permission mismatch between %s and %s",
		   fn_root(finfo->file),
		   (rev1 == NULL ? "working file" : rev1),
		   (rev2 == NULL ? "working file" : rev2));
	    result = 1;
	}
```

## Refutation attempt
* Is there another `result = 1` elsewhere in the function that I missed? No —
  `grep -n "result" update.cpp` over the function body gives exactly
  `3305: int result;`, `3357: result = 0;`, `3370: result = 0;`, `3374: return result;`.
* Could returning 0 be a deliberate downgrade of the check to "warn only"? Possible in
  intent, but then the code would not keep the doc comment "Return 1 if the files differ",
  would not keep the second comment "Return 0 if all characteristics matched, and 1
  otherwise" *immediately above* the assignment, and would not keep the `result` variable
  and its dead re-assignment at all. A deliberate warn-only change would simply drop the
  branch's assignment. This is why it is filed as a typo rather than a design decision —
  though a maintainer should confirm which behaviour is wanted before flipping it, since
  the fix does change output (`C` instead of a silent merge).
* Are the callers dead code? No: `merge_file()` is the `T_NEEDS_MERGE` handler
  (update.cpp:823) and `join_file()` runs for every `-j` (update.cpp:940-941).
* Could `check_modes` always be 0, making the branch unreachable anyway? No —
  `check_modes` starts at 1 and is only cleared when a revision has no `permissions`
  entry in its `other_delta`; CVSNT records `permissions` for every commit that has a
  non-default mode, which is exactly when the check matters.
