---
# sspi client passes server-controlled string as printf format to server_error (format-string bug)
- **File:** cvsnt/cvsnt-2.5.05.3744/protocols/sspi.cpp
- **Line(s):** 241-245 (server_error at 244)
- **Severity:** high
- **Confidence:** high
- **Category:** security

## Code
```cpp
    tcp_readline(protocols, sizeof(protocols));
    if((p=strstr(protocols,"[server aborted"))!=NULL)
    {
        server_error(1, p);          // p points into a server-supplied line
        return CVSPROTO_FAIL;
    }
```
`server_error` (protocols/common.cpp:178) forwards its argument straight into a
`vsnprintf(temp, sizeof(temp), fmt, va)`:
```cpp
int server_error(int fatal, const char *fmt, ...) {
    char temp[1024]; va_list va; va_start(va,fmt);
    vsnprintf(temp,sizeof(temp),fmt,va);   // fmt == p, no matching varargs
    ...
}
```

## Why this is a bug
`protocols` is read from the network by `tcp_readline` during the pre-encryption SSPI
negotiation, so `p` is fully controlled by the peer (a malicious or MITM CVS server).
It is then used as the *format string* of `server_error`, which passes it to
`vsnprintf` with no variadic arguments. Any conversion specifiers embedded in the
server's `"[server aborted ...]"` message are interpreted:

- `%s`/`%x`/`%p` read arbitrary words from the `va_list`/stack — crash or information
  disclosure;
- `%n` (where the CRT honors it) writes to memory — potential control-flow corruption.

This is a remotely triggerable format-string vulnerability against the CVS client,
reachable before any authentication or channel encryption is established.

## Suggested fix
Always use a constant format string:
```cpp
server_error(1, "%s", p);
```
The same defect exists in sspi_unix.cpp:164 (see api-015).

## Note
The nearby call at sspi.cpp:261 (`server_error(1, "...got '%s'...", protocols)`) is
correct — there `protocols` is an argument, not the format. Only the `server_error(1, p)`
form is vulnerable.
---
