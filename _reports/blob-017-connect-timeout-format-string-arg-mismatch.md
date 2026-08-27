# connect_with_timeout log calls have two %d but pass one argument (varargs UB)

- **File:** cvsnt/cvsnt-2.5.05.3744/keyValueServer/clientLib/blob_push_pull_client.cpp
- **Line(s):** 30, 54
- **Severity:** low
- **Confidence:** high
- **Category:** error-handling

## Code
```cpp
if (!raw_set_non_blocking(raw_sock, true))
    blob_logmessage(LOG_ERROR, "ioctlsocket failed with error: %d (%d)\n", blob_get_last_sock_error());
...
if (!raw_set_non_blocking(raw_sock, false))
    blob_logmessage(LOG_ERROR, "ioctlsocket failed with error: %d (%d)\n", raw_get_last_sock_error());
```

## Why this is a bug
The format string has two `%d` conversions but only one integer argument is supplied. `blob_logmessage` forwards to `vsnprintf` (see blob_kv_processor.cpp:169-178), so the second `%d` reads a variadic argument that was never passed — undefined behavior. In practice it prints a garbage integer, but depending on ABI/arg registers it can read an indeterminate value or, in the worst case, fault.

These are on the connect-timeout error path (setting the socket non-blocking failed), so they are reached only under socket errors, but the defect is a real format/argument mismatch.

## Suggested fix
Supply a second argument or drop the extra conversion, e.g.:
```cpp
blob_logmessage(LOG_ERROR, "ioctlsocket failed with error: %d\n", blob_get_last_sock_error());
```
