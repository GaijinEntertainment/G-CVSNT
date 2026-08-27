---
# tcp_connect_http: proxy error message dropped due to missing %s in format string
- **File:** cvsnt/cvsnt-2.5.05.3744/protocols/common.cpp
- **Line(s):** 250
- **Severity:** low
- **Confidence:** high
- **Category:** typo

## Code
```cpp
    if((code/100)!=2)
    {
        if(code==407)
        {
            ...
        }
        else
            server_error(1,"Proxy server connect failed: ",p?p:"No response");
    }
```

## Why this is a bug
`server_error(int fatal, const char *fmt, ...)` treats its second argument as a
`printf` format string. Here the format `"Proxy server connect failed: "` contains no
conversion specifier, so the third argument (`p ? p : "No response"` — the proxy's
actual status text) is passed but never consumed or printed. The diagnostic silently
loses the reason the proxy connection failed, making these failures much harder to
troubleshoot.

This is the inverse of the format-string bugs (api-014/015): there a variable was used
as the format; here the format forgot the `%s` for its intended argument.

## Suggested fix
```cpp
server_error(1,"Proxy server connect failed: %s", p?p:"No response");
```
---
