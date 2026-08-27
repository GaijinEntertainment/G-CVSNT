# start_push_server accept loop tests the should_stop pointer instead of *should_stop

- **File:** cvsnt/cvsnt-2.5.05.3744/keyValueServer/serverLib/blob_push_server.cpp
- **Line(s):** 57
- **Severity:** medium
- **Confidence:** high
- **Category:** logic

## Code
```cpp
bool start_push_server(int portno, int max_connections, volatile bool* should_stop, ...)
{
  ...
  while (!should_stop)                 // tests the POINTER, not *should_stop
  {
    intptr_t client_raw_sock = accept(sockfd, ...);
    ...
  }
  raw_close_socket(sockfd);
  return true;
}
```

## Why this is a bug
`should_stop` is a `volatile bool*`. The accept loop condition `!should_stop` evaluates whether the pointer is null, never dereferencing it, so the intended graceful-shutdown flag `*should_stop` is completely ignored. The correct idiom is used elsewhere in the same subsystem — `blob_push_proc.cpp:421` writes `while (!(should_stop && *should_stop))`.

Consequences:
- All current callers pass `nullptr` (cafs_server.cpp:55, sample_server.cpp:33, cafs_proxy_server.cpp:66), so `!should_stop` is `true` and the server runs as an unstoppable loop — the stop flag mechanism is dead.
- If any caller ever passes a **non-null** `should_stop` (the whole point of the parameter, e.g. an embedded/hosted server wanting clean shutdown), `!should_stop` is `false` immediately and the loop body never runs: the server closes its listen socket and returns without ever accepting a single connection. That is the opposite of the intended "run until asked to stop" behavior.

## Suggested fix
```cpp
while (!(should_stop && *should_stop))
{
  ...
}
```
