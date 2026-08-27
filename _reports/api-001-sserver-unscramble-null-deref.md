---
# sserver auth: NULL-pointer strcpy when client sends malformed scrambled password
- **File:** cvsnt/cvsnt-2.5.05.3744/protocols/sserver.cpp
- **Line(s):** 498
- **Severity:** high
- **Confidence:** high
- **Category:** security

## Code
```cpp
    server_getline(protocol, &tmp, PATH_MAX);
    if (strcmp (tmp,
        sserver_protocol_interface.verify_only ?
        "END SSL VERIFICATION REQUEST" : "END SSL AUTH REQUEST")
    != 0)
    {
        server_printf ("bad auth protocol end: %s\n", tmp);
        free(tmp);
        return CVSPROTO_FAIL;
    }

    strcpy(sserver_protocol_interface.auth_password, scramble.Unscramble(sserver_protocol_interface.auth_password));
```

`CScramble::Unscramble()` (cvstools/Scramble.cpp:63-76) returns `NULL` whenever the
first byte of the cipher text is not `'A'`:

```cpp
const char *CScramble::Unscramble(const char *cypher)
{
    const unsigned char *s = (unsigned char *)(cypher+1);
    unsigned char *d;
    if(cypher[0]!='A')
        return NULL;
    ...
}
```

## Why this is a bug
`auth_password` is read straight off the wire from the (as-yet unauthenticated)
client via `server_getline()`. On the server side this is attacker-controlled input.
If the client sends a password line whose first character is not `'A'` — including the
trivial case of an *empty* password line, where `server_getline` yields `""` and
`cypher[0]=='\0'` — `Unscramble()` returns `NULL`, and the code calls
`strcpy(dst, NULL)`, which dereferences a NULL pointer and crashes the server
connection handler.

This is reachable pre-authentication: the SSL layer is set up with
`SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL)` without
`SSL_VERIFY_FAIL_IF_NO_PEER_CERT`, so `SSL_accept()` succeeds even when the client
presents no certificate. An attacker only needs to complete the SSL handshake and send
`BEGIN SSL AUTH REQUEST` / repository / username / a non-'A' password / `END SSL AUTH
REQUEST`. The result is a remotely triggerable crash (denial of service).

The sibling pserver implementation handles exactly this case correctly
(pserver.cpp:229-235):

```cpp
const char *unscrambled_password = scramble.Unscramble(pserver_protocol_interface.auth_password);
if(!unscrambled_password || !*unscrambled_password)
{
    CServerIo::trace(1,"PROTOCOL VIOLATION: Invalid scrambled password sent by client...");
    unscrambled_password="";
}
strcpy(pserver_protocol_interface.auth_password, unscrambled_password);
```

The sserver path is missing that guard.

## Suggested fix
Mirror the pserver guard:
```cpp
const char *unscrambled = scramble.Unscramble(sserver_protocol_interface.auth_password);
if(!unscrambled)
{
    CServerIo::trace(1,"PROTOCOL VIOLATION: Invalid scrambled password sent by client.");
    unscrambled = "";
}
strcpy(sserver_protocol_interface.auth_password, unscrambled);
```
(The destination buffer is at least as large as the source, so the copy itself is safe
once the NULL is handled.)
---
