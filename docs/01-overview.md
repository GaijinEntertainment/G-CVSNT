# G-CVSNT — Overview

G-CVSNT is Gaijin Entertainment's fork of [CVSNT](http://www.cvsnt.org/) 2.5.05, re-versioned as
**CVSNT 3.5.x**. It keeps the CVS command set, the CVS client/server protocol and the RCS (`,v`)
repository format, but replaces the way *large binary files* are stored and transferred.

The fork exists because classic CVS/CVSNT is a poor fit for game development:

| Classic CVS behaviour | Why it hurts a game repository |
| --- | --- |
| Every revision of a binary file is appended to the `,v` file as a full copy | A 200 MB texture committed 50 times produces a ~10 GB `,v` file |
| The whole `,v` file must be rewritten to add a tag or a branch | Tagging becomes an O(total repository bytes) operation |
| Identical files stored in different paths are stored twice | Art pipelines produce enormous amounts of duplicate data |
| File content travels inline in the client/server protocol stream | One slow connection stalls the whole update; no CDN/proxy caching possible |

## The core idea: `-kB` and content-addressed blobs

G-CVSNT adds a keyword-expansion mode **`-kB`** ("binary delta"), and its compressed variant
**`-kBz`**. For a file registered with `-kB`:

* The file *content* is stored once, by hash, in a **content-addressed file store** (CAFS) that lives
  beside the repository — not inside the `,v` file.
* The `,v` file stores only a fixed-size **blob reference** (`blake3:<64 hex chars>`) per revision.
* Because the store is keyed by content hash, identical content anywhere in the repository — across
  paths, branches, and revisions — occupies exactly one blob.
* Blobs are transferred over a **separate, dedicated TCP protocol** (`blob_push`), not inline in the
  CVS stream. That connection can be pointed at a read-only caching **proxy** near the client.

The result: `,v` files stay small and roughly constant in size, tag/branch operations touch only small
headers, and binary payload moves over a channel that can be cached, parallelised and proxied.

## What is *not* changed

* The CVS command surface (`checkout`, `update`, `commit`, `tag`, `rtag`, `diff`, `log`, …).
* The RCS `,v` on-disk format itself — a G-CVSNT repository is still a CVS repository. Text files
  (`-kkv`, `-kb`, …) are stored exactly as before, as RCS deltas.
* The CVS client/server wire protocol — G-CVSNT only *adds* requests and responses, so an old client
  can still talk to a new server for text-only work.
* ACLs, triggers, `CVSROOT/` administrative files, the lock server, and the Windows service /
  control-panel integration inherited from CVSNT.

## Components shipped by this fork

| Binary | Role |
| --- | --- |
| `cvs` / `cvs.exe` | The client, and (with `SERVER_SUPPORT`) the server-side command processor |
| `cvslockd` | Lock server (advisory locking across concurrent server processes) |
| `cafs_server` | Content-addressed blob store server — the authoritative blob storage |
| `cafs_proxy_server` | Read-through caching proxy for `cafs_server`, deployable near clients |
| `cvtblob` | Converts existing `,v` files with inline binary revisions into blob references |
| `gc-blobs` | Garbage-collects blobs no longer referenced by any `,v` file |
| `repack-blobs` | Recompresses blobs with the best available compressor |
| `blake3-calc` | Prints the BLAKE3 hash (the blob key) of a file |

## Version and identity

* Product version comes from `cvsnt/cvsnt-2.5.05.3744/version_no.h`
  (`CVSNT_PRODUCT_MAJOR/MINOR/PATCHLEVEL`) plus `build.h` (`CVSNT_PRODUCT_BUILD`).
* The product name string is `" (Gan + [Gaijin -kB/-kBz patch])"`, so `cvs --version` prints e.g.
  `Concurrent Versions System (CVSNT) 3.5.24 (Gan + [Gaijin -kB/-kBz patch]) Build 7699`.
* The source directory is still named `cvsnt-2.5.05.3744` for historical reasons; it does **not**
  reflect the shipped version.

## Where to go next

* [02-architecture.md](02-architecture.md) — process and module layout
* [03-content-addressed-storage.md](03-content-addressed-storage.md) — the blob store in detail
* [04-protocols.md](04-protocols.md) — CVS protocol extensions and the blob protocol
* [05-repository-layout.md](05-repository-layout.md) — what is on disk, server and client side
* [06-server-operations.md](06-server-operations.md) — running master, proxy, lock server, GC
* [07-client-usage.md](07-client-usage.md) — commands and options that matter in this fork
* [08-source-map.md](08-source-map.md) — directory-by-directory guide to the source tree
* [../HOWTOBUILD.md](../HOWTOBUILD.md) — building on Windows, Linux and macOS
