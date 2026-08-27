# get_session_blob_reference_hash: fopen result used without NULL check (TOCTOU crash)

- **File:** cvsnt/cvsnt-2.5.05.3744/src/blob_operations.cpp
- **Line(s):** 70-80
- **Severity:** medium
- **Confidence:** high
- **Category:** error-handling

## Code
```cpp
if (get_file_size(blob_ref_file_name) != session_blob_reference_size)
  return false;
unsigned char session_ref_file_content[session_blob_reference_size];
FILE* fp;
fp = fopen(blob_ref_file_name, "rb");
if (fread(&session_ref_file_content[0],1, session_blob_reference_size, fp) != session_blob_reference_size)  // fp may be NULL
{
  error(1,errno,"Couldn't read %s", blob_ref_file_name);
  return false;
}
fclose(fp);
```

## Why this is a bug
`fopen`'s return value is passed straight to `fread` without a `NULL` check. There is a genuine TOCTOU window: `get_file_size` succeeds, but the file can be removed/renamed/locked before the `fopen` on the next lines (this runs on the client commit path via `RCS_cmp_file` in rcs_checkin.cpp:1491, where working files are live). When `fopen` fails it returns `NULL`, and `fread(buf, 1, n, NULL)` dereferences a null `FILE*` — crash.

Secondary: on the short-read branch the code `return false;` without `fclose(fp)`. Here `error(1, ...)` exits the process (confirmed: error.cpp sets should_exit when status != 0), so the leak is normally moot — but if `error_exit()` is ever configured to unwind rather than hard-exit (server contexts), `fp` leaks. The primary defect is the missing NULL check.

## Suggested fix
```cpp
fp = fopen(blob_ref_file_name, "rb");
if (!fp)
  return false;
if (fread(session_ref_file_content, 1, session_blob_reference_size, fp) != session_blob_reference_size)
{
  fclose(fp);
  error(1, errno, "Couldn't read %s", blob_ref_file_name);
  return false;
}
fclose(fp);
```
