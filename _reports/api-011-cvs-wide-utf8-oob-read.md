---
# cvs::wide UTF-8 decoder reads past end of string on truncated multibyte sequence
- **File:** cvsnt/cvsnt-2.5.05.3744/cvsapi/cvs_string.h
- **Line(s):** 225-235 (function utf82ucs2)
- **Severity:** medium
- **Confidence:** high
- **Category:** memory

## Code
```cpp
void utf82ucs2(const char *src)
{
    unsigned char *p=(unsigned char *)src;
    wchar_t ch;
    size_t strlen_src=(src)?strlen(src):0;
    w_str.reserve(strlen_src);
    while(*p)
    {
        if(*p<0x80) { ch = p[0]; p++; }
        else if(*p<0xe0) { ch = ((p[0]&0x3f)<<6)+(p[1]&0x3f); p+=2; }
        else if(*p<0xf0) { ch = ((p[0]&0x1f)<<12)+((p[1]&0x3f)<<6)+(p[2]&0x3f); p+=3; }
        else if(*p<0xf8) { ch = ...p[3]...; p+=4; }
        else if(*p<0xfc) { ch = ...p[4]...; p+=5; }
        else if(*p<0xfe) { ch = ...p[5]...; p+=6; }
        else { ch = '?'; p++; }
        w_str+=(wchar_t)ch;
    }
}
```

## Why this is a bug
The decoder chooses how many bytes to consume based purely on the lead byte and then
reads the continuation bytes `p[1]..p[5]` without checking that they lie within the
input string. When `src` ends with a truncated multibyte sequence — e.g. a lone lead
byte `0xF0` immediately before the terminating NUL — the code reads `p[1]` (the NUL),
`p[2]`, `p[3]` (both past the end of the buffer) and then does `p+=4`, stepping the
cursor *past* the NUL terminator. The `while(*p)` guard then continues reading whatever
memory follows until it happens to hit a zero byte, so a single malformed trailing byte
turns into an unbounded out-of-bounds read (crash / DoS, or leakage of adjacent heap
bytes into the produced wide string).

`cvs::wide` is the standard UTF-8 to UTF-16 helper used throughout the Win32 layer to
feed file names and paths into wide Windows APIs, so malformed UTF-8 arriving from a
repository path, filename, or protocol field can reach it. Continuation bytes are also
never validated (they need not have the `10xxxxxx` form), which compounds the
over-read.

## Suggested fix
Bound every access to the known length and validate continuation bytes, e.g. compute the
sequence length, check `p + n <= src + strlen_src`, verify each continuation byte is
`0x80..0xBF`, and emit `'?'` (and stop/advance by one) on any violation instead of
reading ahead. The sibling `narrow`/`ucs22utf8` encoder is fine; only the decoder lacks
bounds checks.
---
