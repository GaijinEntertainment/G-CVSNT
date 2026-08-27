---
# cvsservice DoUnisonThread races on process-global std handles across concurrent connections
- **File:** cvsnt/cvsnt-2.5.05.3744/cvsservice/Service.cpp
- **Line(s):** 821-838 (esp. 834-838)
- **Severity:** medium
- **Confidence:** high
- **Category:** concurrency

## Code
```cpp
DWORD CALLBACK DoUnisonThread(LPVOID lpParam)
{
    CSocketIOPtr conn = (CSocketIO*)lpParam;
    CRunFile rf;
    ...
    rf.setInput(CRunFile::StandardInput,NULL);
    rf.setOutput(CRunFile::StandardOutput,NULL);
    rf.setError(CRunFile::StandardError,NULL);

    SetStdHandle(STD_INPUT_HANDLE,(HANDLE)conn->getsocket());
    SetStdHandle(STD_OUTPUT_HANDLE,(HANDLE)conn->getsocket());
    SetStdHandle(STD_ERROR_HANDLE,(HANDLE)conn->getsocket());

    rf.run(NULL);
    ...
}
```
`CRunFile::run` reads the *process-global* std handles at spawn time
(cvsapi/win32/RunFile.cpp:167-169):
```cpp
si.hStdInput  = (m_inputFn==StandardInput)?GetStdHandle(STD_INPUT_HANDLE):...;
si.hStdOutput = (m_outputFn==StandardOutput)?GetStdHandle(STD_OUTPUT_HANDLE):...;
si.hStdError  = (m_errorFn==StandardError)?GetStdHandle(STD_ERROR_HANDLE):...;
```

## Why this is a bug
`DoUnisonThread` runs once per accepted Unison connection, each on its own thread
(`CreateThread(... DoUnisonThread ...)` in ServiceMain). It configures the child to
inherit the process std handles, then points those *global* handles at this
connection's socket and spawns `unison.exe`.

`SetStdHandle` mutates state shared by the entire process, and there is no lock around
the `SetStdHandle`+`run` sequence. If two Unison clients connect close together, the
interleaving

```
threadA: SetStdHandle(in/out/err = socketA)
threadB: SetStdHandle(in/out/err = socketB)
threadA: rf.run()   // child A inherits socketB (GetStdHandle now returns B)
threadB: rf.run()   // child B inherits socketB
```

routes one client's Unison process to another client's socket. That is both a
correctness failure and a security problem (one authenticated session's data stream is
handed to a different connection's child process). It can also clobber the service's own
std handles for other concurrent work.

## Suggested fix
Serialize the `SetStdHandle`+spawn with a critical section (the code already has
`g_crit`/`ClientLock`), or better, avoid the global handles entirely: pass the socket to
the child explicitly (as `DoCvsThread` does with `--win32_socket_io=%ld`), or use
`CRunFile`'s explicit-handle path so each spawn gets its own inherited handles without
touching process-global state.
---
