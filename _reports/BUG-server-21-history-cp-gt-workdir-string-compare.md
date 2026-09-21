---
id: BUG-server-21
area: history
file: cvsnt/cvsnt-2.5.05.3744/src/history.cpp
line: 801
severity: low
category: typo
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# `cp > workdir` in `history_write` is a lexicographic string comparison, not the intended pointer bounds check

## Summary
The loop that measures the common suffix between the working directory and the repository path guards its backward walk with `cp > workdir`. `cp` is a `char *` but `workdir` was converted from a `char[]` to a `cvs::string` (`std::basic_string<char>`) in this fork, so the expression silently binds to `operator>(const char*, const basic_string&)` and compares the *text* of the suffix against the *text* of the whole path. It also writes into the string's buffer past `length()` afterwards.

## Code
```cpp
// history.cpp:665-676  (declarations)
	cvs::string workdir;
	...
    char *slash = "", *cp;
    const char *repos,*cp2;
```
```cpp
// history.cpp:799-808
    cp = (char*)workdir.c_str() + workdir.length() - 1;
    cp2 = repos + strlen (repos) - 1;
    for (i = 0; cp2 >= repos && cp > workdir && *cp == *cp2--; cp--)   // <-- string compare
		i++;

    if (i > 2)
    {
		i = strlen (repos) - i;
		sprintf ((cp + 1), "*%x", i);                                   // <-- writes into the string buffer
    }
```

Upstream CVS, from which this is a direct transcription, declares `char workdir[PATH_MAX]`, where `cp > workdir` is a pointer comparison against the start of the buffer. `cvs::string` is `STD_STR_CLASS<char>` = `std::basic_string<char>` (cvsapi/cvs_string.h:123), which has no conversion to `char *`, so overload resolution picks the standard library's `operator>(const charT* lhs, const basic_string<charT>& rhs)` instead — and the compiler emits no warning.

## Why it is a bug
`cp > workdir` now evaluates `workdir.compare(cp) < 0`, i.e. "is the suffix beginning at `cp` lexicographically greater than the entire path". That has nothing to do with whether `cp` has reached the start of the buffer. The loop therefore stops on the first byte value that happens to sort below `workdir[0]`, rather than at the buffer start.

Concretely, with `workdir = "/home/build/proj/src"` and `repos = "myrepo/proj/src"`:
* At `cp` = last byte, the suffix is `"c"`; `'c'(0x63) > '/'(0x2F)` so the guard passes and the walk proceeds while characters match.
* If the path contains any byte below `'/'` — `'.'` (0x2E), `'-'` (0x2D), `'+'`, `' '`, or a `'*'` — the guard flips to false as soon as `cp` reaches it, and the walk stops there even though more characters match.
* If `tilde` is `"~"` (history.cpp:715/740, the home-directory case), `workdir[0]` is `'~'` (0x7E), which sorts above nearly every path character, so the guard is false on the very first test and the loop never runs at all.

The result is that the `*<hex>` suffix compression documented in the long comment block at history.cpp:765-799 is applied inconsistently or not at all. The records stay *readable* — the offset written is `strlen(repos) - i` for whatever `i` was reached, and the reader reconstructs `workdir[0..len-i-1] + repos[len-i..]` correctly for any `i` — so this is a silent loss of the space optimisation rather than corruption.

Secondarily, `sprintf ((cp + 1), "*%x", i)` writes `1 + <hex digits> + 1` bytes at offset `length() - i` of a `std::string`'s buffer. Only `i + 1` bytes are inside the range `c_str()` guarantees (`[0, length()]`). With the minimum qualifying `i` of 3 and a repository path longer than 258 characters, the write needs 5 bytes where 4 are guaranteed — past the NUL terminator. In practice `cvs::sprintf(workdir, 80, ...)` (history.cpp:752) sizes the string with an 80-byte hint so there is usually slack capacity, which is why this has not been observed; it is still outside what `basic_string` promises.

## Failure scenario
Any commit on a server whose working directory is under the CVS user's home directory: `PrCurDir` is advanced past the home prefix and `tilde` becomes `"~"` (history.cpp:715), so `workdir` starts with `'~'`. Every `cp > workdir` test then compares a suffix starting with a normal path character (`'a'`-`'z'`, `'/'`, digits — all below `0x7E`) against a string starting with `'~'`, yielding false immediately. `i` stays 0, `if (i > 2)` never fires, and every `CVSROOT/history` record is written with the full uncompressed working directory instead of the `~/work*9` form the format was designed around. On a busy server the history file grows noticeably faster than intended, and records no longer match the documented format the comment block describes.

## Suggested fix
```cpp
    for (i = 0; cp2 >= repos && cp > workdir.c_str() && *cp == *cp2--; cp--)
```
(and, to be strictly correct about the buffer write, build the compressed value into a temporary and `workdir.assign()` it rather than `sprintf`-ing through `c_str()`).

## Refutation attempt
* *Does `cvs::string` have an implicit conversion to `char*` that would make this a pointer comparison after all?* No. cvsapi/cvs_string.h:123 defines `typedef STD_STR_CLASS<char> string;` where `STD_STR_CLASS` is `std::basic_string` (or `__gnu_cxx::__versa_string` on old libstdc++ ABIs). Neither provides such a conversion; both provide the `const charT*` vs `basic_string` relational operators.
* *Can `cp` walk before the start of the buffer (making this a memory-safety bug)?* No. When `cp` reaches `workdir.c_str()`, the suffix *is* the whole string, so `workdir.compare(cp) < 0` is `0 < 0` — false — and the loop stops. The accidental behaviour is correct at exactly that one point, which is why the bug is only a lost optimisation.
* *Could `workdir` be empty, making the initial `cp = c_str() + length() - 1` already out of range?* `cvs::sprintf(workdir, 80, "%s%s%s%s", tilde, PrCurDir, slash, update_dir)` always produces at least `PrCurDir`, and `tilde` is `"~"` in exactly the case where `PrCurDir` could be empty, so `workdir` is never zero-length.
* *Are the produced history records wrong?* No — the offset is self-consistent with however far the loop got, so `read_hrecs`/`fill_hrec` reconstruct the correct path. Impact is size and format consistency only, hence low severity.
