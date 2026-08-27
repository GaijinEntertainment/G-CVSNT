# Source map

Everything lives under `cvsnt/cvsnt-2.5.05.3744/`. The directory name is historical — it does not
match the shipped version (3.5.x).

Paths below are relative to that directory unless stated otherwise.

## Top-level layout of the repository

| Path | Contents |
| --- | --- |
| `cvsnt/build-linux-server`, `build-macosx`, `build-rhel6`, `build-altlinux` | Platform build drivers |
| `cvsnt/install-yum-packages` | RHEL/CentOS dependency list |
| `cvsnt/*.wxs`, `cvsnt/make_msi*.bat`, `cvsnt/msi_tools/` | WiX installer definitions for Windows |
| `cvsnt/*.patch` | Patches against stock CVSNT kept for reference |
| `cvsnt/tortoiseCVS/` | TortoiseCVS integration |
| `cvsnt/cvsnt-2.5.05.3744/` | The actual source tree |

## The source tree

### Core command implementation — `src/` (~85 kLoC)

The heart of CVS. Each command has one file; most contain both the client half (argument parsing,
sending requests) and the server half (executing against the repository), separated by
`server_active` checks and `#ifdef SERVER_SUPPORT` / `CLIENT_SUPPORT`.

| File | Lines | Role |
| --- | ---: | --- |
| `rcs.cpp` | 7615 | RCS `,v` parsing, revision extraction, symbol/branch manipulation, rewriting. The most delicate file in the tree |
| `server.cpp` | 7227 | Server request loop, request table (`:4908`), response emission |
| `client.cpp` | 6261 | Client request emission, response table (`:4022`), response handlers |
| `update.cpp` | 3480 | `update`: classification, checkout, merge, join |
| `commit.cpp` | 2516 | `commit` |
| `rcs_checkin.cpp` | 2236 | Writing a new revision into a `,v` file |
| `import.cpp` | 1960 | `import` |
| `log.cpp` | 1884 | `log` / `rlog` |
| `main.cpp` | 1873 | Entry point, global options (`:736`), command table (`:159`) |
| `buffer.cpp` | 1790 | The buffered-I/O abstraction the whole protocol sits on |
| `mapping.cpp` | 1636 | Path/name mapping and virtual repositories |
| `tag.cpp` | 1611 | `tag` / `rtag`, including branch creation |
| `subr.cpp` | 1571 | Assorted helpers |
| `edit.cpp` | 1546 | `edit`/`unedit`/`watch` |
| `recurse.cpp` | 1527 | `start_recursion` — the directory walker every command uses |
| `mkmodules.cpp` | 1456 | `CVSROOT` administrative file handling |
| `entries.cpp` | 1429 | `CVS/Entries` reading and writing |
| `history.cpp` | 1410 | `CVSROOT/history` |
| `checkout.cpp` | 1388 | `checkout` / `export` |
| `filesubr.cpp` | 1320 | POSIX filesystem helpers (Windows equivalents are in `windows-NT/`) |
| `lock.cpp` | 1316 | Repository locking, both file-based and lock-server |
| `root.cpp` | 1176 | `CVSROOT` string parsing, protocol plugin selection |
| `modules.cpp` | | Module database |
| `classify.cpp`, `vers_ts.cpp` | | Deciding a file's state (up-to-date / modified / needs merge / …) |
| `hash.cpp` | 521 | The `List`/`Node` container used pervasively for entries, files, directories |
| `httplib.h` | 6707 | Vendored single-header HTTP client, used by the HTTP blob back-end |

#### Gaijin-specific files in `src/`

| File | Role |
| --- | --- |
| `sha_blob_reference.h` | Blob reference format constants and predicates |
| `blob_operations.cpp` | Reference encode/decode, session MAC |
| `blob_network_processor.h` | `BlobNetworkProcessor` / `UrlProvider` interfaces |
| `blob_kv_processor.cpp` | Native `blob_push` transport |
| `blob_http_processor.cpp` | HTTP transport |
| `download_blob_to.cpp` | Client download orchestration, worker threads, URL failover |
| `rcs_cvt_kB.cpp` | `-kB` conversion helpers |
| `concurrent_queue.h` | Work queue for parallel downloads |
| `zstd_buffer.cpp` | zstd stream framing for the CVS connection |
| `sha256/` | SHA-256 (legacy; BLAKE3 is the current hash) |

### The blob store — `ca_blobs_fs/` (~1.5 kLoC)

Small and self-contained. Read it in full before touching anything blob-related.

| File | Role |
| --- | --- |
| `content_addressed_fs.h` | Public API: `start_push`/`stream_push`/`finish`, `start_pull`/`pull`, `exists`, `get_size` |
| `src/content_addressed_fs.cpp` | Implementation: path layout, temp-file-then-rename, dedup |
| `ca_blob_format.h` | The 16-byte blob header and magic values |
| `src/fileio.cpp`, `src/fileio.h` | Filesystem primitives, memory-mapped pull |
| `streaming_compressors.h`, `src/streaming_compressors.cpp` | zlib/zstd abstraction |
| `calc_hash.h`, `src/calc_hash.cpp` | Hex/binary hash conversion |
| `streaming_blobs.h`, `push_whole_blob.h` | Higher-level streaming helpers |

### The blob network layer — `keyValueServer/` (~4 kLoC)

| Path | Role |
| --- | --- |
| `include/blob_push_protocol.h` | **The protocol specification.** Read this first |
| `include/blob_raw_sockets.h`, `blob_common_net.h`, `blob_hash_util.h` | Shared helpers |
| `include/blobs_encryption.h` | Encryption/handshake types |
| `blob_sockets/blob_sockets.cpp` | Portable socket layer |
| `clientLib/` | Client side: `blob_push_pull_client.cpp` plus one file per command |
| `serverLib/` | Server side: `blob_push_proc.cpp` (command dispatch), `blob_push_server.cpp` (accept loop) |
| `server/` | The `cafs_server` executable and its file back-end |
| `proxy/` | The `cafs_proxy_server` executable, cache eviction, disk-space monitoring |
| `sample/`, `sampleImplementation/` | Example client/server and logging stubs |

### Support libraries

| Path | Role |
| --- | --- |
| `cvsapi/` (~26 kLoC) | C++ support library: sockets, HTTP, TLS, `CLibraryAccess` (dynamic loading), XML, codepage conversion, `db/` back-ends (MySQL, PostgreSQL, SQLite, ODBC, Oracle, MSSQL, DB2), `mdns/`, `unix/` and `win32/` platform layers |
| `cvstools/` (~6 kLoC) | `CGlobalSettings`, `CProtocolLibrary`, `CTriggerLibrary`, `EntriesParser`, `RootSplitter`, `Scramble`, `ServerConnection`, `ServerInfo`, plugin-interface headers |
| `lib/` (~11 kLoC) | GNU portability layer: `getline`, `getdelim`, `fnmatch`, `getopt_long`, `regcomp`, `getaddrinfo`/`getnameinfo`, `timegm`, `waitpid`. (`xmalloc` is `src/subr.cpp:67`, `savecwd` is `src/savecwd.cpp`, MD5 is `cvsapi/lib/md5.c`) |
| `diff/` (~8.5 kLoC) | GNU diff |
| `xdiff/` | Pluggable external and XML diff back-ends |
| `windows-NT/` | Windows implementations of `filesubr`, `mkdir`, `pwd`, `setuid`, `waitpid`, plus `cvsdiag`, `gss-ad`, `setuid` helpers |
| `osx/` | macOS packaging and build scripts |

### Vendored third-party code

| Path | What |
| --- | --- |
| `blake3/` | BLAKE3 reference implementation with SIMD kernels |
| `zlib/`, `zstd/` | Compression libraries |
| `pcre/` | Perl-compatible regex |
| `libxml/` | libxml2 |
| `plink/` | PuTTY's plink, for the built-in `ssh` method |
| `ufc-crypt/` | `crypt()` implementation |
| `external_libs/` | Prebuilt OpenSSL / iconv / dnssd import libraries and headers for Windows |
| `tools/tsl/` | Sparse hash map/set used by the maintenance tools |

### Plugins and executables

| Path | Produces |
| --- | --- |
| `protocols/` | One shared library per connection method |
| `triggers/` | One shared library per server-side hook family |
| `lockservice/` | `cvslockd` |
| `cvsservice/`, `cvsntcpl/`, `control-panel/` | Windows NT service and control-panel applet |
| `cvsagent/` | Credential agent |
| `cvsgui/` | GUI integration protocol (used by WinCVS/TortoiseCVS) |
| `cvsdelta/`, `rcs/`, `rcs_convert.vcxproj` | Auxiliary RCS tools (`co`, `rlog`, `rcsdiff`) |
| `mdnsclient/`, `extnt/`, `su/`, `genkey/`, `genbuild/`, `postinst/`, `uninsthlp/` | Small helper executables |
| `tools/` | `cvtblob`, `gc-blobs`, `repack-blobs`, `blake3-calc`. (`simplelock.cpp`, `unlock.cpp` and `sha256_calc.cpp` are present but appear in no build file) |

### Build system

| Path | Role |
| --- | --- |
| `configure.in`, `acinclude.m4`, `Makefile.am` (per directory) | Autotools, used on Linux and macOS |
| `cvsnt.sln`, `*.vcxproj` | Visual Studio solution, used on Windows |
| `debian/`, `redhat/` | Distribution packaging |
| `tools/build_tools` | Standalone clang++ build for the maintenance tools |

## Reading order for a newcomer

1. `src/main.cpp` — the command table and global options; how a command is dispatched.
2. `src/recurse.cpp` — `start_recursion`, the walker every command is built on.
3. `src/update.cpp` — the most-used command, and the clearest example of the client/server split.
4. `src/server.cpp:4908` and `src/client.cpp:4022` — the two protocol tables side by side.
5. `keyValueServer/include/blob_push_protocol.h` — the blob protocol, in one file.
6. `ca_blobs_fs/content_addressed_fs.h` and `ca_blobs_fs/src/content_addressed_fs.cpp` — the store.
7. `src/rcs.cpp` — last, and carefully. Everything about repository integrity lives here.

## Dead code

Three files are present in the tree but are in **no build file** and are included by nothing except
each other:

* `src/Modules1.cpp` / `Modules1.h`
* `src/Modules2.cpp` / `Modules2.h`
* `src/RecurseRepository.cpp` / `RecurseRepository.h`

They appear in neither `src/Makefile.am` nor `cvsnt.vcxproj`. Do not use them as a reference for how
anything currently works, and do not "fix" bugs in them expecting an effect.

## Conventions to expect

* C-style C++: raw pointers, `xmalloc`/`xfree`, `char*` strings, `List`/`Node` from `src/hash.cpp`.
  `xmalloc` never returns NULL — it calls `error(1, ...)` on failure — so unchecked allocations are
  intentional, not bugs.
* `error(status, errno_value, format, ...)` is the universal error path; a non-zero `status` exits.
* Functions passed to `start_recursion` follow fixed signatures: `fileproc`, `filesdoneproc`,
  `direntproc`, `dirleaveproc`.
* Windows and POSIX diverge through `windows-NT/` and `lib/`, not through inline `#ifdef`s, in most
  places. `cvsapi/` splits by `unix/` vs `win32/` subdirectories.
* Indentation is inconsistent (tabs and spaces mixed, several eras of style). Match the surrounding
  block rather than the file.
