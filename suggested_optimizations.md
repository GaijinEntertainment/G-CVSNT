# Suggested performance optimizations

Scope: server and client hot paths of `update`, `tag`/branch and checkout on
very large trees (hundreds of thousands of files). The dominant costs are
fixed per-file overheads: lockserver round-trips, full `,v` header parses,
redundant re-parses, and per-file open/close of administrative files. All
paths below are relative to `cvsnt/cvsnt-2.5.05.3744/`.

Each item lists what/where, expected impact, risk, and a status.

---

## 1. Skip the discarded re-parse after rewriting a `,v` in tag paths

- **What/where**: `RCS_rewrite` (`src/rcs.cpp`) ends with
  `free_rcsnode_contents()` + `RCS_reparsercsfile()` — a complete re-read and
  re-parse (admin header plus every delta header) of the file it has just
  written. The tag/rtag file procs (`src/tag.cpp`: `tag_fileproc`,
  `rtag_fileproc`, `rtag_delete`) call `RCS_rewrite` as their last use of the
  node; the node is freed immediately afterwards (`do_file_proc`,
  `src/recurse.cpp`), so the re-parsed data is thrown away unread.
- **Change**: add an opt-out parameter (default keeps today's behaviour) and
  pass it from the tag-path call sites that discard the node. The
  `.directory_history` pseudo-file is excluded: its node is owned by the
  directory-mapping stack and outlives the file proc, so it keeps the re-parse.
- **Expected impact**: removes 1 of the 3 full parses per tagged file (plus one
  file open). Tagging cost per file is parse-bound, so this is a measurable
  cut in `tag`/`rtag`/branch wall time on big modules.
- **Risk**: low. The node's contents are cleared (idempotently, the eventual
  `freercsnode` clears them again) instead of being rebuilt; any accidental
  later use would hit NULL fields rather than stale data. Commit paths keep
  the re-parse.
- **Status**: implemented (commit "tag: skip the discarded re-parse after
  RCS_rewrite")

## 2. Send the network shutdown trailer before deleting the server temp sandbox

- **What/where**: `server_cleanup` (`src/server.cpp`) deletes the whole
  per-connection temporary working area (`unlink_file_dir(orig_server_temp_dir)`,
  one filesystem entry per directory the command touched) *before* calling
  `buf_shutdown(buf_to_net)`, which emits the compression trailer the client
  waits for. The delete of a huge sandbox therefore sits on the client-visible
  critical path.
- **Change**: reorder — shut down `buf_to_net` first, then delete the temp
  tree. The deletion path is silent (it never writes to the protocol buffers;
  errors are deliberately ignored and tracing is disabled there), so nothing
  it does needs the connection.
- **Expected impact**: removes O(directories) file deletions from the tail
  latency of every remote command; most visible on updates of wide trees.
- **Risk**: low. Pure reorder of two independent blocks in the same function;
  behaviour on the `dont_delete_temp` path is unchanged.
- **Status**: implemented (commit "server: send the shutdown trailer before
  deleting the temp sandbox")

## 3. Keep the per-directory Entries.Log append handles open across files

- **What/where**: `Register` (`src/entries.cpp`) is called once per
  updated/checked-out file and each call does `fopen`+`fprintf`+`fclose` on
  *both* `CVS/Entries.Log` and `CVS/Entries.Extra.Log` — 2 opens, 2 writes and
  2 closes per file, repeated for every file in a directory.
- **Change**: cache the two append handles between consecutive `Register`
  calls, keyed by the current working directory; `fflush` after every append
  (so on-disk state after each call is identical to today's), and close the
  cached handles before `write_entries` rewrites/unlinks the logs and on
  `Entries_Close`. Other writers (`Scratch_Entry`, `subdir_record`, …) keep
  their own short-lived append handles; append-mode writes keep ordering
  correct.
- **Expected impact**: 6 filesystem calls per registered file drop to
  amortized 1 write; biggest on checkouts/updates that touch many files per
  directory (Windows clients benefit most, where opens are the expensive op).
- **Risk**: low-medium. The log write path is crash-recovery data for the
  Entries rewrite; per-append flushing keeps durability identical, and the
  close hooks preserve the open/unlink discipline on Windows.
- **Status**: implemented (commit "entries: keep the Entries.Log append
  handles open across Register calls")

## 4. Batch or lease lockserver locks instead of two round-trips per file

- **What/where**: every `RCS_parse` of every `,v` takes a lock from the lock
  daemon over a socket and releases it when the node is freed
  (`rcsbuf_open`/`freercsnode`, `src/rcs.cpp`; `do_lock_file`/`do_unlock_file`,
  `src/lock.cpp`). Each is a synchronous send+receive. An update of N files
  costs 2·N round-trips; tag costs ~6·N (two recursion passes plus the
  write-lock pair inside the rewrite).
- **Change**: take one directory-granular lease per repository directory
  (read for update, write for tag) and have per-file lock/unlock reuse it;
  or, less invasively, pipeline the unlock so it does not wait for a reply.
- **Expected impact**: very high — removes the dominant per-file latency on
  both update and tag for large trees.
- **Risk**: medium. Touches locking semantics; per-file write locks inside
  `RCS_rewrite` and the version-tracking hooks must keep working. Needs
  careful staging and testing against a live lock daemon.
- **Status**: proposed (deferred: changes lock-acquisition granularity; needs
  dedicated concurrency testing)

## 5. Restore lazy delta parsing in the RCS parser

- **What/where**: `RCS_parsercsfile_i` (`src/rcs.cpp`) unconditionally calls
  `RCS_reparsercsfile`, whose `getdelta` loop reads and allocates the header
  of *every revision* in the file — even when the caller only needs
  head/branch/symbols to decide "up to date". The comment above it still
  describes the original lazy design.
- **Change**: stop the initial parse at the first revision key; parse delta
  headers on first access behind a "partial" flag checked at the version-list
  access points.
- **Expected impact**: high — server CPU and read volume per file drop from
  O(revisions) to O(1) for the common unchanged-file path.
- **Risk**: medium. Every direct user of `rcs->versions` must honour the
  partial flag; Attic and magic-branch edge cases need auditing.
- **Status**: proposed (deferred: broad blast radius in the parser core)

## 6. Per-directory cache of parsed `,v` head state

- **What/where**: there is no cache of parse results anywhere; unchanged files
  are re-opened and re-parsed on every command. A per-repository-directory
  cache file holding `(name, size, mtime, head, kopt, dead, symbols)` would
  let the server answer HEAD/tag classification for unchanged files with one
  `stat` per `,v` and no open/parse/lock at all.
- **Expected impact**: very high for "nothing changed" updates (order of
  magnitude on the per-file server cost), also accelerates branch updates.
- **Risk**: medium — correctness hinges on strict invalidation
  (size+mtime validation, rewrite-via-rename keeps them fresh) and on cache
  updates being advisory only.
- **Status**: proposed (deferred: needs its own design/testing round)

## 7. Single-pass tag when no pre-tag trigger is configured

- **What/where**: `rtag_proc` (`src/tag.cpp`) always runs two full recursions:
  a check pass (`check_fileproc`) that parses every file to build the trigger
  list, then the tag pass that parses every file again. When no pretag trigger
  is installed and up-to-date checking is off, the first pass's results are
  used for nothing except permission checks.
- **Change**: when no trigger is configured, fold the checks into the tag pass
  and skip the first recursion.
- **Expected impact**: medium-high — halves the remaining parse+lock cost of
  tag/branch.
- **Risk**: medium. The two-pass structure is load-bearing when triggers
  exist; the fold must replicate the permission/up-to-date checks exactly.
- **Status**: proposed (deferred: behavioural surface around triggers)

## 8. Parallelize the server-side per-file tag loop

- **What/where**: `do_recursion` (`src/recurse.cpp`) processes files strictly
  sequentially. Tagging touches independent files; only output, history
  append and the lockserver socket are shared.
- **Expected impact**: high on multi-core servers (near-linear for the
  rewrite-bound part of tag).
- **Risk**: high. Statics in the RCS lock path (`rcs_lockfile`, `rcs_lockfd`),
  the shared lockserver socket and error handling are all thread-unsafe today.
- **Status**: proposed (deferred: requires a thread-safety audit first)

## 9. Cheaper Windows stat for working files

- **What/where**: on Windows the default stat path
  (`wnt_stat`/`wnt_lstat`, `windows-NT/win32.cpp`) opens every existing file
  (`CreateFile` + `NtQueryEaFile` + `CloseHandle`) just to fetch a fake Unix
  mode from extended attributes; the client stats every working file at least
  once per update. Setting the environment variable `CVSNT=nontea` already
  disables this today and is a good mitigation on client machines.
- **Change**: skip the extended-attribute read where the caller only needs
  timestamps/size, or change the client-side default; keep `CVSNT=ntea` as
  opt-in.
- **Expected impact**: high on Windows clients (removes a real file open per
  file per update — the syscall AV filters make most expensive).
- **Risk**: low for a fast-path variant; a default flip changes
  executable-bit fidelity for interop setups, so it needs a deliberate
  decision.
- **Status**: proposed (default behaviour intentionally left unchanged; use
  `CVSNT=nontea` as an immediate mitigation)

## 10. Avoid opening the directory-version state three times per directory

- **What/where**: for each directory, update runs `open_directory` twice in
  `update_predirent_proc` (`src/update.cpp`) and a third time in
  `do_dir_proc` (`src/recurse.cpp`); each call probes for
  `.directory_history,v` (and on repositories that have it, re-parses and
  re-checks-out the mapping). The code carries a FIXME about it.
- **Analysis result**: the two calls in `update_predirent_proc` are *not*
  redundant — they intentionally open an old-version/new-version pair that
  `upgrade_entries` consumes from the directory stack. Only the third call
  repeats work, and reusing it means carrying pushed stack state (parsed
  mapping node, rename scripts) across the recursion boundary.
- **Expected impact**: medium on repositories using directory versioning;
  small elsewhere (a few failed probes per directory).
- **Risk**: medium — the directory stack has push/pop side effects that
  `upgrade_entries` depends on positionally.
- **Status**: proposed (deferred: safe memoization needs a larger refactor of
  the directory-stack lifecycle)

## 11. Directory-level manifest short-circuit in the protocol

- **What/where**: the client sends one `Entry`+`Unchanged` pair per file and
  the server classifies each file separately, even when an entire directory
  is unchanged. A negotiated per-directory digest (client sends a hash of its
  entries state; server compares against a cached repository-side digest)
  would let both sides skip unchanged directories wholesale.
- **Expected impact**: very high — turns the "nothing changed" update from
  O(files) into O(directories).
- **Risk**: high. New protocol request (must be negotiated so old
  clients/servers are unaffected) and it depends on a reliable server-side
  state cache (item 6).
- **Status**: proposed (deferred: protocol change)

## 12. Blob download pool tuning

- **What/where**: the client's parallel blob downloader
  (`src/download_blob_to.cpp`) caps its thread pool at
  `min(8, hardware_concurrency-1)`; each queued blob costs one request on a
  persistent connection.
- **Change**: make the cap a first-class configurable (it is env-only today)
  and/or pipeline small-blob requests per connection.
- **Expected impact**: medium for updates fetching many small binary files on
  low-latency links.
- **Risk**: low, but it is client throughput tuning rather than a structural
  fix, and needs network benchmarks to pick defaults.
- **Status**: proposed (deferred: needs measurement infrastructure to justify
  new defaults)
