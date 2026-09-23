---
id: BUG-lib-14
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/src/RecurseRepository.cpp
line: 102
severity: low
category: typo
status: open, but dead code - src/RecurseRepository.cpp is in no build file (docs/08-source-map.md lists it under dead code); fixing it has no effect on any binary
verdict: CONFIRMED
fix_size_loc: 2
behavior_change: yes
---

# `cvs::sprintf` called with a 2-conversion format and 3 arguments: the filename is silently dropped and every child gets the name `<parent>//`

## Summary
`GetPhysTree()` builds each child entry's logical and physical names with the format `"%s/%s"`
but passes **three** value arguments. The second `%s` consumes the literal `"/"`, the real
filename argument is never consumed, and the result is `<parent>//` for every entry in the
directory. The same function also keeps using a reference into the vector it is appending to.

## Code
```cpp
// src/RecurseRepository.cpp:94-105
	while(dir.next(inf))
	{
		if(!inf.isdir)
			continue;

		// Handle islink here like a separate mapping possibly

		CvsBasicEntry ent;
		cvs::sprintf(ent.logical_name,256,"%s/%s",item.logical_name.c_str(),"/",inf.filename.c_str());
		cvs::sprintf(ent.physical_name,256,"%s/%s",item.physical_name.c_str(),"/",inf.filename.c_str());
		list.push_back(ent);
	}
```

## Why it is a bug
`cvs::sprintf` (cvsapi/cvs_string.h:187-194) is variadic and forwards to
`cvs::vsprintf` -> `::vsnprintf`, which consumes exactly one argument per conversion. With
`"%s/%s"` it takes two:

| conversion | argument consumed | output |
|---|---|---|
| `%s` | `item.logical_name.c_str()` | the parent name |
| `/`  | — | `/` |
| `%s` | `"/"` | `/` |
| —    | `inf.filename.c_str()` **never read** | — |

So `ent.logical_name` is `"<parent>//"` and `ent.physical_name` is `"<parent>//"`, identical for
every subdirectory found in the scan — the filename that the whole loop exists to append is
thrown away. Either the format needs a third conversion (`"%s/%s%s"`) or, more likely given the
explicit `"/"` argument, the format should be `"%s%s%s"`.

## Failure scenario
`CRecurseRepository::BeginRecursion()` (src/RecurseRepository.cpp:35) seeds `dirlist` with the
module and then loops `GetPhysTree(dirlist[i],dirlist)` over the growing vector. With this bug,
scanning a repository directory `repo/bar` that contains subdirectories `a` and `b` appends two
entries both named `repo/bar//` instead of `repo/bar/a` and `repo/bar/b`. `CDirectoryAccess::open`
on `repo/bar//` re-opens the *same* directory, so the loop appends `repo/bar///` next, and so on —
an unbounded vector growth rather than a tree walk.

A second, independent defect in the same loop: `item` is a reference to `dirlist[i]`
(`GetPhysTree(CvsBasicEntry& item, std::vector<CvsBasicEntry>& list)` called as
`GetPhysTree(dirlist[i],dirlist)`), and the loop body calls `list.push_back(ent)`. As soon as a
`push_back` reallocates the vector's storage, `item` dangles, and the *next* iteration reads
`item.logical_name.c_str()` from freed memory. `mod1.Translate(dirlist[i],dirlist)` and
`mod2.Translate(dirlist[i],dirlist)` on lines 52-53 have the same shape.

Severity is held at low because `CRecurseRepository::BeginRecursion()` has no callers anywhere in
the tree (grep over `src/` finds only the definition) — this is unfinished work-in-progress. The
defects are nonetheless real and will fire the moment it is wired up.

## Suggested fix
```cpp
		cvs::sprintf(ent.logical_name,256,"%s/%s",item.logical_name.c_str(),inf.filename.c_str());
		cvs::sprintf(ent.physical_name,256,"%s/%s",item.physical_name.c_str(),inf.filename.c_str());
```
and take `item` by value (or index into `list` freshly after each `push_back`) so the reference
cannot dangle.

## Refutation attempt
- Checked `cvs::sprintf`'s signature to be sure it is not some format-checking wrapper that would
  reorder or validate: it is `template<class _Typ> void sprintf(_Typ& str, size_t size_hint,
  const char *fmt, ...)` forwarding straight to `vsnprintf` (cvsapi/cvs_string.h:187).
- Checked `cvs::str_prescan` (cvsapi/cvs_string.cpp:69-215), which the vsprintf path runs first:
  it only walks arguments named by the format, so it neither notices nor compensates for the
  extra one. It cannot rescue the missing filename.
- Checked that passing an extra variadic argument is not itself UB — it is not; it is simply
  ignored. The bug is the lost filename, not a crash here.
- Confirmed the "unused" status by grepping `src/` for `CRecurseRepository` and `BeginRecursion`:
  the only hits are the class's own definition and header.
