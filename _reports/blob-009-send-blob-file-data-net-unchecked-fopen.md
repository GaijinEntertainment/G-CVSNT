# send_blob_file_data_net: fopen result used without NULL check (crash on unreadable file)

- **File:** cvsnt/cvsnt-2.5.05.3744/src/blob_kv_processor.cpp
- **Line(s):** 16-19
- **Severity:** medium
- **Confidence:** high
- **Category:** error-handling

## Code
```cpp
FILE* rf = fopen(file, "rb");
fseek(rf, 0, SEEK_END);          // <-- rf may be NULL
const size_t fsz = ftell(rf);
fseek(rf, 0, SEEK_SET);
```

## Why this is a bug
The return value of `fopen` is used immediately by `fseek`/`ftell`/later `fread(bufIn, 1, sizeof(bufIn), rf)` (line 37) and `fclose(rf)` (line 49) without ever checking for `NULL`. If the file cannot be opened — it was deleted/renamed between being queued and uploaded (the upload queue is asynchronous; see download_blob_to.cpp BackgroundProcessor), a permission error, too many open FDs, or a race with another process — `fopen` returns `NULL` and `fseek(NULL, ...)` dereferences it, crashing the CVS client during commit.

This runs on the upload/commit path (`KVNetworkProcessor::upload` -> `send_blob_file_data_net`), so a missing or locked working-file at commit time takes down the client instead of producing the intended error string.

## Suggested fix
```cpp
FILE* rf = fopen(file, "rb");
if (!rf)
{
  output << "Can't open file " << file; err = output.str();
  return KVRet::Error;
}
```
(returning an error, as every other failure path in this function does).
