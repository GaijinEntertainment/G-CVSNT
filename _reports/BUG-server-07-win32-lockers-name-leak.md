---
id: BUG-server-07
area: locking
file: cvsnt/cvsnt-2.5.05.3744/src/lock.cpp
line: 1053
severity: low
category: leak
verdict: CONFIRMED
fix_size_loc: 2
behavior_change: no
---

# Windows variant of `set_lockers_name` never frees the previous `lockers_name`

## Summary
The POSIX and Win32 overloads of `set_lockers_name` differ: the POSIX one frees the previous value of the static `lockers_name` before overwriting it, the Win32 one does not. `set_lockers_name` is called once per iteration of `set_lock`'s unbounded retry loop, so every lock-contention retry leaks a string.

## Code
```cpp
// lock.cpp:1031-1051  (POSIX)
static void set_lockers_name (struct stat *statp)
{
    struct passwd *pw;

    if (lockers_name != NULL)
	xfree (lockers_name);                 // <-- frees the old value
    if ((pw = (struct passwd *) getpwuid ((uid_t)statp->st_uid)) != ...
```

```cpp
// lock.cpp:1053-1057  (Win32)
static void set_lockers_name (const char *file)
{
	lockers_name = xstrdup(win32getfileowner(file));   // <-- previous value leaked
}
```

## Why it is a bug
`lockers_name` is a file-scope `static char *` (lock.cpp). Every assignment without a preceding `xfree` orphans the previous heap block. The two overloads exist to abstract the same operation on the two platforms and clearly should have the same ownership semantics — `Writer_Lock` (lock.cpp:806) relies on that, doing its own `if (lockers_name != NULL) xfree (lockers_name); lockers_name = xstrdup ("unknown");` in exactly the guarded form the POSIX overload uses.

## Failure scenario
`set_lock` (lock.cpp:1064) retries in an unbounded `for (;;)` loop while a lock is held by someone else:

```cpp
#ifdef _WIN32
		set_lockers_name (masterlock);
#else
	set_lockers_name (&sb);
#endif
	if (!will_wait)
	    return (L_LOCKED);
	lock_wait (lock->repository);
	waited = 1;
```

On a Windows server, a `cvs commit` that waits behind a long-running lock spins through this loop once per `lock_wait` interval, leaking one short string each time. `readers_exist` (lock.cpp:997) leaks another per call. The amounts are small (tens of bytes), but they accumulate for as long as the server process waits, and this is exactly the path taken under heavy contention.

## Suggested fix
```cpp
static void set_lockers_name (const char *file)
{
	if (lockers_name != NULL)
		xfree (lockers_name);
	lockers_name = xstrdup(win32getfileowner(file));
}
```

## Refutation attempt
* *Is `lockers_name` maybe freed by the caller before each call?* No. Both call sites (`readers_exist` lock.cpp:997, `set_lock` lock.cpp:1141) call it directly with no preceding free. `Writer_Lock` frees it, but only once at the top of its outer retry loop, not around these calls.
* *Could `win32getfileowner` return a pointer the caller must not own?* It returns either a string literal (`"Unknown User"`) or a pointer to its own `static char szName[64]` (windows-NT/win32.cpp:887), so `xstrdup` is correct there — the missing free is of the *old* `lockers_name`, not the new one.
* *Is the Win32 overload perhaps rarely used?* It is the only definition compiled under `_WIN32`, and Windows is a primary platform for CVSNT.

## Related (out of scope, flagged for whoever owns `windows-NT/`)
`win32getfileowner` (windows-NT/win32.cpp:887) does `strncpy(szName, name, p-name); szName[p-name]='\0';` into a `static char szName[64]` with no bound on `p-name`. `name..p` is the user name embedded in the lock file name, which `Reader_Lock`/`write_lock` build from `CVS_Username` (lock.cpp:723, 878) — a client-supplied value. A user name longer than 63 characters overflows the static buffer from this same call path.
