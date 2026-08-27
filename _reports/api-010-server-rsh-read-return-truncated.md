---
# server (rsh): tcp_read int return truncated into unsigned char, defeating error check
- **File:** cvsnt/cvsnt-2.5.05.3744/protocols/server.cpp
- **Line(s):** 123, 164-166
- **Severity:** low
- **Confidence:** medium
- **Category:** error-handling

## Code
```cpp
    unsigned char c;
    ...
    if(tcp_read(&c,1)<1)
        return CVSPROTO_FAIL;
    if(c)
    {
        char msg[257];
        if((c=tcp_read(msg,256))<1)     // tcp_read returns int; c is unsigned char
            return CVSPROTO_FAIL;
        msg[c]='\0';
        server_error(0,"rsh server reported: %s",msg);
        return CVSPROTO_FAIL;
    }
```

## Why this is a bug
`tcp_read` returns an `int` (byte count, or a negative value on error). Its result is
assigned to `unsigned char c` *before* the `<1` comparison. The truncation to 8 bits
breaks the error check:

- On a read error, `tcp_read` returns `-1`; `(unsigned char)(-1)` is `255`, so `255 < 1`
  is false. The code does *not* return failure — it proceeds to `msg[255]='\0'` and
  prints a buffer that was never filled (uninitialized stack contents).
- A full 256-byte read returns `256`; `(unsigned char)256` is `0`, so `0 < 1` is true and
  a complete message is misreported as a failed read.

No overflow occurs (`msg[257]`, `c<=255`), so impact is limited to mis-handling the
error/short-read case and leaking uninitialized stack bytes into an error message. This
is the rarely used client-side `:server:` (rsh) path.

## Suggested fix
Use a separate `int` for the return value and range-check before indexing:
```cpp
int n = tcp_read(msg, 256);
if (n < 1)
    return CVSPROTO_FAIL;
msg[n] = '\0';
```
---
