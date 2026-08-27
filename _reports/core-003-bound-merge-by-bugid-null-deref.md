# bound_merge_by_bugid dereferences NULL when the bug's earliest change is the first revision

- **File:** cvsnt/cvsnt-2.5.05.3744/src/update.cpp
- **Line(s):** 2680-2689
- **Severity:** medium
- **Confidence:** medium
- **Category:** logic / memory

## Code
```cpp
	if(!startnode)
	{
		TRACE(3,"bound_merge_by_bugid: no bug changes found");
		return false;
	}
	xfree(rev1);
	xfree(rev2);
	endnode = previous_version(rcs->versions,(RCSVers*)endnode->data);
	rev1=xstrdup(endnode->key);          // <-- endnode may be NULL here
	rev2=xstrdup(startnode->key);
```

`previous_version()` explicitly returns NULL when there is no predecessor:
```cpp
static Node *previous_version(List *versions, RCSVers *vers)
{
	if(numdots(vers->version)==1) /* Main branch */
	{
		if(vers->next)
			node = findnode(versions,vers->next);
		else
			node=NULL;               // revision 1.1 has no ->next
	}
	else
	{
		...
		if(p==versions->list)
			node=NULL;
		...
	}
	return node;
}
```

## Why this is a bug
`bound_merge_by_bugid` (used by `cvs update -j ... -B <bugid>`) walks backward from `rev2` collecting revisions carrying the bug id, then rewrites the merge bounds as (predecessor-of-earliest-bug-rev, latest-bug-rev). If the earliest revision containing the bug id is the first revision of the file (trunk 1.1, or the first revision reached when the backward walk exhausts the list — which happens whenever `rev1` is not an ancestor of `rev2`, e.g. a two-tag `-j T1 -j T2 -B bug` merge with tags on diverged branches), `previous_version()` returns NULL and `endnode->key` dereferences a NULL pointer, crashing the client or, worse, the server process performing the merge.

Note also that `findnode(versions, vers->next)` inside `previous_version` can return NULL for a corrupt/trimmed RCS file, giving the same crash.

## Suggested fix
```cpp
Node *prev = previous_version(rcs->versions,(RCSVers*)endnode->data);
if (prev == NULL)
    rev1 = xstrdup(endnode->key);   /* or treat as "merge from the beginning" */
else
    rev1 = xstrdup(prev->key);
```
(choosing semantics deliberately: with no predecessor the merge base should be the earliest revision itself, mirroring the `rev1?rev1:"0"` handling in join_file).
