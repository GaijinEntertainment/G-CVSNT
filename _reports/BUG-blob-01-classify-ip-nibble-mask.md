---
id: BUG-blob-01
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/keyValueServer/blob_sockets/blob_sockets.cpp
line: 24
severity: critical
category: typo
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# `blob_classify_ip()` masks 4 bits instead of 8, classifying 15 public /8 networks as PRIVATE

## Summary
The `10.0.0.0/8` test uses `ip & 0xF` instead of `ip & 0xFF`. Any IPv4 address whose first
octet has low nibble `0xA` (10, 26, 42, 58, 74, 90, 106, 122, 138, 154, 170, 186, 202, 218, 250)
is reported as `IpType::PRIVATE`. That verdict gates encryption removal, unauthenticated-client
acceptance and fail2ban syslogging on the server, and MITM protection on the client.

## Code
```cpp
// keyValueServer/blob_sockets/blob_sockets.cpp:19-31
IpType blob_classify_ip(uint32_t ip)
{
  if (ip == 0x100007f)//127.0.0.1
    return IpType::LOCAL;
  //https://en.wikipedia.org/wiki/Private_network
  if ( (ip&0xF) == 10)//10.x.x.x            <-- BUG: 0xF, must be 0xFF
    return IpType::PRIVATE;
  if ( (ip&0xFFFF) == 0xa8c0)//192.168.x.x
    return IpType::PRIVATE;
  if ( (ip&0xFF) == 172 && ((ip>>8)&0xFF) >= 16 && ((ip>>8)&0xFF) <= 31)//172.16.x.x -- 172.31.x.x
    return IpType::PRIVATE;
  return IpType::PUBLIC;
}
```

## Why it is a bug
The two neighbouring tests establish the byte order beyond doubt: `0x100007f` is 127.0.0.1 and
`(ip&0xFFFF)==0xa8c0` is 192.168 — the *low* byte of `ip` is the first octet. The same convention
is used when the address is printed in `blob_push_proc.cpp:446`
(`client_ip&0xFF, (client_ip>>8)&0xFF, ...`). So the intended first-octet test is `(ip&0xFF)==10`;
`ip & 0xF` keeps only the low nibble and matches every first octet congruent to 10 mod 16.

The classification is security-load-bearing in three places:

* `blob_push_proc.cpp:405` — `is_public_client_ip = blob_classify_ip(client_ip) == IpType::PUBLIC`.
  * `:327-335` with `CafsServerEncryption::Public`, a *non*-public client makes the server answer
    `NONE` and call `blob_close_encryption()`, i.e. the rest of the session runs in clear text.
  * `:344-350` with `CafsServerEncryption::Public` and a **version-001 (no-auth)** client,
    `badVersion` is only set when `is_public_client_ip` is true. A misclassified client therefore
    completes `authenticate_client()` without ever proving knowledge of the shared secret.
  * `:413` `syslog_on_authentication(..., need_syslog = is_public_client_ip)` — attacks from those
    ranges are never written to syslog, so fail2ban never bans them.
* `blob_push_pull_client.cpp:139` — the client refuses a non-authenticating server only when
  `blob_classify_ip(serverIP) == IpType::PUBLIC`; a hijacked/spoofed server at e.g. 74.x.x.x is
  accepted unauthenticated and unencrypted.

## Failure scenario
A CAFS server started with `CafsServerEncryption::Public` (the documented configuration for a
server reachable from outside) is contacted from the public address `74.125.24.100`
(`ip = 0x64187D4A`, low nibble of first octet `74 & 0xF == 10`).
`blob_classify_ip` returns `PRIVATE`, so:

1. A protocol-001 client from that address is accepted with **no authentication at all** and can
   immediately issue `PUSH`/`PULL`/`SIZE` against any CVS root it names.
2. A protocol-002 client from that address is authenticated, but the server then tears the
   encryption down and streams every blob in plain text across the public Internet.
3. Neither case is syslogged, so `blob_syslog`-driven banning never triggers.

## Suggested fix
```cpp
  if ( (ip&0xFF) == 10)//10.x.x.x
    return IpType::PRIVATE;
```

## Refutation attempt
I checked whether `ip` might be stored host-order (which would make `0xF` a mask of the *last*
octet and merely wrong in a different way): both the 127.0.0.1 constant and the 192.168 constant
in the same function, and the a.b.c.d printing in `blob_syslog`, agree that the low byte is the
first octet, so the mask really is applied to the first octet. I also checked whether some caller
re-validates the classification — `blob_push_proc.cpp` and `blob_push_pull_client.cpp` are the only
consumers and both take the result at face value. The finding stands.
