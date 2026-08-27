# Repository and working-copy layout

## Server side

```
/cvs/                                  <- CVSROOT (the "root")
├── CVSROOT/                           <- administrative directory
│   ├── config            server configuration (LockDir, LockServer, ...)
│   ├── modules           module definitions
│   ├── modules2          CVSNT extended module definitions
│   ├── passwd            pserver accounts
│   ├── users             mail aliases
│   ├── readers, writers  coarse access control
│   ├── group             group definitions for ACLs
│   ├── loginfo           post-commit log trigger
│   ├── commitinfo        pre-commit trigger
│   ├── verifymsg         log message validation
│   ├── taginfo           tag/rtag trigger
│   ├── historyinfo       history record trigger
│   ├── precommand, postcommand, premodule, postmodule, postcommit
│   ├── rcsinfo           log message template
│   ├── cvswrappers       per-pattern -k defaults
│   ├── cvsignore         server-wide ignore patterns
│   ├── keywords          custom keyword definitions
│   ├── triggers          trigger plugin list
│   ├── checkoutlist      extra admin files to keep checked out
│   ├── notify            watch notification trigger
│   ├── history           the history log
│   └── val-tags          cache of known-valid tags
│
├── blobs/                             <- content-addressed store (CAFS)
│   ├── 00/
│   │   ├── 00/
│   │   └── ...
│   ├── 3f/
│   │   └── a9/
│   │       └── 3fa91c2e...            <- one blob, 64-hex filename
│   └── ff/
│
└── mymodule/                          <- an actual module
    ├── CVS/                           <- per-directory repository metadata
    │   └── fileattr.xml               watchers, ACLs, edit state (legacy name: fileattr)
    ├── src/
    │   ├── main.cpp,v                 RCS file (text, normal deltas)
    │   └── Attic/                     ,v files whose head trunk revision is dead
    └── assets/
        └── tex.dds,v                  RCS file whose revisions are blob references
```

The `CVSROOT/` names are the `CVSROOTADM_*` constants at `src/cvs.h:179`, and the per-directory
`CVS/` name is `CVSREP` at `src/cvs.h:170`. The others come from elsewhere: `Attic` is `CVSATTIC`
(`src/cvs.h:217`), `blobs` is `BLOBS_SUB_FOLDER`
(`ca_blobs_fs/src/content_addressed_fs.cpp:16`), and `fileattr.xml` is `CVSREP_FILEATTR`
(`src/fileattr.h:41`).

### A `,v` file for a `-kB` file

An ordinary CVS `,v` file stores each revision's text (or a delta) in a `@...@` block. For a `-kB`
file the block holds only the 71-byte reference:

```
head	1.15;
access;
symbols
	BRANCH_2025:1.14.0.2
	REL_1_0:1.9;
locks; strict;

1.15
date	2025.06.04.10.00.00;	author artist;	state Exp;
branches;
next	1.14;
deltatype	text;
kopt	B;

desc
@@

1.15
log
@new normal map@
text
@blake3:3fa91c2e6d4b...@
```

Two CVSNT-specific details in that header: there is **no `expand` keyword in the admin block** —
the `-k` mode is recorded per revision as `kopt` (`src/rcs.cpp:6680`) — and each revision also
carries a `deltatype` (`src/rcs.cpp:6678`). The admin block CVSNT writes is `head`, `branch`,
`access`, `symbols`, `properties`, `locks`, `comment`, in that order (`RCS_putadmin`,
`src/rcs.cpp:6576` onwards).

Consequences:

* The `,v` file grows by roughly 250 bytes per revision plus the log message, no matter how large
  the asset is. (The delta node alone is ~150-200 bytes: `putdelta`, `src/rcs.cpp:6643`, plus the
  `other_delta` fields added at `src/rcs_checkin.cpp:627`.)
* `cvs log`, `cvs status` and tag operations never touch the payload.
* The payload itself is at `/cvs/blobs/3f/a9/3fa91c2e6d4b...`.
* Nothing in the `,v` file records the blob's *size*; that comes from the blob header or from a
  `SIZE` query.

`ca_blobs_fs/src/content_addressed_fs.cpp:71` (`get_file_path`, with the root set by `set_root` at
`:39`) is where a hash is turned into `<root>/blobs/xx/yy/<64-hex>` on the server.

## Client side

```
myworkdir/
├── CVS/
│   ├── Root                  the CVSROOT this tree was checked out from
│   ├── Repository            path of this directory inside the repository
│   ├── Entries               one line per versioned file/subdirectory
│   ├── Entries.Log           pending appends to Entries (merged lazily)
│   ├── Entries.Extra         CVSNT extra per-entry data
│   ├── Entries.Extra.Log
│   ├── Entries.Static        marks a directory checked out non-recursively
│   ├── Tag                   sticky tag/date for this directory
│   ├── Template              cached log message template
│   ├── Notify                pending watch notifications
│   ├── Base/                 pristine copies of files opened with `cvs edit`
│   ├── Rename                pending renames
│   └── Repository.Virtual    virtual repository mapping, if used
├── src/
│   └── main.cpp
└── assets/
    └── tex.dds               real content, fetched from the blob store
```

Constants are at `src/cvs.h:139`.

### `CVS/Entries` format

```
/<filename>/<revision>/<timestamp>/<options>/<tagdate>
D/<dirname>////
```

* `<timestamp>` is the mtime the file had when CVS last wrote it. A file whose mtime differs is a
  candidate for "modified" and gets content-compared.
* `<options>` is the `-k` string, e.g. `-kB`.
* A leading `D` marks a subdirectory. A bare `D` on its own line signals that this `Entries` file
  lists **all** known subdirectories — it is written precisely when subdirectory tracking is in
  effect, typically with no subdirectories at all (`src/entries.cpp:489`, written at
  `src/entries.cpp:194`).

External tooling sometimes exploits the `D/<dirname>////` form by writing such a stub into
`CVS/Entries` before an update, to bootstrap a partial checkout. Nothing in this source tree
documents or implements that behaviour — treat it as an external convention, not a supported
interface.

### Files CVS creates in the working copy

| Pattern | Created by | Meaning |
| --- | --- | --- |
| `.#<file>.<rev>` | `update` merge (`src/update.cpp:2329`) | Your version before a merge overwrote it; `<rev>` is the revision you were on |
| `.#<file>.<rev>` | `update -C` (`src/update.cpp:801`) | Your locally modified version before it was reverted |
| `.#<file>.<rev>` | `update -C` over client/server (`src/client.cpp:5436`; the flag is set at `src/update.cpp:400`) | Same, on the send path |
| `#cvs.lock`, `#cvs.rfl.*`, `#cvs.wfl.*` | server locking (`src/lock.cpp:684`) | Present in the *repository*, not the working copy |

The prefix is `BAKPREFIX`, defined as `".#"` at `src/cvs.h:269`. CVS never removes these files; the
upstream comment says they are expected to "stay around for a few days before being automatically
removed by some cron daemon" (`src/update.cpp:2319`), which on a developer workstation means they
accumulate forever.

`update -n` sets `backup_local_files = 0` (`src/update.cpp:205`), which suppresses the **`-C`**
backups only — the flag is tested in exactly two places, `src/update.cpp:801` and
`src/client.cpp:5436`. The merge backups at `src/update.cpp:2329` and `src/update.cpp:3105` are
written regardless. Where `-n` does apply, the local version is deleted outright and irreversibly.

## Locking

Two mechanisms coexist:

1. **Filesystem locks** in the repository directory: `#cvs.lock` (the master lock — a *directory*,
   created with `CVS_MKDIR` at `src/lock.cpp:1088`), `#cvs.rfl.<host>(<user>).<pid>` (read) and
   `#cvs.wfl.<host>(<user>).<pid>` (write). Names at `src/cvs.h:222`; construction at
   `src/lock.cpp:722` and `src/lock.cpp:878`. `LockDir` in `CVSROOT/config` can redirect them to a
   scratch filesystem.
2. **The lock server** `cvslockd` on port 2402, selected with `LockServer` in `CVSROOT/config`
   (`src/lock.cpp:152`, connection at `src/lock.cpp:190`). Locks become in-memory state in a single
   daemon rather than files, which removes a large amount of directory churn.

The blob store needs no locking for reads: blobs are immutable, and a partially-written blob lives
under a temporary name until its final `rename`. Maintenance tools that *delete* blobs
(`gc-blobs`) do take a lock, via `tools/simpleLock.cpp.inc`.
