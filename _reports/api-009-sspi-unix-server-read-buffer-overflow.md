---
# sspi (unix) server: client-controlled length drives read() into fixed 1024-byte stack buffer (remote pre-auth overflow)
- **File:** cvsnt/cvsnt-2.5.05.3744/protocols/sspi_unix.cpp
- **Line(s):** 228-230, 264-266, 274
- **Severity:** critical
- **Confidence:** high
- **Category:** overflow

## Code
```cpp
int sspi_auth_protocol_connect(const struct protocol_interface *protocol, const char *auth_string)
{
    ...
    short len;
    char line[1024];
    char buf[1024];
    ...
    do
    {
    read(current_server()->in_fd,&len,2);        // len fully controlled by client
    len=ntohs(len);
    l=read(current_server()->in_fd,buf,len);     // <-- read up to `len` bytes into buf[1024]
    if(l<0)
      return CVSPROTO_FAIL;
    ...
    l=base64enc((unsigned char *)buf,(unsigned char *)line+3,len);  // also overflows line[]
    ...
```

## Why this is a bug
This is the server side of the `:sspi:` (winbind/NTLM) handshake, executed *before the
client is authenticated*. `len` is a signed `short` read straight off the network. It is
byte-swapped and then used, unchecked, as the byte count for
`read(in_fd, buf, len)` where `buf` is a fixed 1024-byte stack array.

- If the client sends any length > 1024 (e.g. 2000), `read` writes up to 2000 bytes into
  the 1024-byte `buf`, smashing the stack.
- Worse, `len` is *signed*: a value like `0x8000` becomes `-32768` after `ntohs`/assign,
  which, converted to the `size_t` parameter of `read`, is an enormous count — an
  effectively unbounded overflow.

Because `buf` is a stack buffer, this is a classic remotely-triggerable stack buffer
overflow reachable prior to authentication (the attacker only needs to reach the SSPI
auth handshake). A second overflow follows at line 274, where `base64enc` expands `len`
bytes (4/3 growth) into `line+3` (a 1021-byte tail of `line[1024]`).

This path is gated on the server having a `WinbindWrapper` configured (see `init()`,
which nulls `auth_protocol_connect` when it is unset). When SSPI/winbind auth is enabled,
this is a critical remote code-execution / crash vector.

Note the Win32 sibling (sspi.cpp `ServerAuthenticate`) avoids this by `malloc`-ing the
client-specified size instead of reading into a fixed buffer; the unix port does not.

## Suggested fix
Validate the length against the buffer size (and treat it as unsigned) before reading,
and check the `read` return values:
```cpp
unsigned short ulen;
if (read(current_server()->in_fd,&ulen,2) != 2) return CVSPROTO_FAIL;
ulen = ntohs(ulen);
if (ulen > sizeof(buf)) return CVSPROTO_FAIL;
l = read(current_server()->in_fd, buf, ulen);
if (l < 0) return CVSPROTO_FAIL;
```
Also bound the subsequent `base64enc` output so it cannot exceed `sizeof(line)-3`.
---
