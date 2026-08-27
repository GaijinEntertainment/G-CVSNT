---
id: BUG-blob-13
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/keyValueServer/serverLib/blob_push_server.cpp
line: 57
severity: medium
category: typo
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# `start_push_server()` tests the `should_stop` *pointer* instead of the flag — passing a real flag makes the server accept nothing

## Summary
The accept loop is `while (!should_stop)` where `should_stop` is a `volatile bool*`. It tests
whether the pointer is null, not whether the flag is set. With `nullptr` (what both shipped mains
pass) the loop is infinite and shutdown is impossible; with any non-null pointer the loop body never
executes and the server closes its listening socket and returns immediately.

## Code
```cpp
// keyValueServer/serverLib/blob_push_server.cpp:17, 55-59, 93-94
bool start_push_server(int portno, int max_connections, volatile bool* should_stop, const char *encryption_secret, CafsServerEncryption encryption)
{
  ...
  listen(sockfd, max_connections);
  struct sockaddr_in client; socklen_t clientSz = sizeof(client);
  while (!should_stop)                       // 57 <-- BUG: null-pointer test, not flag test
  {
    intptr_t client_raw_sock = (client_raw_sock = accept(sockfd, (struct sockaddr *)&client, (socklen_t*)&clientSz));
    ...
  }
  raw_close_socket(sockfd);
  return true;
}
```
The same file's own early-out gets it right (`:19`, `if (should_stop && *should_stop)`), and so does
the per-connection loop in the sibling translation unit:
```cpp
// keyValueServer/serverLib/blob_push_proc.cpp:421
  while (!(should_stop && *should_stop))//command is processed
```

## Why it is a bug
`should_stop` is a pointer parameter (`blob_server.h` declares
`bool start_push_server(int port, int max_pending_connections, volatile bool *should_stop, const char *encryption_secret, CafsServerEncryption encryption);`).
`!should_stop` is a null check. The intended expression is `!(should_stop && *should_stop)` — the
form used everywhere else — so that a null pointer means "run forever" and a non-null pointer means
"run until the flag is set".

That the author meant to wire this up is visible in `cafs_server.cpp`, which declares the flag and
then never uses it:
```cpp
// keyValueServer/server/cafs_server.cpp:51, 55
  volatile bool shouldStop = false;
  ...
  const bool result = start_push_server(port, max_pending, nullptr, encryption_secret, encryption);
```

## Failure scenario
Two concrete consequences:

1. **Any embedder that passes a flag gets a dead server.** `keyValueServer` is packaged as a
   library with a public `include/blob_server.h`; the natural use is
   `volatile bool stop = false; std::thread t([&]{ start_push_server(2403, 1024, &stop, secret, enc); });`
   With `&stop` non-null, `!should_stop` is false on the very first evaluation. The function skips
   `accept()` entirely, runs `raw_close_socket(sockfd)` and returns `true`, so the caller sees
   "server quit normally" and the port is never served. There is no diagnostic.
2. **The shipped servers cannot be shut down cleanly.** With `nullptr` the loop is infinite and the
   only exit is an `accept()` error other than `EAGAIN` (`:85-89`). `cafs_server`'s `shouldStop`,
   `close_gc()` and `blob_close_sockets()` after the call are therefore unreachable in normal
   operation; the process must be killed.

## Suggested fix
```cpp
  while (!(should_stop && *should_stop))
```
and pass `&shouldStop` from `cafs_server.cpp:55` / `cafs_proxy_server.cpp:66` so the flag becomes
usable.

## Refutation attempt
I checked whether `should_stop` could be a reference or a `bool` in some overload — there is one
declaration (`keyValueServer/include/blob_server.h`) and one definition, both taking
`volatile bool*`. I checked whether some other loop guards the accept path — there is none; line 57
is the only loop condition. I confirmed both shipped entry points pass `nullptr`
(`cafs_server.cpp:55`, `cafs_proxy_server.cpp:66`), so the "dead server" half of the bug is latent
for the binaries in this tree but immediate for any library consumer, while the "cannot shut down"
half affects the shipped binaries today. The finding stands.
