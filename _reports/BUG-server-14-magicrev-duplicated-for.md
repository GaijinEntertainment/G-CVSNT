---
id: BUG-server-14
area: rcs
file: cvsnt/cvsnt-2.5.05.3744/src/rcs.cpp
line: 2257
severity: low
category: typo
verdict: CONFIRMED
fix_size_loc: 2
behavior_change: no
---

# Duplicated `for` header in `RCS_magicrev` makes the whole `findnextmagicrev` optimisation dead code

## Summary
A botched merge left two `for` statements (and two copies of the same comment) stacked in `RCS_magicrev`. The outer `for (; ; rev_num += 2)` has the inner `for (rev_num = 2; ; rev_num += 2) {...}` as its entire body; the inner loop only exits by `return`, so the outer loop's increment never runs, and the inner loop's initialiser throws away the value `findnextmagicrev` just computed.

## Code
```cpp
// rcs.cpp:2249-2260
    check_rev = xrev;

    /* Prime the pump by finding the next unused magic rev,
     * if none are found, it should return 2.
     */
    rev_num = findnextmagicrev (rcs, rev, 2);      // result computed...

     /* only look at even numbered branches */
    for (; ; rev_num += 2)                          // ...outer loop, body is the next for
    /* only look at even numbered branches */
    for (rev_num = 2; ; rev_num += 2)               // ...and immediately overwritten with 2
    {
	/* see if the physical branch exists */
	(void) sprintf (xrev, "%s.%d", rev, rev_num);
	test_branch = RCS_getbranch(rcs, xrev, 2);
	...
	/* we found a free magic branch.  Claim it as ours */
	return (xrev);
    }
```

## Why it is a bug
`for (A) for (B) { body }` parses as the outer `for` whose single statement is the inner `for`. Since the inner `for` has no condition and every exit from it is `return (xrev);`, control never returns to the outer loop, so `rev_num += 2` in the outer header is unreachable — the outer statement is pure dead code.

Worse, `for (rev_num = 2; ...)` discards `rev_num = findnextmagicrev (rcs, rev, 2)`. `findnextmagicrev` (rcs.cpp:7341) exists solely to skip the linear scan — its own comment says *"Returns defaultrv if it can't figure anything out, then the caller will end up doing a linear search."* With the duplicated header, the caller **always** does the linear search, and `findnextmagicrev`'s work (a `walklist` over the whole symbol table, a `getlist`/`sortlist`/`dellist` cycle) is paid for and discarded on every call.

The duplicated comment line immediately above each `for` is the giveaway: the optimisation was added as a new `for` header and the original one was never deleted.

## Failure scenario
No incorrect result — the linear scan starting at 2 finds the same free magic branch number the optimisation would have. The cost is what changes.

Creating a branch on a file that already has many branches, e.g. `cvs rtag -b RELEASE_N mymodule` on a repository where the files carry 400 existing branches:

* Intended: `findnextmagicrev` returns ~402, the loop confirms it in one or two iterations.
* Actual: `findnextmagicrev` runs (one full symbol-table walk plus a sort), its answer is dropped, and the loop then runs ~200 iterations, each doing an `RCS_getbranch` (walks the delta list) *and* a `walklist (RCS_symbols (rcs), checkmagic_proc, NULL)` over the whole symbol table.

That is roughly 200 × (symbol-table walk) per file instead of one, multiplied by every file in the module. On the large repositories this fork targets, branch creation degrades from linear to quadratic in the number of existing branches — exactly the regression `findnextmagicrev` was written to fix.

## Suggested fix
```cpp
    rev_num = findnextmagicrev (rcs, rev, 2);

    /* only look at even numbered branches */
    for (; ; rev_num += 2)
    {
```
(delete rcs.cpp:2258-2259, the duplicated comment and the `for (rev_num = 2; ...)` header).

## Refutation attempt
* *Could the outer `for` be intended as a labelled/retry wrapper?* No — there is no `break` anywhere in the inner loop body; its only exits are `continue` (which continues the *inner* loop) and `return (xrev)`. `grep -n "break" rcs.cpp` over rcs.cpp:2260-2280 shows none.
* *Is the result actually different, making this a correctness bug too?* No. Starting at 2 and stepping by 2 while testing both `RCS_getbranch(rcs, xrev, 2)` and `checkmagic_proc` finds the lowest free even magic number, which is a valid (indeed the original pre-optimisation) answer. `findnextmagicrev` would have returned a *higher* number, so branch numbering differs between "intended" and "actual", but both are correct and unused. Severity is therefore low.
* *Would the compiler flag it?* Neither GCC's `-Wall` nor MSVC `/W4` warns about a `for` whose body is another `for`; the duplicated comment makes it read plausibly on a quick scan.
* *Is `findnextmagicrev` maybe called for a side effect?* It has none that outlive it — `info.rev_list` is created and `dellist`ed inside, and it does not touch `rcs`.
