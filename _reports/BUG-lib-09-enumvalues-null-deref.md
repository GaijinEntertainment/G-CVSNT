---
id: BUG-lib-09
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/cvstools/unix/GlobalSettings.cpp
line: 271
severity: medium
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 2
behavior_change: no
---

# `EnumUserValues()`/`EnumGlobalValues()` dereference a NULL `p` on any config line that has no `=`

## Summary
The guard `if(!p && !strlen(token)) continue;` only skips the line when the `=` is missing
**and** the line is empty. A non-empty line with no `=` falls through to
`for(;isspace(*p); p++)` with `p == NULL`, dereferencing a null pointer. The same loop is also
dead code in the `p != NULL` case, so it never does what it was written to do.

## Code
```cpp
// cvstools/unix/GlobalSettings.cpp:262-280  (EnumUserValues; EnumGlobalValues at 452-470 is identical)
    	if(!value_num--)
    	{
      		for(token=line; isspace(*token); token++)
        		;
      		v=p=strchr(token,'=');
      		if(!p && !strlen(token))
        		continue;
      		if(p)
      		{
        		*p='\0';
        		v++;
      		}
      		for(;isspace(*p); p++)          // <-- line 271: p may be NULL
        		*p='\0';
      		for(;v && isspace(*v); v++)
        		;
      		strncpy(value,token,value_len);
```
The very next loop guards its pointer (`for(;v && isspace(*v); v++)`), which shows the author
knew these pointers can be NULL here.

## Why it is a bug
`strchr(token,'=')` returns NULL when the line contains no `=`. The `&&` in the guard makes the
skip conditional on *both* conditions; it should be `||` (skip when there is no `=` **or**
the line is empty) — or the loop needs the same `p &&` guard the following loop has.

Secondary defect at the same two lines: when `p` *is* non-NULL, line 268 has already written
`*p='\0'`, so `isspace(*p)` is `isspace('\0')` — false — and the loop body never executes. The
trailing-whitespace trim it was meant to perform (walking *backwards* from `p`) simply does not
happen, so keys with trailing spaces never match.

## Failure scenario
`EnumGlobalValues()` is driven in a loop by `read_global_config()`
(src/main.cpp:613): `while(!CGlobalSettings::EnumGlobalValues("cvsnt","PServer",n++,token,...))`,
so it walks *every* line of `<confdir>/PServer` in turn.

Give that file a line with no `=` — a stray section header, a hand-edited leftover, a
half-written key:

```
Repository0=/var/lib/cvs
PServerOnly
EncryptionLevel=1
```

On the iteration where `value_num` reaches 0 on the `PServerOnly` line:

1. `line[0]` is not `#` and `strlen(line)` is 11, so the comment/empty skip at line 260 does not
   fire.
2. `strchr("PServerOnly",'=')` returns NULL, so `p == v == NULL`.
3. `!p` is true but `!strlen(token)` is false, so the `continue` at line 267 does **not** fire.
4. `if(p)` is false, so `p` stays NULL.
5. `isspace(*p)` dereferences NULL — the process segfaults during startup configuration
   parsing.

For the server this is a crash at connection setup driven purely by a malformed config line;
for `EnumUserValues` the file is `~/.cvs/<key>`, writable by the invoking user.

## Suggested fix
```cpp
      		v=p=strchr(token,'=');
      		if(!p || !strlen(token))
        		continue;
```
(and, if the trailing-space trim is actually wanted, replace the dead
`for(;isspace(*p); p++) *p='\0';` with a backwards walk from `p-1` down to `token`.)

## Refutation attempt
- Checked the preceding filter `if(line[0]=='#' || !strlen(line)) continue;` (line 260) — it
  removes comments and empty lines but nothing else, so a plain word survives to line 271.
- Checked whether `strchr` could return a non-NULL sentinel for "not found": it returns NULL, and
  the code itself tests `if(p)` on the next line, confirming the author expected NULL.
- Checked whether `value_num--` could prevent ever landing on such a line: `value_num` is
  supplied by the caller and incremented monotonically (`n++` in src/main.cpp:613), so every
  line is visited in turn.
- Checked the win32 implementation (cvstools/win32/GlobalSettings.cpp:352-386) — it enumerates
  registry values with `RegEnumValueA` and has no equivalent parsing, so this is unix-only.
