---
id: BUG-lib-15
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/cvsapi/Codepage.cpp
line: 461
severity: high
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 8
behavior_change: no
---

# `CCodepage::TranscodeBuffer()` `strcpy`s a length-counted binary buffer on its failure path and returns without ever setting the `olen` out-parameter

## Summary
When source and target encodings are equal (or `iconv_open` fails), `TranscodeBuffer()` falls back
to `strcpy(outbuf, inbuf)` — but `inbuf` is a *length-counted* buffer of `len` bytes that may
contain embedded NULs and need not be NUL-terminated at all. It also returns `-1` without
assigning `olen`, so the caller's output length never describes the data actually written.
The server's line-in/line-out codepage wrappers take exactly this path whenever the client's
codepage matches the server's.

## Code
```cpp
// cvsapi/Codepage.cpp:443-463
int CCodepage::TranscodeBuffer(const char *from, const char *to, const void *inbuf, size_t len, void *&outbuf, size_t& olen)
{
	const char *inbufp=(const char *)inbuf;
	size_t in_remaining = len?len:strlen(inbufp)+1;
	...
	outbuf=malloc(in_remaining*4);
	...
	if(!strcmp(from,to) || (ic = iconv_open(to,from))==(iconv_t)-1)
	{
		CServerIo::trace(3,"TranscodeBuffer(%s,%s) failed",to,from);
		strcpy((char *)outbuf,(const char *)inbuf);      // <-- line 461
		return -1;                                       // <-- olen never assigned
	}
```
Only the success path at line 483 sets `olen`.

## Why it is a bug
The `len` parameter exists precisely because callers pass binary data: `len ? len : strlen(...)+1`
on line 446 shows that a non-zero `len` means "this is not a C string". The fallback then ignores
`len` entirely:

* If `inbuf` contains a NUL before offset `len`, the copy stops early — silent truncation of the
  data the caller asked to transcode.
* If `inbuf` has no NUL within `len` bytes — the normal case for file contents — `strcpy` reads
  past the end of the caller's buffer, and keeps writing into `outbuf` (which is only
  `len*4` bytes) until it finds one. A run of more than `4*len` non-NUL bytes after `inbuf`
  overflows the heap block.

And `olen` is left holding whatever the caller initialised it to, while `outbuf` holds real data.

## Failure scenario
`server_buf_output()` (src/server.cpp:6412-6461) is the server's stdout path — it carries file
contents, `M`/`E` protocol lines, everything:

```cpp
	char *ostr=NULL;
	size_t olen=0;
	if(!server_codepage_sent && default_client_codepage)
	{
		...
		server_codepage = CCodepage::GetDefaultCharset();
		int ret = CCodepage::TranscodeBuffer(server_codepage,default_client_codepage,data,len,(void*&)ostr,olen);
		...
		str=ostr;
		len=olen;
	}
 	buf_output (stdout_buf?stdout_buf:buf_to_net, str, len);
 	if(str[len-1]=='\n')
```

A client that announces a codepage equal to the server's default charset (e.g. `UTF-8` on a
UTF-8 server) makes `!strcmp(from,to)` true on **every single output call**:

1. `outbuf = malloc(len*4)`, then `strcpy(outbuf, data)`. `data` is the raw bytes of whatever is
   being sent — a checked-out file, for instance. Binary content with no NUL in it makes the
   `strcpy` walk off the end of `data` into adjacent heap, and keep copying.
2. `olen` is never written, so it stays `0`.
3. Back in the caller: `len = olen` = 0, then `buf_output(buf, str, 0)` sends nothing, and
   `if(str[len-1]=='\n')` evaluates `str[-1]` — a read one byte *before* the `malloc` block.

So the same code path both over-reads/over-writes on the way in and under-reads on the way out,
and every byte of server output is silently dropped. `server_read_line()`
(src/server.cpp:7116-7152) has the identical shape on the input side (`*lenp = ilen` = 0).

Related defects at the same site, worth fixing together:

* Line 468: `if(iconv(...)<0)` — `iconv()` returns `size_t`, so this comparison is **always
  false** and the error branch is dead. iconv signals failure with `(size_t)-1`. Today that means
  an `EILSEQ`/`E2BIG` failure is reported to the caller as success (`return chars_deleted`,
  line 488) with truncated output. The same always-false test is at Codepage.cpp:296 in
  `ConvertEncoding()`.
* Once line 468 is corrected, the `return -1` on line 472 leaks `ic` — `iconv_close(ic)` is only
  reached on line 482.

## Suggested fix
```cpp
	if(!strcmp(from,to) || (ic = iconv_open(to,from))==(iconv_t)-1)
	{
		CServerIo::trace(3,"TranscodeBuffer(%s,%s) failed",to,from);
		memcpy(outbuf,inbuf,in_remaining);
		olen = len ? in_remaining : in_remaining-1;
		return -1;
	}
	do
	{
		if(iconv(ic,(iconv_arg2_t)&inbufp,&in_remaining,&outbufp,&out_remaining)==(size_t)-1
		   && errno!=EILSEQ && errno!=EINVAL)
		{
			CServerIo::trace(3,"Transcode between %s and %s failed",from,to);
			memcpy(outbuf,inbuf,len?len:strlen((const char*)inbuf)+1);
			olen = len ? len : strlen((const char*)inbuf);
			iconv_close(ic);
			return -1;
		}
```
(and apply the `(size_t)-1` correction at Codepage.cpp:296 as well)

## Refutation attempt
- Checked that `len` really can be non-zero with binary content: `server_buf_output(buffer *buf,
  const char *data, int len)` (src/server.cpp:6417) passes the raw output length, and
  `server_read_line` passes `*lenp` from `buf_read_line`.
- Checked whether callers guard against `ret < 0` before using `olen`: they do not —
  src/server.cpp:6455-6456 and :6657-6658 assign `len = olen` unconditionally after the `if/else
  if` chain, and src/server.cpp:7150 does `if(lenp) *lenp=ilen;` the same way.
- Checked whether `!strcmp(from,to)` is unreachable in practice: `default_client_codepage` is
  whatever the client announced, and `GetDefaultCharset()` returns `nl_langinfo(CODESET)` — a
  UTF-8 client on a UTF-8 server matches exactly. There is no earlier short-circuit for the
  equal-codepage case; the caller only checks `!server_codepage_sent && default_client_codepage`.
- Verified `iconv`'s return type is `size_t` (POSIX, glibc, GNU libiconv), so the `<0` tests are
  genuinely always false rather than platform-dependent.
