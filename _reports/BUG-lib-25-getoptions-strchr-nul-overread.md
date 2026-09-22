---
id: BUG-lib-25
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/cvsapi/GetOptions.cpp
line: 55
severity: low
category: memory-safety
status: fixed in this slice (audit/02)
verdict: CONFIRMED
fix_size_loc: 3
behavior_change: yes
---

# A bare `-` token makes `strchr(format_string, '\0')` succeed, so the parser reads past the end of the option-format string and records a bogus option

## Summary
`CGetOptions` searches the option-format string for the character after the leading `-`. For the
token `"-"` that character is the terminating `'\0'`, and `strchr(s, '\0')` returns a pointer to
`s`'s own terminator rather than NULL. The `if(!q)` guard therefore does not fire, and the code
goes on to read `q[1]` and `q[2]` — one and two bytes past the end of the format string — and
pushes an `Option` with `option == 0` and, on one path, an unset `arg`.

## Code
```cpp
// cvsapi/GetOptions.cpp:32-62
		Option opt;
		const char *p = tokens[argnum];

		if(*(p++)!='-')
			return;
		if(*p=='-')
		{ ... }
		else
		{
			if(!format_string)
			{ m_error = true; return; }
			const char *q=strchr(format_string, p[0]);      // <-- line 55: p[0] can be '\0'
			if(!q)
			{ m_error = true; return; }
			opt.option=q[0];
			if(q[1]==':' && q[2]==':')                      // <-- reads past the NUL
```
`Option` is a POD with no constructor (`struct Option { int option; const char *arg; };`,
cvsapi/GetOptions.h:26-30), so `Option opt;` leaves `arg` indeterminate; the no-argument path
(`else argnum++;`, line 84) pushes it without ever assigning `arg`.

## Why it is a bug
C's `strchr` is specified to treat the terminating null character as part of the string, so
`strchr("ad:", '\0')` returns `&"ad:"[3]` — a valid, non-NULL pointer. Every use of `strchr` as a
"is this character in the set" test has to exclude the NUL first; this one does not. Once `q`
points at the terminator, `q[1]` and `q[2]` are outside the object.

## Failure scenario
`CGetOptions` parses `CVSROOT/modules` (src/Modules1.cpp:69, format `"ad:"`),
`CVSROOT/cvswrappers` (src/wrapper.cpp:385, format `"+k:x:m:t:"`) and the xdiff options
(xdiff/xml_xdiff.cpp:68, format `"i:d"`). A `modules` line containing a lone `-`, e.g.

```
mymodule -a - othermodule
```

tokenises to `["mymodule", "-a", "-", "othermodule"]`. On the third token:

1. `p` starts at `"-"`, `*(p++)` is `'-'`, so `p` now points at the token's `'\0'`.
2. `*p=='-'` is false, so control reaches line 55 with `p[0] == '\0'`.
3. `strchr("ad:", '\0')` returns `format_string+3` — the literal's terminator, in `.rodata`.
4. `q[0]` is `'\0'`, so `opt.option = 0`.
5. `q[1]` and `q[2]` read the two bytes following the string literal. Whatever is there decides
   which branch runs:
   * both `':'` -> the "optional argument" branch, `opt.arg = NULL`, `argnum++`;
   * `q[1] == ':'` alone -> the "mandatory argument" branch, which reads `p[1]` (one byte past the
     token's own NUL, inside the `cvs::string`'s buffer) and may swallow `"othermodule"` as this
     phantom option's argument, shifting `argnum` and silently changing how the rest of the line
     is interpreted;
   * neither -> `argnum++` with `opt.arg` left **uninitialised**, and that indeterminate pointer is
     copied into `m_options`.
6. `Modules1.cpp:73-84` then switches on `opt[n].option == 0` and falls into
   `default: error(0,0,"Unrecognised option '%c' in modules file", 0)` — a `%c` of NUL.

The over-read is of a string literal, so on a normal build it neither faults nor is caught by
ASan (literals are not redzoned); the observable damage is the phantom option and the
layout-dependent argument consumption. It becomes a hard fault only if the literal is placed at
the end of a mapping.

## Suggested fix
```cpp
			const char *q = p[0] ? strchr(format_string, p[0]) : NULL;
			if(!q || q[0]==':')
			{
				m_error = true;
				return;
			}
```
and give `Option` a default (`struct Option { int option = 0; const char *arg = NULL; };`) so the
no-argument path cannot publish an indeterminate `arg`.

## Refutation attempt
- Confirmed `strchr`'s contract: C11 7.24.5.2 — "the terminating null character is considered to
  be part of the string", so a search for `'\0'` always succeeds. This is not
  implementation-defined.
- Checked whether a lone `-` token can actually be produced: `CTokenLine::addArgs` splits on
  whitespace and the configured separators and pushes any non-empty run, so `"-"` surrounded by
  spaces becomes its own token (cvsapi/TokenLine.cpp:128-180).
- Checked whether an earlier guard rejects it: line 35 only requires the first character to be
  `'-'`, and line 37 only handles the `--` case; a one-character `"-"` falls straight through to
  line 55.
- Checked whether `q[0]==':'` could also slip through for a format like `"a::b"` where the token is
  `-:`; it can, which is why the suggested fix rejects that too.
- Checked the `Option::arg` half against real callers: src/Modules1.cpp reads `arg` only for `'d'`
  (which is a mandatory-argument option, so `arg` is always assigned) and xdiff/xml_xdiff.cpp only
  for `'i'` — so today nobody dereferences the indeterminate value. It is a latent defect, listed
  here because the same line is being fixed.
