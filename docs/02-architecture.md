# Architecture

## Process view

```
                        ┌─────────────────────────────────────────┐
   developer workstation│                                         │
                        │   cvs.exe  (client mode)                │
                        │     │                    │              │
                        └─────┼────────────────────┼──────────────┘
                              │ CVS protocol       │ blob_push protocol
                              │ (metadata, text,   │ (binary payload,
                              │  RCS deltas)       │  content-addressed)
                              ▼                    ▼
        ┌──────────────────────────────┐   ┌──────────────────────────┐
        │ inetd/service → protocol lib │   │ cafs_proxy_server        │
        │      ↓                       │   │  (read-through cache,    │
        │ cvs  (server mode)           │   │   near the client)       │
        │      │           │           │   └───────────┬──────────────┘
        │      │           │           │               │ miss → master
        │      ▼           ▼           │               ▼
        │  RCS ,v files   blob refs    │   ┌──────────────────────────┐
        │  (repository)        └───────┼──▶│ cafs_server (master)     │
        │      │                       │   │   <root>/blobs/xx/yy/... │
        │      ▼                       │   └──────────────────────────┘
        │  cvslockd (port 2402)        │
        └──────────────────────────────┘
```

Three independent network services:

| Service | Default port | Purpose |
| --- | --- | --- |
| CVS server | 2401 (`pserver`), or via `ext`/`sserver`/`sspi`/`gserver` | Command processing, metadata, RCS storage |
| `cvslockd` | 2402 | Advisory read/write locks shared between concurrent CVS server processes (`src/lock.cpp:190`) |
| `cafs_server` / `cafs_proxy_server` | 2403 | Content-addressed blob storage (`keyValueServer/server/cafs_server.cpp`) |

The blob channel is deliberately separate. It carries no repository semantics — only
`hash → bytes` — which is what makes a dumb caching proxy possible.

## The single binary

`cvs`/`cvs.exe` is both client and server. `src/main.cpp` dispatches on `argv[1]` through the command
table at `src/main.cpp:159`; the `server`, `authserver`/`pserver` entries put the same binary into
server mode (`src/server.cpp`). Which half of a source file runs is guarded by `server_active` and by
the `SERVER_SUPPORT` / `CLIENT_SUPPORT` compile-time macros — most command files (`update.cpp`,
`commit.cpp`, `tag.cpp`) contain both the client-side argument parsing and the server-side execution.

## Connection methods (protocol plugins)

The transport is a **loadable shared library**, resolved from the `:method:` part of `CVSROOT` by
`CProtocolLibrary::LoadProtocol` (`cvstools/ProtocolLibrary.cpp:168`), called from `parse_cvsroot`
(`src/root.cpp:618`). Each plugin lives in `protocols/` and exports a `protocol_interface`:

| Method | Source | Notes |
| --- | --- | --- |
| `pserver` | `protocols/pserver.cpp` | Cleartext password auth over TCP |
| `sserver` | `protocols/sserver.cpp` | `pserver` inside TLS |
| `sspi` | `protocols/sspi.cpp` | Windows SSPI / NTLM / Kerberos |
| `gserver` | `protocols/gserver.cpp` | GSSAPI |
| `ssh` | `protocols/ssh.cpp` | Built-in SSH client (`plink/`) |
| `ext` | `protocols/ext.cpp` | External `rsh`/`ssh` command |
| `server` | `protocols/server.cpp` | `:server:` — spawn via rsh |
| `fork` | `protocols/fork.cpp` | Local child process, used for testing |
| `enum` | `protocols/enum.cpp` | Answers the `BEGIN ENUM` query with the server name, its supported protocols and its repository list (client side: `cvstools/ServerInfo.cpp:41`) |

Protocol plugins are discovered in the **library** directory, configured at build time with
`--with-protocol-dir` (`configure.in:812`, which sets `cvs_library_dir`) and overridden at run time
with the global `-L` option (`src/main.cpp:1022`). The global `-C` option overrides the *config*
directory, which is a different thing.

## Layered libraries

```
        cvs / cafs_server / cvslockd / triggers
                        │
        ┌───────────────┼────────────────┬─────────────────┐
        ▼               ▼                ▼                 ▼
     cvsapi         cvstools        ca_blobs_fs      keyValueServer
   (C++ support   (config, paths,   (blob store     (blob wire protocol,
    library)       library load)     on disk)        client + server + proxy)
        │                                 │                 │
        └──────────────┬──────────────────┴─────────────────┘
                       ▼
          zlib · zstd · pcre · libxml2 · blake3 · OpenSSL
```

* **`cvsapi/`** — the C++ support layer: `CSocketIO`, `CHttpSocket`, `CSqlConnection` and the
  database back-ends under `cvsapi/db/`, `CCrypt`, `CLibraryAccess` (the `dlopen`/`LoadLibrary`
  wrapper), XML (`XmlTree`/`XmlNode`), codepage conversion (`Codepage`), mDNS (`cvsapi/mdns/`), and
  the platform split under `cvsapi/unix/` and `cvsapi/win32/`.
* **`cvstools/`** — `CGlobalSettings` (registry / config-file settings), `CProtocolLibrary` and
  `CTriggerLibrary` (plugin loading, built on `CLibraryAccess`), `EntriesParser`, `RootSplitter`,
  `Scramble`, and the `protocol_interface` / `trigger_interface` headers.
* **`lib/`** — the GNU portability layer inherited from CVS: `getline`, `getdelim`, `fnmatch`,
  `getopt_long`, `regcomp`, `getaddrinfo`/`getnameinfo`, `timegm`, `waitpid`. Note that `xmalloc`
  lives in `src/subr.cpp:67`, `savecwd` in `src/savecwd.cpp`, and MD5 in `cvsapi/lib/md5.c`.
* **`diff/`, `xdiff/`** — GNU diff, plus the pluggable external/XML diff back-ends.
* **`ca_blobs_fs/`** — the on-disk content-addressed store (see
  [03-content-addressed-storage.md](03-content-addressed-storage.md)).
* **`keyValueServer/`** — the `blob_push` wire protocol: `clientLib/`, `serverLib/`, the
  standalone `server/`, the caching `proxy/`, and the socket layer `blob_sockets/`.

## Request flow — `cvs update` in client/server mode

1. **Client** (`src/update.cpp:165` → `src/client.cpp`) parses options, walks the working copy with
   `start_recursion` (`src/recurse.cpp`), and for each directory sends `Directory`, `Entry`,
   and `Modified`/`Unchanged`/`Is-modified` requests, then `update`.
2. **Server** (`src/server.cpp`, request table at `src/server.cpp:4908`) reconstructs a temporary
   working directory from those requests, then runs the same `update` code with `server_active`
   set.
3. For each file the server compares the client's revision against the repository
   (`src/classify.cpp`, `src/vers_ts.cpp`) and emits a response
   (`Updated`, `Merged`, `Patched`, `Rcs-diff`, `Removed`, …; table at `src/client.cpp:4022`).
4. For a `-kB` file the server emits **`Blob-ref`** instead of file content — the 71-byte reference
   only. The client then fetches the actual bytes over the blob channel
   (`src/download_blob_to.cpp`), optionally with several worker threads (`cvs -j N`).
5. **Client** writes the file, updates `CVS/Entries`, and moves on.

Step 4 is the pivot of the whole design: the CVS connection never carries large payloads, so it stays
responsive and its cost is proportional to the *number* of files, not their size.

## Commit flow for a `-kB` file

1. Client hashes the working file with BLAKE3, compresses it, and **pushes the blob first**
   (`src/blob_kv_processor.cpp` / `src/blob_http_processor.cpp`). If the store already has that hash,
   the push is deduplicated server-side and costs one round trip.
2. Client sends `Blob-ref-transfer` with the reference in place of file content.
3. Server checks in a new revision whose *text* is the 71-byte reference. The shrink from the
   79-byte server-side session marker down to the 71-byte reference happens in
   `RCS_write_binary_rev_data` (`src/rcs_cvt_kB.cpp:91`).

Because the `,v` file grows by ~100 bytes per revision regardless of file size, tag, branch and
`rlog` costs stop tracking payload size.

## Triggers

Server-side extension points are shared libraries implementing `trigger_interface`
(`cvstools/trigger_interface.h`), loaded through `CTriggerLibrary::LoadTrigger`
(`cvstools/TriggerLibrary.cpp:419`). Shipped triggers: `info_trigger`
(the classic `loginfo`/`commitinfo`/`taginfo`/`historyinfo` files), `script_trigger`,
`audit_trigger` (SQL audit log), `email_trigger`, `checkout_trigger`. See
[06-server-operations.md](06-server-operations.md).
