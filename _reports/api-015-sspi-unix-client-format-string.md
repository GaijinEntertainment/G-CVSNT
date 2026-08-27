---
# sspi (unix) client passes server-controlled string as printf format to server_error
- **File:** cvsnt/cvsnt-2.5.05.3744/protocols/sspi_unix.cpp
- **Line(s):** 161-165 (server_error at 164)
- **Severity:** high
- **Confidence:** high
- **Category:** security

## Code
```cpp
    tcp_readline(protocols, sizeof(protocols));
    if((p=strstr(protocols,"[server aborted"))!=NULL)
    {
        server_error(1, p);          // p is a server-supplied string used as format
    }
```

## Why this is a bug
Identical defect to api-014 (sspi.cpp), in the unix winbind SSPI client. `protocols` is
read from the socket by `tcp_readline` during the cleartext SSPI negotiation, so `p` is
attacker-controlled (malicious or MITM server). It is handed to `server_error` as the
format string, which passes it to `vsnprintf(temp, sizeof(temp), p, va)` with no
matching variadic arguments. Format specifiers in the server's `"[server aborted ...]"`
text are interpreted: `%s`/`%x` cause out-of-bounds `va_list`/stack reads (crash or
info leak), and `%n` (where honored) yields an arbitrary write.

Note also that, unlike sspi.cpp, this call is not followed by a `return`, so control
continues after `server_error` (relying on fatal=1 aborting) — but the format-string
issue is the primary bug.

## Suggested fix
```cpp
server_error(1, "%s", p);
```
---
