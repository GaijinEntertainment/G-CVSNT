---
id: BUG-server-20
area: commit
file: cvsnt/cvsnt-2.5.05.3744/src/rcs_checkin.cpp
line: 1450
severity: low
category: typo
verdict: PLAUSIBLE
fix_size_loc: 1
behavior_change: no
---

# `RCS_cmp_file` uses bitwise `&` instead of `&&`, silently testing only bit 0 of `ignore_keywords`

## Summary
`if(ignore_keywords&!(expand.flags&KFLAG_BINARY))` combines an arbitrary integer flag with a boolean using bitwise AND. Since `!(...)` is exactly 0 or 1, the expression degenerates to `ignore_keywords & 1` — it consults only the low bit of `ignore_keywords`, and it loses short-circuiting.

## Code
```cpp
// rcs_checkin.cpp:1449-1454
    // Effectively do a -k-v in the expansion
    if(ignore_keywords&!(expand.flags&KFLAG_BINARY))
    {
        expand.flags&=~(KFLAG_VALUE|KFLAG_VALUE_LOGONLY);
        options = RCS_rebuild_options(&expand,_opt);
    }
```

The parameter it tests is a plain `int`:
```cpp
// rcs_checkin.cpp:1433
int RCS_cmp_file (RCSNode *rcs, const char *rev, const char *options, const char *filename, int ignore_keywords)
```

Every other flag test in the same function uses the correct operator, e.g. rcs_checkin.cpp:1457 `if ((expand.flags&KFLAG_BINARY_DELTA))` and rcs_checkin.cpp:1605 `if((data->expand.flags&KFLAG_BINARY) || (!data->ignore_keywords && !(data->expand.flags&KFLAG_ENCODED)))` — note the `&&` there, on the same variable.

## Why it is a bug
`&` has lower precedence than `!` but is not a logical operator: it AND-s the *bit patterns*. `!(expand.flags&KFLAG_BINARY)` yields the `int` 0 or 1, so the whole condition is `ignore_keywords & 1`. Any even non-zero value of `ignore_keywords` would be treated as "false" even though the caller meant "true", and the keyword-suppression step (`-k-v`) would be skipped, causing `RCS_cmp_file` to compare expanded keywords that the caller explicitly asked to ignore — i.e. report a file as *different* when it is not, which for `commit -f` handling means an unnecessary revision.

## Failure scenario
I could **not** construct a failing case with the current sources, which is why this is marked PLAUSIBLE rather than CONFIRMED. `ignore_keywords` reaches `RCS_cmp_file` only through:

* `commit.cpp:99` `static int ignore_keywords;` set solely by `commit.cpp:406` `ignore_keywords = 1;`, threaded through `Classify_File` (commit.cpp:813) -> `No_Difference` (classify.cpp:107/244/311/358) -> `RCS_cmp_file` (no_diff.cpp:42);
* literal `0` at checkin.cpp:87, diff.cpp:1002, import.cpp:807.

So today the value is always 0 or 1 and `& 1` is indistinguishable from `&&`. The defect becomes live the moment `ignore_keywords` is widened into a bitmask (the obvious next step given the surrounding `kflag` bitmask style) or set from an option value other than 1 — at which point the failure is silent and hard to spot, because the line reads as a logical test.

## Suggested fix
```cpp
    if(ignore_keywords && !(expand.flags&KFLAG_BINARY))
```

## Refutation attempt
* *Could `&` be deliberate here?* No plausible reading. `ignore_keywords` is not a bitmask in any current code, and the right-hand operand is a logical negation, not a mask — masking a flag word with `0`/`1` is meaningless.
* *Is there a precedence trap making it worse than described?* `!` binds tighter than `&`, so it parses as `ignore_keywords & (!(expand.flags & KFLAG_BINARY))` — which is the reading above. There is no additional surprise.
* *Does losing short-circuiting matter?* Not here; both operands are side-effect-free.
* *Is the same mistake repeated elsewhere in scope?* I grepped for `if (...)` conditions using a single `&`/`|` between comparison-like operands across all sixteen files; every other hit was a legitimate bitmask test against a `KFLAG_*`/`RQ_*`/`CVS_CMD_*` constant. This is the only one.
