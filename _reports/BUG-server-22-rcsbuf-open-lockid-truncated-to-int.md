---
id: BUG-server-22
area: server/locking
file: cvsnt/cvsnt-2.5.05.3744/src/rcs.cpp
line: 875
severity: medium
category: correctness
status: fixed on this branch (Tier 1 slice: rcsbuf_open snapshots the lock id as size_t)
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: no
---

# `rcsbuf_open` truncates the `size_t` lock id to `int` before testing it, so a lock id with zero low 32 bits reads as "no lock held"

## Summary
`rcsbuf_open` snapshots the currently-held lock id into an `int` and later uses it as a boolean
"was a lock already held?". `lockId` is a `size_t`. On any LLP64 or LP64 target `int` is 32 bits
and `size_t` is 64, so the top half is discarded. A lock id whose low 32 bits happen to be zero is
seen as zero, and the function acquires a **second** lock on a file that already holds one,
overwriting `rcsbuf->lockId` and leaking the first lock for the lifetime of the process.

## Code
```cpp
// src/rcs.cpp:872-875
static int rcsbuf_open(struct rcsbuffer *rcsbuf, const char *filename)
{
	FILE *fp;
	int orig_lockId = rcsbuf->lockId;      // <-- size_t truncated to int
```

The field:
```cpp
// src/rcs.h:75
	size_t lockId;
```

The producer, which also returns `size_t`:
```cpp
// src/cvs.h:599
size_t do_lock_file(const char *file, const char *repository, int write, int wait);
```

The only use, as a boolean:
```cpp
// src/rcs.cpp:906-913
	if(!orig_lockId)
	{
		rcsbuf->lockId=do_lock_file(filename, NULL, lock_for_write, 1);
		if(!rcsbuf->lockId)
		{
			rcsbuf_close(rcsbuf);
			return 0;
		}
```

## Why it is a bug
`orig_lockId` exists solely to answer "does this rcsbuffer already hold a lock?", so that reopening
a file that is already locked does not lock it twice. Narrowing the value to `int` breaks that test
in two ways:

* **Zero low half.** A lock id such as `0x0000000100000000` truncates to `0`, the guard says "no
  lock", and a second `do_lock_file` runs. The new id overwrites `rcsbuf->lockId`, so the original
  is never passed to `do_unlock_file` — the lock is held until the process exits.
* **Sign.** A lock id with bit 31 set becomes negative. `!orig_lockId` is still false, so this half
  is harmless today, but the value is meaningless as an `int` and the compiler is entitled to warn.

Nothing else reads `orig_lockId`, so the fix is purely the declared type.

Note that the two `rcsbuf_close(rcsbuf)` calls on the failure paths (`:911`, `:929`) do not clear
`rcsbuf->lockId` — `rcsbuf_close` (`src/rcs.cpp:967`) frees the buffer and the relocation array but
does not touch `lockId`. That is correct for the second site, which unlocks explicitly first
(`:926`), and is why the leak above is silent rather than fatal.

## Failure scenario
Run a server whose lock service hands out 64-bit ids — any generator that mixes a per-connection
counter into the high half, or that seeds from a timestamp, will eventually produce one with a zero
low half. The first `rcsbuf_open` on a `,v` takes lock `L1 = 0x0000000100000000`. A later reopen of
the same `rcsbuffer` (for example the re-open on the POSIX race path at `:924`, or any second parse
of the node) reads `orig_lockId == 0`, takes `L2`, and assigns it over `L1`. When `freercsnode`
unlocks (`src/rcs.cpp:766`) it releases `L2`. `L1` stays held. Every later writer on that `,v`
blocks on a lock nobody can release, and the only recovery is restarting the lock server.

The probability per id is low, but the consequence is a wedged repository path rather than a
transient error, and the cost of correctness here is one keyword.

## Suggested fix
```cpp
	size_t orig_lockId = rcsbuf->lockId;
```

## Refutation attempt
Checked whether `orig_lockId` is used anywhere that needs an `int` — `grep -n orig_lockId
src/rcs.cpp` returns exactly two lines, the declaration and the `if(!orig_lockId)` test, so widening
the type cannot affect anything else. Checked whether `lockId` might really be 32-bit in practice —
it is declared `size_t` in `src/rcs.h:75` and produced by `do_lock_file`, declared `size_t` in
`src/cvs.h:599`, so the narrowing is real on every 64-bit build. Checked whether a zero id is
reserved as "no lock" and therefore never issued with a zero *low half* specifically — the
zero-means-none convention applies to the whole value, and nothing constrains the low 32 bits. The
finding stands, with severity medium rather than high because it depends on the lock service's id
distribution.
