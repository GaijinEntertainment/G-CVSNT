---
id: BUG-blob-18
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/src/blob_operations.cpp
line: 71
severity: low
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 6
behavior_change: no
---

# Two blob helpers use the result of `fopen()` without a null check

## Summary
`get_session_blob_reference_hash()` and `send_blob_file_data_net()` both call `fopen()` and pass the
result straight to `fread`/`fseek`. Both are preceded by a separate existence/size probe on the same
path, so the window is a classic TOCTOU: any change to the file (removal, rename, permission
change, exclusive lock on Windows) between the probe and the open turns a recoverable I/O error into
a null-pointer dereference.

## Code
```cpp
// src/blob_operations.cpp:67-81
size_t get_file_size(const char *file);
bool get_session_blob_reference_hash(const char *blob_ref_file_name, char *hash_encoded)
{
  if (get_file_size(blob_ref_file_name) != session_blob_reference_size)
    return false;
  unsigned char session_ref_file_content[session_blob_reference_size];
  FILE* fp;
  fp = fopen(blob_ref_file_name, "rb");                                     // 71: unchecked
  if (fread(&session_ref_file_content[0],1, session_blob_reference_size, fp) != session_blob_reference_size)
  {
    error(1,errno,"Couldn't read %s", blob_ref_file_name);
    return false;
  }
  fclose(fp);
```
```cpp
// src/blob_kv_processor.cpp:16-19
  FILE* rf = fopen(file, "rb");                                             // unchecked
  fseek(rf, 0, SEEK_END);
  const size_t fsz = ftell(rf);
  fseek(rf, 0, SEEK_SET);
```

## Why it is a bug
`fopen` returning `NULL` is the documented failure mode, and `fread(ptr, 1, n, NULL)` /
`fseek(NULL, ...)` are undefined behaviour — glibc dereferences the `FILE*` immediately
(`_IO_fread` reads `fp->_flags`), so the practical result is a SIGSEGV at address 0.

Neither call site is protected by anything:

* `get_session_blob_reference_hash` only knows that `stat()` reported a 79-byte file
  (`session_blob_reference_size` = `hash_type_magic_len` 7 + `hash_encoded_size` 64 +
  `session_crypt_magic_size` 8). `stat()` succeeds on files the caller cannot *open* — a file whose
  mode denies read to the current user, or one held with a deny-read share on Windows.
* `send_blob_file_data_net` runs after `caddressed_fs::get_file_content_hash(file, ...)` and a
  network round trip (`blob_size_on_server`, `blob_kv_processor.cpp:141-152`), which is a wide
  window during which the user's working file can be deleted or replaced.

The neighbouring code in the same subsystem does check — `download_blob_to.cpp:375-381` tests
`if (!tmp)` and reports "can't write temp", and `proxy_file_lib.cpp:336` tests
`if (!tmpf)` — so the two sites above are omissions rather than a house style.

There is a second defect in the first snippet: on the `fread` short-read branch, `fp` is leaked
(no `fclose` before `return false`). It is currently masked because `error(1, ...)` never returns,
but that also means a *recoverable* short read on a file that shrank is escalated into a process
exit.

## Failure scenario
`cvs commit` (or `cvs update`) in a sandbox that contains a 79-byte file the invoking user cannot
read — e.g. a colleague's file left behind with mode `0600` under a different owner, or, on Windows,
a file another process has open with `FILE_SHARE_NONE`.

`RCS_cmp_file` for that path reaches `rcs_checkin.cpp:1491`:
```cpp
  if (!get_session_blob_reference_hash(filename, hash_encoded_sent))
```
`get_file_size` (which uses `stat`) returns 79, matching `session_blob_reference_size`, so the early
`return false` is skipped. `fopen(filename, "rb")` fails with `EACCES` and returns `NULL`.
`fread(..., NULL)` dereferences a null `FILE*` and the CVS client segfaults, losing the whole
commit with no diagnostic.

The `blob_kv_processor` variant is triggered by deleting the file being committed during the
upload's network round trip: `fseek(NULL, 0, SEEK_END)` crashes inside the worker thread.

## Suggested fix
```cpp
  FILE* fp = fopen(blob_ref_file_name, "rb");
  if (!fp)
    return false;
  if (fread(&session_ref_file_content[0], 1, session_blob_reference_size, fp) != session_blob_reference_size)
  {
    fclose(fp);
    error(0, errno, "Couldn't read %s", blob_ref_file_name);
    return false;
  }
  fclose(fp);
```
and, in `send_blob_file_data_net`:
```cpp
  FILE* rf = fopen(file, "rb");
  if (!rf)
  {
    output << "Can't open " << file; err = output.str();
    return KVRet::Error;
  }
```

## Refutation attempt
I checked whether `get_file_size` might itself open the file and thereby prove openability —
`src/filesubr.cpp:564` uses a `stat`-family call, which only needs execute permission on the parent
directory. I checked whether some wrapper macro redefines `fopen` to an aborting variant in this
translation unit — `blob_operations.cpp` includes only `<stdio.h>`, `<string.h>`, `<errno.h>` and
the blob headers, not `cvs.h`, so it is the plain libc `fopen`. I checked whether the
`blob_kv_processor` site is unreachable because `get_file_content_hash` already mapped the file
successfully — it does prove the file was openable *earlier*, which is exactly what makes this a
TOCTOU rather than an always-crash. The finding stands; severity is low because triggering it
requires an unusual permission state or a concurrent delete.
