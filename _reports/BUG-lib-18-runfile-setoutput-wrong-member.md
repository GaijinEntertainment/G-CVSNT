---
id: BUG-lib-18
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/cvsapi/unix/RunFile.cpp
line: 100
severity: high
category: typo
verdict: CONFIRMED
fix_size_loc: 2
behavior_change: no
---

# unix `CRunFile::setOutput()` stores the user data in `m_inputData`, so the output callback is invoked with an uninitialised `void*` — used directly as a `this` pointer

## Summary
`setOutput()` assigns `m_inputData = userData` where it must assign `m_outputData` (the win32
build of the same method gets it right). `m_outputData` is never written anywhere else and is
not initialised by the constructor, so `m_outputFn(buf, wsize, m_outputData)` passes an
indeterminate pointer. `CServerConnection` casts exactly that pointer to `CServerConnection*`
and calls a member function through it.

## Code
```cpp
// cvsapi/unix/RunFile.cpp:96-102
bool CRunFile::setOutput(int (*outputFn)(const char *,size_t,void*), void *userData)
{
	m_outputFn = outputFn;
	m_inputData = userData;            // <-- line 100: must be m_outputData
	return true;
}
```
The neighbouring setters are correct — `setInput` -> `m_inputData` (line 93),
`setError` -> `m_errorData` (line 107), `setDebug` -> `m_debugData` (line 114) — and the win32
build has:
```cpp
// cvsapi/win32/RunFile.cpp:101-105
bool CRunFile::setOutput(int (*outputFn)(const char *,size_t, void *), void *userData)
{
	m_outputFn = outputFn;
	m_outputData = userData;
	return true;
}
```

The unix constructor (cvsapi/unix/RunFile.cpp:44-52) initialises only the four function
pointers and `m_child`; `m_inputData`, `m_outputData`, `m_errorData` and `m_debugData` are left
indeterminate.

## Why it is a bug
`m_outputData` has exactly one producer (`setOutput`) and three consumers:

```cpp
// cvsapi/unix/RunFile.cpp:270, 283, 337
       	(m_errorFn?m_errorFn:m_outputFn)(buf,wsize,m_errorFn?m_errorData:m_outputData);
		m_outputFn(buf,wsize,m_outputData);
       	m_outputFn(buf,wsize,m_outputData);
```

With the producer writing to the wrong member, `m_outputData` reads back whatever was on the
heap where the `CRunFile` was allocated (or on the stack for a local `CRunFile rf;`).
Simultaneously `m_inputData` is clobbered with the *output* callback's user data, so a caller
that sets both gets its input callback invoked with the output closure.

There is a second, related divergence in the same file: unix `run()` does
```cpp
// cvsapi/unix/RunFile.cpp:143-144
	if(!m_errorFn)
		m_errorFn = m_outputFn;
```
while win32 does the same *and* copies the closure (cvsapi/win32/RunFile.cpp:152-153:
`m_errorFn = m_outputFn; m_errorData = m_outputData;`). So on unix, when the caller sets only an
output handler, line 270 dispatches to that handler with the never-assigned `m_errorData`.

## Failure scenario
`CServerConnection::Connect()` (cvstools/ServerConnection.cpp:92-93) is the direct victim:

```cpp
		CRunFile rf;
		rf.setOutput(_ServerOutput,this);
		rf.setDebug(debugFn,userData);
		...
		rf.run(NULL);
		int res;
		rf.wait(res);
```

and the callback is

```cpp
// cvstools/ServerConnection.cpp:153-156
int CServerConnection::_ServerOutput(const char *data,size_t len,void *param)
{
	return ((CServerConnection*)param)->ServerOutput(data,len);
}
```

1. `setOutput(_ServerOutput, this)` puts `this` into `m_inputData`. `m_outputData` stays
   indeterminate — `rf` is a stack local, so it holds whatever bytes the previous frame left
   there.
2. `run()` forks the child cvs; the child writes its first line of output.
3. `wait()` reaches line 283, `m_outputFn(buf,wsize,m_outputData)` = `_ServerOutput(buf,wsize,
   <garbage>)`.
4. `((CServerConnection*)garbage)->ServerOutput(data,len)` runs a non-virtual member function on
   a wild pointer. `ServerOutput()` writes `m_error = 1/2/3/4` (cvstools/ServerConnection.cpp:174,
   180, 186, 192) and calls `m_callback->ProcessOutput(...)` (line 194) — i.e. an
   attacker-influenced *store* through a stack-garbage pointer and an indirect call through a
   vtable read out of it.

Since `ServerOutput` branches on the child's stdout text (`"authorization failed"`,
`"Rejected access"`, …), which of those stores happens is influenced by what the remote cvs
server sends back. In practice the process crashes; with a favourable stack layout it corrupts
whatever object `m_outputData` happens to point at.

`xdiff/ext_xdiff.cpp:53` and `triggers/*` pass `NULL`, so they only lose the (unused) closure —
`CServerConnection` is the one that actually dereferences it.

## Suggested fix
```cpp
bool CRunFile::setOutput(int (*outputFn)(const char *,size_t,void*), void *userData)
{
	m_outputFn = outputFn;
	m_outputData = userData;
	return true;
}
```
and, to match win32, in `run()`:
```cpp
	if(!m_errorFn)
	{
		m_errorFn = m_outputFn;
		m_errorData = m_outputData;
	}
```
Initialising the four `*Data` members to NULL in the constructor would have turned this into a
NULL-deref rather than a wild pointer, and is worth adding regardless.

## Refutation attempt
- Grepped every assignment to `m_outputData` in cvsapi/unix/RunFile.cpp: there is none, so the
  member genuinely is never set on this platform.
- Checked the constructor (line 44-52) for a `memset`/initialiser list that would zero it: it
  assigns only `m_args`, the four `*Fn` pointers and `m_child`.
- Checked `RunFile.h` for in-class member initialisers that would zero `m_outputData` in C++11:
  cvsapi/RunFile.h:48-57 declares the members plainly with no initialisers.
- Checked whether `CServerConnection` might be compiled only for win32 (which would make the
  dereference unreachable on the affected platform): cvstools/ServerConnection.cpp is in the
  common source list, not under `cvstools/win32/`.
- Confirmed the win32 build differs, which rules out "this is the intended convention".
