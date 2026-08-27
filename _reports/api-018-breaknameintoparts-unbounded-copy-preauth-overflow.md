---
# BreakNameIntoParts (UNICODE path) unbounded _tcscpy/_tcsncpy → pre-auth stack overflow via client username
- **File:** cvsnt/cvsnt-2.5.05.3744/windows-NT/win32.cpp
- **Line(s):** 530-536 (BreakNameIntoParts), 591-613 (win32_valid_user)
- **Severity:** high
- **Confidence:** high
- **Category:** overflow

## Code
BreakNameIntoParts:
```cpp
    ptr=(TCHAR*)_tcschr(name, '\\');
    if (ptr)
    {
#ifdef _UNICODE
        _tcscpy(w_name,ptr+1);                 // unbounded
        _tcsncpy(w_domain,name,ptr-name);      // bound = domain length, not dest size
        w_domain[ptr-name]='\0';               // index can exceed dest
#else
        w_name[MultiByteToWideChar(win32_global_codepage,0,ptr+1,-1,w_name,UNLEN+1)]='\0';   // bounded
        w_domain[MultiByteToWideChar(win32_global_codepage,0,name,ptr-name,w_domain,DNLEN)]='\0'; // bounded
#endif
    }
    else
    {
#ifdef _UNICODE
        _tcscpy(w_name,name);                  // unbounded
#else
        w_name[MultiByteToWideChar(...,w_name,UNLEN+1)]='\0';   // bounded
#endif
        ...
    }
```
win32_valid_user (server-side password check, reached before LogonUser):
```cpp
    TCHAR User[UNLEN+1] = {0};          // 257 wchars
    TCHAR Domain[DNLEN+1] = {0};        // 257 wchars (DNLEN is #defined 256 in this file)
    TCHAR user[UNLEN+DNLEN+2] = {0};    // 514 wchars
    ...
    // user := [domain "\\"] username   (client-controlled username copied in)
    if(BreakNameIntoParts(user, User, Domain, NULL))   // splits 514-wide buffer into 257-wide targets
        return 0;
    ...
    LogonUserW(User,Domain,Password,...);
```

## Why this is a bug
`win32_valid_user` validates a username/password pair supplied by an **unauthenticated**
pserver client. It assembles `user` (a 514-wide-char buffer) from the client's username
(which may itself contain a `\`), then calls `BreakNameIntoParts` to split it into `User`
and `Domain`, each only `UNLEN+1`/`DNLEN+1` = 257 wide chars.

The `_UNICODE` branch of `BreakNameIntoParts` copies with `_tcscpy(w_name, ptr+1)` and
`_tcsncpy(w_domain, name, ptr-name)` (plus `w_domain[ptr-name]='\0'`) using the *source*
lengths, with no clamp to the destination size. Since the source `user` can hold up to
~513 wide chars while the destinations hold 257, a username whose component before or
after the `\` exceeds 256 characters overflows `User` or `Domain` on the stack. The `\0`
store at `w_domain[ptr-name]` can also land one past the end.

This runs before authentication succeeds (before `LogonUserW`), so it is a remotely
triggerable, pre-auth server-side stack buffer overflow. The ANSI branch of the same
function is correctly bounded (it passes `UNLEN+1`/`DNLEN` as `cchWideChar` to
`MultiByteToWideChar`), and the other caller `win32getpwnam` pre-bounds its input to a
257-wide buffer — so only the `user`-is-larger-than-target case in `win32_valid_user`
is exposed. The exact overflow length depends on the username-length cap on the path to
this function (PATH_MAX), but the unbounded copy is the defect regardless.

## Suggested fix
Bound the UNICODE copies to the destination capacities, e.g. use `lstrcpynW(w_name,
ptr+1, UNLEN+1)` and copy at most `min(ptr-name, DNLEN)` wide chars into `w_domain`
(NUL-terminating within bounds), mirroring the ANSI branch. Additionally, validate/limit
the username length in `win32_valid_user` before splitting.
---
