# Server receive path uses int/atoi for file size — >2GB uploads overflow (large-binary fork)

- **File:** cvsnt/cvsnt-2.5.05.3744/src/server.cpp
- **Line(s):** serve_modified 1992/2035/2044-2045; receive_file 1731; receive_partial_file 1656; serve_binary_transfer 2085/2119/2140-2141
- **Severity:** high
- **Confidence:** high
- **Category:** overflow / integer

## Code
```cpp
static void serve_modified (char *arg)
{
    int size, status;               // <-- int
    ...
	size = atoi (size_text);        // <-- atoi: UB / overflow past INT_MAX
    ...
    if (size >= 0)
		receive_file (size, arg, !(serve_file_kopt(arg).flags&(KFLAG_BINARY|KFLAG_ENCODED)));
}

static void receive_file (int size, char *file, bool check_textfile)   // int size
...
static void receive_partial_file (int size, int file, ...)             // int size
{
    while (size > 0) { ... size -= nread; }
}
```

## Why this is a bug
The entire non-blob upload path types the file size as `int` and parses it with `atoi`. This fork's whole reason for existing is very large binary files, and while the new blob path (`serve_blob`, line 1812) correctly uses `uint64_t size = atoll(...)`, the legacy `Modified` path any client falls back to — and `serve_binary_transfer` — still use 32-bit `int`.

Consequences for a `Modified` upload of a file ≥ 2 GiB:
- `atoi` on a value > INT_MAX is undefined behavior; in practice it yields INT_MAX or a negative number.
- If it comes back negative, `if (size >= 0)` is false, so `receive_file` is skipped entirely: the server never reads the file bytes off the socket, then treats the multi-GB file body as a stream of protocol requests (protocol desync / possible mis-execution), or the connection wedges.
- If it comes back positive-but-truncated (`size mod 2^32` when it happens to land positive), the server writes only the truncated prefix and again leaves gigabytes of file data in the socket to be parsed as commands.

This is the server-side mirror of the client-side truncation reported in core-011; the two together mean a >2GB non-blob commit cannot work and actively corrupts the protocol stream in both directions.

## Suggested fix
Thread a 64-bit unsigned size through `serve_modified`, `serve_binary_transfer`, `receive_file`, and `receive_partial_file` (parse with `strtoull`, like `serve_blob` uses `atoll`), or explicitly reject non-blob transfers above a safe size with a clear protocol error.
