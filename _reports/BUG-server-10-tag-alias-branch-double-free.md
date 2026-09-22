---
id: BUG-server-10
area: tag/rtag
file: cvsnt/cvsnt-2.5.05.3744/src/tag.cpp
line: 767
severity: high
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 6
behavior_change: no
---

# `cvs tag -A -b` / `cvs rtag -A -b`: `rev` aliases `version`, then both are freed — double free

## Summary
`rev` is only a *fresh* allocation when `!alias_branch && branch_mode`; otherwise it aliases `version`. The cleanup code, however, frees `rev` whenever `branch_mode` is set and then unconditionally frees `version` too. With `-A` and `-b` given together (`alias_branch && branch_mode`), `rev == version` and the same heap block is freed twice.

## Code
```cpp
// tag.cpp:767  (rtag_fileproc)
    rev = (!alias_branch && branch_mode) ? RCS_magicrev (rcsfile, version) : version;
```
```cpp
// tag.cpp:826-830  (rtag_fileproc, success tail)
	else
		tag_set_ok = 1;
    if (branch_mode)
		xfree (rev);          // frees version's block when alias_branch is set
    xfree (version);          // frees it again
```

The same shape in `tag_fileproc`:
```cpp
// tag.cpp:1131
    rev = (!alias_branch && branch_mode) ? RCS_magicrev (vers->srcfile, version) : version;
...
// tag.cpp:1197-1218
    if (branch_mode)
	xfree (rev);
    ...
    if (nversion != NULL)
    {
        xfree (nversion);     // version == nversion  =>  double free
    }
    freevers_ts (&vers);      // version == vers->vn_user / vn_rcs  =>  double free
```

## Why it is a bug
`xfree` is `xfree_s((void**)(&_p))` (lib/system.h:561), and `xfree_s` (subr.cpp:134) frees `*ptr` and NULLs *that variable only*:

```cpp
void xfree_s(void **ptr)
{
	MALLOC_CHECK();
	if(*ptr)
	{
		free(*ptr);
		MALLOC_CHECK();
	}
	*ptr=NULL;
}
```

So `xfree (rev)` sets `rev = NULL` but leaves the aliased `version` (or `nversion`, or `vers->vn_user`) pointing at the freed block. The subsequent `xfree (version)` / `xfree (nversion)` / `freevers_ts (&vers)` sees a non-NULL pointer and calls `free()` on it a second time.

The guard on the allocation (`!alias_branch && branch_mode`) and the guard on the free (`branch_mode`) are not the same predicate — that mismatch is the whole bug. The `// was : numtag` comment at tag.cpp:740 shows this ternary was edited when `-A` was added, and the matching free was not updated.

`-A` and `-b` are both accepted simultaneously: `rtag_opts` is `"+AabdFBflnQqRr:D:m:"` and `tag_opts` is `"+AbcdFBflQqRr:D:Mm:"` (tag.cpp:86, 107). The only mutual exclusion enforced is `-M` vs `-A` (tag.cpp:147, 178) — nothing rejects `-A -b`.

## Failure scenario
Server-side (the repository half of `cvs rtag` runs in the server):

```
cvs rtag -A -b -r EXISTING_BRANCH NEW_ALIAS mymodule
```

In `rtag_fileproc` for the first file:
1. `numtag && !date && alias_branch` is true, so `version = RCS_tag2rev (rcsfile, numtag)` — a heap block.
2. `numtag` is not numeric, so the `isdigit` branch at tag.cpp:736 is skipped and control reaches tag.cpp:767: `alias_branch` is 1, so `rev = version` (no `RCS_magicrev` call).
3. The tag does not exist yet, so `oversion == NULL` and both early returns are skipped.
4. `RCS_settag` succeeds, `retcode == 0`.
5. tag.cpp:828 `if (branch_mode) xfree (rev);` frees the block.
6. tag.cpp:830 `xfree (version);` frees it again.

glibc aborts with `free(): double free detected in tcache 2`; MSVC's CRT trips a heap assertion; with a hardened allocator this is a heap-corruption primitive rather than a clean abort. The same happens once per file in the module, so an attacker-supplied module name is not even needed — a normal user with tag permission triggers it.

`cvs tag -A -b -r EXISTING_BRANCH NEW_ALIAS` reaches the `tag_fileproc` variant, where the second free is either `xfree (nversion)` (tag.cpp:1216) or, if `-r` was omitted, `freevers_ts (&vers)` freeing `vers->vn_user` (tag.cpp:1218).

## Suggested fix
Make the free predicate match the allocation predicate in both functions:
```cpp
    if (!alias_branch && branch_mode)
		xfree (rev);
    xfree (version);
```
(applies at tag.cpp:821-822, 828-829, 1150-1151, 1174-1175, 1188-1189 and 1197-1198). Alternatively introduce a `bool rev_allocated = (!alias_branch && branch_mode);` next to the ternary and test that.

## Refutation attempt
* *Does `xfree` NULL both aliases?* No — `xfree_s` receives the address of one variable and NULLs only that one (subr.cpp:134). This is exactly why the alias survives.
* *Is `-A -b` rejected somewhere?* No. `cvstag` (tag.cpp:128) only errors for `-M` combined with `-A`, and for `-r` combined with `-D`. Both `A` and `b` are in the getopt strings for `tag` and `rtag`.
* *Could `RCS_tag2rev`/`RCS_getversion` return a non-owned pointer, making both frees wrong anyway?* Both return `xmalloc`/`xstrdup` storage — the surrounding code frees their results at tag.cpp:784, 799, 858, 865 — so the block is genuinely heap-owned and genuinely freed twice.
* *Do the early returns at tag.cpp:784/799 avoid it?* They avoid this particular pair (they free `version` without freeing `rev`), but that just leaks `rev` in the `!alias_branch && branch_mode` case; the success path at 828-830 is unguarded.
* *Is `alias_branch` only honoured when `numtag` is set?* The *version lookup* at tag.cpp:687 requires `numtag && !date && alias_branch`, but the `rev` ternary at tag.cpp:767 tests `alias_branch` alone, so `cvs tag -A -b NEWTAG` (no `-r`) also aliases `rev` to `version`.
