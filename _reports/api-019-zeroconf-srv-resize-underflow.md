---
# CZeroconf::_service_srv_func: size_t underflow in resize(i-1) on crafted mDNS name
- **File:** cvsnt/cvsnt-2.5.05.3744/cvsapi/Zeroconf.cpp
- **Line(s):** 71-84 (bug at 77)
- **Severity:** low
- **Confidence:** high
- **Category:** logic

## Code
```cpp
void CZeroconf::_service_srv_func(const char *name, unsigned short port, const char *target)
{
    cvs::string nm = name;
    size_t i = nm.find(m_service);
    if(i==cvs::string::npos)
        return;
    nm.resize(i-1);            // i can be 0 -> resize((size_t)-1)
    ...
}
```

## Why this is a bug
`name` is taken from an mDNS/DNS-SD response, i.e. it is supplied by whatever device is
answering service discovery on the local network. The code locates the service substring
with `find`, guards only against `npos`, then trims with `nm.resize(i-1)`, assuming the
service name is always preceded by at least one character.

If a (malicious or simply malformed) responder returns a name whose text begins with the
service string, `find` returns `i == 0`, and `i-1` wraps to `SIZE_MAX`.
`std::string::resize(SIZE_MAX)` throws `std::length_error`/`bad_alloc`, which is not
caught here, terminating the client. This is a remotely-triggerable (local-segment)
denial of service when Zeroconf discovery is in use.

## Suggested fix
Validate `i` before subtracting:
```cpp
if(i==cvs::string::npos || i==0)
    return;
nm.resize(i-1);
```
(or `nm.resize(i ? i-1 : 0)` if an empty service name is acceptable).
---
