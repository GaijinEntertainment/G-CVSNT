---
id: BUG-lib-11
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/cvsapi/unix/SocketIO.cpp
line: 311
severity: critical
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: no
---

# unix `CSocketIO::recv()` advances `m_bufpos` by the whole request length instead of the part taken from the buffer, letting `m_buflen - m_bufpos` underflow into a `memcpy` length

## Summary
In the "partial data was already buffered, refill and finish" path, unix does
`m_bufpos += len` where the win32 build of the same function does
`m_bufpos += (len - oldlen)`. Only `len - oldlen` bytes were taken from the freshly filled
buffer, so `m_bufpos` ends up `oldlen` too large — and can end up **greater than `m_buflen`**.
Both are `size_t`, so the very next call evaluates `m_buflen - m_bufpos` as a near-`SIZE_MAX`
value and passes it straight to `memcpy`.

## Code
```cpp
// cvsapi/unix/SocketIO.cpp:299-316
	int rd=_recv(m_buffer,(int)m_bufmaxlen,0);
	size_t oldlen=m_buflen;
	m_bufpos=0;
	if(rd<0)
	{
		m_buflen=0;
		return rd;
	}
	m_buflen = rd;
	if((len-oldlen)<=m_buflen)
	{
		memcpy(buf+oldlen,m_buffer,len-oldlen);
		m_bufpos+=len;                        // <-- line 311
		return len;
	}
```

win32 build of the identical function, cvsapi/win32/SocketIO.cpp:
```cpp
	if((len-oldlen)<=m_buflen)
	{
		memcpy(buf+oldlen,m_buffer,len-oldlen);
		m_bufpos+=(len-oldlen);               // <-- correct
		return len;
	}
```

The fields are unsigned: `size_t m_bufpos,m_bufmaxlen,m_buflen;` (cvsapi/SocketIO.h:75).

## Why it is a bug
`m_bufpos` is the read cursor **into `m_buffer`**. Entering this branch, `m_bufpos` has just been
set to 0 and `m_buffer` holds `rd` fresh bytes; the `memcpy` on the line above consumes exactly
`len-oldlen` of them (the first `oldlen` bytes of the caller's buffer came from the *previous*
contents, already copied at line 288). Charging the cursor the full `len` double-counts the
`oldlen` bytes that never came out of the current buffer.

The two consequences:

1. **Silent stream desync** — `oldlen` bytes of the peer's data are skipped on the next read.
2. **`size_t` underflow into `memcpy`** — the branch is entered whenever `len-oldlen <= rd`,
   which permits `rd < len <= rd + oldlen`. In that range `m_bufpos = len > m_buflen = rd`.
   The next call then hits line 287:
   `if(m_buflen-m_bufpos) memcpy(buf,m_buffer+m_bufpos,m_buflen-m_bufpos);`
   with `m_buflen-m_bufpos == (size_t)(rd-len)`, i.e. roughly `2^64 - (len-rd)`.

## Failure scenario
`CHttpSocket` (cvsapi/unix/HttpSocket.cpp, built on unix — cvsapi/Makefile.am:90) reads the
status line and headers one byte at a time via `CSocketIO::getline()` -> `recv(&c,1)`, then reads
the body in one shot at line 340:
`CSocketIO::recv((char*)m_content.data(),(int)len)` with `len` = the server's `Content-Length`.
`CHttpSocket::request()` then loops (`do { _request(...) } while(bAgain);`,
HttpSocket.cpp:128-269) and re-issues `_request()` on the **same socket** for a 302 redirect or a
407 proxy-auth challenge.

A hostile (or MITM'd — this is plain HTTP) server:

1. Sends the response headers plus the first `K = 10` bytes of a 100-byte body in one TCP
   segment. `getline()` drains the headers, leaving `m_bufpos = H`, `m_buflen = H+10`.
2. `recv(content, 100)` is called. `H+100 <= H+10` is false, so the slow path runs:
   the 10 leftover bytes are copied out, `m_buflen` becomes 10, `oldlen = 10`.
   `100-10 = 90 < BUFSIZ`, so `rd = _recv(m_buffer, BUFSIZ)`.
3. The server now sends exactly the remaining **90** bytes and stops. `rd = 90`.
4. `(len-oldlen) = 90 <= m_buflen = 90` -> the buggy branch: `memcpy(buf+10, m_buffer, 90)`,
   then `m_bufpos += 100` -> **`m_bufpos = 100`, `m_buflen = 90`**.
5. The response code is 302, so `request()` loops and `_request()` calls
   `CSocketIO::getline(line)` -> `recv(&c,1)`:
   * `m_bufpos(100) + 1 <= m_buflen(90)`? no;
   * `m_buflen - m_bufpos` = `90 - 100` = `0xFFFFFFFFFFFFFFF6`, non-zero;
   * `memcpy(buf, m_buffer+100, 0xFFFFFFFFFFFFFFF6)` — a `memcpy` of ~18 exabytes into a
     one-byte stack variable.

The process dies immediately, and before it faults it linearly overwrites the whole stack above
`buf` with heap contents — a remote crash, and a remotely-controlled overwrite in the window
before the fault. The step-3 segment boundary is entirely under the peer's control, so this is
not a rare race.

The same shape applies to `sock.recv(buffer,buffer_len)` in
cvstools/unix/GlobalSettings.cpp:53 for any peer on 127.0.0.1:32401.

## Suggested fix
```cpp
		m_bufpos+=(len-oldlen);
```

## Refutation attempt
- Checked whether `m_bufpos`/`m_buflen` might be signed (which would make the underflow a
  harmless negative rather than a huge unsigned): cvsapi/SocketIO.h:75 declares all three as
  `size_t`.
- Checked whether the one-byte `getline` path alone could trigger it: with `len == 1` a non-zero
  leftover always satisfies the fast path at line 281, so `oldlen` is 0 and `+= len` happens to be
  correct. The bug needs a caller with `len > 1`, which is exactly what HttpSocket.cpp:340 does.
- Checked whether `close()` resets the cursors between requests: it sets `m_buffer = NULL`
  (cvsapi/unix/SocketIO.cpp:202), and the `if(!m_buffer)` block at line 274 re-zeroes
  `m_buflen`/`m_bufpos`, so a *reconnect* clears the poison — but the redirect/auth retry path in
  `CHttpSocket::request()` for 302 does **not** close the socket (the `CSocketIO::close()` calls
  are commented out at HttpSocket.cpp:142-146), so the poisoned cursor survives into the next
  `_request()`.
- Compared against win32/SocketIO.cpp line by line: the two functions are byte-identical apart
  from this one expression, which is what makes this a transcription slip rather than a
  deliberate platform difference.
