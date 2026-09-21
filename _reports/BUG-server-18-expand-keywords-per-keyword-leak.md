---
id: BUG-server-18
area: rcs
file: cvsnt/cvsnt-2.5.05.3744/src/rcs.cpp
line: 3937
severity: medium
category: leak
verdict: CONFIRMED
fix_size_loc: 4
behavior_change: no
---

# `expand_keywords` leaks two heap strings on every `$KEYWORD$` candidate it examines

## Summary
Inside `expand_keywords`'s per-`$` scan loop, `RCS_branchfromversion()` and `printable_date()` each allocate a fresh string that is stored into the trigger-argument struct and then dropped when the iteration ends. Neither is ever freed, on any of the loop's exits.

## Code
```cpp
// rcs.cpp:3909-3956  (inside "while ((srch_next = memchr (srch, '$', srch_len)) != NULL)")
		const char *branch = RCS_branchfromversion(rcs,ver->version);   // <-- allocated

		/* See if this is one of the keywords.  */
		keywords_param_t args;
		args.file = last_component(rcs->path);
		args.directory = dir.c_str();
		args.keyword = srch;
		args.keyword_len = s - srch;
		args.author = ver->author;
		args.printable_date = printable_date(ver->date);                 // <-- allocated
		args.rcs_date = ver->date;
		...
		args.branch = branch;
```

Nothing frees them. The only `xfree` calls between here and the bottom of the loop are `xfree (date)` (rcs.cpp:4117, a *different*, correctly-managed `printable_date` result used by the `$Log$` branch), `xfree (leader)` and `xfree (sub)`; and after the loop only `xfree(prop)` / `xfree(locker)` (rcs.cpp:4221-4222).

Both callees return owned storage:
```cpp
// rcs.cpp:3714-3726
static char *printable_date (const char *rcs_date)
{
    ...
    return xstrdup (buf);
}
```
```cpp
// rcs.cpp:2433-2443
char *RCS_branchfromversion (RCSNode *rcs, const char *rev)
{
    ...
	version = (char*)xmalloc(strlen(rev)+32);
```
(and its symbol-table path allocates a further `branch=(char*)xmalloc(cq-cp+1)` which it returns instead).

## Why it is a bug
The allocations are per *iteration*, not per call: the loop body runs once for every `$` in the file that is followed by an alphanumeric run terminated by `$` or `:`. There are also two `continue` statements *after* the allocations (rcs.cpp:3972 `if(!args.value) continue;` and rcs.cpp:3985 `if (s == send || *s != '$') continue;`), so even candidates that turn out not to be keywords leak both strings.

The correct pattern is right there in the same function, in the `$Log$` branch: `date = printable_date (ver->date); ... xfree (date);`.

## Failure scenario
`expand_keywords` is called once per file from `RCS_checkout` (rcs_checkin.cpp:191) whenever the expansion mode is not `-ko`/`KFLAG_PRESERVE`.

A source file with the usual header block containing `$Id$`, `$Author$`, `$Date$` and `$Revision$` leaks 4 × (≈`strlen(rev)+32` + 20 bytes + two allocator headers) ≈ 400 bytes per checkout of that file. `cvs checkout` of a module with 100 000 such files leaks ~40 MB in a single server request, held until the process exits.

It is worse for any file that merely *contains* `$word$` or `$word:` sequences without being a real keyword — JSON with `"$ref":`, shell/Perl with `$var:`, TeX, template languages — because the `continue` at rcs.cpp:3972 (taken when the trigger yields no value) is after both allocations. A large generated file with tens of thousands of such tokens leaks several megabytes on its own.

## Suggested fix
Free both before every exit from the iteration. The simplest correct form is to hoist `printable_date` and `RCS_branchfromversion` out of the loop (both depend only on `ver`, which does not change), and free them once next to `xfree(locker)`:

```cpp
    /* before the while loop */
    char *branch = ver ? RCS_branchfromversion (rcs, ver->version) : NULL;
    char *printdate = ver ? printable_date (ver->date) : NULL;
    ...
		args.printable_date = printdate;
		args.branch = branch;
    ...
    /* after the loop, next to the existing cleanup */
	xfree(prop);
	xfree(locker);
	xfree(branch);
	xfree(printdate);
```

## Refutation attempt
* *Does `keywords_param_t`/`run_trigger` take ownership?* No. `run_trigger(&args, keywords_proc)` passes the fields straight through to `cb->parse_keyword(...)` (rcs.cpp:7586); the struct is a stack object holding borrowed `const char *`s (`args.file`, `args.author`, `args.rcs_date`, `args.state`, `args.version` are all pointers into the RCSNode), so a trigger cannot know which of them to free — and none of the shipped trigger implementations do.
* *Is `branch` freed under a different name?* `grep -n "branch" rcs.cpp` over rcs.cpp:3808-4240 shows exactly three references: the declaration, `args.branch = branch;`, and nothing else. Same for `args.printable_date`.
* *Could `RCS_branchfromversion` return a non-owned pointer?* It returns either `NULL`, its own `xmalloc`'d `version` buffer, or a separately `xmalloc`'d `branch` string — always owned, and its other callers (`RCS_rewrite` at rcs.cpp:7190-7194) do `xfree(branch)`.
* *Does the loop usually run only once or twice?* No — it iterates over *every* `$` byte in the file contents, and the expensive part is reached for every `$` followed by `[A-Za-z][A-Za-z0-9_]*` and then `$` or `:`.
