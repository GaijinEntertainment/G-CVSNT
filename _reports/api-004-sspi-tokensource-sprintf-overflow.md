---
# SSPI client: unbounded sprintf of hostname into 60-byte SPN buffer
- **File:** cvsnt/cvsnt-2.5.05.3744/protocols/sspi.cpp
- **Line(s):** 662, 693
- **Severity:** medium
- **Confidence:** high
- **Category:** overflow

## Code
```cpp
int ClientAuthenticate(const char *protocol, const char *name, const char *pwd, const char *domain, const char *hostname)
{
    ...
    char myTokenSource[DNLEN*4];         // DNLEN == 15 -> 60 bytes
    ...
    if(strcmp(secPackInfo->Name,"Schannel"))
    {
        ...
        sprintf (myTokenSource, "cvs/%s", hostname);   // <-- unbounded
    }
    else
    {
        ...
        strncpy(myTokenSource,hostname,sizeof(myTokenSource));  // bounded (Schannel branch)
    }
```

## Why this is a bug
`myTokenSource` is a fixed 60-byte stack buffer (`DNLEN` is 15 in `<lmcons.h>`). In the
NTLM/Kerberos/Negotiate branch the service principal name is built with an unbounded
`sprintf(... "cvs/%s", hostname)`. `hostname` is the CVSROOT host and is not length
checked anywhere on this path. Writing `"cvs/"` (4 bytes) + hostname + NUL overflows the
buffer whenever the hostname is 56 characters or longer — well within the range of a
legitimate fully-qualified domain name (DNS names may be up to 255 characters).

The result is a classic stack buffer overflow that can corrupt the return address /
adjacent locals and crash (or potentially be exploited). Note the sibling Schannel
branch (line 728) correctly uses a bounded `strncpy(..., sizeof(myTokenSource))`, which
shows the size limit was known; the `sprintf` path simply forgot it.

## Suggested fix
Use a bounded formatter and size the buffer for a real hostname, e.g.:
```cpp
char myTokenSource[MAX_PATH];
...
snprintf(myTokenSource, sizeof(myTokenSource), "cvs/%s", hostname);
```
(and make the Schannel branch's `strncpy` explicitly NUL-terminate).
---
