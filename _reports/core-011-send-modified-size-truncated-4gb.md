# send_modified announces file size with "%lu" — silently truncates >4GB files on Windows, desyncing the protocol

- **File:** cvsnt/cvsnt-2.5.05.3744/src/client.cpp
- **Line(s):** 5251-5255 (also the receive-side counterpart: `int size = atoi(size_string)` at 1513/1522 and 2374/2379)
- **Severity:** high
- **Confidence:** high
- **Category:** overflow / integer

## Code
```cpp
    		send_to_server ("Modified ", 0);
    		send_to_server (file, 0);
    		send_to_server ("\n", 1);
    		send_to_server (mode_string, 0);
    		send_to_server ("\n", 1);
    		sprintf (tmp, "%lu\n", (unsigned long) newsize);   // <-- unsigned long is 32-bit on Win64
    		send_to_server (tmp, 0);

    		if (newsize > 0)
    			send_to_server_untranslated(buf, newsize);     // <-- sends the full 64-bit size
```
Contrast with the blob path a few hundred lines later, which was fixed correctly:
```cpp
  sprintf(tmp, "%llu\n", (unsigned long long) (dataWritten + sizeof(hdr)));   // line 5813
```

## Why this is a bug
This fork exists specifically to handle very large binary files, and the platform is Windows, where `unsigned long` is 32 bits even in x64 builds. `newsize` is a `size_t` holding the full file size (the file is read wholesale into `buf`; `wnt_stat` correctly reports 64-bit `st_size`, win32.cpp:2270).

A file larger than 4 GiB that goes through the legacy `Modified` path — any large file *not* marked with the new `-kB` (KFLAG_BINARY_DELTA) option, e.g. an old-style `-kb` binary, or any file when the server lacks `Blob-ref-transfer` — is announced with `newsize mod 2^32` bytes but transmitted with all `newsize` bytes. The server consumes the truncated count as file data and then **interprets the remaining gigabytes of raw file content as CVS protocol commands**. Best case the connection errors out with "unrecognized request"; worst case byte sequences that happen to look like valid requests get executed, corrupting the working session. There is no error message pointing at the actual cause.

The receive side has the matching bug: `int size = atoi (size_string);` (lines 1513-1522, annotated `//since we use atoi`) makes a client updating such a file break at 2 GiB (`atoi` overflow is UB; typically INT_MAX or negative, and `size_t(size)` of a negative value is astronomically large — see line 1720 `size_t sizeLeft = size_t(size);`).

## Suggested fix
- Send: `sprintf (tmp, "%llu\n", (unsigned long long) newsize);`
- Receive: parse with `strtoull` into a `size_t`/`uint64_t` in `update_entries`, `update_blob_ref_entries`, `handle_mbinary`, `read_counted_file`, and `proxy_updated`/`proxy_file`.
- Alternatively, refuse (with a clear error) to send >4GiB files through the non-blob path.
