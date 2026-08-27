# sample_server.cpp does not compile: redeclared `pc`, undefined `secret`, wrong start_push_server args

- **File:** cvsnt/cvsnt-2.5.05.3744/keyValueServer/sample/sample_server.cpp
- **Line(s):** 9, 18, 33
- **Severity:** low
- **Confidence:** high
- **Category:** logic

## Code
```cpp
int main(int argc, const char **argv)
{
  const int pc = 1;                    // line 9
  ...
  const char *encryption_secret = 0;
  int pc = 1;                          // line 18: redeclaration of pc in the same scope
  ...
  const bool result = start_push_server(port, max_pending, nullptr, secret, encryption_secret);  // line 33
}
```

## Why this is a bug
Three compile errors in this sample:
1. `pc` is declared twice in the same function scope (`const int pc = 1;` at line 9 and `int pc = 1;` at line 18) — a redefinition error.
2. `secret` (line 33) is never declared; the only similar variable is `encryption_secret`.
3. `start_push_server`'s signature is `(int, int, volatile bool*, const char* encryption_secret, CafsServerEncryption encryption)`. The call passes `secret` where the `const char*` secret is expected and passes `encryption_secret` (a `const char*`) where the `CafsServerEncryption` enum is expected — type-mismatched arguments (and the `encryption` value is never computed here at all, unlike cafs_server.cpp).

The file therefore cannot build as-is; it appears to be stale sample code that was never updated to the current `start_push_server` API (compare the working cafs_server.cpp:55). Low impact (demonstration only), but it is broken.

## Suggested fix
Remove the duplicate `const int pc = 1;`, and call e.g.:
```cpp
CafsServerEncryption encryption = encryption_secret ? CafsServerEncryption::All : CafsServerEncryption::Local;
const bool result = start_push_server(port, max_pending, nullptr, encryption_secret, encryption);
```
