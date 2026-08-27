---
# sserver (win32) uses uninitialized `certonly` when CertificatesOnly registry value is absent
- **File:** cvsnt/cvsnt-2.5.05.3744/protocols/sserver_win32.cpp
- **Line(s):** 361, 375-376, 460
- **Severity:** medium
- **Confidence:** high
- **Category:** logic

## Code
```cpp
int sserver_auth_protocol_connect(const struct protocol_interface *protocol, const char *auth_string)
{
    CScramble scramble;
    char *tmp;
    int certonly;                 // <-- NOT initialized
    ...
    if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","CertificatesOnly",keyfile,sizeof(keyfile)))
        certonly = atoi(keyfile); // <-- only assigned when the value exists
    ...
    switch(certonly)              // <-- read here; may be indeterminate
    {
    case 0:
        break;
    case 1:
        if(!cert) { ... return CVSPROTO_AUTHFAIL; }
        free(sserver_protocol_interface.auth_password);
        sserver_protocol_interface.auth_password = NULL;
        break;
    case 2:
        if(!cert) { ... return CVSPROTO_AUTHFAIL; }
        break;
    };
```

## Why this is a bug
`certonly` is declared without an initializer. It is assigned only inside the `if(!CGlobalSettings::GetGlobalValue(...))` branch, i.e. only when the `CertificatesOnly`
registry value is present and readable. When that value is *absent* — which is the
default configuration for a password-based sserver — `GetGlobalValue` returns non-zero,
the assignment is skipped, and `switch(certonly)` reads an indeterminate stack value.

This is undefined behavior. In practice it makes the server's authentication policy
depend on stack garbage: if the garbage happens to be `1` or `2`, the server will
demand a client certificate (`case 1`/`case 2` → `CVSPROTO_AUTHFAIL` when the client
presented none) or free/NULL the password (`case 1`), instead of performing normal
password authentication. The result can be intermittent, machine-dependent
authentication failures (or an unintended change in whether the password is even
considered).

The portable Unix implementation gets this right — sserver.cpp:385 declares
`int certonly = 0;`. The win32 port dropped the initializer.

## Suggested fix
Initialize the variable to the safe default:
```cpp
int certonly = 0;
```
---
