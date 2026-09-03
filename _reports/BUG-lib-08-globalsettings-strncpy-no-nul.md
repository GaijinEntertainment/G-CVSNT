---
id: BUG-lib-08
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/cvstools/unix/GlobalSettings.cpp
line: 151
severity: medium
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 8
behavior_change: no
---

# `strncpy(buffer, …, buffer_len)` leaves the caller's buffer unterminated when a config value is at least `buffer_len` bytes

## Summary
All four value-returning paths in the unix `CGlobalSettings` implementation copy with
`strncpy(dst, src, dst_len)` and never write a terminator. `strncpy` only NUL-terminates when
`strlen(src) < n`, so a config line whose value is `>= buffer_len` characters hands the caller a
non-terminated buffer. Every caller immediately treats it as a C string (`atoi`, `xstrdup`,
`cvs::string` assignment), reading past the end of a stack array.

## Code
```cpp
// cvstools/unix/GlobalSettings.cpp:150-153  (_GetUserValue)
	        if(p)
                strncpy(buffer,p+1,buffer_len);
        	else
          		*buffer='\0';

// :347-350 (GetGlobalValue) — identical
// :277-280 (EnumUserValues)
      		strncpy(value,token,value_len);
      		if(p && v && strlen(v))
        		strncpy(buffer,v,buffer_len);
// :467-470 (EnumGlobalValues) — identical
```

## Why it is a bug
`line` is `char line[1024]`, so `p+1` can be up to 1022 characters. The `buffer_len` values the
API is called with are much smaller:

* `GetGlobalValue(product,key,value,int& ival)` — `char tmp[32]` then `atoi(tmp)`
  (GlobalSettings.cpp:301-308)
* `GetGlobalValue(product,key,value,cvs::string& sval)` — `char tmp[512]` then `sval = tmp`
  (GlobalSettings.cpp:310-318)
* `read_global_config()` — `char buffer[MAX_PATH]` then `xstrdup(buffer)`
  (src/main.cpp:514, 583, 599, 602, 609)

None of these can recover from a missing terminator; they all call `strlen`-family code on the
result. Note that the `else` branch two lines down writes `*buffer='\0'` explicitly, so the
author was aware termination is the callee's job — the `strncpy` branch just forgot it.

## Failure scenario
Put a long value in `<confdir>/PServer`:

```
Chroot=/aaaaaaaa……a          (>= MAX_PATH characters after the '=')
```

1. `read_global_config()` (src/main.cpp:599) calls
   `GetGlobalValue("cvsnt","PServer","Chroot",buffer,sizeof(buffer))` with
   `buffer_len == MAX_PATH`.
2. `strncpy(buffer, p+1, MAX_PATH)` fills all `MAX_PATH` bytes with no terminator.
3. `chroot_base = xstrdup(buffer)` (src/main.cpp:600) calls `strlen(buffer)`, which runs off the
   end of the `MAX_PATH` stack array into whatever follows — in this frame that is `token[1024]`
   and `buffer2[MAX_PATH]` (src/main.cpp:575), i.e. uninitialised stack — and then `strcpy`s
   that far.

The value that ends up in `chroot_base` is the configured path *plus* a tail of adjacent stack
bytes, so the server chroots somewhere other than the administrator intended (or fails). The
same shape applies to `allowed_clients`, `runas_user` and `remote_init_root`, and to the
`int&` overload where `atoi(tmp)` on a 32-byte unterminated `tmp` reads past the array.

The value read from `~/.cvs/cvspass` via `_GetUserValue` is user-writable, so on the client side
the over-read is directly controllable.

## Suggested fix
Bound the copy at `buffer_len-1` and terminate explicitly, at all four sites:

```cpp
	        if(p)
                {
                    strncpy(buffer,p+1,buffer_len-1);
                    buffer[buffer_len-1]='\0';
                }
        	else
          		*buffer='\0';
```

## Refutation attempt
- Verified `strncpy`'s contract: it pads with NUL only when the source is shorter than `n`; at
  `strlen(src) >= n` it copies exactly `n` bytes and stops. No libc adds a terminator here.
- Verified the `line` buffer really can hold 1000+ character values (`char line[1024]`,
  `fgets(line,sizeof(line),f)`), so the precondition is reachable from a plain text config file
  rather than requiring some exotic input.
- Verified the callers do not re-terminate: `src/main.cpp:583/600/603/610` go straight to
  `xstrdup(buffer)`; `GlobalSettings.cpp:306` goes straight to `atoi(tmp)`; `:317` does
  `sval = tmp`.
- Checked whether `buffer_len` might conventionally exclude the terminator (which would make
  `strncpy(...,buffer_len)` correct): every call site passes `sizeof(array)`, so it is the full
  size.
