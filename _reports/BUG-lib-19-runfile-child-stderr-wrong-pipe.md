---
id: BUG-lib-19
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/cvsapi/unix/RunFile.cpp
line: 190
severity: medium
category: typo
verdict: CONFIRMED
fix_size_loc: 3
behavior_change: yes
---

# In the forked child, the stderr branch closes and dups `fd2` (the stdout pipe) instead of `fd3`; and the non-blocking `fcntl` for the input pipe is applied to `m_errFd`

## Summary
`CRunFile::run()` creates three pipes, `fd1` (stdin), `fd2` (stdout) and `fd3` (stderr). The
child's stderr block is a copy of the stdout block with the variable never changed: it does
`close(fd2[PIPE_READ]); dup2(fd2[PIPE_WRITE],2);`. `fd3` is therefore never wired to the child at
all, and when the stdout pipe was not created `fd2` is uninitialised stack, so the child
`close()`s and `dup2()`s an arbitrary descriptor number. A second wrong-variable slip three lines
earlier applies `O_NONBLOCK` to `m_errFd` under an `if(m_inFd>=0)` guard.

## Code
```cpp
// cvsapi/unix/RunFile.cpp:157-162
	if(m_outFd>=0)
		fcntl(m_outFd,F_SETFL, O_NONBLOCK);
	if(m_errFd>=0)
		fcntl(m_errFd,F_SETFL, O_NONBLOCK);
	if(m_inFd>=0)
		fcntl(m_errFd,F_SETFL, O_NONBLOCK);      // <-- line 162: m_errFd, should be m_inFd

// cvsapi/unix/RunFile.cpp:181-194  (inside the child)
		if(m_outFd>=0)
		{
		  close(fd2[PIPE_READ]);
		  dup2(fd2[PIPE_WRITE],1);
		}
		else if(!m_outputFn)
			dup2(nullfd,1);
		if(m_errFd>=0)
		{
		  close(fd2[PIPE_READ]);                 // <-- line 190: fd2, should be fd3
		  dup2(fd2[PIPE_WRITE],2);               // <-- line 191: fd2, should be fd3
		}
		else if(!m_errorFn)
			dup2(nullfd,2);
```
`int fd1[2],fd2[2],fd3[2];` are plain uninitialised locals (line 118); `fd2` is only filled by
`pipe(fd2)` at line 137, which runs only when `m_outputFn` is a real callback.

## Why it is a bug
The parent reads the child's stderr from `fd3[PIPE_READ]` (`m_errFd = fd3[PIPE_READ]`, line 149)
and closes `fd3[PIPE_WRITE]` after the fork (line 205). For that to ever produce data, the child
has to `dup2(fd3[PIPE_WRITE], 2)`. It never does. Consequently:

* the child's real stderr is aimed at the **stdout** pipe (or, when `fd2` is uninitialised, at a
  random descriptor);
* `fd3[PIPE_WRITE]` stays open in the child forever, so the parent's `read(m_errFd, …)` cannot see
  EOF until the child exits;
* `close(fd2[PIPE_READ])` is executed twice when both pipes exist — benign (`EBADF`), but a clear
  marker of the copy-paste.

The `fcntl` slip means the stdin pipe never becomes non-blocking, so `write(m_inFd, …)` in
`wait()` (cvsapi/unix/RunFile.cpp:245) can block indefinitely instead of returning `EAGAIN` — and
when `m_errFd` is `-1` the call is simply `fcntl(-1, …)`, a silent no-op.

## Failure scenario
**Uninitialised descriptor.** Reach `m_outFd < 0` with `m_errFd >= 0` by calling
`setError(realFn, …)` without a real `setOutput` (or with `setOutput(CRunFile::StandardOutput,…)`,
which sets `m_outFd = -1` at line 141 while `m_errorFn` stays a real callback and gets its own
pipe at line 148). In the child:

```cpp
		close(fd2[PIPE_READ]);      /* fd2[0] is uninitialised stack */
		dup2(fd2[PIPE_WRITE],2);    /* fd2[1] is uninitialised stack */
```

`close()` on a garbage small integer silently closes an unrelated descriptor that the child
inherited — in the cvs server that set includes the client socket, the repository lock socket and
open RCS files — and `dup2(garbage, 2)` then points the child's stderr at whatever that descriptor
is, or fails with `EBADF` leaving stderr pointing at the parent's stderr. This all happens between
`fork()` and `execvp()`, so it is invisible to the parent.

**Merged streams (the common in-tree case).** `xdiff/ext_xdiff.cpp:53` calls
`run.setOutput(xdiff_output_fn, NULL)` and never calls `setError`. Line 143-144 copies
`m_errorFn = m_outputFn`, so both pipes are created. The child then sends its stderr into the
stdout pipe, `fd3` carries nothing, and the parent sits in its select loop with `m_errFd >= 0`
until the child exits. Diagnostics from the external diff program arrive interleaved into the
diff output stream instead of on the error channel.

## Suggested fix
```cpp
	if(m_inFd>=0)
		fcntl(m_inFd,F_SETFL, O_NONBLOCK);
...
		if(m_errFd>=0)
		{
		  close(fd3[PIPE_READ]);
		  dup2(fd3[PIPE_WRITE],2);
		}
```

## Refutation attempt
- Checked that `fd3` is genuinely a separate pipe and not an alias of `fd2`: `pipe(fd3)` at line
  148 is a distinct call, and `m_errFd = fd3[PIPE_READ]` is what the parent later reads from
  (lines 149, 205, 265-271).
- Checked whether the child might be relying on stdout/stderr being deliberately merged: the class
  exposes separate `setOutput`/`setError` callbacks and `wait()` dispatches them separately
  (lines 270 and 283), so merging is clearly not the design.
- Checked whether `fd2` could be initialised on some other path when `m_outFd < 0`: the only
  writer is `pipe(fd2)` inside the `if(m_outputFn && m_outputFn != StandardOutput)` block at lines
  135-139, and the `else` at 140-141 only sets `m_outFd = -1`.
- Checked the win32 implementation for the same shape: it uses `CreatePipe`/`STARTUPINFO`
  (cvsapi/win32/RunFile.cpp:190-260) with separate `hOutputWrite`/`hErrorWrite` handles and does
  not share this defect — another unix-only divergence.
- Verified the `fcntl` line is not a deliberate double-application: line 160 already sets
  `O_NONBLOCK` on `m_errFd` under its own guard, so line 162 is redundant as written and can only
  have been meant for `m_inFd`.
