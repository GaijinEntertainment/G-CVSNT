# join_file frees global `options` instead of local `t_options` on "no difference" merge path

- **File:** cvsnt/cvsnt-2.5.05.3744/src/update.cpp
- **Line(s):** 3204-3208 (the `xfree(options)` at 3207); compare cleanup at 3281 (`xfree (t_options)`)
- **Severity:** high
- **Confidence:** high
- **Category:** typo / memory

## Code
```cpp
    else
	status = RCS_merge (finfo->rcs, vers->srcfile->path, finfo->file,
			t_options, (char*)(rev1?rev1:"0"), rev2, conflict_3way, &mode);

	if(status == 3)
	{
		//cvs_output ("No difference between revisions ", 0);
		...
		xfree(rev1);
		xfree(rev2);
		xfree(backup);
		xfree(options);   // <-- BUG: frees the file-scope static `options`, not `t_options`
		return;
	}
```

For reference, the normal exit path of the same function correctly does:
```cpp
    xfree (backup);
	xfree (t_options);       // line 3281
	baserev_update(finfo, vers, T_NEEDS_MERGE);
```

## Why this is a bug
`t_options` is the local copy (`t_options = xstrdup(vers->options);` at line 3065) that this function owns and must free. `options` is the file-scope static (`static const char *options;` line 83) that holds the `-k` option for the *entire* update/checkout run; it is either allocated once by `RCS_check_kflag()` in `update()` or aliased directly from the caller's `xoptions` in `do_update()`/`rcs_update_fileproc()`.

When `RCS_merge` returns 3 ("no difference between revisions" — a CVSNT/Gaijin-added status; see the parallel handling in `merge_file()` at line 3380) during a multi-file `cvs update -j`, this path:
1. Frees the memory `options` points to and nulls the static (xfree is `free + set NULL`). Every *subsequent* file processed in the same update loses its sticky `-k` option — files after the first "no difference" join are checked out/merged with default keyword expansion instead of e.g. `-kb`. For a fork whose whole purpose is large *binary* files, silently dropping `-kb` mid-run can corrupt working files.
2. If `do_update()` was called from `checkout`/`switch` with a caller-owned `xoptions` string, the caller still holds a pointer to the freed block → later use-after-free/double-free in the caller.
3. Leaks `t_options` (never freed on this path).

## Suggested fix
Replace `xfree(options);` with `xfree(t_options);` in the `status == 3` early-return block.
