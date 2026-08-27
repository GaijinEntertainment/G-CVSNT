---
# sspi (unix) client: server-controlled length drives tcp_read into fixed challenge struct (remote overflow)
- **File:** cvsnt/cvsnt-2.5.05.3744/protocols/sspi_unix.cpp
- **Line(s):** 327, 337-342
- **Severity:** high
- **Confidence:** high
- **Category:** overflow

## Code
```cpp
int ClientAuthenticate(const char *protocol, const char *name, const char *pwd, const char *domain, const char *hostname)
{
    tSmbNtlmAuthChallenge challenge;   // fixed struct, sizeof ~= 1076 bytes
    ...
    short len;
    ...
    if(tcp_read(&len,2)!=2)
      return 0;
    if(!len)
      return 0;
    if(tcp_read(&challenge,ntohs(len))!=ntohs(len))   // len is server-controlled
      return 0;
    buildSmbNtlmAuthResponse(&challenge, &response, ...);
    ...
}
```

## Why this is a bug
This is the unix `:sspi:` (winbind/NTLM) *client*. During the NTLM handshake the server
sends a 2-byte length followed by that many bytes of challenge data. The client reads the
length into `len`, byte-swaps it with `ntohs` (range 0..65535), and then does
`tcp_read(&challenge, ntohs(len))` into a fixed-size stack struct.

`tSmbNtlmAuthChallenge` is only ~1076 bytes (`ident[8]` + headers + `buffer[1024]` +
`bufIndex`). There is no check that `ntohs(len) <= sizeof(challenge)`, so a malicious or
MITM server can specify a length up to 65535 and overflow the struct on the client's
stack by tens of kilobytes — a remotely triggerable stack buffer overflow against the
CVS client.

This is the client-side counterpart of the server-side overflow reported in api-009;
both stem from using an unvalidated on-wire length as the size of a fixed-buffer read.

## Suggested fix
Clamp the length to the destination size before reading, and check the read:
```cpp
unsigned short ulen = ntohs((unsigned short)len);
if(ulen == 0 || ulen > sizeof(challenge))
    return 0;
if(tcp_read(&challenge, ulen) != ulen)
    return 0;
```
---
