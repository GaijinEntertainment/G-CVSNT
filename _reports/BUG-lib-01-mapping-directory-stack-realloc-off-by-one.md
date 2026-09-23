---
id: BUG-lib-01
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/src/mapping.cpp
line: 1044
severity: high
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: no
---

# `open_directory()` restores `current_directory` one slot too far after `xrealloc`, causing a permanent +1 index drift and a one-element heap overflow

## Summary
After growing `directory_stack`, `open_directory()` re-points `current_directory` at
`directory_stack + directory_stack_size` instead of `directory_stack + directory_stack_size - 1`.
The subsequent `current_directory++` therefore lands one element beyond the intended slot.
The drift is never corrected, so from that moment on `current_directory` indexes
`directory_stack[directory_stack_size]` — which is exactly one past the last valid element when
the stack has grown to full capacity, producing an out-of-bounds `memset` of a whole
`directory_data` (~80 bytes on LP64/LLP64) past the end of the heap block.

## Code
```cpp
// src/mapping.cpp:1034-1052
	if(directory_stack_size == directory_stack_count)
	{
		directory_stack_count *= 2;
		if(directory_stack_count < 50)
			directory_stack_count = 50;
		directory_stack = (directory_data *)xrealloc(directory_stack,sizeof(directory_data)*directory_stack_count);
		if(!directory_stack)
			error(1,errno,"Out of memory");
		if(current_directory)
			current_directory = directory_stack + directory_stack_size;   // <-- line 1044, should be -1
	}

	if(!current_directory)
		current_directory = directory_stack;
	else
		current_directory++;
	directory_stack_size++;
	TRACE(3,"open_directory() directory_stack_size increased by one to %d (rubbish %d)",directory_stack_size,directory_stack_rubbish);
	memset(current_directory,0,sizeof(directory_data));        // <-- line 1052, the OOB write
```

## Why it is a bug
`directory_stack_count` is the *capacity*, `directory_stack_size` is the *used count*
(see the `xrealloc` sizing at line 1039 and the `for(n=0; n<directory_stack_size; n++)` free
loop at line 77). The class invariant on entry to `open_directory()` — established by the
`current_directory++` at line 1049 and the matching `current_directory--` in
`close_directory()` (line 1402) — is:

```
current_directory == directory_stack + (directory_stack_size - 1)
```

The realloc fix-up at line 1044 must therefore reproduce `directory_stack_size - 1`, not
`directory_stack_size`. Because it does not, the `++` at line 1049 skips one slot, and since
`directory_stack_size` is also incremented, the new invariant becomes
`current_directory == directory_stack + directory_stack_size`, i.e. permanently one past where
the size counter says the top of stack is. `close_directory()` decrements pointer and counter
in lockstep, so the drift is never resynchronised; every later realloc re-establishes exactly
the same +1 offset.

Two distinct consequences:

1. **Skipped, uninitialised slot.** `directory_stack[50]` is never `memset`, yet it is inside
   the range `0 .. directory_stack_size-1` that `free_modules2()` (src/mapping.cpp:77-94)
   walks, calling `freercsnode(&directory_stack[n].repository_rcsfile)`, `dellist()` and
   `xfree()` on whatever `xrealloc` left there. That is a free of uninitialised heap words.
2. **One-past-the-end write.** With capacity 100 the 100th push writes to index 100.

## Failure scenario
`directory_stack_count` starts at 0 and is bumped to 50 on the first call, then doubles.
Counting non-"rubbish" `open_directory()` calls (`recurse.cpp:1265`, `recurse.cpp:1487`,
`update.cpp:1145-1146`, `checkout.cpp:525-526` each push one or two entries per directory
level):

| call | `directory_stack_size` on entry | `directory_stack_count` | index written |
|---|---|---|---|
| 1   | 0  | 0 -> 50   | 0   |
| ... | .. | 50        | ..  |
| 50  | 49 | 50        | 49  |
| 51  | 50 | 50 -> 100 | **51** (slot 50 skipped, left as raw realloc garbage) |
| 52  | 51 | 100       | 52  |
| ... | .. | 100       | ..  |
| 99  | 98 | 100       | 99  (last valid) |
| 100 | 99 | 100       | **100 — out of bounds** |

At call 100 the check `directory_stack_size(99) == directory_stack_count(100)` is false, so no
growth happens, yet `current_directory++` yields `&directory_stack[100]` and
`memset(current_directory,0,sizeof(directory_data))` writes ~80 bytes past the end of the
8000-byte block. Everything written into that entry afterwards
(`repository_rcsfile`, `virtual_repos`, `real_repos`, `rename_script` …) also lands out of
bounds, and `close_directory()` later calls `freercsnode`/`dellist`/`xfree` on those
out-of-bounds fields.

Reaching 100 stack entries only needs ~100 nested directories (or ~50 with the double-push in
`update.cpp`/`checkout.cpp`) in a checkout/update — trivially creatable by any user with
commit rights.

## Suggested fix
```cpp
		if(current_directory)
			current_directory = directory_stack + directory_stack_size - 1;
```

## Refutation attempt
- Checked that `xrealloc` never returns NULL and never shrinks (lib/ `xrealloc`), so the fix-up
  is purely about re-basing the pointer — the `-1` is the only correct value.
- Checked `close_directory()` (line 1383) — it decrements pointer *and* counter together, so it
  cannot absorb the drift.
- Checked the `directory_stack_rubbish` early-return path (line 1024-1032): it returns before
  the push, so it cannot compensate either.
- Checked whether the stack is really LIFO nested rather than reset per call: `recurse.cpp`
  opens at 1265/1487 and closes at 1362/1379/1511 around the recursive descent, so depth
  accumulates.
- The `if(!directory_stack) error(...)` guard after `xrealloc` is dead code, but harmless and
  not the issue here.
