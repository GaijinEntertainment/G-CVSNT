---
id: BUG-update-06
area: client/update core
file: cvsnt/cvsnt-2.5.05.3744/src/update.cpp
line: 2687
severity: medium
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 6
behavior_change: yes
---

# `bound_merge_by_bugid()` dereferences the result of `previous_version()` without a NULL check

## Summary
After locating the oldest revision that carries the requested bug id, `bound_merge_by_bugid()`
steps one revision further back with `previous_version()` and immediately reads
`endnode->key`. `previous_version()` returns NULL when the revision has no predecessor
(trunk revision `1.1`), producing a NULL dereference.

## Code
```cpp
/* src/update.cpp:2612-2624, previous_version() — NULL is a normal return value */
static Node *previous_version(List *versions, RCSVers *vers)
{
	Node *node;

	if(numdots(vers->version)==1) /* Main branch */
	{
		if(vers->next)
			node = findnode(versions,vers->next);
		else
			node=NULL;                                  /* <-- 1.1 has no `next` */
	}
	...

/* src/update.cpp:2680-2691 */
	if(!startnode)
	{
		TRACE(3,"bound_merge_by_bugid: no bug changes found");
		return false;
	}
	xfree(rev1);
	xfree(rev2);
	endnode = previous_version(rcs->versions,(RCSVers*)endnode->data);   /* 2687 */
	rev1=xstrdup(endnode->key);                                         /* 2688 <-- NULL deref */
	rev2=xstrdup(startnode->key);
	TRACE(3,"bound_merge_by_bugid end: %s -> %s",rev1,rev2);
	return true;
```

## Why it is a bug
`RCSVers::next` is only populated when the RCS `next` field carries a value
(`rcs.cpp:6273-6277`), and the oldest trunk revision `1.1` has an empty `next` by
construction. So `previous_version(versions, vers_of_1_1)` takes the `node=NULL` branch at
update.cpp:2620 and returns NULL. Line 2688 then reads `NULL->key`.

The backwards walk at update.cpp:2662-2679 only stops early when it reaches `rev1`:
```cpp
		/* rev1 isn't normally considered */
		if(!strcmp(node->key,rev1))
			break;
```
That guard protects the case where `rev1` really is an ancestor of `rev2`. It does nothing
when `rev1` is *not* on `rev2`'s ancestor chain — then the walk runs all the way down to
`1.1` and `1.1` becomes a legitimate candidate for `endnode`. The loop's own termination
condition `while(node)` acknowledges that `previous_version` can return NULL; line 2687
forgets it.

Note the two `-j` arguments are unconstrained user input: `join_file()` resolves them
independently with `RCS_getversion(vers->srcfile, jrev1, ...)` and
`RCS_getversion(vers->srcfile, jrev2, ...)` (update.cpp:2740-2748), with no requirement that
one be an ancestor of the other.

## Failure scenario
A repository with two divergent branches and a file whose revision `1.1` was committed with
a bug id (CVSNT records the bug id in `other_delta` under key `"bugid"`, which is exactly
what the loop matches on):

```
cvs update -j TAG_ON_BRANCH_A -j TAG_ON_BRANCH_B -B 12345
```

1. `join_file()`: `rev1 = RCS_getversion(srcfile, "TAG_ON_BRANCH_A", ...)` -> e.g. `1.2.2.4`;
   `rev2 = RCS_getversion(srcfile, "TAG_ON_BRANCH_B", ...)` -> e.g. `1.3.4.2`.
2. `merge_bugid` is `"12345"`, so update.cpp:2804 calls
   `bound_merge_by_bugid(vers->srcfile, rev1, rev2, "12345")`.
3. The walk starts at `1.3.4.2` and follows `previous_version` -> `1.3.4.1` -> `1.3` ->
   `1.2` -> `1.1`. `node->key` is never equal to `rev1` (`1.2.2.4` lives on the other
   branch), so the `break` never fires.
4. `1.1`'s `other_delta` contains `bugid = 12345`, so `startnode = endnode = node("1.1")`
   and the loop then exits because `previous_version` returns NULL.
5. Line 2687 `endnode = previous_version(rcs->versions, vers_of_1_1)` -> NULL.
6. Line 2688 `xstrdup(endnode->key)` -> **segfault** (or, in the server, the child process
   handling the client dies mid-protocol).

The same crash reproduces on a single `-j` with `-i` (`inverse_merges`), because the second
`getmergepoint()` call at update.cpp:2784 deliberately returns a revision on
`vers->vn_rcs`'s branch while `rev2` is on the other branch, again making `rev1`
unreachable from `rev2`.

## Suggested fix
```cpp
	endnode = previous_version(rcs->versions,(RCSVers*)endnode->data);
	if(!endnode)
	{
		/* The bug reaches back to the first revision - nothing to merge from. */
		TRACE(3,"bound_merge_by_bugid: bug present in the initial revision");
		return false;
	}
	rev1=xstrdup(endnode->key);
	rev2=xstrdup(startnode->key);
```
(returning `false` makes `join_file` take its existing `xfree(rev1); xfree(rev2); return;`
path at update.cpp:2804-2810 — note `rev1`/`rev2` have already been nulled by the `xfree`
at 2685-2686, so that path is safe.)

## Refutation attempt
* Could `previous_version` never return NULL in practice? No — update.cpp:2620 has an
  explicit `node=NULL;` for a trunk revision with no `next`, and `rcs.cpp:6273-6277` only
  assigns `vnode->next` when the RCS `next` key has a value, which `1.1` never does.
* Does the `if(!strcmp(node->key,rev1)) break;` guard make `endnode == 1.1` impossible?
  Only when `rev1` is an ancestor of `rev2`. With two independent `-j` tags (or with
  `-i`), `rev1` and `rev2` can sit on different branches, and the walk then runs off the
  bottom of the trunk.
* Is `startnode` guaranteed non-NULL so `startnode->key` is fine? Yes — line 2680 returns
  early when `startnode` is NULL. Only `endnode` after the extra step back is unguarded.
* Is `-B` dead/unused? No: `update()` parses it at update.cpp:191-198 and forwards it to
  the server at update.cpp:361-362; `merge_bugid` gates the call at update.cpp:2803.
