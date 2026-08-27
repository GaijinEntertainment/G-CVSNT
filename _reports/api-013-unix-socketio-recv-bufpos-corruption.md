---
# unix CSocketIO::recv over-advances m_bufpos (drops buffered bytes) — diverges from win32
- **File:** cvsnt/cvsnt-2.5.05.3744/cvsapi/unix/SocketIO.cpp
- **Line(s):** 308-313 (esp. 311)
- **Severity:** medium
- **Confidence:** high
- **Category:** logic

## Code
unix (buggy):
```cpp
    m_buflen = rd;
    if((len-oldlen)<=m_buflen)
    {
        memcpy(buf+oldlen,m_buffer,len-oldlen);
        m_bufpos+=len;                 // <-- wrong
        return len;
    }
```
win32 (correct — same function, FileAccess sibling):
```cpp
    m_buflen = rd;
    if((len-oldlen)<=m_buflen)
    {
        memcpy(buf+oldlen,m_buffer,len-oldlen);
        m_bufpos+=(len-oldlen);        // <-- correct
        return len;
    }
```

## Why this is a bug
This branch is reached when the request could not be satisfied from the bytes already
buffered, so the code first copies the `oldlen` leftover bytes into `buf[0..oldlen-1]`,
refills `m_buffer` with `rd` freshly-read bytes (resetting `m_bufpos=0`), then copies the
remaining `len-oldlen` bytes out of the new buffer into `buf[oldlen..len-1]`.

The number of bytes consumed *from the freshly read buffer* is therefore `len-oldlen`,
so `m_bufpos` must become `len-oldlen`. The unix version instead does `m_bufpos+=len`,
overshooting by `oldlen`. On the next `recv`, reads resume from `m_buffer[len]` instead of
`m_buffer[len-oldlen]`, silently **skipping `oldlen` bytes** of received data — a stream
corruption/desync. (The win32 twin computes `m_bufpos+=(len-oldlen)`, confirming the
intended value.)

The defect only manifests when `recv` is called with `len > 1` and there was leftover
buffered data (`oldlen > 0`); the common `getline` path uses `recv(&c,1)` where
`oldlen` is always 0, which is why it hides. But multi-byte callers exist and use this
exact unix path, e.g. `cvstools/unix/GlobalSettings.cpp:53`
(`sock.recv(buffer,buffer_len)`), so corrupted reads are reachable.

## Suggested fix
```cpp
    m_bufpos += (len - oldlen);
```
to match the win32 implementation and the buffer invariant.
---
