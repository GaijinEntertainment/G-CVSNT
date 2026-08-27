# HTTP download error message appends status code as a raw char instead of a number

- **File:** cvsnt/cvsnt-2.5.05.3744/src/blob_http_processor.cpp
- **Line(s):** 22-27
- **Severity:** low
- **Confidence:** high
- **Category:** logic

## Code
```cpp
if (!res || res->status != 200)
{
  err = res ? httplib::detail::status_message(res->status) : "unknown";
  err += " err code";
  err += res ? res->status : -1;   // <-- appends a single char, not the number
  return false;
}
```

## Why this is a bug
`err` is a `std::string`. `res->status` (and the `-1`) are `int`. `std::string` has no `operator+=(int)`; the expression binds to `operator+=(char)`, so the `int` is converted to a single `char` and that one byte is appended. Instead of `"... err code404"` the message gets `"... err code"` followed by the byte value 404&0xFF (0x94, a non-printable/garbage character), or byte 0xFF for the `-1` (no-response) case.

So every failed HTTP blob download produces a corrupted, misleading error string — exactly when a human is trying to diagnose a download failure. Not a memory-safety issue (a valid char is appended), purely wrong output.

## Suggested fix
Convert the code to text explicitly, e.g.:
```cpp
err += " err code ";
err += std::to_string(res ? res->status : -1);
```
