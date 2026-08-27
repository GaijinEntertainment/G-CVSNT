---
id: BUG-server-05
area: locking
file: cvsnt/cvsnt-2.5.05.3744/src/lock.cpp
line: 246
severity: medium
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 4
behavior_change: no
---

# Off-by-one stack write when a lock-server reply exactly fills the receive buffer

## Summary
Both places that read from the lock-server socket call `recv(sock, line, <full buffer size>, 0)` and then immediately write `line[l] = '\0'` where `l` is the byte count returned. When `recv` fills the buffer, `l == sizeof(line)` and the NUL is written one byte past the end of a stack array.

## Code
```cpp
// lock.cpp:228-249  (lock_server_command)
static int lock_server_command(char *line, int line_len, const char *cmd, ...)
{
    ...
	if((l=recv(lock_server_socket,line,line_len,0))<=0)
	{
		error(1,errno,"Error communicating with lock server (recv)");
	}
	do
	{
		line[l--]='\0';          // <-- l can be == line_len  =>  one-byte OOB write
	} while(l && isspace(line[l]));
```

```cpp
// lock.cpp:194-202  (lock_register_client)
		if((l=recv(lock_server_socket,line,sizeof(line),0))<=0)
		{
			error(1,errno,"Error communicating with lock server");
		}
		do
		{
			line[l--]='\0';      // <-- same, line is char[1024]
		} while(l && isspace(line[l]));
```

## Why it is a bug
`recv()` returns the number of bytes placed in the buffer, in the range `1 .. len`. The code treats the return value as an *index at which to append a terminator*, which is only safe when `l < len`. There is no `-1` on the length passed to `recv` and no clamp on `l`.

Both call sites pass the full array size:
* `lock_register_client` — `char line[1024]`, `recv(..., sizeof(line), 0)`.
* `lock_server_command` — `line_len` comes straight from `sizeof(...)` at every call site: `do_lock_server` (`char line[MAX_PATH*4]`, lock.cpp:256), `do_unlock_file` (`char line[1024]`), `do_lock_version` (`char line[1024]`), `do_modified` (`char line[1024]`).

Writing `'\0'` at `line[sizeof(line)]` clobbers whatever the compiler placed next on the stack — an adjacent local (`p`, `q`, `ob`, `bWaited`, `id`, `helper`), a saved register slot, or a stack-protector canary. `do_lock_server` declares them all on one line as `char line[MAX_PATH*4],*p,*q, *ob = NULL;` (lock.cpp:256), so the byte immediately after the array is a plausible home for `p`/`q`/`ob` under common layouts; the observable effect is then a corrupted pointer dereferenced a few lines later by `strchr(line,'(')` / `*q='\0'`.

## Failure scenario
The socket is a byte stream with no framing — the code assumes one `recv` equals one reply, but TCP does not guarantee that. Two ways to reach `l == line_len`:

1. **Long path in a busy-lock reply.** `lockservice/LockParse.cpp:618` emits
   `002 WAIT Lock busy|<user>|<client_host>|<path>\n`
   and `LockParse.cpp:632` emits `002 busy|<user>|<client_host>|<path>\n`. On a POSIX build `MAX_PATH` is `PATH_MAX` (cvs.h:1128) so `do_lock_server`'s buffer is 16 KB and this is hard to hit; on the Windows build `MAX_PATH` is 260 (cvs.h:1124), making the buffer 1040 bytes. A deep repository path plus a long user name and host name in a contended-lock reply exceeds that and `recv` returns exactly 1040.
2. **Coalesced replies.** If client and server desynchronise (for example a `Lock` retry loop where an earlier `002 WAIT` reply arrives late), several `NNN ...\n` lines sit in the socket buffer at once and a single `recv` returns the full `line_len` bytes.

Either way the stack write past `line` happens, and in case 1 it is followed by `p=strchr(line,'(')` / `sscanf(p,"%u",&helper)` on a buffer that is not NUL-terminated inside its own bounds, so `strchr` also scans past the array.

## Suggested fix
```cpp
	if((l=recv(lock_server_socket,line,line_len-1,0))<=0)
	{
		error(1,errno,"Error communicating with lock server (recv)");
	}
	do
	{
		line[l--]='\0';
	} while(l && isspace(line[l]));
```
and identically at lock.cpp:194 (`recv(lock_server_socket,line,sizeof(line)-1,0)`).

## Refutation attempt
* *Does `recv` reserve room for a terminator?* No — unlike `fgets`, `recv` is not string-oriented and will fill all `len` bytes.
* *Is `l` clamped anywhere before the write?* No; the only test is `<= 0`, which rejects errors and orderly shutdown but not a full buffer.
* *Could the reply always be short in practice?* The signon banner and the `000`/`001` replies are short, but the `002 busy|user|host|path` replies embed a repository path and two identity strings supplied by other clients, and there is no framing to prevent coalescing. The buffer at lock.cpp:256 is only 1040 bytes on the Windows build.
* *Is `line` NUL-terminated by the server anyway?* Irrelevant — the write happens unconditionally at index `l` before anything is inspected.
