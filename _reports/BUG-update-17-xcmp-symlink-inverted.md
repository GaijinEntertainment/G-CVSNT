---
id: BUG-update-17
area: client/update core
file: cvsnt/cvsnt-2.5.05.3744/src/filesubr.cpp
line: 967
severity: medium
category: logic
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# `xcmp()` returns inverted results when both operands are symlinks

## Summary
`xcmp()` is documented and used as "0 = identical, non-zero = different" — that is how its
regular-file path (`memcmp`), its device path, and its type-mismatch path all behave. The
symlink path returns `strcmp(...) == 0`, i.e. **1 when the two links are identical and 0
when they differ** — exactly backwards.

## Code
```cpp
/* src/filesubr.cpp:936-976 */
/*
 * Compare "file1" to "file2". Return non-zero if they don't compare exactly.
 * If FILE1 and FILE2 are special files, compare their salient characteristics
 * (i.e. major/minor device numbers, links, etc.
 */
int xcmp (const char *file1, const char *file2)
{
    ...
    /* If FILE1 and FILE2 are not the same file type, they are unequal. */
    if ((sb1.st_mode & S_IFMT) != (sb2.st_mode & S_IFMT))
	return 1;                                   /* 955: different -> 1  (correct) */

    /* If FILE1 and FILE2 are symlinks, they are equal if they point to
       the same thing. */
    if (S_ISLNK (sb1.st_mode) && S_ISLNK (sb2.st_mode))
    {
	int result;
	buf1 = xreadlink (file1);
	buf2 = xreadlink (file2);
	result = (strcmp (buf1, buf2) == 0);        /* 964: 1 when EQUAL */
	xfree (buf1);
	xfree (buf2);
	return result;                              /* 967: <-- inverted */
    }

    /* If FILE1 and FILE2 are devices, they are equal if their device
       numbers match. */
    if (S_ISBLK (sb1.st_mode) || S_ISCHR (sb1.st_mode))
    {
	if (sb1.st_rdev == sb2.st_rdev)
	    return 0;                               /* equal -> 0  (correct) */
	else
	    return 1;
    }
    ...
	    ret = memcmp(buf1, buf2, read1);        /* equal -> 0  (correct) */
	...
    return (ret);
}
```

## Why it is a bug
Every other exit of `xcmp()` follows the documented "non-zero if they don't compare
exactly" convention, and both call sites rely on it:

```cpp
/* src/update.cpp:2462, merge_file() */
    if (!noexec && !xcmp (backup, finfo->file))
    {
	cvs_output (fn_root(finfo->fullname), 0);
	cvs_output (" already contains the differences between ", 0);
	...
	retval = 0;
	goto out;
    }
```
```cpp
/* src/edit.cpp:924, unedit/revert */
		if (!force_unedit && isfile(entdata->user) && xcmp (entdata->user, basefilename) != 0)
		{
			... "%s has been modified; revert changes? " ...
			if (yesno_prompt(tmp,"Modified file",0)!='y')
				return 0;      /* keep the user's changes */
		}
		... rename_file (basefilename, entdata->user);   /* discard them */
```

With two symlinks the sense of both tests flips:

| state | correct `xcmp` | actual `xcmp` | edit.cpp:924 effect | update.cpp:2462 effect |
|---|---|---|---|---|
| links identical | 0 | 1 | prompts "has been modified; revert changes?" for an unmodified file | skips the "already contains the differences" short-circuit |
| links differ | non-zero | 0 | **no prompt — the user's changed link is overwritten silently** | wrongly reports "already contains the differences" and returns success without merging |

The `edit.cpp` case is the damaging one: the confirmation prompt is the only thing standing
between `cvs unedit` and `rename_file (basefilename, entdata->user)`, which destroys the
working copy's content.

## Failure scenario
POSIX working directory (CVSNT's watch/edit workflow, so `CVS/Base` copies exist):

1. `cvs edit link` on a file that is a symlink in the working directory. `edit.cpp` copies it
   to `CVS/Base/link` with `copy_file()`, which reproduces symlinks as symlinks
   (filesubr.cpp:50-56: `if (islink (from)) { char *source = xreadlink (from); symlink (source, to); ... }`),
   so both `link` and `CVS/Base/link` are symlinks to the same target.
2. The user re-points the link: `ln -sfn other_target link`.
3. `cvs unedit link`. At edit.cpp:924 `xcmp("link", "CVS/Base/link")` compares two symlinks
   with *different* targets and returns **0**, so the `!= 0` test is false, the
   "has been modified; revert changes?" prompt is skipped entirely, and edit.cpp:940
   `rename_file (basefilename, entdata->user)` restores the old link. The user's change is
   discarded without any warning — the exact outcome the prompt exists to prevent.
4. Conversely, unediting an *untouched* symlink returns 1 and nags the user with a bogus
   "has been modified" prompt on every `cvs unedit`.

## Suggested fix
```cpp
	result = (strcmp (buf1, buf2) != 0);
	xfree (buf1);
	xfree (buf2);
	return result;
```

## Refutation attempt
* Could the callers be compensating for the inversion? No — `update.cpp:2462` uses
  `!xcmp(...)` to mean "identical" and `edit.cpp:924` uses `xcmp(...) != 0` to mean
  "different"; both match the documented convention and the regular-file path, and neither
  special-cases symlinks.
* Could the symlink branch be unreachable because CVS never versions symlinks? The branch
  compares two *working-directory* paths, not repository entries, and `copy_file()` goes out
  of its way to reproduce symlinks (filesubr.cpp:50-56) precisely so that `CVS/Base` and
  `.#file.rev` backups mirror them. A symlink placed over a working file by the user is
  enough.
* Could `xreadlink` return NULL and make `strcmp` crash first? `xreadlink`
  (filesubr.cpp:1215) loops on `readlink` and errors out fatally on failure, so both buffers
  are non-NULL when `strcmp` runs; the inversion is the only defect here.
* Is it fork-introduced? Probably not — this shape matches upstream CVS's `xcmp`. It is
  nonetheless a live inversion in this tree with two real callers, and the one-character
  fix is safe because no caller depends on the current behaviour.
