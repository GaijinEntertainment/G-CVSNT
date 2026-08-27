---
id: BUG-lib-05
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/cvsapi/cvs_string.h
line: 228
severity: high
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 10
behavior_change: no
---

# `cvs::wide::utf82ucs2()` walks past the terminating NUL on any truncated/invalid UTF-8 sequence, and still dereferences a NULL `src`

## Summary
The UTF-8 decoder advances `p` by the length implied by the lead byte without ever checking
that the continuation bytes are actually there. A string ending in a truncated multi-byte
sequence — or containing a stray continuation byte, since `0x80..0xBF` fall into the
`*p<0xe0` branch — makes `p` step *over* the terminating NUL, after which `while(*p)` keeps
reading unallocated heap. Separately, the explicit NULL guard on line 223 protects only
`strlen()`; the loop on line 225 still dereferences `p == NULL`.

## Code
```cpp
// cvsapi/cvs_string.h:218-236
		void utf82ucs2(const char *src)
		{
			unsigned char *p=(unsigned char *)src;
			wchar_t ch;
			// strlen(NULL) causes a crash (VS2008) - so handle it nice
			size_t strlen_src=(src)?strlen(src):0;
			w_str.reserve(strlen_src);
			while(*p)                                                        // <-- NULL deref if src==NULL
			{
				if(*p<0x80) { ch = p[0]; p++; }
				else if(*p<0xe0) { ch = ((p[0]&0x3f)<<6)+(p[1]&0x3f); p+=2; }  // <-- line 228
				else if(*p<0xf0) { ch = ((p[0]&0x1f)<<12)+((p[1]&0x3f)<<6)+(p[2]&0x3f); p+=3; }
				else if(*p<0xf8) { ch = ((p[0]&0x0f)<<18)+...+(p[3]&0x3f); p+=4; }
				else if(*p<0xfc) { ch = ((p[0]&0x07)<<24)+...+(p[4]&0x3f); p+=5; }
				else if(*p<0xfe) { ch = ((p[0]&0x03)<<30)+...+(p[5]&0x3f); p+=6; }
				else { ch = '?'; p++; }
				w_str+=(wchar_t)ch;
			}
		}
```

## Why it is a bug
`strlen_src` is computed but only used for `reserve()`; it never bounds the loop. The loop's
only stopping condition is landing exactly on the NUL byte. Every branch except the first and
last advances `p` by 2..6 unconditionally, so the NUL is *skipped* whenever the source ends
mid-sequence:

* `"\xC3"` (1 byte + NUL): lead byte `0xC3 < 0xe0`, so `p[1]` (the NUL, still in bounds) is
  consumed as a continuation byte and `p += 2` lands one byte *past* the NUL.
* Any lone continuation byte `0x80..0xBF` takes the same branch — the classifier never
  rejects them.
* `"\xF0"`: `p[1]`, `p[2]`, `p[3]` are read (2 of them already past the NUL) and `p += 4`.

From there the loop keeps decoding whatever follows the allocation, appending a `wchar_t` per
step to `w_str`, until it happens to hit a zero byte. That is an unbounded heap over-read that
also copies the leaked bytes into `w_str`.

The NULL case is a plain oversight: the comment on line 222 shows the author knew `src` can be
NULL, and guarded `strlen()`, but left `while(*p)` unguarded one line below. See also
cvsapi/win32/LibraryAccess.cpp:98-99, whose comment explicitly complains that VS2005 emits a
call to `cvs::wide(NULL)` — so the NULL input path is known to occur in practice.

## Failure scenario
On Windows every string that crosses into a Win32 API goes through `cvs::wide` — registry
paths (cvsapi/win32/FileAccess.cpp:577), module names (cvsapi/win32/LibraryAccess.cpp:102,
108), event-log messages (cvsapi/ServerIO.cpp:140), SQL statements and every field written
into the DB layer (cvsapi/db/mssql/MssqlRecordset.cpp:190, cvsapi/db/db2/Db2Recordset.cpp:193,
cvsapi/SqlVariant.cpp:279).

`CServerIo::error()`/`trace()` text is built from protocol-supplied names. A client that sends
a filename, tag or module name whose UTF-8 is truncated at the buffer boundary — e.g. a name
ending in the single byte `0xE9` (very common when a Latin-1 client's name is passed through
unconverted) — produces:

1. `str.c_str()` = `"...\xE9"`, allocation is `len+1` bytes.
2. `0xE9 >= 0x80` and `< 0xf0`, so the third branch reads `p[1]` (the NUL) and `p[2]`
   (**1 byte past the allocation**) and does `p += 3`, landing 2 bytes past the NUL.
3. The loop continues over adjacent heap, appending garbage `wchar_t`s until it finds a zero
   byte — with a large adjacent allocation this reads (and copies) kilobytes of unrelated heap
   contents, which are then written into the Windows event log / trace file, or sent to the
   SQL server as part of a statement.

With the allocation at the end of a page the over-read faults and the server process dies —
a remote DoS from a malformed filename.

## Suggested fix
Bound the walk by the length that was already computed and validate continuation bytes:

```cpp
		void utf82ucs2(const char *src)
		{
			if(!src) return;
			const unsigned char *p=(const unsigned char *)src;
			const unsigned char *end=p+strlen(src);
			w_str.reserve(end-p);
			while(p<end)
			{
				unsigned n;
				wchar_t ch;
				if(*p<0x80)      { ch=*p;        n=1; }
				else if(*p<0xc0) { ch='?';       n=1; }   /* stray continuation byte */
				else if(*p<0xe0) { ch=*p&0x1f;   n=2; }
				else if(*p<0xf0) { ch=*p&0x0f;   n=3; }
				else if(*p<0xf8) { ch=*p&0x07;   n=4; }
				else             { ch='?';       n=1; }
				if((size_t)(end-p) < n) { w_str+=(wchar_t)'?'; break; }   /* truncated */
				for(unsigned i=1;i<n;i++)
					ch = (wchar_t)((ch<<6) | (p[i]&0x3f));
				p += n;
				w_str += ch;
			}
		}
```

## Refutation attempt
- Checked whether callers pre-validate UTF-8 before constructing `cvs::wide`: they do not —
  every call site listed above passes a `cvs::string::c_str()` or a raw `const char *`
  straight to the constructor.
- Checked `cvs::narrow` (the inverse, line 239-267) for the same class of bug: it is bounded
  by `while(*p)` over `wchar_t` and advances exactly one unit per iteration, so it is safe.
  This asymmetry is what makes the `wide` version stand out as an oversight rather than a
  deliberate "trusted input" contract.
- Considered whether `*p<0xe0` on a byte in `0x80..0xBF` might be unreachable because callers
  only ever pass valid UTF-8: `CServerIo::error("...%s...", filename)` formats names taken
  verbatim off the wire, and the codepage layer (cvsapi/Codepage.cpp) is not applied on these
  paths.
- Verified `p` is `unsigned char*`, so the comparisons are not affected by `char` signedness;
  the bug is in the advance, not the classification.
