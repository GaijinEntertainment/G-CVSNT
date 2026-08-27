---
id: BUG-blob-07
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/keyValueServer/server/cafs_server.cpp
line: 45
severity: high
category: logic
verdict: CONFIRMED
fix_size_loc: 2
behavior_change: yes
---

# `cafs_server ... off` never disables hash trust — `set_allow_trust()` is only ever called with `false`, and only when the operator asked for `on`

## Summary
`cafs_server` parses `allow_trust(on|off)` into `bool allow` and prints it, but never passes it to
`caddressed_fs::set_allow_trust()`. The only call site is `if (allow) set_allow_trust(false);`
inside the encryption branch — the inverse of the flag, and reachable only when encryption is
configured. Starting the server with `off` therefore leaves trust at its default `true`, so the
server writes client-supplied bytes to disk under the client-supplied hash without ever computing
the real hash.

## Code
```cpp
// keyValueServer/server/cafs_server.cpp:18-19, 45-46
  bool allow = strcmp(argv[2], "on") == 0;
  printf("Starting content-addressed file server with root=<%s> and %s\n", argv[1], allow ? "trust client" : "don't trust client");
  ...
    if (allow)//stop trusting clients, we are encrypting traffic
      caddressed_fs::set_allow_trust(false);
```
```cpp
// ca_blobs_fs/src/content_addressed_fs.cpp:19
static bool allow_trust = true;       // default
```

## Why it is a bug
`allow` is written, printed, and then used exactly once — with its meaning inverted. The full
truth table:

| `argv[2]` | encryption args | `set_allow_trust` called | effective `allow_trust` | matches CLI? |
|---|---|---|---|---|
| `on`  | no  | never          | `true`  | yes |
| `on`  | yes | `false`        | `false` | no (deliberate override per the comment) |
| `off` | no  | never          | `true`  | **no** |
| `off` | yes | never          | `true`  | **no** |

The two `off` rows are the bug: an operator who explicitly disables trust gets a server that still
trusts, while the banner printed at line 19 says "don't trust client".

With `allow_trust == true` and a client-provided hash, verification is skipped end to end:

```cpp
// ca_blobs_fs/src/content_addressed_fs.cpp:143  (start_push)
  if (!allow_trust || !r->provided_hash[0])
    blake3_hasher_init(&r->hasher);           // hasher not even initialised
// :158  (stream_push)
  if (allow_trust && fp->provided_hash[0])//we trusted cache.
    return true;                              // no decode, no hashing
// :203  (finish)
  if (!allow_trust || !fp->provided_hash[0]) { ...verify... }
  else
    memcpy(final_hash_p, fp->provided_hash, 64);   // store under the CLAIMED hash
```

## Failure scenario
A site runs `cafs_server /srv/cafs off 2403` (no encryption), believing the CLI, and exposes it to
its build network.

1. An attacker opens a protocol-001 session and sends
   `PUSH blake3:<H> size=N` where `<H>` is the blake3 of a *legitimate future* artefact
   (or simply of any file the attacker can predict), followed by N bytes of arbitrary content.
2. `handle_push` (`blob_push_proc.cpp:114`) -> `blob_start_push_data(ctx, htype, hash_hex_str, blob_sz)`
   -> `caddressed_fs::start_push(ctx, hhex)`; `provided_hash` is set from the wire.
3. `stream_push` writes the bytes and returns at line 158 without hashing or decoding them.
4. `finish` takes the `else` branch, copies the *claimed* hash into `final_hash_p`, and renames the
   temp file to `<root>/blobs/xx/yy/<H>`.
5. The store now permanently holds wrong content under `<H>`. Because `finish` uses
   `blob_fileio_rename_file_if_nexist` (`fileio.h:131-139`), the real blob can never replace it —
   the honest push is silently discarded as a duplicate.
6. Every client that later checks out that revision downloads `<H>`, fails its own blake3 check
   (`download_blob_to.cpp:413`), retries 16 times and gives up. The revision is unrecoverable
   without manual filesystem surgery.

## Suggested fix
```cpp
  bool allow = strcmp(argv[2], "on") == 0;
  caddressed_fs::set_allow_trust(allow);
  ...
    if (allow)//stop trusting clients, we are encrypting traffic
      caddressed_fs::set_allow_trust(false);
```

## Refutation attempt
I grepped the whole tree for `set_allow_trust` / `allow_trust`: the only definition is
`content_addressed_fs.cpp:20`, the only default is `true` at line 19, and the only caller is
`cafs_server.cpp:46`. No other translation unit sets it, and `blob_file_lib.cpp:212`
(`blob_start_push_data`) unconditionally forwards the network-supplied `hhex` into `start_push`, so
`provided_hash[0]` is always non-zero on the server. I also checked whether the client-side blake3
check makes the poisoning harmless: it detects the corruption but cannot repair it, because
`rename_file_if_nexist` refuses to overwrite an existing blob — so the effect is a permanent DoS on
that hash rather than silent corruption. The finding stands.
