---
id: BUG-lib-12
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/src/root.cpp
line: 385
severity: high
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 2
behavior_change: no
---

# `strcpy(newroot->method, xstrdup(value))` overflows the existing `method` allocation (or dereferences NULL), and leaks the duplicate

## Summary
`parse_keyword()` handles the CVSROOT `method=`/`protocol=` keyword by `strcpy`ing the new value
*into* the block `newroot->method` already points at, instead of replacing the pointer. The
existing block is sized for the method taken from the `:method:` prefix, so a longer keyword
value overflows the heap; when `method` is NULL — the `local` case, the empty-method case, and
the whole `[keyword=value,…]` root form — it is a NULL dereference. The `xstrdup()` result is
also leaked on every call.

## Code
```cpp
// src/root.cpp:376-386
static int parse_keyword(char *keyword, char **p, cvsroot *newroot)
{
	char value[256];

	get_keyword(value,sizeof(value),p);
	if(!strcasecmp(keyword,"method") || !strcasecmp(keyword,"protocol"))
	{
		CProtocolLibrary lib;
		if(*value && strcasecmp(value,"local"))
			strcpy(newroot->method,xstrdup(value));      // <-- line 385
		if(newroot->method)
```

`method` is `char *method;` (cvstools/cvsroot.h:31) — a pointer, not an inline array. Every other
assignment to it in this file uses the correct form, e.g. src/root.cpp:601
`newroot->method = xstrdup(method);`.

## Why it is a bug
`strcpy(dst,src)` writes `strlen(src)+1` bytes at `dst`. Here `dst` is `newroot->method`, whose
allocation was made by `xstrdup(method)` at src/root.cpp:601 and is exactly
`strlen(<prefix method>)+1` bytes. Nothing re-sizes it before the copy. And `newroot->method` is
explicitly set to NULL on three separate paths before `parse_keyword()` can run:

* `new_cvsroot_t()` initialises it to NULL (src/root.cpp:275) — this is the state for the
  `[method=…,…]` bracket root form, whose `parse_keyword()` loop is at src/root.cpp:524-544;
* src/root.cpp:603-607 — `:local;…:` frees it and sets NULL;
* src/root.cpp:609-613 — an empty method (`:;…:`) frees it and sets NULL.

The intended statement is plainly `newroot->method = xstrdup(value);` — that also fixes the leak,
since as written the freshly duplicated string is never stored anywhere and never freed.

## Failure scenario
Both variants are reachable from a single command-line argument (`cvs -d`), from `$CVSROOT`,
from a sandbox `CVS/Root` file (read by `Name_Root()`, src/root.cpp:78), and from
`cvs switch` (src/switch.cpp:60), all of which funnel into `parse_cvsroot()`.

**Heap overflow.** `cvs -d ":ext;method=pserverpserverpserver:user@host:/repo"`

1. src/root.cpp:593-601: the prefix method is `"ext"`, so
   `newroot->method = xstrdup("ext")` — a **4-byte** heap block.
2. The keyword loop at src/root.cpp:636-644 calls
   `parse_keyword("method", …)` with `value = "pserverpserverpserver"` (21 chars).
3. `strcpy(newroot->method, …)` writes **22 bytes into the 4-byte block** — 18 bytes past the
   end. `value` is a `char value[256]` filled by `get_keyword()`, so up to 255 bytes of
   attacker-chosen data can be written past a 2-byte (`:x;…`) allocation.

**NULL dereference.** `cvs -d ":local;method=pserver:/repo"` or
`cvs -d "[method=pserver,host=h,directory=/repo]"`

In both, `newroot->method` is NULL when `parse_keyword()` runs, so `strcpy(NULL, …)` faults
immediately.

## Suggested fix
```cpp
		if(*value && strcasecmp(value,"local"))
		{
			xfree(newroot->method);
			newroot->method = xstrdup(value);
		}
```

## Refutation attempt
- Checked `cvstools/cvsroot.h:31` to be sure `method` is not a fixed-size array member (which
  would make `strcpy` merely ugly rather than wrong) — it is `char *method;`.
- Checked whether `newroot->method` is guaranteed non-NULL and large enough at every
  `parse_keyword()` call site: the `[...]` form at src/root.cpp:524-544 runs *before* any method
  is assigned, and the `;` form at src/root.cpp:636-644 runs after src/root.cpp:601-613, which
  can leave the pointer NULL. Neither guarantees a size.
- Checked whether `get_keyword()` bounds `value`: it does (`get_keyword(value,sizeof(value),p)`,
  src/root.cpp:380), so the 256-byte stack buffer itself is safe — the overflow is entirely in
  the destination heap block.
- Checked the surrounding code for a compensating `xrealloc`: there is none; line 386 immediately
  uses `newroot->method` as if the copy had succeeded.
