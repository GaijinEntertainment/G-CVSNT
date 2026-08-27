# Server operations

## The three services

A complete G-CVSNT deployment runs up to three server processes. Only the first is mandatory.

### 1. The CVS server

The same `cvs` binary, invoked as `cvs server` (over `ext`/`ssh`) or `cvs authserver` /
`cvs pserver` (direct TCP). Traditionally started from `inetd`/`xinetd` (a sample is in
`cvsnt/cvsnt.xinetd`) or, on Windows, by the `cvsservice` NT service.

Default port for `pserver` is 2401.

### 2. `cvslockd` — the lock server

`cvslockd` (sources in `lockservice/`) keeps read/write locks in memory instead of as files in the
repository. Enable it by putting a `LockServer` line in `CVSROOT/config`:

```
LockServer=localhost:2402
```

Without it, CVS falls back to `#cvs.lock` / `#cvs.rfl.*` / `#cvs.wfl.*` files created and removed in
every repository directory it touches (`src/lock.cpp:684`). On a repository with tens of thousands
of directories that is a very large amount of filesystem churn, so the lock server is strongly
recommended at this scale. The wire protocol is documented in
`lockservice/cvslock_protocol.txt`.

### 3. `cafs_server` — the blob store

```
cafs_server <dir_for_roots> <allow_trust: on|off> [norepack]
            [encryption|mandatory_encryption <secret>]
            [port] [max_pending_connections]
```

(`keyValueServer/server/cafs_server.cpp:12`)

| Argument | Meaning |
| --- | --- |
| `dir_for_roots` | Parent directory of all repository roots; `/` means "roots are absolute paths" |
| `allow_trust` | `on` accepts client-supplied hashes without re-hashing; `off` always verifies |
| `norepack` | Do not recompress blobs on arrival |
| `encryption` | Offer encryption; clients may still opt out |
| `mandatory_encryption` | Refuse unencrypted clients. Also forces `allow_trust` off |
| `<secret>` | Shared secret, at least `minimum_shared_secret_length` characters |
| `port` | Default 2403 |
| `max_pending` | Listen backlog, default 1024 |

Blobs land in `<dir_for_roots>/<root>/blobs/xx/yy/<hash>`.

### 4. `cafs_proxy_server` — the caching proxy

```
cafs_proxy_server <master_url> <cache_folder>
                  [validate_blobs_from_master] [update_mtime_on_access]
                  [encryption|mandatory_encryption <secret>]
                  [cache_soft_limit_size_mb]
```

(`keyValueServer/proxy/cafs_proxy_server.cpp:17`)

A read-through cache. On a miss it pulls from the master and stores locally; on a hit it serves from
its own store. Because blobs are immutable there is no invalidation problem — the cache is
correct by construction.

* `validate_blobs_from_master` re-hashes what the master sends before caching it.
* `update_mtime_on_access` makes the cache LRU-evictable by mtime.
* The default soft cache limit is 102400 MB (100 GB); eviction is driven by
  `keyValueServer/proxy/gc_thread_monitor.cpp` and `keyValueServer/proxy/free_disk_space.cpp`.

Deploy one proxy per studio/office/build farm and point clients at it. Latency and WAN bandwidth for
binary payload drop to near zero for anything another local user has already fetched.

## Telling clients where the blobs are

The CVS server advertises blob URLs to clients using its own global settings (see
`src/server.cpp:3346`):

| Setting (`cvsnt` / `PServer` / *key*) | Meaning |
| --- | --- |
| `BlobURL` | Primary blob URL |
| `BlobURL0` … `BlobURL31` | Additional URLs; the client may round-robin. Enumeration stops at the first missing index |
| `BlobOTP` | Shared secret for TOTP authentication to an encrypted blob server |
| `BlobEncryptedURL0` … `BlobEncryptedURL31` | URLs used instead of the plain ones once `BlobOTP` is configured |

Values are `host[/path][@port]`, e.g. `cvs-proxy.lan@2403`, or `http://cache.lan@8080` for the HTTP
back-end.

Where these settings live:

* **Unix** — a plain `key=value` file at `<sysconfdir>/cvsnt/PServer`
  (`cvstools/unix/GlobalSettings.cpp:91`); `<sysconfdir>` is set by `--with-config_dir` at configure
  time. Per-user settings go in `~/.cvs/<key>`.
* **Windows** — the registry, under the CVSNT product key
  (`cvstools/win32/GlobalSettings.cpp`).

A client can always override with `cvs --blob_url ...`.

## `CVSROOT/config`

Parsed by `src/parseinfo.cpp`. Keys recognised by this build:

| Key | Values | Notes |
| --- | --- | --- |
| `RCSBIN` | path | Legacy; ignored |
| `SystemAuth` | `yes`/`no` | Fall back to system accounts for `pserver` |
| `PreservePermissions` | `yes`/`no` | |
| `TopLevelAdmin` | `yes`/`no` | Create `CVS/` at the top of a checkout |
| `AclMode` | `none`/`compat`/`normal` | CVSNT ACL behaviour |
| `LockDir` | path | Put lock files somewhere other than the repository |
| `LockServer` | `host:port` | Use `cvslockd` instead of lock files |
| `LogHistory` | letters | Which record types to write to `CVSROOT/history` |
| `AtomicCommits` | `yes`/`no` | |
| `RereadLogAfterVerify` | `no`/`never`/`yes`/`always`/`stat` | |
| `Watcher` | | Watch/notify configuration |

## Triggers

Server-side hooks are shared libraries implementing `trigger_interface`
(`cvstools/trigger_interface.h`), listed in `CVSROOT/triggers` and resolved through
`cvsnt`/`Plugins` settings (`cvstools/TriggerLibrary.cpp:448`).

| Trigger | Source | Purpose |
| --- | --- | --- |
| `info` | `triggers/info_trigger.cpp` | Implements the classic file-driven hooks: `loginfo`, `commitinfo`, `taginfo`, `verifymsg`, `historyinfo`, `precommand`, `postcommand`, `premodule`, `postmodule`, `postcommit` |
| `script` | `triggers/script_trigger.cpp` | Runs a scripting-language hook |
| `audit` | `triggers/audit_trigger.cpp` | Writes an audit trail to a SQL database (`triggers/sql/`) |
| `email` | `triggers/email_trigger.cpp` | Commit notification mail |
| `checkout` | `triggers/checkout_trigger.cpp` | Keeps a working copy in sync with commits |

Note that `historyinfo` receives a directory parameter `%p`, and that `taginfo` fires per tag
operation. Triggers that spawn a process are a real cost on large tag operations — see
`_reports/PERF-02-tag-branch-path.md`.

## Maintenance tools

Built by `tools/build_tools` (clang++, C++17). All of them operate directly on the repository
filesystem and take the lock via `tools/simpleLock.cpp.inc`, so they can run against a live server.

### `cvtblob` — migrate existing binaries into the blob store

`tools/convert_to_blob.cpp`. Walks `,v` files, extracts each binary revision's content, pushes it
into the blob store, and rewrites the revision text as a `blake3:` reference. This is the one-time
migration that turns a classic CVSNT repository into a G-CVSNT one.

```
cvtblob -root <cvs_root> -lock_url <lock_server_url> -user <lock_user>
        [-dir <subdir>] [-file <file>] [-j <threads>]
        [-tmp_rcs <dir>] [-tmp_blobs <dir>]
        [-db <assist_db_path>] [-max_files <n>]
```

* `-lock_url offline` declares that nobody else is using the repository, so no lock server is
  needed. Use it only when that is actually true.
* `-max_files` (default 256) bounds how many revisions are held at once, and therefore how much
  scratch space the conversion needs.
* `-db` (default `cvs_cvt_db.txt`) records progress so an interrupted conversion can resume.

### `gc-blobs` — reclaim space

`tools/gc-blobs.cpp`. Nothing reference-counts blobs, so collection is mark-and-sweep:

1. Scan every `,v` file in the repository and collect every `blake3:` reference into a sparse hash
   set (`tools/tsl/sparse_set.h`).
2. Walk `blobs/` and act on every file whose name is not in the set.

```
gc-blobs -root <rootDir> -lock_url <lock_url> -user <lock_user> used|unused|broken|delete_unused
```

The mode is positional and decides what the tool does: `used` and `unused` only *list*, `broken`
reports blobs that fail their own hash check, and `delete_unused` is the only mode that removes
anything. Start with `unused` and read the output before ever running `delete_unused`.

It takes a per-`,v` read lock through the lock server while scanning (`tools/gc-blobs.cpp:102`), so
it can run against a live repository. Do not work around that lock: a blob pushed during the mark
phase and referenced only after the sweep would otherwise be deleted while in use.

### `repack-blobs` — improve compression

`tools/repack-blobs.cpp`. Recompresses blobs with the best available settings and sets the
`BEST_POSSIBLE_COMPRESSION` flag so later runs skip them. Because the hash is of the *uncompressed*
content, repacking never changes a blob's identity. Supports `-j <threads>`; run `repack-blobs -h`
for the full option list.

### `blake3-calc`

`tools/blake3-calc.cpp`. Prints a file's blob key. Useful for answering "is this asset already in
the store?" with a `CHCK`.

## Operational notes for large repositories

* Turn on `LockServer`. Lock-file churn dominates otherwise.
* Put `LockDir` on a fast local filesystem if you cannot use the lock server.
* Deploy a `cafs_proxy_server` at every site. Clients should never pull blobs across a WAN twice.
* Use `cvs -j N` on clients to parallelise blob downloads.
* `cvs update --blob_zero` writes zero-length files instead of real content — intended for
  "hot-proxy" scenarios where a machine only needs to populate a cache, not the files themselves
  (`src/update.cpp:157`).
* Schedule `repack-blobs` off-hours; schedule `gc-blobs` rarely and only when you are confident no
  migration or mass import is in flight.
