---
id: BUG-lib-02
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/src/mapping.cpp
line: 484
severity: high
category: memory-safety
status: fixed in this slice (audit/02)
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# Misplaced closing parenthesis makes `fnncmp()` compare 0 or 1 characters, then indexes `file` past its end

## Summary
In `lookup_module2()` the length argument of `fnncmp()` swallowed the whole
`&& file[...] == '/'` test. The third argument is therefore the boolean `0` or `1` instead of
`strlen(virtual_repos)`. `__fnncmp(a,b,0)` returns 0 ("equal"), so the branch is taken for
essentially *every* file, and the body then does `file + strlen(virtual_repos)`, reading past
the end of `file` whenever `file` is shorter than `virtual_repos`, and `sprintf`ing the result
into a `char tmp[MAX_PATH]` stack buffer.

## Code
```cpp
// src/mapping.cpp:481-491
		else 
		{
		TRACE(3,"lookup_module2() check between virtual_repos length and file[%d]",strlen(current_directory->virtual_repos));
		if(!fnncmp(file,current_directory->virtual_repos,strlen(current_directory->virtual_repos) && file[strlen(current_directory->virtual_repos)]=='/'))
		{
			sprintf(tmp,"%s%s",current_directory->real_repos,file+strlen(current_directory->virtual_repos));
			file = tmp;
			renamed = 1;
		}
		}
```
`tmp` is declared at src/mapping.cpp:467 as `char tmp[MAX_PATH]`.

The corresponding correct form is used two lines earlier for the exact-match case
(`if(!fncmp(file,current_directory->virtual_repos))`, line 476).

## Why it is a bug
`fnncmp` is a macro (cvsapi/lib/api_system.h) that resolves to `__fnncmp` on win32 (line 35),
`strncasecmp` on `__APPLE__`/`__OS400__` (lines 92, 109) and `strncmp` everywhere else (line 137).
**All three return 0 for a length of 0**: C99 7.24.4.4 specifies that `strncmp` with `n == 0`
compares equal, and the in-tree `__fnncmp` (cvsapi/lib/fncmp.c:57-68) does the same explicitly:

```c
int __fnncmp(const char *a, const char *b, size_t len)
{
	int r;
	while(len && *a && *b) { if((r = __cfc(*a,*b,FsCaseSensitive))!=0) return r; a++; b++; len--; }
	return len?(*a)-(*b):0;
}
```

The third argument here evaluates as

```
(strlen(vr) != 0) && (file[strlen(vr)] == '/')   ->   0 or 1
```

Three separate defects follow:

1. **Prefix test is gone.** When the length evaluates to 1, only the first character of
   `file` is compared with the first character of `virtual_repos`.
2. **The `'/'` boundary check is gone**, and worse, evaluating it *reads* `file[strlen(vr)]`
   even when `file` is shorter than `virtual_repos` — an out-of-bounds read on its own.
3. **When the boundary check fails the length becomes 0, so the comparison "succeeds".**
   This is the inverted-logic case: the branch fires precisely for the files it was meant to
   reject, and then `file + strlen(vr)` is a pointer past the NUL of `file`.

## Failure scenario
Server-side, `current_directory->virtual_repos` is the first line of `CVS/Repository.Virtual`
(`CVSADM_VIRTREPOS`, src/cvs.h:160), loaded in `open_directory()` at src/mapping.cpp:1183 —
i.e. content supplied by the client's sandbox.

Take `virtual_repos = "some/quite/long/virtual/path"` (28 chars) and a file being looked up
named `file = "a.c"` (3 chars, buffer of 4 bytes):

1. `fncmp("a.c", "some/...")` != 0, so the `else` branch at line 481 is entered.
2. `strlen(vr)` is 28, so the length argument evaluates `file[28]` — **28 bytes past the start
   of a 4-byte allocation**. Almost certainly not `'/'`, so the argument is `0`.
3. `fnncmp(file, vr, 0)` returns 0, `!0` is true — the branch is taken.
4. `sprintf(tmp, "%s%s", real_repos, file + 28)` copies whatever unrelated heap bytes follow
   `"a.c\0"` until the next NUL into the `MAX_PATH` stack buffer `tmp`. With enough
   non-NUL heap bytes this is a **stack buffer overflow**; with fewer it is an
   information leak into `file`, which is then used as a repository path and echoed back to
   the client through the module lookup / `TRACE` output.

Even with no crash, every filename in a directory carrying a `Repository.Virtual` gets
silently rewritten to `real_repos + garbage`, so virtual-repository mapping is simply broken.

## Suggested fix
```cpp
		size_t vrlen = strlen(current_directory->virtual_repos);
		if(!fnncmp(file,current_directory->virtual_repos,vrlen) && file[vrlen]=='/')
```

## Refutation attempt
- Verified the "always matches" reading holds for every platform mapping of the `fnncmp` macro,
  not just one: `__fnncmp` returns 0 for `len == 0` at cvsapi/lib/fncmp.c:67, and `strncmp` /
  `strncasecmp` are specified to compare equal for `n == 0`. There is no build in which
  `fnncmp(x,y,0)` is non-zero.
- Verified `fnncmp` resolves only to those three comparators — cvsapi/lib/api_system.h lines 35,
  92, 109 and 137 are the complete set of definitions.
- Verified `tmp` is a fixed `char tmp[MAX_PATH]` on the stack (line 467) and that the
  `sprintf` has no bound.
- Verified `virtual_repos` is only non-NULL when `CVS/Repository.Virtual` exists
  (src/mapping.cpp:1176-1186), so the bug is dormant on plain repositories — this is why it
  has survived; it is not a false positive, just conditionally reached.
- Checked whether some caller guarantees `strlen(file) >= strlen(virtual_repos)`:
  `lookup_module2()` is reached from `_lookup_module2` callers with arbitrary
  short filenames (e.g. `RCSREPOVERSION`, plain file names inside the directory), so no such
  guarantee exists.
