---
id: BUG-update-18
area: client/update core
file: cvsnt/cvsnt-2.5.05.3744/src/subr.cpp
line: 309
severity: high
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 2
behavior_change: no
---

# `line2argv()` skips separators with `strchr(sepchars, *p)`, which is true for the NUL terminator — it walks off the end of the buffer

## Summary
`line2argv()`'s separator-skipping loop is `while (strchr(sepchars, *p)) p++;`. `strchr(s, '\0')`
returns a pointer to `s`'s own terminator, i.e. **non-NULL**, so when `p` reaches the end of
the input the loop keeps advancing past it. Any input containing a run of two or more
separators that reaches the end of the string causes an out-of-bounds read (and an extra,
garbage `argv[]` element built from whatever follows the buffer).

## Code
```cpp
/* src/subr.cpp:298-359 */
void line2argv (int *pargc, char ***argv, const char *line, const char *sepchars)
{
  const char *p;
  char *q,*qstart;
  ...
  for(p=line;*p;p++)
  {
    while(strchr(sepchars,*p))          /* 309 <-- true when *p == '\0' */
		p++;

	qstart=q=(char*)xstrdup(p);         /* 312 <-- reads past the end of `line` */
    for(;*p;p++)
    { ... }
    *q='\0';
	(*argv)[(*pargc)++]=(char*)xrealloc(qstart,strlen(qstart)+1);
    ...
    if(!*p)
      break;
  }
  (*argv)[*pargc]=NULL;
}
```

## Why it is a bug
C says the terminating null character is part of the string for `strchr`, so
`strchr(" \t\r\n", '\0')` is non-NULL. The outer `for (p=line; *p; p++)` only guards the
*entry* to the body; once inside, nothing stops line 310's `p++` at the terminator.

Trace of the minimal failing input `line2argv(&argc, &argv, "a  ", " ")` (buffer `"a  \0"`,
4 bytes, valid indices 0-3):

1. `p = &line[0]` (`'a'`), body entered. `strchr(" ", 'a') == NULL`, so no skipping.
2. The inner loop copies `a`, hits `' '` at index 1, `q--`, `break`. `argv[0] = "a"`, `p == 1`.
3. `if(!*p) break;` — `*p` is `' '`, so no break. Outer `p++` -> `p == 2` (`' '`), body entered again.
4. Line 309: `strchr(" ", ' ')` non-NULL -> `p++` -> `p == 3` (`'\0'`).
   `strchr(" ", '\0')` **non-NULL** -> `p++` -> `p == 4`, **one past the allocation**.
5. The loop keeps testing `line[4]`, `line[5]`, … reading heap memory the caller does not
   own, stopping only at the first byte that is neither a separator nor NUL — then
   `xstrdup(p)` at line 312 `strlen`s and copies that heap garbage into a new `argv[]`
   element.

A *single* trailing separator is safe (the outer `for`'s `p++` lands exactly on the NUL and
the loop condition ends it); the bug needs a run of two or more separators reaching the end,
or an input consisting only of separators.

## Failure scenario
Three live triggers, one of them remote:

**(a) `cvs @argsfile` with a trailing blank line or CRLF endings** — `append_args()`
(main.cpp:364-390) reads the whole file into `buf = xmalloc(buflen+1)` and calls
`line2argv(&newargc,&newargv,buf," \t\r\n")` (main.cpp:383). A file whose last line is
followed by an empty line (`"update\n-d\n\n"`) or that uses CRLF on a POSIX client
(`"update -d\r\n"`) ends with two separators, so step 4 above runs `p` past
`buf[buflen]`. Result: heap over-read, plus a bogus extra argument prepended to `argv`,
which CVS then tries to interpret as a command or filename.

**(b) `cvs admin -a` / `-e` from a remote client** — admin.cpp:621
`line2argv (&argc, &users, arg + 2, " ,\t\n")`, where `arg` is `admin_data->av[i]`, i.e. an
`Argument` string sent by the client. A client sending `-auser1, ` (comma **and** trailing
space) makes the server read past the end of that argument buffer and hand the garbage to
`RCS_addaccess()`. This is attacker-controlled input reaching the over-read in the server
process.

**(c) `CVSROOT/modules` values with trailing double whitespace** — modules.cpp:438 builds
`line = xmalloc (strlen (value) + 5)` holding exactly `"XXX " + value`, then calls
`line2argv (&xmodargc, &xmodargv, line, " \t")`. A modules entry ending in two spaces or
`" \t"` (trivially produced by an editor) runs off that exact-sized allocation.
modules.cpp:995 has the same shape.

## Suggested fix
```cpp
    while(*p && strchr(sepchars,*p))
		p++;
    if(!*p)
      break;
```
(the added `if` also prevents the empty trailing token that would otherwise be appended;
alternatively just `while (*p && strchr(sepchars,*p)) p++;` plus letting the existing
`for(;*p;p++)`/`if(!*p) break;` logic produce an empty final element, which the callers
already tolerate less well — the explicit break is safer.)

## Refutation attempt
* Is `strchr(s, '\0')` really non-NULL? Yes — C99 7.21.5.2: "The terminating null character
  is considered to be part of the string." Every conforming implementation returns
  `s + strlen(s)`.
* Could `xmalloc` over-allocate enough that the read stays inside the block? Not
  reliably, and not at all for main.cpp:378 (`xmalloc(buflen+1)` with `buf[buflen]='\0'`) or
  modules.cpp:433 (`xmalloc (strlen (value) + 5)` holding exactly `strlen(value)+5` bytes) —
  both are exact-fit allocations. Even inside a rounded-up malloc bin the read is
  out-of-bounds and produces a wrong `argv`.
* Is the outer `for (p=line;*p;p++)` enough of a guard? It only guards entry to the body.
  The failing sequence enters the body at a separator that is *not* the last character and
  then walks forward inside the `while`.
* Do all callers pass a non-empty `sepchars`? Yes (admin.cpp:621 `" ,\t\n"`, admin.cpp:650
  `" \t\n"`, main.cpp:383 `" \t\r\n"`, modules.cpp:438 and 995 `" \t"`), so the bug is the
  NUL match, not an empty separator set.
* Could a single trailing separator already trigger it (making this common enough to have
  been noticed)? No — see the trace: one trailing separator exits cleanly via the outer
  `for` condition. That asymmetry is exactly why the bug survives ordinary use.
