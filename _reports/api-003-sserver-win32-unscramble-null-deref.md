---
# sserver (win32) auth: NULL-pointer strcpy when client sends malformed scrambled password
- **File:** cvsnt/cvsnt-2.5.05.3744/protocols/sserver_win32.cpp
- **Line(s):** 456
- **Severity:** high
- **Confidence:** high
- **Category:** security

## Code
```cpp
    server_getline(protocol, &tmp, MAX_PATH);
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

## Why this is a bug
This is the Windows/SChannel port of the same defect reported in
api-001 (protocols/sserver.cpp:498). `CScramble::Unscramble()`
(cvstools/Scramble.cpp:63-76) returns `NULL` whenever the cipher text does not begin
with `'A'`:

```cpp
if(cypher[0]!='A')
    return NULL;
```

`auth_password` is read directly from the network from the not-yet-authenticated
client via `server_getline()`. If the client sends a password line whose first byte is
not `'A'` (including an empty line, which yields `""`), `Unscramble()` returns `NULL`
and the server executes `strcpy(dst, NULL)` — a NULL-pointer dereference that crashes
the connection handler. It is reachable after only the SChannel handshake (a client
certificate is explicitly optional, see the `SEC_E_NO_CREDENTIALS` handling around
line 401), so an unauthenticated peer can trigger it: remote denial of service.

The pserver implementation guards this correctly (pserver.cpp:229-235); both sserver
variants do not.

## Suggested fix
Add the same NULL guard used by pserver before copying:
```cpp
const char *unscrambled = scramble.Unscramble(sserver_protocol_interface.auth_password);
if(!unscrambled)
    unscrambled = "";
strcpy(sserver_protocol_interface.auth_password, unscrambled);
```
---
