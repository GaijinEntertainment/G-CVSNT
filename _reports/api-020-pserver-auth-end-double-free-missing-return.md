---
# pserver_auth_protocol_connect: double-free of tmp and accepts bad protocol end if server_error returns
- **File:** cvsnt/cvsnt-2.5.05.3744/protocols/pserver.cpp
- **Line(s):** 219-238
- **Severity:** medium
- **Confidence:** low
- **Category:** memory

## Code
```cpp
    server_getline(protocol, &tmp, PATH_MAX);
    if (strcmp (tmp,
        pserver_protocol_interface.verify_only ?
        "END VERIFICATION REQUEST" : "END AUTH REQUEST")
    != 0)
    {
        server_error (1, "bad auth protocol end: %s", tmp);
        free(tmp);                 // freed here ...
    }

    const char *unscrambled_password = scramble.Unscramble(...);
    ...
    free(tmp);                     // ... and again here
    return CVSPROTO_SUCCESS;
```

## Why this is a bug
When the client's trailing line is not the expected `END AUTH REQUEST` /
`END VERIFICATION REQUEST`, the code calls `server_error(1, ...)` and `free(tmp)`, but
does **not** return. Control falls through to the common tail, which frees `tmp` a second
time (line 237) and returns `CVSPROTO_SUCCESS`.

This is correct only if `server_error` with `fatal=1` never returns (i.e. it aborts the
request via exit/longjmp). If that callback can return — which is not guaranteed at this
layer, since `server_error` just forwards to `_current_server->error(...)` — then:

1. `tmp` is freed twice (double-free / heap corruption), and
2. the server proceeds to accept the authentication with `CVSPROTO_SUCCESS` even though
   the client violated the protocol framing.

The sibling `sserver_auth_protocol_connect` handles the same situation defensively —
it frees `tmp` and immediately `return CVSPROTO_FAIL;` (sserver.cpp:493-495), with no
fall-through. The pserver path lost that `return`, making it rely on `server_error(1)`
being fatal.

## Suggested fix
Return after reporting the framing error, mirroring sserver:
```cpp
    if (strcmp(tmp, ...) != 0)
    {
        server_error(1, "bad auth protocol end: %s", tmp);
        free(tmp);
        return CVSPROTO_FAIL;
    }
```
---
