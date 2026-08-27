---
id: BUG-lib-16
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/cvsapi/unix/DnsApi.cpp
line: 225
severity: medium
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 12
behavior_change: no
---

# `CDnsApi::Close()` frees a `new[]` array with scalar `delete`; the same file leaks debug `printf`s onto stdout and passes the wrong base pointer to `dn_expand()`

## Summary
`m_pdnsBase` is allocated with `new u_char[16384]` and released with `delete` instead of
`delete[]` — undefined behaviour, and a hard abort under hardened/checked allocators. The same
file also contains a dozen leftover `printf()` debug statements that write to stdout, and every
`dn_expand()` call passes `m_pdnsCurrent` where the DNS message *base* is required, so
compression pointers resolve against the wrong origin.

## Code
```cpp
// cvsapi/unix/DnsApi.cpp:129
	m_pdnsBase = new u_char[16384];

// cvsapi/unix/DnsApi.cpp:222-228
bool CDnsApi::Close()
{
	if(m_pdnsBase)
		delete m_pdnsBase;          // <-- line 225, must be delete[]
	m_pdnsBase=m_pdnsCurrent=NULL;
	return true;
}
```

## Why it is a bug
`new T[n]` must be paired with `delete[]`. Mismatching them is undefined behaviour regardless of
whether `T` has a trivial destructor: the standard permits `operator new[]` and `operator new`
to be entirely separate allocators, and an implementation is free to place an element-count
cookie before the returned pointer. It happens to be survivable with the stock glibc allocator,
which is why it has gone unnoticed, but it aborts under MSVC's debug CRT, under
`-fsanitize=address` (`alloc-dealloc-mismatch`), and under allocators that segregate array
allocations.

`Close()` is called from the destructor (line 120) and at the top of every `Lookup()` (line 125),
so the mismatch fires on every DNS lookup — `CServerInfo` (cvstools/ServerInfo.cpp:121) performs
one per server enumeration.

## Failure scenario
Build the tree with `-fsanitize=address` (or on a platform with a checking allocator) and run any
code path that enumerates servers via SRV records — `cvstools/ServerInfo.cpp:121-146`:

```cpp
	CDnsApi dns;
	...
	CDnsApi::SrvRR *rr = dns.GetRRSrv();
```

`dns` goes out of scope, `~CDnsApi()` calls `Close()`, and ASan reports
`alloc-dealloc-mismatch (operator new [] vs operator delete)` and aborts the process. On an
allocator that stores an array cookie the `delete` frees the wrong address outright.

Two further defects in the same file, both confirmed, that a fix should sweep up:

**Stray debug output.** `printf("ancount=%d\n",m_nCount)` (line 138) plus
`printf("name=%s\n",...)`, `printf("type=%d\n",...)`, `printf("class=%d\n",...)`,
`printf("ttl=%d\n",...)`, `printf("rdlength=%d\n",...)` (lines 185-190), `printf("count=0\n")`
(line 205), `printf("getheader failed\n")` (lines 145, 213), `printf("next failed\n")` (line 150),
`printf("dn_expand failed\n")` (line 170), `printf("GetRRPtr\n")` / `printf("GetRRTxt\n")` /
`printf("GetRRSrv\n")` (lines 237, 256, 275). These are unconditional writes to stdout in a
library, not `CServerIo::trace()` calls like the rest of cvsapi. Any process whose stdout is a
protocol stream gets the DNS record dump injected into it.

**Wrong `dn_expand()` origin.** Every call is of the form
```cpp
	int n=dn_expand(m_pdnsCurrent, m_pdnsEnd, p, m_dnsName, sizeof(m_dnsName));   // line 169
	...
	if(dn_expand(m_pdnsCurrent, m_pdnsEnd, m_prdata, m_dnsTmp, sizeof(m_dnsTmp))<1)  // lines 249, 268
	int n = dn_expand(m_pdnsCurrent, m_pdnsEnd, p, m_dnsTmp, sizeof(m_dnsTmp));      // line 285
```
`dn_expand(msg, eomorig, comp_dn, exp_dn, length)` requires `msg` to be the start of the DNS
*message*, because compression pointers are byte offsets from there. The correct argument is
`m_pdnsBase`, not the walking cursor `m_pdnsCurrent`. As written, any answer that uses name
compression — which every real DNS server does for the SRV target and PTR data — expands to a
name taken from the wrong offset, so `GetRRSrv()->server` and `GetRRPtr()` return garbage
hostnames. (glibc's `ns_name_unpack` bounds-checks `msg <= cp < eom`, so this is a correctness
bug rather than an over-read.)

## Suggested fix
```cpp
bool CDnsApi::Close()
{
	if(m_pdnsBase)
		delete[] m_pdnsBase;
	m_pdnsBase=m_pdnsCurrent=NULL;
	return true;
}
```
plus replace each `printf(...)` with `CServerIo::trace(3,...)`, and pass `m_pdnsBase` as the
first argument to all four `dn_expand()` calls.

## Refutation attempt
- Checked `cvsapi/DnsApi.h:61` to confirm the unix member is `unsigned char *m_pdnsBase` (a plain
  pointer, so the array form of `delete` is not implied by the type) and that line 129 is the only
  allocation.
- Checked the win32 branch of the same class: it uses the Windows `DnsQuery`/`DnsRecordListFree`
  API and has neither `new[]` nor `printf`, so this is unix-only — i.e. a platform divergence, not
  a house style.
- Checked whether `delete` on `new u_char[]` is merely a style issue: for a trivially destructible
  type it commonly works on glibc, but it remains UB and is diagnosed by ASan and MSVC's debug
  heap, so it is a real defect rather than a lint nit.
- Checked whether the `printf`s might be inside a debug-only guard: they are not — the enclosing
  `#ifndef HAVE_MDNS / #else` only selects between "unsupported" and the real implementation.
- Checked `dn_expand`'s contract in the local HP-UX declaration block (lines 88-94 of the same
  file) and against the glibc man page: the first parameter is documented as the message start.
