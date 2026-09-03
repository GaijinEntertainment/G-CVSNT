---
id: BUG-update-04
area: client/update core
file: cvsnt/cvsnt-2.5.05.3744/src/update.cpp
line: 3207
severity: high
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# `join_file()` frees the file-static `options` instead of its local `t_options` (double free / use-after-free via `checkout()`)

## Summary
In `join_file()`'s "no difference between revisions" early return, the cleanup block frees
`options` — the *file-static* `-k` option string shared by the whole update run and owned
by the caller — instead of the function-local `t_options` it just allocated. The result is
a caller-visible dangling pointer (double free at `checkout.cpp:550`), a silent loss of the
`-k` setting for every remaining file, and a leak of `t_options`.

## Code
```cpp
/* src/update.cpp:3065-3072 — the local that *should* be freed */
    t_options = xstrdup(vers->options);
	RCS_get_kflags(t_options, false, kf);
	if(kf.flags&KFLAG_VALUE)
	{
		kf.flags=(kf.flags&~KFLAG_VALUE)|KFLAG_VALUE_LOGONLY;
		char *topt = (char*)xmalloc(128);
		xfree(t_options);
		t_options=RCS_rebuild_options(&kf,(char*)topt);
	}

/* src/update.cpp:3194-3209 — the buggy cleanup */
	if(status == 3) 
	{
		...
		xfree(rev1);
		xfree(rev2);
		xfree(backup);
		xfree(options);       /* 3207 <-- should be xfree(t_options); */
		return;
	}
```

Every *other* exit from `join_file` gets this right — the normal fall-through at
update.cpp:3279-3281 is:
```cpp
    xfree (backup);
	xfree (t_options);
	baserev_update(finfo, vers, T_NEEDS_MERGE);
```

## Why it is a bug
`options` is `static const char *options;` (update.cpp:82). It is set once per command from
the caller's own pointer:

```cpp
/* src/update.cpp:620-621, do_update() */
    /* fill in the statics */
    options = xoptions;
```

`checkout.cpp` keeps its *own* `static char *options = NULL;` (checkout.cpp:102), fills it
from `RCS_check_kflag()` (checkout.cpp:167-169), hands the same pointer to `do_update()`
(checkout.cpp:1223 and checkout.cpp:1280), and frees it itself at the end:

```cpp
/* src/checkout.cpp:550 */
	xfree (options);
```

So when `join_file` frees it, `update.cpp`'s static is nulled (harmless) but
`checkout.cpp`'s static still points at the freed block — CVSNT's `xfree` only nulls the
*variable it was handed*, not aliases:
`#define xfree(_p) xfree_s((void**)(&_p))` (lib/system.h:556).

`status == 3` is `RCS_merge`'s "no difference between the two revisions" result
(update.cpp:3192), which is an entirely ordinary outcome for a `-j` merge — most files in a
branch typically have no changes between the two join points.

Secondary damage in the same three lines: `t_options` is never freed on this path (leak,
one per unchanged file), and `xfree(options)` blanks the update run's `-k` override so all
subsequent files in the same command are checked out with the wrong keyword-expansion mode.

## Failure scenario
```
cvs checkout -kkv -j BR1 -j BR2 mymodule
```
(or any `cvs co -k<mode> -j...` / `cvs co -k<mode> -j...` on a module where at least one
text file is identical between BR1 and BR2)

1. `checkout()` parses `-kkv`; `options = RCS_check_kflag("kv",...)` -> heap block `P`
   (checkout.cpp:167-169).
2. `checkout_proc()` -> `do_update(..., options /* == P */, ...)` (checkout.cpp:1223 or
   1280) -> update.cpp's static `options = P`.
3. `update_fileproc` -> `join_file()` for the first unchanged file. `RCS_merge` returns 3.
4. update.cpp:3207 `xfree(options)` -> `free(P)`; update.cpp's static becomes NULL,
   checkout.cpp's static still holds `P`.
5. Every remaining file in the run is processed with `options == NULL`, i.e. the `-kkv`
   the user asked for is silently dropped (`Version_TS(finfo, options, ...)`,
   `checkout_file`).
6. `checkout()` returns and executes `xfree (options)` at checkout.cpp:550 ->
   **double free of `P`** -> glibc `free(): double free detected` / heap abort, or
   silent heap corruption on Windows.

With two or more modules on one command line (`cvs co -kkv -j BR1 -j BR2 modA modB`) step 5
becomes a **use-after-free**: the `for (i = 0; i < argc; i++) ... do_module(...)` loop at
checkout.cpp:496-547 hands the freed `P` to `do_update` again for `modB`.

## Suggested fix
```cpp
	if(status == 3) 
	{
		xfree(rev1);
		xfree(rev2);
		xfree(backup);
		xfree(t_options);
		return;
	}
```

## Refutation attempt
* Could `options` be a local shadowing the static in `join_file`? No — `join_file`
  (update.cpp:2698-3282) declares `backup`, `t_options`, `kf`, `status`, `rev1`, `rev2`,
  `jrev1`, `jrev2`, `jdate1`, `jdate2`, `mode`. There is no local named `options`, and it
  is read as the static two lines away at update.cpp:2986-2992
  (`const char *saved_options = options; ... options = xvers->options; ... options = saved_options;`).
* Could `xfree` null the aliased pointer too? No — `xfree_s(void **ptr)` (subr.cpp:134-143)
  nulls exactly the one lvalue it is given.
* Is `status == 3` unreachable? No: `RCS_merge(...)` is called at update.cpp:3191-3192 and
  `merge_file()` handles the identical `status == 3` case at update.cpp:2599, so the value
  is a normal documented return.
* Does `checkout()` avoid the double free by nulling `options` first? It nulls it only on
  the `cat` early-return path (checkout.cpp:458-461); the main path reaches
  `xfree (options)` at line 550 with the static unchanged.
* Is the damage limited to the `-k` case? `xfree(NULL)` is safe, so with no `-k` on the
  command line only the `t_options` leak remains — but `-k` with `-j` is exactly the
  combination CVSNT documents for merging binary/expanded files.
