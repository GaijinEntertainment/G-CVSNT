# Performance: why update / tag / branch scale with file count

This is a developer-facing summary. The full analysis, with per-operation
cost tables and `file:line` evidence for every claim, is in
[`_reports/perf-analysis.md`](../_reports/perf-analysis.md).

## The short version

On huge trees the wall-clock of `cvs update` and `cvs tag`/branch is dominated
by **fixed per-file overhead on the server**, not by data transfer. The blob
system already removed the data-transfer cost for binaries (references instead
of content, parallel out-of-band pulls). What remains is per-`,v`-file work
that runs once per file, sequentially, no matter how little changed.

The client↔server protocol itself is **not** the bottleneck: requests and
responses are fully streamed/buffered, with no per-file network round-trip.
The per-file round-trips that hurt are **server↔lockserver**.

## Where the time goes

1. **Lockserver round-trips — ~2 per file on update, ~6 per file on tag.**
   Every `RCS_parse` takes a lock from the lock daemon over a socket and
   releases it when the node is freed (`src/rcs.cpp:905`, `:766`; lockserver
   on by default). Nothing is batched. 300k files ⇒ 600k+ synchronous
   round-trips on a single update; tag does it twice (two recursion passes)
   plus a write-lock pair inside the rewrite.
2. **Full `,v` header parse per file, every time, no cache.**
   `RCS_parsercsfile_i` unconditionally calls `RCS_reparsercsfile`, parsing
   *every delta header* — even when the answer is "up to date". The comment at
   `src/rcs.cpp:336-340` still advertises lazy parsing, but the code
   (`src/rcs.cpp:349`) is no longer lazy. There is no RCS cache anywhere.
3. **Tag rewrites the whole `,v`, then re-parses it and throws the result
   away.** `RCS_rewrite` writes full admin + raw-copies the entire body +
   renames, then does a complete re-parse of the file it just wrote
   (`src/rcs.cpp:7199-7200`) which the tag file-proc immediately discards. Tag
   also runs two full recursion passes (`src/tag.cpp:409` check pass, `:431`
   tag pass), each re-parsing every file.
4. **Windows `stat()` opens every file.** Default `use_ntea=1`
   (`windows-NT/win32.cpp:363`) makes every stat of an existing file do
   `CreateFile(FILE_READ_EA)` + `NtQueryEaFile` + `CloseHandle` on top of
   `GetFileAttributesEx` — a real file open per file, which AV filters
   intercept. Client stats every working file once per update.
5. **Server "fake sandbox" churn** — per `Directory` request the server
   materializes a temp working area (`dirswitch`+`create_adm_p`+`mkdir_p`,
   O(depth) syscalls + ~6 file ops), and the final `rm -rf` of that temp tree
   happens *before* the compressed-stream shutdown trailer, i.e. partly on the
   client-visible critical path.
6. **No parallelism on the server path** — recursion is strictly sequential.
   The only existing parallelism is the client blob-transfer pool.

## What's already fast (Gaijin work)

Blob references keep the server data-free on update; client blob pulls/pushes
run on a ≤8-thread pool with persistent per-server connections that overlap the
protocol phase; the MD5 in `Entries.Extra` avoids re-sending touched-but-
identical files; modified `-kB` files send only a BLAKE3 hash.

## Improvement proposals

Ordered roughly by impact-to-risk. Server-only changes don't affect protocol
compatibility; protocol changes must be negotiated via `Valid-requests` so old
clients are unaffected. Full sketches (with the exact functions/statics to
touch and the compatibility notes) are in the
[report](../_reports/perf-analysis.md).

| # | Change | Speeds up | Risk |
|---|--------|-----------|------|
| P1/P2 | **Lockserver leasing** — one directory-granular Read (update) / Write (tag) lease instead of per-file lock/unlock | update, tag, branch | med (matches classic per-directory CVS lock semantics; server-only) |
| P3 | **Restore lazy delta parsing** — stop at the first revision key, parse deltas on demand | update, tag | med (touches the RCS parser core; needs care) |
| P4 | **Per-directory `(size,mtime)`-validated head/symbols cache** — skip `RCS_parse` entirely for unchanged files | update, tag | med |
| P5 | **Drop the discarded re-parse** after `RCS_rewrite` in tag paths | tag, branch | low (removes 1 of 3 parses) |
| P6 | **Single-pass tag** when no pretag trigger is configured | tag, branch | low–med |
| P7 | **Parallelize the tag file loop** (files are independent) | tag, branch | med (audit statics like `rcs_lockfile`) |
| P8 | **Default Windows `ntea` off** (users can already set `CVSNT=nontea` today as a zero-code mitigation) | update (Windows) | low |
| P9 | **Keep `Entries.Log` handles open per directory** instead of open/close per file | update | low |
| P10 | **Delete the server temp sandbox after the stream shutdown trailer** | update tail latency | low |
| P11 | **Memoize `open_directory`** (3× per directory today, with an in-code FIXME) | update | low |
| P12 | **Opt-in per-directory manifest-hash protocol** — negotiate a directory digest so a no-change update is O(dirs), not O(files) | update | high (protocol; negotiated, old clients unaffected) |
| P13 | **Blob pool cap tuning** (currently `min(8, cores-1)`) | update/commit of many `-kB` files | low |

**Quick wins with no protocol impact:** P5, P8, P9, P10, P11 — all local
server/client changes, each removing a constant per-file cost.
**Biggest structural wins:** P1/P2 (lock batching) and P3/P4 (parse
elimination) attack the two dominant per-file costs; P12 changes the scaling
class of the common "nothing changed" update.
