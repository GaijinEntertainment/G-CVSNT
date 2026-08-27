---
id: BUG-lib-04
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/cvstools/RootSplitter.cpp
line: 45
severity: medium
category: logic
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# `&&` instead of `||` makes the quoted-keyword loop in `CRootSplitter::Split()` exit on the first quote, so every quoted CVSROOT is rejected

## Summary
The loop that is supposed to skip over `:` characters *inside* quotes has its continuation
condition written as `!InQuote && *p!=':'`. As soon as `InQuote` is set the condition becomes
false and the loop terminates immediately, which then trips the `if(*p!=':' || InQuote) return
false;` check. The entire quote-handling block below it is unreachable dead code, and any
CVSROOT containing a quoted keyword value fails to parse.

## Code
```cpp
// cvstools/RootSplitter.cpp:41-58
	if(*p==';')
	{
		char InQuote = 0;
		q=++p;
		for(; *p && (!InQuote && *p!=':'); p++)     // <-- line 45
		{
			if(InQuote && *p==InQuote)
			{
				InQuote=0;
				continue;
			}
			if(*p=='"' || *p=='\'')
			{
				InQuote=*p;
				continue;
			}
		}
		if(*p!=':' || InQuote)
			return false;
		m_keywords.assign(q,p-q);
	}
```

## Why it is a bug
`InQuote` exists for exactly one purpose: to keep the loop running past a `:` that appears
between quotes. The condition therefore has to be "keep going while we are inside quotes **or**
the character is not a colon" — `InQuote || *p!=':'`. Written with `&&`, the flag has the
opposite effect: setting it *stops* the scan.

Trace of the body: `if(*p=='"' || *p=='\'')` sets `InQuote` and `continue`s. In a `for` loop
`continue` runs the increment (`p++`) and then re-tests the condition, which is now
`*p && (!InQuote && ...)` → `!InQuote` is 0 → loop exits with `InQuote` still non-zero.
Control reaches `if(*p!=':' || InQuote) return false;`, which is unconditionally true.
The `if(InQuote && *p==InQuote)` clearing branch can therefore never execute — it is
provably dead.

## Failure scenario
`CServerConnection::Connect()` (cvstools/ServerConnection.cpp:41-47) calls
`split.Split(info->root.c_str())` on the user's CVSROOT and — without checking the return
value — copies `split.m_protocol`, `m_username`, `m_password`, `m_server`, `m_directory` and
`m_keywords` into `info`.

Given a perfectly legal CVSROOT with a quoted keyword, e.g.

```
:pserver;proxy="host:8080";username=fred:cvs.example.com:/repo
```

`Split()` reaches line 45 with `p` at `proxy=...`, hits the `"` on the 7th character, sets
`InQuote='"'`, and the loop exits. `if(*p!=':' || InQuote)` returns `false` at line 57.

Because the caller ignores the return value, `info->protocol`, `info->server`,
`info->directory` and friends are all left as the empty strings the freshly-constructed
`CRootSplitter` holds. The `cvs::sprintf(info->root,80,":%s%s:%s%s%s:%s",...)` a few lines
later then synthesises the nonsense root `":::"` and the connection attempt fails with a
misleading error instead of the real "malformed root" diagnostic.

Even with the caller fixed, quoted keyword values — the documented way to embed a `:` in a
keyword — can never be used.

## Suggested fix
```cpp
		for(; *p && (InQuote || *p!=':'); p++)
```

## Refutation attempt
- Re-read the loop body assuming C `continue` semantics inside `for` (increment then condition)
  to make sure the exit really happens on the *first* quote; it does.
- Checked whether some caller pre-strips quotes before calling `Split()`: the only in-tree
  callers are cvstools/ServerConnection.cpp:41 and cvstools/win32/CvsCommonDialogs.cpp:593,
  644, 1193, all of which hand the raw root string straight in.
- Confirmed `m_keywords` is genuinely meant to be able to contain quoted values — `Join()`
  (line 118-127) re-emits `m_keywords` verbatim between the protocol and the `:`, so a
  round-trip of a quoted keyword is the intended use.
- Noted separately (not this finding) that all four call sites ignore `Split()`'s `bool`
  return, which is what turns the parse failure into silent garbage rather than an error.
