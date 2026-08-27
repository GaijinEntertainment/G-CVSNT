---
# CFileAccess::mimetype writes NUL at byte-count index into wchar_t buffer (OOB write)
- **File:** cvsnt/cvsnt-2.5.05.3744/cvsapi/win32/FileAccess.cpp
- **Line(s):** 580-587
- **Severity:** medium
- **Confidence:** high
- **Category:** overflow

## Code
```cpp
TCHAR str[256];
DWORD len = sizeof(str)-sizeof(TCHAR);            // 512 - 2 = 510 (BYTES)
if(RegQueryValueEx(hk,L"Content Type",NULL,NULL,(LPBYTE)str,&len))
{
    RegCloseKey(hk);
    return "";
}
str[len]='\0';                                    // len is BYTES, str is wchar_t[]
return (const char *)cvs::narrow(str);
```

## Why this is a bug
`RegQueryValueEx` reports the size of the returned data in **bytes** via `len`. `str` is
`TCHAR str[256]`, i.e. `wchar_t[256]` in this Unicode build, so `str[len]` indexes by
*wide characters* — it writes at byte offset `len * 2`.

For a `Content Type` registry value larger than 256 bytes (128 wide chars) but not so
large that the call fails with `ERROR_MORE_DATA` (the cap is 510 bytes), `len` lands in
[256, 510], and `str[len]` writes a 2-byte NUL at wide-char index 256..510 — byte offset
512..1020 — which is up to ~508 bytes past the end of the 512-byte stack buffer. That is
an out-of-bounds stack write. Even for shorter values the terminator is placed at twice
the intended offset.

The value comes from `HKEY_CLASSES_ROOT\<.ext>\Content Type`, selected by the file's
extension, so a file whose extension maps to an unusually long registry Content Type
triggers the overwrite.

## Suggested fix
Index by wide-character count, and guard the range:
```cpp
DWORD n = len / sizeof(TCHAR);
if(n >= (sizeof(str)/sizeof(TCHAR))) n = (sizeof(str)/sizeof(TCHAR)) - 1;
str[n]='\0';
```
(REG_SZ data is normally already NUL-terminated, but the terminator index must be a
character count, not a byte count.)
---
