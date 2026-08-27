---
id: BUG-lib-17
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/cvsapi/TokenLine.cpp
line: 152
severity: medium
category: typo
verdict: CONFIRMED
fix_size_loc: 4
behavior_change: yes
---

# `arg.append('\n',1)` picks the `(count, char)` overload: `\n` expands to ten `0x01` bytes instead of one newline

## Summary
The four escape cases in `CTokenLine::addArgs()` call `std::string::append` with a `char` first
argument and `1` as the second. There is no `append(char, int)` overload, so overload resolution
selects `append(size_type n, charT c)` — the arguments swap roles. `'\n'` becomes the *count*
(10) and `1` becomes the *character* (`0x01`). Every `\n`, `\r`, `\b` and `\t` escape in a
tokenized line therefore produces a run of SOH bytes.

## Code
```cpp
// cvsapi/TokenLine.cpp:146-158
			if(*p=='\' && *(p+1))
			{
				p++;
				switch(*p)
				{
				case 'n':
					arg.append('\n',1); break;
				case 'r':
					arg.append('\r',1); break;
				case 'b':
					arg.append('\b',1); break;
				case 't':
					arg.append('\t',1); break;
```
`arg` is `cvs::string arg;` (TokenLine.cpp:133), i.e. `std::basic_string<char>`.

The *correct* idiom is used a few lines further down in the same function
(`arg.append(p,1);`, line 168) and throughout `toString()`
(`m_line.append("\"",1);`, `m_line.append("\\",1);`, lines 80-88) — a `const char*` first
argument.

## Why it is a bug
For the call `append('\n', 1)` with argument types `(char, int)` the candidate set is:

* `append(const charT* s, size_type n)` — not viable, `char` does not implicitly convert to
  `const char*`;
* `template<class InputIterator> append(InputIterator, InputIterator)` — not viable, deduction
  conflicts (`char` vs `int`);
* `append(size_type n, charT c)` — **viable**, via integral conversions `char -> size_type` and
  `int -> char`.

Only the last survives, so the compiler emits `arg.append((size_type)10, (char)1)`. There is no
ambiguity and no warning — it is a silently valid call with completely different semantics:

| written | actually executed | result |
|---|---|---|
| `arg.append('\n',1)` | `append(10, '\x01')` | 10 × `0x01` |
| `arg.append('\r',1)` | `append(13, '\x01')` | 13 × `0x01` |
| `arg.append('\b',1)` | `append(8,  '\x01')` | 8 × `0x01` |
| `arg.append('\t',1)` | `append(9,  '\x01')` | 9 × `0x01` |

## Failure scenario
`CTokenLine::addArgs()` is the tokenizer for every quoted/escaped configuration line in the
server: `CVSROOT/modules` (src/Modules1.cpp:60), `CVSROOT/modules2` (src/Modules2.cpp:77),
`CVSROOT/cvswrappers` (src/wrapper.cpp:364), the xdiff options (src/xdiff.cpp:683-686), a
protocol line in src/server.cpp:3997, and — through `CRunFile::setArgs`
(cvsapi/unix/RunFile.cpp:64-67) — the command line handed to `run_setup()`/`run_exec()`
in src/run.cpp:31-37 for the editor and for trigger programs.

Put a tab escape in `CVSROOT/modules2`, which is the documented way to embed a separator:

```
mymodule  -d "col1\tcol2"  dir
```

The token that reaches the module handler is `col1` followed by **nine `0x01` bytes** and then
`col2`, instead of `col1<TAB>col2`. Nothing errors; the module simply never matches, or matches
a directory name containing control characters. The same applies to any `\n` in an editor or
trigger command string passed through `run_setup()`, where the argv entry handed to `execvp`
gains ten stray control bytes.

## Suggested fix
```cpp
				case 'n':
					arg.append(1,'\n'); break;
				case 'r':
					arg.append(1,'\r'); break;
				case 'b':
					arg.append(1,'\b'); break;
				case 't':
					arg.append(1,'\t'); break;
```
(or `arg += '\n';`, matching the `arg+=*p;` style used in the `default:` branch two lines below)

## Refutation attempt
- Walked the `basic_string::append` overload set explicitly (C++98 through C++20 all have the
  same relevant members) to confirm `(char, int)` cannot match `(const charT*, size_type)` and
  that the `InputIterator` template fails deduction rather than being selected. `append(size_type,
  charT)` is the only viable candidate, so this is not "maybe the compiler does the right thing".
- Checked that `arg` is a real `std::basic_string` and not some cvsapi wrapper with a custom
  `append(char,int)`: `cvs::string` is `STD_STR_CLASS<char>` = `std::basic_string<char>`
  (cvsapi/cvs_string.h:120, 42), and `cvs::filename`/`cvs::username` (which do have custom traits)
  are not used here.
- Checked that the surrounding code is not compensating downstream — the tokens go straight into
  `m_args` and out through `operator[]`/`toArgv()`.
- Confirmed the escape branch is reachable: it fires on `'\'` followed by any character
  (line 146), with no quoting precondition.
