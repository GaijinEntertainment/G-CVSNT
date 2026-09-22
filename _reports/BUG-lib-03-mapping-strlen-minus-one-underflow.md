---
id: BUG-lib-03
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/src/mapping.cpp
line: 1186
severity: medium
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 12
behavior_change: no
---

# `buf[strlen(buf)-1] = '\0'` writes one byte *before* the buffer when a CVS admin line starts with a NUL byte

## Summary
Five places in `src/mapping.cpp` strip a trailing newline with `x[strlen(x)-1]='\0'` without
checking that `strlen(x)` is non-zero. `strlen()` returns `size_t`, so `strlen(x)-1` on an
empty string is `SIZE_MAX` and `x[SIZE_MAX]` wraps to `x[-1]` — a one-byte out-of-bounds write
immediately before a heap block (`getline` cases) or before a stack array (`fgets` case).
`getline`/`fgets` can legitimately return a non-empty *record* whose `strlen()` is zero when
the file contains an embedded NUL byte.

## Code
```cpp
// src/mapping.cpp:1150-1153   (CVS/Rename, heap buffers from getline)
		while(getline(&from, &n, fp)>0 && getline(&to, &n, fp)>0)
		{
			from[strlen(from)-1]='\0';
			to[strlen(to)-1]='\0';

// src/mapping.cpp:1184-1186   (CVS/Repository.Virtual, heap buffer from getline)
		if(getline(&current_directory->virtual_repos,&len,fp)<1)
			error(1,errno,"Couldn't read %s",CVSADM_VIRTREPOS);
		current_directory->virtual_repos[strlen(current_directory->virtual_repos)-1]='\0';

// src/mapping.cpp:1193-1195   (CVS/Repository, heap buffer from getline)
		if(getline(&current_directory->real_repos,&len,fp)<1)
			error(1,errno,"Couldn't read %s",CVSADM_REP);
		current_directory->real_repos[strlen(current_directory->real_repos)-1]='\0';

// src/mapping.cpp:1441-1444   (CVS/Rename rewrite, stack arrays char from[MAX_PATH], to[MAX_PATH])
		while(fgets(from,sizeof(from),fpin) && fgets(to,sizeof(to),fpin))
		{
			from[strlen(from)-1]='\0';
			to[strlen(to)-1]='\0';
```

## Why it is a bug
The code assumes "return value > 0 implies the buffer ends in `'\n'`". Neither `getline`
nor `fgets` guarantees that:

* `getdelim()` (lib/getdelim.c:130-140) stores every byte it read, including `'\0'`, and
  returns `read_pos - *lineptr`. For a file whose first byte is `0x00`, it returns 1 while
  `strlen(buf)` is 0.
* `fgets()` likewise stores the NUL byte from the file and its own terminator; `strlen()` sees 0.
* Even without embedded NULs, a final line with no trailing newline (or, for `fgets`, a line
  longer than `MAX_PATH-1`) makes the code silently truncate a *real* character rather than a
  newline — a separate correctness defect at the same sites.

The guards that exist (`> 0`, `< 1`) test the *byte count*, not `strlen()`, so they do not
prevent the underflow.

## Failure scenario
Server-side, `open_directory()` reads these files out of the per-connection sandbox; locally
they are the user's own `CVS/` admin files. Create a working directory whose
`CVS/Repository.Virtual` consists of a single NUL byte:

```sh
printf '\000' > CVS/Repository.Virtual
```

Then in `open_directory()` (src/mapping.cpp:1176-1186):

1. `isfile(tmp)` is true, `fopen` succeeds.
2. `getline(&current_directory->virtual_repos,&len,fp)` returns **1** (one byte consumed), so
   the `< 1` guard does not fire and no `error()` is raised.
3. `strlen(current_directory->virtual_repos)` is **0**.
4. `virtual_repos[(size_t)-1] = '\0'` writes a zero byte at `virtual_repos - 1`, i.e. into the
   last byte of the malloc chunk header of the `getdelim` allocation.

On glibc that clears the low byte of the chunk `size` field (the `PREV_INUSE`/size bits),
so the next `free()`/`realloc()` of that block aborts with `malloc(): invalid size` or
corrupts the arena. The same construction against `CVS/Rename` (line 1152) and the stack
arrays at line 1443 gives a one-byte stack underflow instead.

## Suggested fix
Introduce a helper and use it at all five sites:

```cpp
static void chop_newline(char *s)
{
	size_t l = s ? strlen(s) : 0;
	while(l && (s[l-1]=='\n' || s[l-1]=='\r'))
		s[--l] = '\0';
}
...
		chop_newline(from);
		chop_newline(to);
...
		chop_newline(current_directory->virtual_repos);
...
		chop_newline(current_directory->real_repos);
```

## Refutation attempt
- Checked `lib/getdelim.c` to confirm the return value counts bytes, not `strlen`, and that a
  lone `0x00` byte followed by EOF yields `ret == 1` rather than `-1`
  (`if (c == EOF) { if (read_pos == *lineptr) return -1; else break; }`, lines 118-124).
  glibc's native `getline` behaves identically.
- Checked that the `error(1,...)` calls really do not return (they call `exit`), so the
  `< 1` guard cannot be relied on for the `strlen == 0` case — it simply never fires.
- Checked that `xfree`/`xfree_s` (src/subr.cpp:134) sets the pointer to NULL, so the
  `if(!to[0]) xfree(to);` at line 1156-1157 is *not* a use-after-free; that part is fine.
- Considered whether a NUL byte could never reach these files: `CVSADM_RENAME` is appended to
  by `client.cpp:3657` (`fopen(CVSADM_RENAME,"a")`) with filenames from the protocol, and all
  three files are plain sandbox files the invoking user can write directly, so no filter
  stands between an arbitrary byte and this code.
