# base_repo trailing-slash check indexes the NUL terminator (always true, yields "//")

- **File:** cvsnt/cvsnt-2.5.05.3744/src/download_blob_to.cpp
- **Line(s):** 248-251
- **Severity:** low
- **Confidence:** high
- **Category:** logic

## Code
```cpp
base_repo = repo;
if (base_repo[0] != '/')
  base_repo = "/" + base_repo;
if (base_repo[base_repo.length()] != '/')   // <-- indexes size(), i.e. the '\0'
  base_repo += "/";
```

## Why this is a bug
`base_repo[base_repo.length()]` reads the character at index `size()`, which for `std::string` is the terminating `'\0'` (defined, but never `'/'`). The condition is therefore **always true**, so the block always appends `'/'`. The clearly-intended check is on the last character, `base_repo[base_repo.length()-1]`, exactly as the analogous code does in content_addressed_fs.cpp:46 (`root_path[root_path.length()-1] != '/'`) and proxy_file_lib.cpp:46 (`cache_folder[cache_folder.length() - 1] != '/'`).

Consequence: when the configured repo already ends in `/` (or is empty, becoming `"/"` after the first `if`), a second slash is appended, producing `"//"`. `base_repo` is returned by `getRoot()` and sent to the blob KV server as the CVS root, which the server turns verbatim into the blob storage path (`set_root` does not collapse `//`). A root of `/repo//` maps to a different directory string than `/repo/`, so the client can look for blobs under the wrong root and fail to find them.

## Suggested fix
```cpp
if (base_repo[base_repo.length()-1] != '/')
  base_repo += "/";
```
(guard against an empty string if that is possible).
