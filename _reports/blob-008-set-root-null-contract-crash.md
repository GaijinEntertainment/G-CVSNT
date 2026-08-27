# set_root() dereferences its root argument despite documented nullptr contract

- **File:** cvsnt/cvsnt-2.5.05.3744/ca_blobs_fs/src/content_addressed_fs.cpp
- **Line(s):** 39-50 (esp. 43, 45); contract in content_addressed_fs.h:21
- **Severity:** low
- **Confidence:** high
- **Category:** error-handling

## Code
```cpp
void set_root(context *ctx, const char *p)
{
  ctx->root_path = dir_for_roots;
  if (ctx->root_path.length() && ctx->root_path[ctx->root_path.length()-1] != '/' && p[0] != '/')  // p[0] deref
    ctx->root_path += "/";
  ctx->root_path += p;   // std::string += (const char*)nullptr is UB
  ...
}
```
Header contract:
```cpp
void set_root(context *ctx, const char*);//if passed root is nullptr, we wil use default root
```

## Why this is a bug
`content_addressed_fs.h:21` documents that passing `nullptr` for the root is valid and means "use the default root". The implementation never checks `p` for null: it reads `p[0]` (line 43) and appends `p` to a `std::string` (line 45). Passing `nullptr` therefore dereferences a null pointer / invokes `std::string::operator+=(const char*)` with null — undefined behavior, in practice a crash.

Reachability: `blob_create_ctx(rootBuf)` (blob_file_lib.cpp:8) and `server.cpp:5368` pass `current_parsed_root->directory`; if any of these ever yields null (e.g. an unparsed/removed root) the server crashes. Even absent a current null-passing caller, the exported API's stated contract is simply not honored.

## Suggested fix
```cpp
void set_root(context *ctx, const char *p)
{
  if (!p)               // honor documented "nullptr -> default root"
    p = "";             // or restore the previous/default root explicitly
  ...
}
```
Alternatively, correct the header comment to state that a valid non-null path is required.
