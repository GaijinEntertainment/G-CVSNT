---
id: BUG-lib-20
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/cvsapi/unix/RunFile.cpp
line: 229
severity: medium
category: correctness
verdict: CONFIRMED
fix_size_loc: 8
behavior_change: yes
---

# `CRunFile::wait()` returns `-1` from a `bool` function (i.e. `true`) and leaves the `result` out-parameter unwritten, so `run_exec()` returns an uninitialised exit status

## Summary
`bool CRunFile::wait(int& result, int timeout)` has three early returns that never assign
`result`, and the first of them is written `return -1` — which converts to `true`, reporting
success. `src/run.cpp:run_exec()` declares `int status;` uninitialised, ignores `run()`'s failure
return, calls `wait(status)`, and returns `status` to callers that branch on it. A third path
computes `WEXITSTATUS(status)` from a `status` that `waitpid()` never filled in.

## Code
```cpp
// cvsapi/unix/RunFile.cpp:222-229
bool CRunFile::wait(int& result, int timeout)
{	
	char buf[BUFSIZ],inbuf[BUFSIZ];
	int status,size,wsize,w,l;

	CServerIo::trace(3,"wait() called, m_child=%d",m_child);
	
	if(!m_child)
	  return -1;                       // <-- line 229: bool function; -1 == true; result unset

// cvsapi/unix/RunFile.cpp:302-303, 323-324
	if(!w && timeout!=-1 && timeout<=0) /* timed out */
	   return false;                   // result unset
			...
				if(!w)
				   return false;       // result unset

// cvsapi/unix/RunFile.cpp:311-352
	if(!w)
	{
		if(timeout==-1)
			waitpid(m_child,&status,0);
		...
	}
	else /* App died.. soak up stdio */
	{ ... }

	m_exitCode=result=WEXITSTATUS(status);   // <-- status is indeterminate when w == -1
```

And the caller:
```cpp
// src/run.cpp:56-84
int run_exec (bool bShow)
{
	int status;                                  // uninitialised
	...
	if(!run->run(NULL,bShow))
	{
		CServerIo::trace(3,"run->exec failed"); // failure noted, then ignored
	}
	run->wait(status);
	...
	return status;                               // may never have been written
}
```

## Why it is a bug
`-1` is not `false`. `return -1` in a function whose return type is `bool` yields `true` under
the standard boolean conversion, so the one place that reports "there is no child to wait for"
reports it as success. The two `return false` statements at lines 303 and 324 show the intended
convention, which makes line 229 an outright typo.

Independently, `result` is an out-parameter that only the final line 352 ever writes. Every early
return leaves the caller's variable exactly as it was — and `run_exec()` never initialises it.

The `w == -1` case is a third route to an indeterminate value: `waitpid()` returns `-1` on error
(`ECHILD` when the child has already been reaped, which happens whenever `SIGCHLD` is `SIG_IGN`
or a `SIGCHLD` handler is installed elsewhere in the process). `!w` is then false, so control
takes the `else` branch at line 327 and reaches `WEXITSTATUS(status)` with `status` never having
been assigned by any `waitpid` call.

## Failure scenario
`run()` returns `false` without setting `m_child` when `fork()` fails (cvsapi/unix/RunFile.cpp:
165-167) — under `RLIMIT_NPROC` pressure, in a cgroup at its pids limit, or simply out of memory.
`m_child` is `0` from the constructor (line 51).

1. A trigger or editor invocation calls `run_setup()` / `run_arg()` / `run_exec()`.
2. `run->run(NULL,bShow)` fails; `run_exec` only traces it and carries on.
3. `run->wait(status)` hits line 228, returns `-1` (= `true`, "succeeded") and leaves `status`
   holding whatever was on the stack.
4. `run_exec` returns that garbage.

Callers treat the return as the child's exit code:

```cpp
// src/logmsg.cpp:331-332
	if ((retcode = run_exec (true)) != 0)
		error (0, retcode == -1 ? errno : 0, "warning: editor session failed");
```

If the stack garbage happens to be `0`, a *failed fork* is reported as a successful editor
session and CVS proceeds to read the (unwritten) log-message temp file as if the user had edited
it. If it is non-zero, the user gets an error attributed to the wrong cause. The same pattern
governs whether trigger scripts are considered to have succeeded.

## Suggested fix
```cpp
bool CRunFile::wait(int& result, int timeout)
{	
	char buf[BUFSIZ],inbuf[BUFSIZ];
	int status=0,size,wsize,w,l;

	result = -1;                       /* nothing ran */

	if(!m_child)
	  return false;
```
and in src/run.cpp, initialise `int status = -1;` and skip `wait()` when `run()` returned false.

Note also line 304: `if(m_inFd) { close(m_inFd); m_inFd=-1; }` uses a truthiness test where every
other site in the file uses `>= 0`. It is harmless for the `-1` sentinel (`close(-1)` just fails)
but leaks the descriptor in the case `pipe()` handed back fd `0`.

## Refutation attempt
- Confirmed the declared return type really is `bool`: cvsapi/RunFile.h:40
  `CVSAPI_EXPORT bool wait(int& result, int timeout = -1);` — so `return -1` is a conversion to
  `true`, not an `int` return that a caller could test for `-1`.
- Checked whether `result` might be pre-initialised by any caller: `src/run.cpp:59` declares
  `int status;` with no initialiser, and `cvstools/ServerConnection.cpp:111` declares `int res;`
  the same way.
- Checked whether `run_exec` guards on `run()`'s return before calling `wait()` — it does not
  (src/run.cpp:77-80); the failure is only traced.
- Checked the timeout paths at lines 302 and 323: they are unreachable from `src/run.cpp` (which
  uses the default `timeout == -1`), so the fork-failure path at line 228 is the live one; the
  timeout paths still matter for `cvstools`/`triggers` callers that pass an explicit timeout.
- Checked the win32 `wait()` (cvsapi/win32/RunFile.cpp): it returns a proper `false` on
  `WAIT_TIMEOUT` and, when there is no process handle, still falls through to `result=(int)exit`
  with `exit` initialised to 0 — so it never returns `true` with `result` untouched, and never
  returns `-1` from a `bool`. Its `WAIT_TIMEOUT` path does share the "result left unset on early
  return" half of this finding, so the `result = -1;` prologue is worth adding there too.
