# Architecture

All paths below are relative to `cvsnt/cvsnt-2.5.05.3744/`.

## Big picture

```
                ┌───────────────────────────── client machine ─────────────────────────────┐
                │  cvs.exe / cvs                                                           │
                │   ├─ cvsapi.dll   (sockets, codepages, filename mapping, DB drivers)     │
                │   ├─ cvstools.dll (settings, trigger/COM glue)                           │
                │   ├─ protocols\*.dll  :pserver: :sserver: :sspi: :ssh: :ext: :fork: ...  │
                │   └─ keyValueServer clientLib + blob_sockets (static)  ──────────────┐   │
                └────────────┬──────────────────────────────────────────────────────── │ ──┘
                             │ CVS wire protocol (TCP 2401, zlib/zstd compressed)      │ blob push/pull
                             ▼                                                         ▼ (TCP 2403, parallel)
                ┌─────────── server ───────────┐                       ┌────────────────────────────┐
                │ cvsservice / xinetd → cvs    │                       │  blob server (keyValue-    │
                │  server mode ("server" cmd)  │                       │  Server sample / proxy)    │
                │  ├─ RCS ,v files (text +     │                       │  storage: blobs/xx/yy/hash │
                │  │   blob refs for -kB)      │◄── shared repo disk ─►│  (zlib/zstd packed, mmap)  │
                │  ├─ cvslock (lockserver)     │                       └────────────────────────────┘
                │  └─ triggers\*.dll, scripts  │
                └──────────────────────────────┘
```

Everything except binary file *content* moves over the classic CVS
client/server protocol. Content of `-kB` (binary) files is stored and
transferred as **content-addressed blobs**: the RCS `,v` file holds only a
77-byte reference `blake3:<64 hex chars>`, and clients exchange the actual
bytes with the blob server directly (see [blob-storage.md](blob-storage.md)).

## Components

### cvs (`cvsnt.vcxproj` → `cvs.exe`, `src/`)

One binary is both the command-line client and the server (`cvs server`,
launched per-connection by cvsservice on Windows or xinetd/inetd on Unix).
Notable source files:

* `src/main.cpp` — command table, global options (including Gaijin additions:
  `--blob_url <url>` to override the blob server, `-j <n>` blob download
  concurrency, `-F <file>` to read command arguments from a file)
* `src/client.cpp` / `src/server.cpp` — the two halves of the wire protocol.
  Requests are lines like `Directory`, `Entry`, `Modified`, `Unchanged`,
  `Argument`, terminated by a command (`update`, `ci`, `tag` …); responses are
  `Checked-in`, `Updated`, `Mod-time`, `Mode`, plus Gaijin's `Blob-ref`
  (`update_blob_ref`), `Blob-url`, `Blob-OTP` extensions
* `src/update.cpp`, `checkout.cpp`, `commit.cpp`, `tag.cpp`, … — one file per
  command; each runs over the tree via `src/recurse.cpp`
  (`start_recursion`), which calls per-directory and per-file callbacks
* `src/rcs.cpp`, `rcs_checkin.cpp` — RCS `,v` parsing/rewriting (the server's
  storage format)
* `src/vers_ts.cpp`, `entries.cpp` — join of `CVS/Entries` (client state) with
  RCS state (`Version_TS`)
* `src/classify.cpp` — decides per file what update action is needed
* `src/lock.cpp` — repository read/write locks (filesystem lock files or the
  separate `cvslock` lock daemon)
* `src/blob_operations.cpp`, `download_blob_to.cpp`, `blob_kv_processor.cpp`,
  `blob_http_processor.cpp`, `zstd_buffer.cpp`, `sha_blob_reference.h` — the
  blob client machinery (see [blob-storage.md](blob-storage.md))

### cvsapi (`cvsapi/` → `cvsapi.dll` / `libcvsapi`)

Platform abstraction: sockets with SSL, codepage/Unicode translation,
filename mapping, protocol library loading, the `CGlobalSettings`
configuration store (registry on Windows, `/etc/cvsnt/PServer` on Unix),
pluggable database drivers (`cvsapi/db/sqlite`, `odbc`) used by auditing etc.

### cvstools (`cvstools/` → `cvstools.dll`)

Higher-level shared services: trigger library loading (`TriggerLibrary.cpp`,
COM `trigger.idl` on Windows so triggers can be written as COM objects),
server info/mapping helpers.

### Protocols (`protocols/` → `protocols/*.dll` / `*.so`)

Each access method is a plugin implementing `protocol_interface`:
`pserver` (classic password server), `sserver` (SSL), `sspi` (NTLM/Kerberos on
Windows), `ext` (external command), `ssh` (built-in ssh via bundled plink on
Windows), `gserver` (GSSAPI), `fork` (local), `enum` (enumeration helper).
The client picks one from the `CVSROOT` string (`:pserver:user@host:/cvs`).

### Server-side plugins (`triggers/` → `triggers/*.dll`)

Pre/post command hooks (`info_triggers` implements the classic
`CVSROOT/loginfo`, `commitinfo`, … scripts; `audit` writes to a database;
`email` sends notifications; `script` runs user scripts / COM on Windows).
Interface: `cvstools/TriggerLibrary` / `triggers/server.idl`.

### Services (Windows)

* `cvsservice/` → `cvsservice.exe` — the NT service that listens on port 2401
  (and 2402 for lockserver) and spawns `cvs.exe server` per connection
* `lockservice/` → `cvslock.exe` — the lock daemon (`cvslockd` on Unix);
  advisory read/write locks per repository path so concurrent commands don't
  need lock files on disk
* `control-panel`, `cvsntcpl/` — configuration UI

### Repository format

A repository is a directory tree of RCS `,v` files plus the administrative
module `CVSROOT/` (config, `passwd`, trigger scripts, `history`, `val-tags`).
G-CVSNT adds `blobs/` (content-addressed store, see
[blob-storage.md](blob-storage.md)) and keeps everything else stock, so
standard CVSNT tooling still understands the text parts of the repository.

## Command flow example: `cvs update`

1. Client walks the working copy (`src/client.cpp`: `send_files`), sending for
   every directory a `Directory` request and for every file `Entry` +
   (`Unchanged` | `Modified` | `Is-modified`) based on `CVS/Entries`
   timestamps.
2. Server (`src/server.cpp`: `serve_*`) reconstructs the state, takes read
   locks, and runs the same `update()` code a local CVS would
   (`src/update.cpp` with `server_active`), using `src/classify.cpp` per file.
3. For text files needing update it sends `Updated`/`Merged`/`Patched` +
   contents (zlib/zstd-compressed); for `-kB` files it sends the blob
   reference (`Blob-ref`) instead of the content.
4. Client applies responses, rewrites `CVS/Entries`, and enqueues blob
   downloads to a background thread pool (`src/download_blob_to.cpp`, up to 8
   threads, round-robin over blob proxy URLs) which fetches missing blobs
   directly from the blob server and writes the working files.
5. `wait_threads()` joins the pool before cvs exits.
