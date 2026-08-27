---
id: BUG-server-11
area: rcs
file: cvsnt/cvsnt-2.5.05.3744/src/rcs.cpp
line: 2545
severity: medium
category: leak
verdict: CONFIRMED
fix_size_loc: 4
behavior_change: no
---

# `RCS_getbranch` frees its `branch` buffer only inside the `atomic_checkouts` branch, leaking on every call in the default configuration

## Summary
`RCS_getbranch` allocates `branch` unconditionally but only calls `xfree(branch)` inside `if(atomic_checkouts)`. `atomic_checkouts` defaults to 0 (main.cpp:78) and is only ever set from an optional config value, so in a stock server every call to this very hot function leaks the buffer. The `v` returned by `do_lock_version` in the same block is never freed either.

## Code
```cpp
// rcs.cpp:2533-2552
	if(strchr(tag,'.')) /* This is a dotted name.  Convert to branch name */
	{
		branch = RCS_branchfromversion(rcs,tag);
		if(!branch)
			branch=xstrdup("HEAD");
	}
	else
		branch = xstrdup(tag);

	if(atomic_checkouts)
	{
		do_lock_version(rcs->rcsbuf.lockId, branch, &v);
		xfree(branch);                       // <-- only freed here
		if(v && v[0])
			return xstrdup(v);               // <-- v leaked too
	}

	if(!strcmp(tag,"HEAD"))
		return RCS_head(rcs);                // branch leaked
	...
```

There is no other `xfree(branch)` anywhere in the function — the remaining ~140 lines refer only to `vn->branches` (the list member), never to this local.

## Why it is a bug
`branch` is always a fresh allocation: either `RCS_branchfromversion()` (which returns `xmalloc`/`xstrdup` storage) or `xstrdup("HEAD")` or `xstrdup(tag)`. Every one of the function's fifteen-plus `return` statements after the `atomic_checkouts` block leaves it unreachable.

`atomic_checkouts` is initialised to `0` at main.cpp:78 and only assigned at main.cpp:549 from a config value, so the freeing branch is *not* taken unless the administrator explicitly enables atomic checkouts. The default build therefore leaks on 100% of calls.

`v` is likewise a fresh allocation — `do_lock_version` (lock.cpp:389) does `*version = (char*)xmalloc((q-p)+1)` — and is copied with `xstrdup(v)` and then dropped, so the atomic-checkouts configuration trades one leak for two. The same `do_lock_version`/`v` leak exists in `RCS_head` (rcs.cpp:2819-2822).

## Failure scenario
`RCS_getbranch` sits on the main tag-resolution path: `RCS_gettag` (rcs.cpp:2153) calls it for every file whose sticky tag is a branch, and it is reached again from `RCS_getversion`, `RCS_isbranch` (rcs.cpp:2263), `RCS_branch_head` (rcs.cpp:2713), `RCS_tag2rev` (rcs.cpp:1989), `RCS_checkout_raw_value` (rcs.cpp:4323), `commit.cpp:1814`, `log.cpp:1170/1178`, `rcs_checkin.cpp:789/1801` and others.

A `cvs checkout -r SOMEBRANCH bigmodule` on a repository with 200 000 files calls it at least once per file. Each leak is `strlen(tag)+1` bytes plus allocator overhead — realistically 32-48 bytes with glibc — so the server process grows by roughly 6-10 MB for that single command, and more for `cvs update -r` / `cvs log -r` which hit several of the call sites per file. On a 32-bit server build (which this fork still supports; see commit "cvs restore 32 bit client") a large enough module can exhaust address space.

## Suggested fix
```cpp
	if(atomic_checkouts)
	{
		do_lock_version(rcs->rcsbuf.lockId, branch, &v);
		if(v && v[0])
		{
			char *ret = xstrdup(v);
			xfree(v);
			xfree(branch);
			return ret;
		}
		xfree(v);
	}
	xfree(branch);
```

## Refutation attempt
* *Is `branch` freed further down?* No. `grep -n "branch" rcs.cpp` over the body of `RCS_getbranch` (rcs.cpp:2515-2700) shows the local is referenced only at the declaration and in the block above; every other hit is `vn->branches`, a list member of the delta node.
* *Does something else own the string?* `do_lock_version` takes it as `const char *branch` and only formats it into a request line (lock.cpp:400); it does not retain it.
* *Is `atomic_checkouts` on by default, making this a non-issue?* No — `int atomic_checkouts = 0;` at main.cpp:78, set only from a config lookup at main.cpp:549.
* *Could `branch` be NULL on some path (making `xfree` unnecessary)?* No; the `if(!branch) branch=xstrdup("HEAD");` fallback guarantees it is non-NULL, and `xfree`/`xfree_s` handles NULL safely anyway.
