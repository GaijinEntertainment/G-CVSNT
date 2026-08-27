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
| `-j N` | Number of blob download worker threads. `0` downloads on the main thread. When not given, the default is `min(8, max(1, cpu_count - 1))` (`src/download_blob_to.cpp:241`). This is the single biggest client-side knob for update speed on binary-heavy trees (`src/main.cpp:1013`) |
| `--blob_url <spec>` | Override the blob server(s) the server advertised. `\|`-separated; each entry `host[/path][@port]`; `def` means the master (`src/main.cpp:1017`) |
| `-z <0-9>` | Stream compression level for the CVS connection (gzip or zstd, negotiated) |
| `-x` / `-y` | Require / request encryption of the CVS connection |
| `-a` | Authenticate (sign) all traffic |
| `-q` / `-Q` | Quiet / very quiet — worth using on huge trees, output formatting is not free |
| `@response-file` | Read the remaining arguments from a file, one per line. Must be the last argument. Use this instead of chunking long file lists to dodge the Windows ~8 KB command-line limit |
| `-t` | Trace execution; `-t -t` and more increase the trace level. Invaluable for diagnosing blob-fetch problems |

Note the collision hazard: **`-j` is a global option here** (blob concurrency) but **`-j rev` is a
per-command option** for `update`, `checkout` and `diff` (merge from revision). Position decides
which one you get:

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

(`src/rcs.cpp:42`, `src/rcs.cpp:3501`)

Use `-kB`/`-kBz` for **every** large binary asset. A binary added with plain `-kb` will bloat its
`,v` file and slow down every tag, branch and `rlog` that touches its directory.

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

(`src/update.cpp:130`)

| Option | Meaning |
| --- | --- |
| `-d` | Create directories that appeared in the repository. **Without `-d`, new directories are silently skipped** |
| `-P` | Prune directories that became empty |
| `-A` | Reset sticky tag/date/kopt back to HEAD |
| `-C` | Overwrite locally modified files with clean repository copies, keeping a `.#file.rev` backup |
| `-n` | Do *not* keep those backups — silently discard local modifications. Irreversible |
| `-r rev` | Update to a tag/branch/revision (sticky) |
| `-D date` | Update as of a date (sticky) |
| `-f` | Fall back to the head revision when the tag/date does not match a file |
| `-t` | Use the last check-in time as the file mtime instead of "now" |
| `-l` / `-R` | Local only / recursive (recursive is the default) |
| `-I ign` | Extra ignore pattern; `-I !` resets the ignore list |
| `--blob_zero` | Write downloaded blobs as zero-length files. For "hot proxy" machines that only need to warm a cache |

Note the asymmetry between `-C` and `-n`: `-C` is what you want for a clean rebuild of a working
copy, but by default it leaves a `.#name.rev` backup for every modified file, and those are never
cleaned up. `cvs update -C -n` discards instead of backing up.

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
keep the full module path underneath it.

## `tag`, `rtag` and branches

```
cvs tag  [-bcdFflR] [-r rev|-D date] tag [files...]
cvs rtag [-abdFflnR] [-r rev|-D date] tag modules...
```

| Option | Meaning |
| --- | --- |
| `-b` | Make it a branch tag |
| `-A` | Make an alias of an existing branch (needs `-r`) |
| `-M` | Create a floating branch |
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

Per-user default options live in `~/.cvsrc` (`%USERPROFILE%\.cvsrc` on Windows), one line per
command:

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
(`src/server.cpp:3355`, `src/server.cpp:4431`). This is the first thing to collect when blobs fail
to download.

```
cvs version          # client and server versions
cvs status -v file   # revision, sticky tag, and all tags on the file
cvs ls -e            # server-side directory listing without a working copy
```
