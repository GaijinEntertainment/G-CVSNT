# finish() leaks the temp blob file on WRONG_HASH / IO_ERROR / failed-rename paths

- **File:** cvsnt/cvsnt-2.5.05.3744/ca_blobs_fs/src/content_addressed_fs.cpp
- **Line(s):** 182-220 (esp. 195-199, 208-209, 219)
- **Severity:** medium
- **Confidence:** high
- **Category:** error-handling / resource-leak

## Code
```cpp
PushResult finish(PushData *fp, char *actual_hash_str)
{
  ...
  const int fcloseRet = fclose(fp->fp);
  fp->fp = nullptr;

  if (fcloseRet != 0)
    return PushResult::IO_ERROR;                 // <-- temp file left on disk

  ...
    if (fp->provided_hash[0] && memcmp(fp->provided_hash, final_hash_p, 64) != 0)
      return PushResult::WRONG_HASH;             // <-- temp file left on disk
  ...
  make_blob_dirs(filepath);
  return blob_fileio_rename_file_if_nexist(...) ? PushResult::OK : PushResult::IO_ERROR;  // <-- on failure, temp file left on disk
}
```

## Why this is a bug
`finish()` takes ownership of the PushData (`std::unique_ptr<PushData> kill(fp);`) and closes the temp file, setting `fp->fp = nullptr`. From that point, no other code path can clean up the temp file:

- `destroy(PushData*)` (line 171-180) only unlinks when `fp->fp` is still non-null, and finish() has already consumed/deleted the object anyway.
- The successful-dedup path (line 213-216) correctly unlinks; the three failure paths above do not.

So every push that ends in `WRONG_HASH` (client lied about the hash while `allow_trust` is off), `fclose` failure, or a failed rename leaves an orphaned `blob_*` temp file in the blobs tree (or the configured temp dir) forever. In the multithreaded CVS/KV server, a misbehaving or malicious client can repeatedly push bad-hash blobs and fill the server disk with orphaned temp files (each up to the full blob size). WRONG_HASH is precisely the path used to reject forged data, so this is remotely triggerable when trust is disabled.

## Suggested fix
Unlink on all terminal error paths, e.g. add `blob_fileio_unlink_file(fp->temp_file_name.c_str());` before the `IO_ERROR` returns and the `WRONG_HASH` return (and after a failed `blob_fileio_rename_file_if_nexist`, which on failure due to concurrent push should also unlink and may then be reported as DEDUPLICATED).
