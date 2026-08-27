---
id: BUG-server-15
area: commit
file: cvsnt/cvsnt-2.5.05.3744/src/checkin.cpp
line: 129
severity: low
category: typo
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: no
---

# `error()` format string has `%s` with no corresponding argument in `Checkin`'s checksum path

## Summary
`error(1,errno,"Unable to reopen %s for checksum")` passes a `%s` conversion with no matching vararg. `error` forwards its arguments to `vfprintf` (error.cpp:221), so this reads whatever happens to be in the next vararg slot and dereferences it as a `char *` — turning a diagnostic into a crash or a leak of arbitrary memory into the server log.

## Code
```cpp
// checkin.cpp:123-133
			if(server_active)
			{
				char buf[BUFSIZ*10];
				FILE *cf = CVS_FOPEN(finfo->file,"r");
				if(!cf)
				{
					error(1,errno,"Unable to reopen %s for checksum");   // <-- missing finfo->file
				}
```

Every other message in this file supplies its arguments, e.g. checkin.cpp:100-101:
```cpp
		    error (1, 0, "failed when checking out new copy of %s",
			   fn_root(finfo->fullname));
```

## Why it is a bug
`error` is a variadic printf wrapper:
```cpp
// error.cpp:77
void error(int status, int errnum, const char* message, ...)
// error.cpp:221
    vfprintf (fp, message, args);
```
`vfprintf` has no way to know an argument is missing; `%s` makes it call `va_arg(args, char*)`, yielding an indeterminate value from the register save area or the stack. Dereferencing it is undefined: on x86-64 it is usually whatever the caller last left in `rcx`/`r8`, so the typical outcome is SIGSEGV inside `vfprintf`; when it happens to be a readable address, the log line contains an arbitrary run of process memory instead of the file name.

Because `status` is 1, `error` exits after printing — so the intended behaviour was a clean fatal diagnostic, and the actual behaviour is a crash with no diagnostic at all, in the middle of a commit that has *already* written the new revision.

## Failure scenario
`Checkin` runs server-side after `RCS_checkin` has succeeded (checkin.cpp:64, `case 0:`). It then reopens the working file to compute an MD5 for the `Register` call. Reaching the failure branch requires `CVS_FOPEN(finfo->file,"r")` to fail — e.g. the working file was removed or made unreadable between the checkout at checkin.cpp:97 and this point, the process hit its open-file limit on a large multi-file commit, or a `chmod` in a trigger/`commitinfo` script stripped read permission.

At that moment the RCS file has already been rewritten with the new revision, but the client never receives the `Checked-in`/`Updated` response — instead the server dies with a segfault (or logs a garbage string). The user sees a broken connection after a commit that actually landed, which is the worst possible time to lose the diagnostic.

## Suggested fix
```cpp
					error(1,errno,"Unable to reopen %s for checksum", fn_root(finfo->file));
```

## Refutation attempt
* *Is `error` maybe not printf-style?* It is: `void error(int status, int errnum, const char* message, ...)` (error.cpp:77) ending in `vfprintf (fp, message, args)` (error.cpp:221).
* *Would the compiler have caught it?* Only with a `__attribute__((format(printf,3,4)))` on the declaration, which this tree does not have — I checked `src/cvs.h`; `error` is declared without a format attribute, so neither GCC nor MSVC diagnoses the mismatch.
* *Is this the only such mismatch in scope?* I audited every `error()`, `sprintf()`, `fprintf()`, `snprintf()` and `TRACE()` call in the sixteen files under review with a conversion-count-vs-argument-count scanner. The only real hit was this one; the other flagged sites were false positives caused by backslash-continued string literals (commit.cpp:977, rcs.cpp:1396/4484/5000/6205, add.cpp:543/559/649, remove.cpp:214 — all verified by hand to supply their arguments), plus two `TRACE()` calls with one *extra* argument (server.cpp:3503, commit.cpp:778), which are harmless.
