---
id: BUG-update-15
area: client/update core
file: cvsnt/cvsnt-2.5.05.3744/src/find_names.cpp
line: 297
severity: medium
category: leak
verdict: CONFIRMED
fix_size_loc: 3
behavior_change: no
---

# `find_rcs()` leaks a `Node` plus its key for every repository file that is already in the list — i.e. for nearly every file of every update

## Summary
`find_rcs()` allocates a `Node` and `xstrdup`s the filename into `p->key` *before* testing
whether the name is already in the list. When `findnode_fn()` says it is, the freshly
allocated node is simply dropped: no `freenode(p)`, no `xfree`. Since `Find_Names()` fills
the list from `CVS/Entries` first and only then scans the repository, the "already present"
branch is the normal case for every tracked file.

## Code
```cpp
/* src/find_names.cpp:290-302 */
			q = map_fixed_rename(dir,dp->d_name);

			if(q && *q && (!regex || regex_filename_match(regex, q)))
			{
				p = getnode ();                       /* 294 */
				p->type = FILES;
				p->key = xstrdup (q);                 /* 296 */
				if(!findnode_fn(list,p->key))         /* 297 */
				{
					if(addnode (list, p) != 0)
						freenode (p);
				}
				/*  <-- no else: when findnode_fn() hits, `p` is leaked */
			}
```

## Why it is a bug
`getnode()` returns a node taken from the free cache or freshly `xmalloc`'d, and
`p->key = xstrdup (q)` allocates a second block. `freenode(p)` (hash.cpp:201-219) is the only
thing that releases the key (via `freenode_mem`) and returns the node to `nodecache`. On the
`findnode_fn() != NULL` path neither happens, so both blocks are unreachable for the rest of
the process.

The `findnode_fn` pre-check is a deliberate fork addition: `addnode` -> `insert_before`
(hash.cpp:269-303) buckets by `hashp (p->key)` (case-sensitive) but compares with
`fncmp (p->key, q->key)` (case-insensitive on case-folding platforms), so a case-differing
duplicate would slip past `addnode`'s own duplicate detection. The check is correct; only
the cleanup on its taken branch is missing. The stock shape it replaced was:
```cpp
	    p = getnode ();
	    p->type = FILES;
	    p->key = xstrdup (dp->d_name);
	    if (addnode (list, p) != 0)
		freenode (p);
```
— note `freenode` on every rejection.

## Failure scenario
Local (non-client/server) `cvs update`, or the server side of a remote update:

1. `update()` sets `which = W_LOCAL | W_REPOS` (update.cpp:506).
2. `do_recursion` calls
   `Find_Names (mapped_repository, lwhich, frame->aflag, &entries, repository)`
   (recurse.cpp:766-767).
3. `Find_Names` first walks `CVS/Entries` and adds every registered file to `files`
   (find_names.cpp:66-79), then calls `find_rcs (repository, files, regex)`
   (find_names.cpp:88) for the same directory.
4. `find_rcs` iterates the `*,v` files. For each one that is already registered — which is
   every file under version control that the user has checked out — `findnode_fn(list, p->key)`
   returns non-NULL and `p` (node + key string) leaks.
5. The Attic is then scanned with the same function (find_names.cpp:92-104), leaking again
   for every Attic file that shadows a live name.

For a module with 100,000 files this leaks 100,000 `Node` structures plus 100,000 filename
strings in a single `cvs update` — on the order of 10 MB and growing linearly with tree
size, in the server process for remote clients. It is a leak only: no corruption and no
wrong output.

## Suggested fix
```cpp
				p = getnode ();
				p->type = FILES;
				p->key = xstrdup (q);
				if(findnode_fn(list,p->key) || addnode (list, p) != 0)
					freenode (p);
```

## Refutation attempt
* Does `freenode` really own the key? Yes — `freenode` (hash.cpp:201) calls `freenode_mem (p)` (hash.cpp:181) before
  recycling the node into `nodecache`, which is what releases `p->key`. Nothing
  else in `find_rcs` touches `p` after the `if`.
* Could `p` be reachable through `list` anyway (so `dellist` would free it)? No — it is
  only linked in by `addnode`, which is not called on this branch. `p` is a plain local
  whose value is overwritten on the next loop iteration.
* Is `q` itself leaked too? No — `map_fixed_rename` (mapping.cpp) returns either its
  `name` argument or an interior pointer into an existing node's data; it allocates nothing.
* Is the branch rare? The opposite: `Find_Names` deliberately seeds the list from
  `CVS/Entries` before calling `find_rcs` (find_names.cpp:66-88), so for a normal checked-out
  directory every repository file is already present.
* Note the sibling function `find_dirs` also leaks its `tmp` buffer on the `errno != 0`
  early return (find_names.cpp:417-422, which returns without the `xfree (tmp)` performed on
  the success path at find_names.cpp:425-426) — same class, far rarer trigger.
