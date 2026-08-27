# server_updated uses 32-bit `unsigned long` size and "%lu" — truncates >4GB checkouts on Windows

- **File:** cvsnt/cvsnt-2.5.05.3744/src/server.cpp
- **Line(s):** 4385 (declaration), 4395/4417 (assignment), 4556 (buf_read_file), 4567 (sprintf "%lu")
- **Severity:** high
- **Confidence:** high
- **Category:** overflow / integer

## Code
```cpp
	struct buffer_data *list, *last;
	unsigned long size;                          // <-- 32-bit on LLP64 (Win64)
	...
	    size = buf_length (filebuf);
	...
	    size = sb.st_size;                       // <-- off_t (64-bit) truncated into unsigned long
	...
			status = buf_read_file (f, size, &list, &last);   // reads truncated count
	...
		char text[16];
		sprintf(text,"%lu\n",size);              // announces truncated count
		buf_output0(buf_to_net,text);
```

## Why this is a bug
This is the server→client mirror of the size-truncation problems reported in core-011 (client send) and core-013 (server receive). On Windows x64 (LLP64, the platform this fork targets) `unsigned long` is 32 bits, but `sb.st_size` is a 64-bit `off_t` (see win32.cpp `_statcore`: `buf->st_size = (((off_t)nFileSizeHigh)<<32)+nFileSizeLow`).

When the server checks out / updates a file larger than 4 GiB through the normal (non-blob) `Updated`/`Created`/`Merged`/`Rcs-diff` path, `size` wraps to `st_size mod 2^32`. The server then both announces the truncated length and only streams that many bytes via `buf_read_file(f, size, ...)`. The client (which also parses the count with `atoi`, core-011) receives a silently truncated file — data loss on the working copy with no error on either side. For a fork whose reason to exist is very large binary files, checking one out over any client/server transport that falls back to the classic file path corrupts it.

## Suggested fix
Type `size` as `uint64_t` (or `size_t`), print it with a 64-bit-correct format (`"%" PRIu64` / `"%llu"` with a suitably sized buffer — note `text[16]` is also too small for a 20-digit 64-bit value plus newline), and make `buf_read_file` take a 64-bit length. Coordinate with the client-side `strtoull` fix from core-011.
