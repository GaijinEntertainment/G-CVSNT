# send_blob_file_direct never checks fopen result before fread/fclose

- **File:** cvsnt/cvsnt-2.5.05.3744/src/client.cpp
- **Line(s):** 5775-5817 (fopen at 5784, use at 5790, fclose at 5805)
- **Severity:** medium
- **Confidence:** high
- **Category:** error-handling

## Code
```cpp
bool send_blob_file_direct(const char *file, char *hash_encoded, bool blob_binary_compressed)
{
  size_t fsz = get_file_size(file);
  ...
  char bufIn[128<<10];
  FILE* rf = fopen(file, "rb");            // <-- result never checked
  std::vector<char> blob;blob.resize(fsz); size_t dataWritten = 0;
  StreamStatus st = compress_lambda(
    [&](const char *&src, size_t &src_pos, size_t &src_size)
      {...
       src_size = fread(bufIn, 1, sizeof(bufIn), rf);   // <-- fread(NULL FILE*)
       update_blob_hash(hctx, bufIn, src_size);
       return ferror(rf) ? StreamStatus::Error : ...    // <-- ferror(NULL)
      },
    ...);
  if (st != StreamStatus::Finished)
    error(1,0, "Can't send binary blob for %s", file);
  fclose(rf);                              // <-- fclose(NULL)
```

## Why this is a bug
`fopen` can legitimately fail here: the working file can be deleted or exclusively locked (very common on Windows, which this fork targets) between the moment commit classifies the file and the moment the blob is streamed, or the process can be out of FILE handles. `fread`/`ferror`/`fclose` on a NULL `FILE*` is undefined behavior and crashes on the MSVC CRT (invalid parameter handler) — the client dies mid-commit with no diagnostic instead of the graceful "file disappeared during commit" message that its sibling `finish_send_blob_file()` (line 5823-5827) produces for exactly this scenario.

Note also `blob.resize(fsz)` right below pre-allocates the entire file size in RAM; for the multi-GB files this fork exists to handle, an allocation failure raises `std::bad_alloc` which nothing catches (straight `std::terminate`), another abrupt-death path in the same function.

## Suggested fix
```cpp
  FILE* rf = fopen(file, "rb");
  if (!rf)
  {
    error (0, errno, "cannot open %s for blob transfer", file);
    return false;
  }
```
(and consider capping the initial `blob.resize()` / catching bad_alloc).
