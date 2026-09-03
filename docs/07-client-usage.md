# Client usage

This page covers what is different or important in G-CVSNT. For the base CVS command set, the
CVSNT manual (`doc/cvs.dbk`, shipped as `cvs.chm` on Windows) still applies.

## Connecting

```
cvs -d :pserver:user@cvs.example.lan:/cvs checkout mymodule
```

`CVSROOT` syntax is CVSNT's:

```
:method[;keyword=value...]:[user[:password]@]host[:port]:/path
```

Common methods: `pserver`, `sserver` (TLS), `sspi` (Windows integrated), `gserver` (GSSAPI),
`ssh`, `ext`, `server`, `fork`. See [02-architecture.md](02-architecture.md#connection-methods-protocol-plugins).

Log in once per root:

```
cvs -d :pserver:user@cvs.example.lan:/cvs login
```

## Global options that matter here

Run `cvs --help-options` for the full list. The ones specific to, or important for, this fork:

| Option | Effect |
| --- | --- |
| `-j N` | Number of blob download worker threads. When not given, the default is `min(8, max(1, cpu_count - 1))` (`src/download_blob_to.cpp:241`). The single biggest client-side knob for update speed on binary-heavy trees (`src/main.cpp:1013`). `-j 0` is documented as "download in the main thread" but is currently a no-op — `src/client.cpp:2210` treats 0 as "unset", so the default applies |
| `--blob_url <spec>` | Override the blob server the CVS server advertised. Takes a **single** URL `host[/path][@port]`, and disables round-robin (`src/download_blob_to.cpp:258`). The `\|`-separated list and `def` shown in `cvs --help-options` are not implemented — only the first `@` is parsed (`src/client.cpp:2123`) |
| `-z <0-9>` | Stream compression level for the CVS connection (gzip or zstd, negotiated) |
| `-x` / `-y` | Require / request encryption of the CVS connection |
| `-a` | Authenticate (sign) all traffic |
| `-q` / `-Q` | Quiet / very quiet — worth using on huge trees, output formatting is not free |
| `@response-file` | Read the remaining arguments from a file, one per line. Must be the last argument. Use this instead of chunking long file lists to dodge the Windows ~8 KB command-line limit |
| `-t` | Trace execution; `-t -t` and more increase the trace level. Invaluable for diagnosing blob-fetch problems |
| `--rename-in-use` | (Windows) When a file the client must write is currently open or memory-mapped by another process — typically a running `.exe`/`.dll` being updated — the destination cannot be replaced. Instead of retrying for ten seconds and then aborting the whole run, move the in-use file aside to `.#name.inuse.<pid>.<timestamp>` in the same directory so the write completes. A Windows image can be renamed while loaded even though it cannot be deleted, which is what this relies on; the leftover aside file becomes removable once the process using it exits, and sits in the `.#` ignore family until then. Off by default: it moves a file another process is actively using. Accepted and ignored on non-Windows clients |

Note the collision hazard: **`-j` is a global option here** (blob concurrency) but **`-j rev` is a
per-command option** for `update` and `checkout` (merge from revision; `diff` has no `-j`). Position
decides which one you get:

```
cvs -j 8 update -d          # 8 blob download threads
cvs update -j BRANCH_X      # merge from BRANCH_X
cvs -j 8 update -j BRANCH_X # both
```

## `-kB` and `-kBz` — the binary modes

```
cvs add -kB huge_texture.dds
cvs add -kBz compressible_binary.bin
```

| Mode | Flags | Storage |
| --- | --- | --- |
| `-kb` | `KFLAG_BINARY` | Classic CVS binary: every revision stored whole inside the `,v` file |
| `-kB` | `KFLAG_BINARY \| KFLAG_BINARY_DELTA` | Content goes to the blob store; the `,v` holds a 71-byte reference |
| `-kBz` | as `-kB` plus `KFLAG_COMPRESS_DELTA` | Same, and the blob is stored compressed |

(`src/rcs.cpp:38` and `src/rcs.cpp:55` for the two flag tables; `src/rcs.cpp:3502` for serialisation)

Use `-kB`/`-kBz` for **every** large binary asset. A binary added with plain `-kb` will bloat its
`,v` file and slow down every tag, branch and `rlog` that touches its directory.

Binary content is detected on `add` and `import` **by content**, never by name. A NUL byte means
binary: the first 8 KB settles the common cases (a NUL there is binary, an all-normal 8 KB is
text), and a file that is neither - unusual bytes but no NUL yet - is read up to 64 KB further
for one, so a UTF-8 file full of accents or em dashes stays text while a binary file whose first
NUL is past 8 KB is still caught. UTF-16/32 text (BOM) is exempt. A binary file is added as
`-kBz` (blob, zstd-compressed), or `-kB` when the sampled bytes will not compress - already-
compressed data such as jpeg, png or zip - whatever `cvswrappers` or the extension say, with a
note on stderr; an explicit text `-k` on such a
file is refused (use `-kB`). The binary file is never stored as text in any case.

`add` refuses the whole command up front, before it registers anything. `import` walks a tree and
is not transactional: an explicit text `-k` on binary content aborts it at that file, but each file
it reaches before that is
committed as it goes, so files earlier in the walk may already be imported. A current cvsnt client
refuses during its own upload, before the server is asked to import anything, so a client/server
import leaves the repository unchanged; an older client that skips the check relies on the server,
which refuses the same way but, like a local import, commits file by file.

The detector is `CFileAccess::looks_binary()` in cvsapi, so TortoiseCVS and other
cvsapi clients get the same answer. Wrappers remain useful for choosing `-kBz` over `-kB`:

To make it automatic, put patterns in `CVSROOT/cvswrappers`:

```
*.dds   -k 'B'
*.tga   -k 'B'
*.fbx   -k 'B'
*.wav   -k 'Bz'
```

To change an existing file's mode:

```
cvs admin -kB path/to/file
```

Then re-commit; existing revisions stay as they are, new ones become blob references. To convert
history in bulk, use the server-side `cvtblob` tool
([06-server-operations.md](06-server-operations.md#cvtblob--migrate-existing-binaries-into-the-blob-store)).

## `update`

```
cvs update [-3ACPdfilRpbmnt] [-k kopt] [-r rev] [-D date] [-j rev]
           [-B bugid] [-I ign] [-W spec] [--blob_zero] [files...]
```

(`update_usage`, `src/update.cpp:131`. The synopsis above is expanded to include `-3`, `-n` and
`--blob_zero`, which the getopt string at `src/update.cpp:184` accepts but the usage text omits.)

| Option | Meaning |
| --- | --- |
| `-d` | Create directories that appeared in the repository. **Without `-d`, new directories are silently skipped** |
| `-P` | Prune directories that became empty |
| `-A` | Reset sticky tag/date/kopt back to HEAD |
| `-C` | Overwrite locally modified files with clean repository copies, keeping a `.#file.rev` backup |
| `-n` / `--no-backups` | Do *not* keep `.#file.rev` backups — neither the `-C` copies nor the pre-merge copies that merges and `-j` joins leave behind. Silently discards the only remaining copy of local modifications. Irreversible |
| `-r rev` | Update to a tag/branch/revision (sticky) |
| `-D date` | Update as of a date (sticky) |
| `-f` | Fall back to the head revision when the tag/date does not match a file |
| `-t` | Use the last check-in time as the file mtime instead of "now" |
| `-l` / `-R` | Local only / recursive (recursive is the default) |
| `-I ign` | Extra ignore pattern; `-I !` resets the ignore list |
| `--blob_zero` | Write downloaded blobs as zero-length files. For "hot proxy" machines that only need to warm a cache |
| `--move-in-the-way` | When an unversioned file occupies the path of a file the repository wants to create — the `move away <file>; it is in the way` situation, which otherwise blocks that file on every run until someone deletes it by hand — rename the obstruction to `.#name.notversioned.<timestamp>` in the same directory and install the incoming file. A rename, never a delete; the `.#` name is already on the default ignore list. Off by default. Also accepted by `checkout` |
| `--recreate-entries` | A subdirectory whose `CVS/Entries` file is missing normally aborts the **entire** update (`while updating <dir>, CVS/Entries is missing ... create empty Entries to get all files`). This switch performs that documented remedy automatically: an empty `Entries` is written, the run continues, and every file in the directory is fetched again. The survivors on disk are unversioned after the rewrite: content-identical ones are silently re-checked in, edited ones are exactly the in-the-way case — reported `C` and left alone; `-C` does not overwrite them — so pair with `--move-in-the-way` to keep the edited copies as `.#*.notversioned.*` backups. Off by default: a vanished `Entries` can indicate wider corruption worth a human look |

Note the asymmetry between `-C` and `-n`: `-C` is what you want for a clean rebuild of a working
copy, but by default it leaves a `.#name.rev` backup for every modified file, and those are never
cleaned up. `cvs update -C -n` discards instead of backing up. The same applies to merges: `-n`
(spelled readably: `--no-backups`) keeps the `.#name.rev` merge copies from being left behind —
including the copies a server instructs the client to make before a merge. An update merge still
uses a transient copy under `-n`, so a failed merge is restored and a no-op merge is detected; the
copy is removed before the command returns. A `-j` join that fails under `-n` leaves the
half-merged file in place (its copy is never made) and says so. In client/server mode the server
does not see `-n`, so for a nonmergeable file it may still print
`file from working directory is now in .#...` even though the client, under `-n`, has not created
that file.

**`-n` means two different things depending on position.** As a *global* option it is the classic
CVS dry run (`noexec`, `src/main.cpp:938`); as an *update* option it means "do not keep backups"
(`src/update.cpp:204`):

```
cvs -n update -d       # dry run: show what would happen, change nothing
cvs update -d -n       # really update, and destroy local modifications without a backup
```

Getting these the wrong way round destroys work. Prefer `cvs -n update` written exactly that way
when you mean a dry run.

### Naming files limits the update to those files

`cvs update file1 dir2` restricts the operation to the named paths — but a named directory is
still only *descended into*; **files and directories that are new in the repository are only picked
up where `-d` applies**, and naming specific paths means anything outside them is untouched. There
is currently no way to say "update everything except X" — see
`_reports/` for the design work on an exclusion option.

## `checkout`

```
cvs checkout [-ANPRcflnps] [-r rev] [-D date] [-d dir] [-j rev1] [-j rev2] [-k kopt] modules...
```

`-r` and `-D` imply `-P`. `-d dir` checks out into `dir` instead of the module name; add `-N` to
keep the full module path underneath it. `--move-in-the-way` works as in `update` — useful when
checking out over a directory that already contains stray files.

## `tag`, `rtag` and branches

```
cvs tag  [-bcdFflR] [-r rev|-D date] tag [files...]
cvs rtag [-abdFflnR] [-r rev|-D date] tag modules...
```

| Option | Meaning |
| --- | --- |
| `-b` | Make it a branch tag |
| `-A` | Make an alias of an existing branch (needs `-r`) |
| `-M` | (`tag` only) Create a floating branch. `rtag`'s usage text advertises it, but `rtag_opts` (`src/tag.cpp:86`) does not accept it |
| `-d` | Delete the tag |
| `-F` | Move the tag if it already exists |
| `-B` | Permit moving/deleting a *branch* tag. Not recommended |
| `-c` | (`tag` only) Fail if any working file is modified |
| `-f` | Use the head revision when the tag/date does not match |
| `-n` | (`rtag` only) Skip the tag program |

`rtag` works directly on the repository and does not need a working copy. **Prefer it for tagging a
whole module** — it is substantially cheaper than `tag`: it writes one history record per module
instead of one per file, and it skips the client-side working-copy walk and the `Directory`/`Entry`
upload entirely.

Tagging and branching are the operations that scale worst with file count in this codebase. Every
tagged file rewrites its whole `,v`, and the cost grows with the number of tags already present, so
each tag makes the next one slower. See `_reports/PERF-02-tag-branch-path.md` for the measured
reasons and the proposed fixes.

## `commit`

Nothing fork-specific in the option set, but the blob path changes the shape of a commit:

1. The client hashes each `-kB` file with BLAKE3.
2. It asks the blob server `CHCK <hash>`. If the answer is `HAVE`, no bytes are transferred at all.
3. Otherwise it compresses and `PUSH`es the blob.
4. Only then does it send `Blob-ref-transfer` on the CVS connection.

So re-committing an unchanged asset, or committing an asset that already exists elsewhere in the
repository, costs almost nothing.

## `.cvsrc`

Per-user default options live in `~/.cvsrc`, one line per command. On Windows the home directory
comes from `%HOME%`, falling back to `%HOMEDRIVE%%HOMEPATH%` — `%USERPROFILE%` is never consulted
(`windows-NT/filesubr.cpp:1137`):

```
cvs -q -j 8
update -d -P
diff -u
checkout -P
```

`cvs -f ...` ignores the file for one invocation.

## Ignoring files

Sources, in increasing precedence: the built-in list, `CVSROOT/cvsignore`, `~/.cvsignore`,
`$CVSIGNORE`, the `-I` option, and a `.cvsignore` file in each directory. `!` anywhere in a list
clears everything seen so far (`src/ignore.cpp`).

## Diagnostics

```
cvs -t -t -t update -d 2> trace.log
```

Trace level 3 logs blob URL selection, blob-reference detection and per-file decisions
(`src/server.cpp:3354`, `src/server.cpp:4431`). This is the first thing to collect when blobs fail
to download.

```
cvs version          # client and server versions
cvs status -v file   # revision, sticky tag, and all tags on the file
cvs ls -e            # server-side listing, printed in CVS/Entries format
```
