# Suggested optimizations

Both `cvs update` and `cvs tag` scale badly with the **number of files** in a repository rather than
with the amount of data. This document ranks the causes and the proposed fixes, cheapest-and-safest
first, so they can be landed one at a time and measured.

The full analyses, with call chains and evidence, are in:

* [`_reports/PERF-01-update-path.md`](_reports/PERF-01-update-path.md) — the update path
* [`_reports/PERF-02-tag-branch-path.md`](_reports/PERF-02-tag-branch-path.md) — tag, rtag and branch

Every item below carries an ID from those reports. **Status** is updated as work lands; a commit
implementing an item should flip its status in the same commit.

## Ground rules for this work

1. **Tests before implementation.** `testcvs/regress.py` is the regression harness; extend it to
   cover the behaviour an optimization touches *before* changing the code, and confirm the new test
   passes against the current build first.
2. **One optimization per commit**, under 100 lines of change and preferably under 50.
3. **Refactors land separately**, before the optimization that needs them.
4. **Repository integrity outranks speed.** Anything that rewrites a `,v` gets a byte-exact
   round-trip test on real repository files before it is considered.

---

## Why it is slow, in one table

| Operation | Dominant per-file cost | Scales with |
| --- | --- | --- |
| `update` | 2 blocking lock-server round trips + a full RCS parse | file count |
| `tag` / `tag -b` | a complete rewrite of the `,v`, plus 6 lock round trips | file count **× tag count** |
| `tag` **during** a concurrent `update` | waits on the update's per-file read locks, 1 s per collision, fatal after 20 | duration of the concurrent update |

The tag figure is the one worth internalising: because adding a symbol re-serialises the whole
symbol table and copies every deltatext, **each tag makes the next tag slower, permanently.**

---

## Ranked plan

### Tier 1 — safe, small, measurable

| # | ID | What | LoC | Risk | Status |
| --- | --- | --- | ---: | --- | --- |
| 1 | PERF-01 F5 | `rcsbuf_valfree()` is a linear scan of the relocation array, called ~4× per revision from `free_rcsvers_contents()` — O(revisions²) just to free one file. Set a teardown flag in `freercsnode()`; nothing can call `rcsbuf_fill()` during teardown, so the scan is unnecessary there. | ~15 | low | skipped — premise refuted, see below |
| 2 | PERF-01 F6 | `rcsbuf_fill()` grows the parse buffer by a constant `MAX_INCR` (2 MiB), so a large `,v` is memcpy'd O(size²/2 MiB) times. Pre-size the buffer from `fstat`. | ~15 | low | implemented |
| 3 | PERF-02 F2.1 | `RCS_rewrite()` re-parses the file it has just written and throws the result away. Gate that behind a parameter and pass "don't re-parse" from the tag path — removes ~⅓ of all parse work in a tag. | ~8 | low | implemented |
| 4 | PERF-01 F3 | `Register()` writes each `Entries.Log` record **twice**: `write_ent_ex_proc` already writes the `Entries` line, so calling both it and `write_ent_proc` duplicates it. Delete the duplicate call. | 1 | low | implemented |
| 5 | PERF-01 F10a | `find_rcs()` looks a name up with `findnode_fn` and then calls `addnode` anyway, which looks it up again — and leaks the `Node` plus its key when the name is already present. Use `addnode`'s return value. | ~10 | low | implemented |
| 6 | PERF-01 F4 | `write_entries()` byte-copies and `fsync`s both Entries files to `.Old` backups on every directory. Drop the `.Old` copies. | ~25 | low | implemented |
| 7 | PERF-02 F1a + F7 | I/O is done in tiny chunks: `RCSBUF_BUFSIZE` is `BUFSIZ*10` (5120 bytes on MSVC) and the deltatext copy uses an 8 KiB stack buffer, with no `setvbuf` anywhere. Fixed 64 KiB parse buffer, 1 MiB copy buffer, `setvbuf` both streams in `RCS_rewrite`. | ~25 | low | implemented |

### Tier 2 — needs real testing

| # | ID | What | LoC | Risk | Status |
| --- | --- | --- | ---: | --- | --- |
| 8 | PERF-02 F8 | `RCS_putdtree()` calls `fflush(fp)` at the end of *every* recursive invocation — one forced partial write per branch node per file. **Must be committed together with an explicit `fflush(fout)` before `CVS_FTELL(fout)` at `src/rcs_checkin.cpp:952`**, which silently depends on it. Splitting the two produces a wrong `delta_pos`, which is repository corruption. | ~10 | low *with* the companion change | implemented |
| 9 | PERF-02 F3.2 | The first of the two tag passes takes a **write** lock on every file although it only reads. Replace the `lock_for_write` global with a per-recursion value. | ~25 | low | implemented |
| 10 | PERF-02 F6 | `RCS_magicrev()` has two stacked `for` headers; the inner one resets `rev_num = 2` and discards the `findnextmagicrev` result computed just above, forcing a linear rescan with a full symbol-list walk per candidate. The outer loop is dead code. Needs branch-numbering tests first — the optimised path has never actually run. | 2 | medium | implemented |
| 11 | PERF-01 F11 | A batch of per-file micro-costs: `strlen` in the dispatch table, the Entry line built twice, an unhoisted `getline` buffer, the history file handle reopened, `xgetwd` uncached. Individually small, together perhaps 10–20% of a large update. | ~165 | low | partial: dispatch scan, Entries `getline` buffer and history handle implemented; Entry-line assembly, mapping/modules2 lookups and `xgetwd` left `not started` — the first needs byte-exact codepage handling on the client send path, which the local suites cannot exercise, and the others sit on paths (directory mappings, modules2, blob download cwd) with no local test coverage and, for `xgetwd`, no invalidation point that does not itself cost a syscall |
| 12 | PERF-01 F7 | The server flushes on every newline-terminated string, so a 300 k-file checkout does ~300 k tiny `write()` calls. Switch to a byte-threshold flush. **Requires a flush-before-read audit** or the session deadlocks. | ~30 | medium | implemented — the flush-before-read audit is in the commit message |
| 13 | PERF-01 F9 | `open_directory()` fully parses and checks out `.directory_history,v` per directory, and `CVS/Tag` is opened up to three times. Memoise per directory. | ~90 | low-medium | skipped — premise partly refuted and no clean memo key, see below |
| 14 | PERF-01 F8 | A libxml2 XPath is compiled and evaluated **per checked-out file** to answer "is this file watched?" — a fresh context, namespace and variable registration, and a re-parse of the expression each time. Hoist to one query per directory. Must preserve `fncmp` case-folding. | ~70 | low-medium | implemented |

### Tier 3 — structural, needs design and a soak test

| # | ID | What | LoC | Risk | Status |
| --- | --- | --- | ---: | --- | --- |
| 15 | PERF-02 F4 | `history_write` per tagged file costs stat+open+append+close on one global file plus a full trigger dispatch. With any matching `CVSROOT/historyinfo` line that becomes **one process spawn per tagged file**, and `CVSROOT/history` grows ~60 MB per 500 k-file tag. Hold the file open for the command; move `historyinfo` from per-file to per-directory, mirroring `pretag`; add a config switch to restore the current behaviour. | ~125 | medium (trigger ABI) | not started |
| 16 | PERF-01 F1a | `rcsbuf_open()` takes a lock-server lock on **every** `,v` it opens and `freercsnode()` releases it — two blocking round trips per file, on one socket, even for files that turn out to be up to date. Suppress per-file locks for read-only recursions and restore the per-directory read lock. **The single biggest win available.** Stage behind a config switch and soak-test against concurrent commits. | ~40 | medium | not started |
| 17 | PERF-02 F3.1 | `rcs_internal_lockfile()` takes a *second* write lock on a file `rcsbuf_open` has already locked. Reuse `rcs->rcsbuf.lockId`. Removes 2 of the 6 round trips per file in a tag. Must be validated against `lockservice/` or `RCS_rewrite` runs unlocked. | ~10 | medium | not started |
| 18 | PERF-01 F2 | The lazy RCS parse was removed: `RCS_parsercsfile_i()` unconditionally calls `RCS_reparsercsfile()`, parsing the entire admin block *and every delta node*, when update needs only the head, the expand mode and one revision. The `PARTIAL` flag survives only as a stale comment. Reinstate a two-phase parse; ship a debug-build assert-on-unparsed guard first. ~80 direct `rcs->versions` users to audit. | ~120 | medium-high | not started |
| 19 | PERF-02 F5 | Stop exploding the symbol table: splice the new symbol textually in `RCS_settag` so `RCS_putadmin` can take its fast `symbols_data` path. **This is the change that stops tag time growing with the tag count.** Needs byte-exact round-trip tests on real `,v` files. | ~120 | medium-high | not started |
| 20 | PERF-02 F2.2 | Fuse or elide the first tag pass when `!check_uptodate` and no trigger implements `pretag`; free per-directory `tlist`s eagerly. Changes the all-or-nothing failure semantics at `src/tag.cpp:414`. | ~80–140 | medium | not started |
| 21 | PERF-01 F1b | Batched `LockMany`/`UnlockMany` in the lock service — only if #16 proves insufficient. | ~150 | medium | not started |
| 22 | PERF-02 F1 | In-place header patching with a padding newphrase, so tagging does not copy the deltatexts at all. Direct repository-corruption exposure and needs a format upgrade path. Items 3, 7, 8, 15 and 19 may remove enough of the constant factor to make this unnecessary. | large | high | not started |
| 23 | PERF-02 F9 | Parallelise the tag walk. Requires de-globalising `rcs_lockfile`/`rcs_lockfd`, `lock_server_socket` and the `recurse.cpp` frame globals, and rethinking `rcs_cleanup`'s signal-time unlink. | large | high | not started |

---

### Tier 4 — cross-command lock contention

Why a `cvs tag` cannot run while a slow client is updating, from
[`_reports/PERF-03-tag-update-lock-contention.md`](_reports/PERF-03-tag-update-lock-contention.md).
This is a different problem from the two above: not "the command is slow" but "the command dies
because someone else is running one".

The short version: `update` takes a **read** lock on every `,v` it opens (`src/rcs.cpp:908`), `tag`
takes a **write** lock on every `,v` it opens, and the loser does not queue — it gets an immediate
`002 busy` and polls on a hardcoded schedule with a **1 second floor** and a **hard 20-retry fatal**
(`src/lock.cpp:339`). Roughly 39 seconds of collision kills the tag outright.

| # | ID | What | LoC | Risk | Status |
| --- | --- | --- | ---: | --- | --- |
| 24 | PERF-03 F2 | Tag pass 1 validates — it only reads — but took **write** locks, because `lock_for_write` is a global raised across the whole command (`src/tag.cpp:282`). Take read locks in pass 1. Halves the tag's exclusive footprint and lets validation run alongside any number of updates. | ~25 | low | **implemented** (Tier 2 item 9) |
| 25 | PERF-03 F3 | The busy path is a fixed-schedule poll: ≥1 s per retry, 20 retries, then a fatal `Failed to obtain lock`. A 5 ms conflict costs a full second; a busy repository kills the command. Exponential backoff from ~50 ms, and make the cap a configurable timeout. **Removes the fatal**, which is the actual operational failure. | ~20 | low | not started |
| 26 | PERF-03 F5 | `open_directory` parses `.directory_history,v` (`src/mapping.cpp:1057`) and holds its lock until `close_directory` (`:1391`) — spanning the entire directory, including every blocking write to the client. **This is the only lock genuinely held across client network I/O**, and it is the object a tag must write-lock to enter the directory at all. Release it once the version and mappings are read. Also collapses the double `open_directory` at `src/update.cpp:1145`. | ~35 | medium | not started |
| 27 | PERF-03 F9 | `tag_fileproc` emits progress with a bare `cvs_output(..., 1)` (`src/tag.cpp:1160`), which flushed synchronously to the client while holding both the file's write lock and the directory lock. Largely addressed by the Tier 2 output batching (item 12); confirm the tag path no longer flushes inside the lock window. | ~15 | low | partly covered by item 12 |
| 28 | PERF-03 F4 | No fairness: a stream of read lock acquisitions can starve a waiting writer indefinitely, and the 20-retry cap converts starvation into a fatal rather than a delay. Fixed properly only by item 29. | — | — | see 29 |
| 29 | PERF-03 F7 | Give the lock server a real wait queue with writer preference, replacing `002 busy` plus client polling. Kills both the 1 s granularity and the starvation. **Note the constraint:** the service is thread-per-connection under **one global mutex**, and `DoLock` writes its reply with `s->printf` *while holding it* — a blocking wait must not be held under that mutex or the whole service stalls. | ~200 | high | not started |
| 30 | PERF-03 F10 | Lock keys are raw path strings, so an Attic move or two callers spelling the same file differently lock different keys. Normalise to a canonical attic-independent path. Correctness, not performance — and it may *introduce* contention that today silently does not exist. | ~30 | medium | not started |

**If only two of these land:** 25 and 26. Item 25 is ~20 LoC and turns "the branch operation dies"
into "it is slower"; item 26 removes the one lock that a slow client genuinely extends.

## Operational advice that needs no code

* **`cvs rtag` is already substantially cheaper than `cvs tag`** for tagging a whole module: one
  history record per module instead of one per file, and no client-side working-copy walk or
  `Directory`/`Entry` upload. Prefer it.
* **Turn on `LockServer`.** Without it every repository directory touched gets lock files created
  and removed; with tens of thousands of directories that dominates.
* **Use `cvs -j N`** on clients. Blob downloads are already parallel with persistent connections;
  the default is `min(8, cpu_count - 1)`.
* **Deploy a `cafs_proxy_server` per site.** Blobs are immutable, so the cache is correct by
  construction and never needs invalidating.

For the tag-blocked-by-update problem specifically:

* **Convert large binaries to `-kB` and use blob-capable clients.** Highest leverage by far. The
  server then sends a ~71-byte reference and the client pulls the content out-of-band from CAFS, so
  the server's walk stops being paced by the client's link — which collapses the window in which the
  two commands can collide.
* **Check whether `<repos>/<dir>/.directory_history,v` exists.** If it does, a tag cannot enter a
  directory an update is inside, for the whole time that update is in the directory. If it does not,
  `RCS_parse` returns NULL and that entire blocking class disappears.
* **Do not tag the subtree a slow client is updating.** Contention is per exact `,v` path; disjoint
  modules never collide. Narrow the operation with `-l`, or schedule it.
* **`LockServer=none` in `CVSROOT/config`, with eyes open.** It converts the 39-second fatal into an
  unbounded wait at directory granularity — but it also **silently disables `AtomicCheckouts`**
  (`src/rcs.cpp:2573`, `:2848`). Check that setting first. Note that `LockServer` is otherwise
  always on: `src/main.cpp:587` force-defaults it to `127.0.0.1:2402` when unset, so the per-file
  locks are not opt-in.
* **The retry budget is not tunable.** The 20 retries and the 1 s / 5 s sleeps are compiled in.

## Housekeeping found along the way

* `src/RecurseRepository.cpp` / `.h`, `src/Modules1.cpp` and `src/Modules2.cpp` are in no build file
  and are included by nothing but each other. `RecurseRepository.cpp:102` also contains a
  format-string/argument mismatch. Deleting them removes a source of confusion.

## Hypotheses that the code refutes

Recorded so they are not re-investigated:

* **Tier 1 item 1 (PERF-01 F5) is a no-op — the teardown scans already iterate zero times.**
  Every caller of `free_rcsnode_contents()` runs `rcsbuf_close()` first (`freercsnode` at
  `src/rcs.cpp:769→771`, `RCS_rewrite` at `:7239→7244`, `rcs_checkin.cpp:41→62` and
  `:965→969`), and `rcsbuf_close()` frees `reloc_ptr_base` and sets
  `reloc_ptr_count = 0` — so by the time the ~4-per-revision `rcsbuf_valfree()` calls run
  during teardown, each scan is over an **empty** array and costs only the function call.
  The O(revisions²) teardown described in PERF-01 F5 does not exist; a teardown flag would
  skip zero-iteration loops. (The scans that do run against a live array — single-node
  `delnode`s in `RCS_checkin`/`RCS_delete_revs`, one-shot frees like `RCS_symbols`, and
  `freedeltatext` during a delta walk — genuinely need the removal, so the flag must not
  cover them anyway.)
* The client→server protocol **is** properly batched — a 160 KiB flush threshold, not per-file.
* Blob downloads **are** already parallel, with persistent connections and a single end-of-command
  join.
* `fileattr.xml` is read once per directory; only the *query* is per-file.
* `sortlist` uses `qsort`, not an insertion sort.
* The per-file `stat` count is exactly **one** on each side, not several.
* `RCS_fully_parse` is never on the update path.
* Per-directory read locks are already skipped when a lock server is configured — the cost moved to
  per-file locks, which is item 16.
* Tagging never touches the blob store, never calls `RCS_fully_parse`, never `fsync`s, and runs
  `taginfo` once per directory rather than per file.
* Because CAFS keeps `-kB` `,v` files small (a 71-byte reference per revision), the
  rewrite-the-whole-file cost of tagging is **not** catastrophic for binaries. It is for text files,
  which keep their full inline delta history.
* **Tier 2 item 13 (PERF-01 F9) was skipped: "CVS/Tag is opened up to three times" counts two
  different files.** The `ParseTag` in `do_dir_proc` at `src/recurse.cpp:1211` runs before the
  chdir into `dir` — every neighbouring path in that block is `dir/CVS/...` — so it reads the
  *parent's* `CVS/Tag` for the permission check, while `ParseTag_Dir(dir, ...)` at `:1264` reads
  the child's. The only true duplicate pair is `:1264` versus the `ParseTag` inside
  `Entries_Open` (`src/entries.cpp:826`), which runs after the chdir; deduplicating that pair
  needs either a cwd-keyed memo (a `getcwd` per lookup — the syscall it would save) or threading
  the values through `Entries_Open`'s many callers, and the memo would have to be invalidated by
  `WriteTag` mid-recursion to keep sticky tags correct. The `.directory_history` half is two
  failed `fopen`s per directory in repositories that do not use directory versioning; a correct
  cache needs a per-repository flag that survives concurrent creation, which the local suites
  cannot exercise. Neither half fits in 100 lines with test coverage, and the syscalls saved are
  single-digit per directory.
