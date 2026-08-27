# commit_fileproc error paths skip do_unlock_file, leaving the file write-locked on the lock server

- **File:** cvsnt/cvsnt-2.5.05.3744/src/commit.cpp
- **Line(s):** 1374-1378, 1407-1413, 1420-1424 (the `goto out` sites); 1548-1550 (unlock placed before the label)
- **Severity:** medium
- **Confidence:** high
- **Category:** concurrency / error-handling

## Code
```cpp
	if(!commit_keep_edits)
		err += notify_do ('C', finfo->file, getcaller (), ...);

	do_unlock_file(ci->lockId);      // line 1548

out:                                     // line 1550
	xfree(options);
    if (err != 0 ...)
```
Error paths that jump past the unlock:
```cpp
				int status = RCS_checkout (finfo->rcs, NULL, bra, ...);
				if (status > 0)
				{
					err = 1;
					goto out;        // lock ci->lockId never released
				}
...
			if (lock_RCS (finfo->file, finfo->rcs, ci->rev, finfo->repository) != 0)
			{
				unlockrcs (finfo->rcs);
				err = 1;
				goto out;            // same
			}
...
		if (checkaddfile (...) != 0)
		{
		    fixaddfile (finfo->file, finfo->repository);
		    err = 1;
		    goto out;                // same
		}
```
The lock was acquired per-file in `check_fileproc` (lines 1114-1126):
```cpp
			ci->lockId = do_lock_file(f, finfo->repository, 0, 0);
			...
				ci->lockId = do_lock_file(f, finfo->repository, 1, 1);   // write lock, waiting
```

## Why this is a bug
`do_unlock_file(ci->lockId)` sits *above* the `out:` label, so every `goto out` error path (failed `lock_RCS`, failed `checkaddfile`, failed slide-tags checkout) returns without releasing the advisory lock this process took on `<file>,v` via the CVSNT lock server. The lock is only implicitly dropped when `Lock_Cleanup()` closes the lock-server connection after the *entire* commit recursion finishes. During a large multi-directory commit where one file fails early, that file (and the failing commit continues processing all remaining directories) stays write-locked for the rest of the operation, blocking other users' commits and (for write locks) reads of that file for an unbounded time. Note the failure of one file does not abort the recursion — `commit_fileproc` returns err and processing continues — so the window can be minutes on big trees.

## Suggested fix
Move `do_unlock_file(ci->lockId);` below the `out:` label (guarding against the T_UPTODATE/early-return cases where `ci` is valid), e.g.:
```cpp
out:
	do_unlock_file(ci->lockId);
	xfree(options);
```
(`ci` is always valid at any `goto out`, which all occur after `ci = (struct commit_info *) p->data;`).
