# G-CVSNT Performance Analysis: per-file / per-directory costs in `update`, `tag`, branch

Code base: `D:\G-CVSNT\cvsnt\cvsnt-2.5.05.3744\` (all paths below relative to that root; line numbers from current working tree).

---

## Executive summary

The time of `cvs update` / `cvs tag` on huge trees is dominated by **fixed per-file overheads on the server**, plus one large per-file overhead on Windows clients. Ranked by expected impact:

1. **Lockserver round-trips: 2 per file for update, ~6 per file for tag.** Every `RCS_parse` of every `,v` file takes a lock from the lockserver over a socket and releases it when the RCSNode is freed (`src/rcs.cpp:905`, `src/rcs.cpp:766-767`; enabled by default, `src/main.cpp:551-560,587-591`). One synchronous send+recv per Lock and per Unlock (`src/lock.cpp:228-251,291,377`). Nothing is batched. For N=300k files that is 600k+ synchronous RTs on update; tag does it twice (two recursion passes) plus a write-lock pair inside `RCS_rewrite`.
2. **Full `,v` header parse per file, every time, no cache.** `RCS_parsercsfile_i` unconditionally calls `RCS_reparsercsfile`, which parses *every delta header* of the file (`src/rcs.cpp:326-352`, delta loop at `src/rcs.cpp:533`, `getdelta` at `src/rcs.cpp:6148`). The comment at `src/rcs.cpp:336-340` still claims lazy parsing, but the code isn't lazy any more. Cost is proportional to the number of revisions per file even when the answer is "up to date". There is no cache anywhere (no hits for "cache" in `src/rcs.cpp`).
3. **Tag rewrites the whole `,v` and then re-parses it.** `RCS_rewrite` writes full admin + raw-copies the whole body to `,file,`, renames, then does a *complete re-parse* of the file it just wrote (`src/rcs.cpp:7154-7201`, reparse at 7199-7200) — the re-parse result is immediately thrown away by the tag file-proc. Tag also runs **two full recursion passes** (permission/`check_fileproc` pass + tag pass), each re-parsing every file (`src/tag.cpp:409` and `:431`).
4. **Windows stat() opens every file.** Default `use_ntea=1` (`windows-NT/win32.cpp:362-363`) makes every `CVS_STAT`/`CVS_LSTAT` of an existing file do `CreateFile(FILE_READ_EA)` + `NtQueryEaFile` + `CloseHandle` on top of `GetFileAttributesEx` (`windows-NT/win32.cpp:2203-2213, 1946-1986`). The client stats every working file once per update (`src/vers_ts.cpp:389`). ~4 kernel calls (incl. a file open that AV filters intercept) × N files.
5. **Server "fake sandbox" materialization/deletion is O(M·depth) filesystem ops** — `dirswitch`+`create_adm_p`+`mkdir_p` per `Directory` request (`src/server.cpp:1304-1500, 705-, 892-`), and the final `rm -rf` of the temp dir happens **before** the compressed-stream shutdown trailer is sent (`src/server.cpp:5100-5121`), i.e. partially on the client-visible critical path.
6. **No parallelism anywhere on the server path** (recursion is strictly sequential); the only existing parallelism is the client-side blob transfer pool (≤8 threads, persistent connections — already good, `src/download_blob_to.cpp:236-299`).

There is **no per-file client↔server network round-trip** — both request and response phases are fully streamed/buffered. The per-file round trips are server↔lockserver (and client↔blob-server for changed `-kB` files, but those are parallel and pipelined per-thread).

---

## 1. Update path costs (N files, M directories, almost nothing changed)

### 1.1 Client, send phase (per file)

Driven by `send_files` → `start_recursion` → `send_fileproc` (`src/client.cpp:5909-5980, 5297`).

| Operation | Where | Cost |
|---|---|---|
| Entries lookup | `Version_TS` → `findnode_fn` (`src/vers_ts.cpp:132-134`) | O(1) hash |
| **stat of working file** | `src/vers_ts.cpp:389` `time_stamp` → `CVS_LSTAT` | 1 lstat; on Windows = `GetFileAttributesEx` + `CreateFile` + `NtQueryEaFile` + `CloseHandle` (`windows-NT/win32.cpp:2135, 2203-2213, 1947-1986` — `use_ntea` default on, `:363`) |
| `Entry /name/ver/ts.../` line | `src/client.cpp:5322-5355` | ~80-150 B to buffer |
| `EntryExtra` line (cvsnt ext.) | `src/client.cpp:5358-5377` | ~60 B |
| `Unchanged name` line | `src/client.cpp:5452-5461` | ~15 B |
| md5 re-check when only timestamp differs | `src/client.cpp:5400-5421` | full file read + MD5 (only for touched-but-identical files with md5 stored in `Entries.Extra`) |

If a file is genuinely modified: `Is-modified` (update passes `SEND_NO_CONTENTS`? No — update does *not* set SEND_NO_CONTENTS; it sends contents for modified text files via `send_modified`, `src/client.cpp:5432`, but for `-kB` blob files it sends only `Blob-ref-transfer` + blake3 hash because update passes `SEND_NO_BLOBS_CONTENT`, `src/update.cpp:410`, `src/client.cpp:5098, 5223-5243`; computing that hash reads the whole local file).

Per directory (client): `send_dirent_proc` does `isdir(dir/CVS)` + `Name_Repository` (reads `CVS/Repository`) (`src/client.cpp:5539-5559`); the recursion additionally reads `CVS/Entries`, `CVS/Entries.Extra`, `CVS/Entries.Log` (`src/entries.cpp:843-917`), `CVS/Tag` (`ParseTag`, `src/entries.cpp:1140`), `CVS/Root` per directory (`src/recurse.cpp:639,1177`), and `open_directory` does 2 `isfile` probes (`CVS/Rename`, `CVS/Repository.Virtual`, `src/mapping.cpp:1136,1177`). If `Entries.Log` exists, Entries is fully rewritten at open (`src/entries.cpp:938-942`).

**No round trips**: all requests are appended to a buffered stream; the single flush + `update\n` happens once (`src/update.cpp:444`).

### 1.2 Server, request phase (per file / per directory)

* Per `Directory` request (`serve_directory` → `dirswitch`, `src/server.cpp:1563, 1304-1500`):
  * `server_write_entries()` — drains queued Entry lines into the temp sandbox `CVS/Entries` + `CVS/Entries.Extra` (append-open, fprintf per file, close) (`src/server.cpp:2489-2540`).
  * `mkdir_p(dir)` — one mkdir attempt per path component (`src/server.cpp:892-930`).
  * `create_adm_p` — walks from the deepest component **up to the root**, doing mkdir + `isfile(CVS/Repository)` (+ creates on first visit) per level (`src/server.cpp:705-`).
  * `chdir`, `mkdir CVS`, write `CVS/Repository`, touch `CVS/Entries`, touch `CVS/Entries.Extra` (`src/server.cpp:1407-1499`).
  * Net: **O(depth) syscalls + ~6 file ops per directory**, ~M·(depth+6) total.
* Per `Entry` request: O(1) malloc + prepend (`src/server.cpp:2336-2367`).
* Per `Unchanged`/`Is-modified` request: linear scan of the per-directory entry list (`src/server.cpp:2224-2246, 2263-2293`) — but the just-prepended Entry is at the head, so with the standard client ordering (Entry immediately followed by Unchanged) it's O(1). Worst case (non-adjacent ordering) is O(F²) per directory.

### 1.3 Server, command phase (per file)

`serve_update` → `do_cvs_command(update)` → `do_update` → `start_recursion(update_fileproc, …, readlock=1, dosrcs=1)` (`src/update.cpp:651-654`).

Per file (`do_file_proc`, `src/recurse.cpp:884-970`):

1. `map_filename` — in-memory modules2/rename lookup, several MAX_PATH strcpy/snprintf (`src/recurse.cpp:894`, `src/mapping.cpp:658-681, 464-`). Cheap but nonzero CPU.
2. **`RCS_parse`** (`src/recurse.cpp:915`):
   * `CVS_FOPEN` of `name,v`; on miss, second attempt in `Attic/` (`src/rcs.cpp:267-289`).
   * **lockserver `Lock Read` round trip** (`src/rcs.cpp:905` → `do_lock_file` → `do_lock_server` send+recv, `src/lock.cpp:362-367, 291, 228-251`). Enabled by default (`LockServer=localhost:2402`, `src/main.cpp:551-560`; forced `127.0.0.1:2402` for the server at `:587-591`). On Linux a Unix socket is used (`src/lock.cpp:184-187`); on Windows it's TCP loopback.
   * **Full admin parse including every delta header**: `RCS_parsercsfile_i` → `RCS_reparsercsfile` unconditionally (`src/rcs.cpp:341-351`); the `while ((vnode = getdelta(…)))` loop at `src/rcs.cpp:533-544` reads/allocates version, date, author, state, branches, next, kopt, … for **every revision** (`getdelta`, `src/rcs.cpp:6148-`). I/O is buffered in `RCSBUF_BUFSIZE` chunks (`src/rcs.cpp:864`), but bytes-read ≈ size of the whole delta-header section per file, and heap churn ≈ number of revisions.
3. `Classify_File` → `Version_TS` (`src/update.cpp:719`, `src/classify.cpp:41`):
   * `RCS_getversion` (tag/branch resolution over in-memory lists, symbols parsed once per node via `translate_symtag`, `src/rcs.cpp:1902, 3217`).
   * `RCS_getproplist` ×2, `RCS_getfilename`, `RCS_getrevtime`, `RCS_getexpand` (`src/vers_ts.cpp:296-316`).
   * `time_stamp_server` → `CVS_LSTAT` of the (nonexistent for unchanged files) temp-sandbox file (`src/vers_ts.cpp:411-441`) — on a Windows server a *failed* stat costs up to 4 failed probes (`GetFileAttributesEx`→`FindFirstFile`→`CreateFile`→`GetFileAttributes`, `windows-NT/win32.cpp:2126-2179`).
   * With `AtomicCheckouts=1` (off by default, `src/main.cpp:548-549`): an extra lockserver `Version` round trip per file in `RCS_getbranch`/`RCS_head` (`src/rcs.cpp:2542-2548, 2817-2823`).
4. `T_UPTODATE` → nothing is emitted (`src/update.cpp:779-781`).
5. `freercsnode` → **lockserver `Unlock` round trip** (`src/rcs.cpp:753-774`, unlock at 766-767).
6. `cvs_flushout()` per file (`src/recurse.cpp:967`) → non-blocking `buf_flush(stdout_buf,0)` (`src/server.cpp:6740-6758`) — cheap when nothing pending.

Per directory (server):

* `Reader_Lock` — **no-op when lockserver enabled** (`src/lock.cpp:704-708`); with `LockServer=none` it creates+removes `#cvs.lock` dir plus a read-lock file per repo dir (`src/lock.cpp:695-780, 1064-1148`).
* `update_predirent_proc` calls **`open_directory` twice** (`src/update.cpp:1145-1146`; the code itself says `/* This should be made more efficient. FIXME */` at `:1144`), then `do_dir_proc` opens it a **third** time (`src/recurse.cpp:1265`). Each `open_directory` tries `RCS_parse(".directory_history", repo)` and, if that file exists, does `RCS_getversion` + a full `RCS_checkout` of the mapping content (+ its own lockserver lock/unlock) (`src/mapping.cpp:1009-1124`, checkout at 1113); plus 2 `isfile` probes (`src/mapping.cpp:1136,1177`). Also `Entries_Open_Dir` + `upgrade_entries` (`src/update.cpp:1148-1150`).
* `Find_Names`: `Entries_Open` of the sandbox + `readdir` of the repository dir + `readdir` of `Attic` + sort (`src/find_names.cpp:55-121`).
* `WriteTag` per dir → writes sandbox `CVS/Tag` and emits `Set-sticky`/`Clear-sticky` to the client (`src/entries.cpp:1056-1121`, response at 1117-1119); plus one `E Updating <dir>` message per dir (`src/update.cpp:1346-1354`).
* `fileattr_startdir` is lazy/cheap (`src/fileattr.cpp:32-42`); `fileattr.xml` is only read if something asks (`fileattr_read` on demand). But `checkout_file` *does* ask per checked-out file (`fileattr_getroot` + XPath at `src/update.cpp:1724-1731`), so directories containing any updated file pay one fileattr read + per-file XPath evaluation.

### 1.4 Server → client response phase

* For up-to-date files: **zero bytes**.
* For changed text files: `Update-existing`/`Created` + mode + size + contents, all appended to `buf_to_net` (`src/server.cpp:4356-4610`), streamed; flushes happen per-command, not per file (`serve_noop`/final `ok`, `src/server.cpp:4078-4088`).
* For changed `-kB` files: `RCS_checkout` of the head returns the 71-byte blob ref without touching blob content (`is_ref` plumbing: `src/update.cpp:1676-1682`, `src/rcs_checkin.cpp:129-142`, `src/rcs_cvt_kB.cpp:59-88`), server sends `Blob-ref` (`src/server.cpp:4429-4453, 4504-4510`).

### 1.5 Client, response phase (per changed file)

* `call_in_directory` caches open Entries per directory (`last_entries`, `src/client.cpp:909-916, 1158`); on each directory *change*: chdir to top + chdir to dir + `Entries_Open` + `Find_Directories` (readdir) (`src/client.cpp:929-1180`).
* Text file: write `_new_<file>` temp, close, `rename_file`, `utime`, `Register` (`src/client.cpp:1689-1807, 1974, 2092`).
* **`Register` appends to `CVS/Entries.Log` *and* `CVS/Entries.Extra.Log` with `fopen`/`fclose` per call** — 2 opens + 2 writes + 2 closes per updated file (`src/entries.cpp:394-429`). At directory close, if a log exists, Entries and Entries.Extra are each fully rewritten to `.Bak` + renamed, and both logs unlinked (`src/entries.cpp:141-231, 965-981`).
* Blob (`-kB`) file: `update_blob_ref_entries` → `add_download_queue` (`src/client.cpp:2301-2470`) → background pool: ≤8 threads (`min(8, hw_concurrency-1)`, `src/download_blob_to.cpp:241`; override `CVS_BLOB_DOWNLOAD_THREADS` / `blob_concurrency_download_level`, `src/client.cpp:2177-2180`), each with a **persistent** `BlobSocket` reused across files (`src/blob_kv_processor.cpp:78-160`, member `client`, `start()` reuses), round-robin over public/private proxy URLs with fallback to master (`src/download_blob_to.cpp:186-228, 253-283`). Per task: unlink old file, fopen temp, streamed pull + zstd decode + blake3 verify, rename, chmod, utime, 2× `get_file_size` (`src/download_blob_to.cpp:366-474`). Downloads overlap the whole command; joined at process end (`wait_threads`, `src/main.cpp:1716-1717`).
* Windows rename = `MoveFileEx` + 2×`GetFileAttributes` + `SetFileAttributes` (`windows-NT/filesubr.cpp:669-758`). No fsync on this path (fsync exists only in `copy_file`, `src/filesubr.cpp:135` etc.).

### 1.6 Round-trip / streaming verdict

* Client↔server: fully streamed both ways; exactly one logical round trip per command. Confirmed by buffered `send_to_server` and single `get_responses_and_close` (`src/update.cpp:444-446`), buffered responses (`src/server.cpp:4356-`).
* Server↔lockserver: **2 synchronous round trips per file** (the dominant per-file latency), plus 1 per directory-history file.
* Client↔blob server: 1 request per changed `-kB` file, but on 8 parallel persistent connections and off the critical path until final join.

---

## 2. Tag / branch path costs (`cvs tag NAME`, `cvs tag -b`, `rtag`)

Driver: `cvstag` → `rtag_proc` (`src/tag.cpp:128-309, 315-447`). `lock_for_write=1` for the whole command (`src/tag.cpp:282,305`) — so *every* `RCS_parse` takes a **write** lock from the lockserver (`src/rcs.cpp:35, 905`).

Per file, in order:

1. **Pass 1 (check):** `start_recursion(check_fileproc, …, dosrcs=1)` (`src/tag.cpp:409-412`) → full `RCS_parse` (all delta headers) + lockserver **Write-Lock** RT; `check_fileproc` does `Version_TS` + up to 2 `RCS_getversion` (`src/tag.cpp:452-567`); node freed → **Unlock** RT. Results are stored only as (file, version-string) in `mtlist` for the pretag trigger — the parsed RCS data is thrown away.
2. `lock_tree_for_write` (`src/tag.cpp:426`): no-op with lockserver (`src/lock.cpp:1239-1240`); with `LockServer=none` it is a **third full recursion** over the tree plus `Writer_Lock` creating a master-lock dir + write-lock file in *every* repo directory and scanning each directory for read locks (`src/lock.cpp:1232-1249, 851-945, 952-1024`).
3. **Pass 2 (tag):** `start_recursion(tag_fileproc/rtag_fileproc, …, dosrcs=1)` (`src/tag.cpp:431-434`) → *second* full `RCS_parse` + Write-Lock/Unlock RT pair.
4. `tag_fileproc` (`src/tag.cpp:903-1221`): `Version_TS`, `RCS_getversion` for the new rev and for the existing tag (`:961, 1135`); **fast-path**: if the tag already points at the same rev and isn't a branch, returns without writing (`src/tag.cpp:1147-1154`; same in rtag at `:781-786`).
5. `RCS_settag` — pure in-memory symbol list edit (`src/rcs.cpp:4786-4840`). Branch mode adds `RCS_magicrev` (scans versions list, `src/tag.cpp:1131`).
6. **`RCS_rewrite`** (`src/tag.cpp:1204`, rtag `:748, 811`, delete `:1048`, `src/rcs.cpp:7154-7201`):
   * `rcs_internal_lockfile`: **another lockserver Write-Lock RT** (`src/rcs.cpp:7057`) + `unlink` + `CVS_OPEN(O_CREAT|O_EXCL)` of `,file,` **in the repository directory** (`src/rcs.cpp:7072-7083`).
   * `RCS_putadmin` (rewrites all symbols — cost scales with number of tags/branches already on the file) + `RCS_putdtree` (every delta header) + `RCS_putdesc` (`src/rcs.cpp:7169-7171`).
   * `RCS_copydeltas`: with no new delta and nothing outdated, `actions==0` so the per-delta parse loop is skipped and the **entire remaining body is raw-copied in 8 KB blocks** (`src/rcs.cpp:6822-6828, 6903-6941`) — i.e. tag still reads and writes the *whole* `,v` (unavoidable for an in-place header edit, but it is sequential fread/fwrite, no fsync; flushes only, `src/rcs.cpp:7179,7185`).
   * `rename_file(,file, → file,v)` (`src/rcs.cpp:7113`); on Windows ≈5 syscalls (`windows-NT/filesubr.cpp:669-758`).
   * `do_unlock_file` — **Unlock RT** (`src/rcs.cpp:7115`).
   * **`free_rcsnode_contents` + `RCS_reparsercsfile`** — a full re-parse of the file just written (`src/rcs.cpp:7199-7200`). For tag the caller immediately frees the node (`freevers_ts`/`do_file_proc`), so this parse is 100% wasted work.
7. `history_write` per file (`src/tag.cpp:1196`) — append to `CVSROOT/history`.

Per directory: `tag_dirproc` prints `Tagging <dir>` and **also tags the `.directory_history,v` pseudo-file** (parse + settag + full rewrite) when present (`src/tag.cpp:1227-1260`, `get_directory_finfo` `src/mapping.cpp:1490`).

val-tags: the old "scan the whole repository to validate a tag" is **disabled** (early return with comment "val-tags sucks…", `src/tag.cpp:1398-1403`); `add_to_valtags` runs **once per rtag/tag invocation**, not per file (`src/tag.cpp:440-444, 1517-1611`, `myndbm` open/fetch/store once). ✔ not a hotspot.

**Totals per tagged file (lockserver mode): 3 write-lock + 3 unlock round trips, 3 full `,v` parses (2 passes + post-rewrite reparse), 1 full-file read+write + rename.** Nothing is parallel — strictly sequential single thread. No fsync (crash-safety relies on `,file,`+rename).

---

## 3. Existing optimizations found (upstream cvsnt + Gaijin)

| # | What | Evidence |
|---|---|---|
| 1 | `-kB` blob references: `,v` stores a 71-byte `blake3:<hex64>` ref; content lives in the content-addressed store | `src/sha_blob_reference.h:6-13`; write path `src/rcs_cvt_kB.cpp:91-106` |
| 2 | Server never touches blob content on update; sends `Blob-ref` response | `src/update.cpp:1676-1682`, `src/server.cpp:4429-4453`, `src/rcs_cvt_kB.cpp:59-88` |
| 3 | **Parallel client blob downloads**: ≤8 worker threads, `concurrent_queue`, per-thread persistent sockets, URL round-robin/fallback, background overlap with protocol phase | `src/download_blob_to.cpp:54-299` (threads `:241,297-299`), `src/blob_kv_processor.cpp:78-160`, join at `src/main.cpp:1716-1717` |
| 4 | Parallel client blob uploads before commit; server-side dedup via `blob_size_on_server`; local "already sent" cache | `src/client.cpp:5874-5898`, `src/commit.cpp:650-651`, `src/blob_kv_processor.cpp:139-156`, `src/download_blob_to.cpp:475-502` |
| 5 | Update never uploads `-kB` contents: `SEND_NO_BLOBS_CONTENT` → client sends only the blake3 hash (`Blob-ref-transfer`) | `src/update.cpp:410`, `src/client.cpp:5098, 5223-5243` |
| 6 | MD5-in-Entries.Extra: touched-but-identical files are detected client-side and sent as `Unchanged` (avoids content upload) | `src/client.cpp:5400-5421` |
| 7 | `--blob_zero` "hot proxy" mode (write zero-length files instead of blob contents) | `src/update.cpp:157,187-190`, `src/download_blob_to.cpp:392` |
| 8 | val-tags repository scan disabled; val-tags updated once per run | `src/tag.cpp:1398-1403, 440-444` |
| 9 | Lockserver replaces per-directory physical lock files (per-dir `Reader_Lock`/`lock_tree_for_write` become no-ops) — but introduces per-*file* round trips | `src/lock.cpp:704-708, 1239-1240` vs `src/rcs.cpp:905` |
| 10 | Linux lockserver connection via Unix domain socket ("way faster") | `src/lock.cpp:184-187` |
| 11 | Tag/rtag "already tagged at this rev" early-out avoids the `,v` rewrite when re-applying a tag | `src/tag.cpp:1147-1154, 781-786` |
| 12 | Whole-protocol zstd/gzip stream compression | `src/server.cpp:4795-4841` |
| 13 | `RCS_copydeltas` raw-copies the body when only the header changed (tag case) | `src/rcs.cpp:6822-6828, 6903-6941` |
| 14 | fileattr is lazy — no read unless something queries it | `src/fileattr.cpp:32-42, 58-87` |
| 15 | Head-revision checkout reads only the first deltatext (no delta walking for trunk head) | `src/rcs.cpp:4331-4394` |

**Notable absences**: no cache of parsed RCS data (grep "cache" in `src/rcs.cpp` → nothing); no server-side parallelism; no lockserver batching; `RCS_parse` is not lazy despite the comment (`src/rcs.cpp:336-349`); the tag command parses every file 3×.

---

## 4. Blob pulls during update (Q4 answers)

* **Parallel: yes.** Default `min(8, hw_concurrency-1)` threads (`src/download_blob_to.cpp:241`), configurable via `CVS_BLOB_DOWNLOAD_THREADS` env or `blob_concurrency_download_level` (`src/client.cpp:2106, 2177-2180`).
* **Connection reuse: yes.** Each worker owns a `KVNetworkProcessor` whose `BlobSocket client` persists across all its tasks; `start()` only reconnects when invalid (`src/blob_kv_processor.cpp:83, 100-106, 157`). Failover reconnects to the next URL (`attemptReconnect`, `:84-99`).
* Downloads start as soon as `Blob-ref` responses arrive and are joined only at process exit (`src/main.cpp:1716-1717`) → good overlap with the server's response stream.
* Weakness: hard cap of 8 threads regardless of link/latency (`:241`); each queue item does 1 request per blob (no request pipelining inside a connection, though streaming makes big blobs fine; many *small* blobs pay 1 RTT each ÷ 8 threads).

---

## 5. Improvement proposals

Compat rule respected: old clients must keep working. Everything below is server-side or client-local, or gated by `Valid-requests`/`Valid-responses` negotiation.

| # | Proposal | Speeds up | Impact | Risk | 
|---|---|---|---|---|
| P1 | Eliminate per-file lockserver RTs for reads (directory-granular session read lock) | update, checkout, status, log | Very high (removes 2 RTs/file) | Low–Med |
| P2 | Batch lockserver ops for tag (per-directory write lock; or pipelined Lock/Unlock) | tag, rtag, branch | Very high (removes ~6 RTs/file) | Low–Med |
| P3 | Restore lazy `,v` parsing (skip delta-header loop until needed) | update, tag pass 1, ls/status | High (CPU+I/O ∝ revision count → ∝ 1) | Med |
| P4 | Per-directory head/state cache keyed by `,v` (size,mtime) | update HEAD & `-r tag` of unchanged trees | Very high (skip open+parse entirely) | Med |
| P5 | Skip the post-rewrite re-parse in `RCS_rewrite` for callers that discard the node | tag, rtag, admin | Med-high (1 of 3 parses gone) | Low |
| P6 | Merge tag's two recursion passes / reuse pass-1 parse results | tag, rtag | Med-high (1 of remaining 2 parses gone) | Med |
| P7 | Parallelize server file loop per directory (thread pool) | tag first; optionally update classification | High on multi-core servers | Med-High |
| P8 | Windows: default `nontea` stat (skip `CreateFile`+`NtQueryEaFile` per stat) | client update/commit on Windows; Windows servers | High on Windows clients | Low |
| P9 | Client: keep `Entries.Log` handles open per directory (batch `Register`) | checkout/update with many changed files | Med | Low |
| P10 | Server: reorder temp-sandbox deletion after stream shutdown (or background it); consider `Noop`-less sandbox reuse | all remote commands | Low-Med (tail latency) | Low |
| P11 | Cache `open_directory` result across the 3 calls per directory | update on repos with `.directory_history` files | Med | Low |
| P12 | Protocol: per-directory manifest hash to skip unchanged directories wholesale | update of mostly-unchanged trees | Very high (turns O(N) into O(M)) | High (new protocol, needs P4-style cache) |
| P13 | Blob pool: raise/auto-tune thread cap; per-connection pipelining for small blobs | update with many changed small `-kB` files | Med | Low |

### P1 — kill per-file lockserver round trips on read paths

* **Change**: `rcsbuf_open` (`src/rcs.cpp:903-936`) currently always calls `do_lock_file(filename, NULL, lock_for_write, 1)`. Add a "directory lease" mode: when `lock_for_write==0`, take **one** lockserver Read lock for the whole repository directory at `Reader_Lock` time (`src/recurse.cpp:806`, `src/lock.cpp:695`) — the lockserver protocol already namespaces objects by directory (`Lock Read|dir/obj`, `src/lock.cpp:291`) — and make `do_lock_file` return that lease id (refcounted) for files inside the current locked directory; `do_unlock_file` just decrements. Release at `Lock_Cleanup_Directory` (`src/lock.cpp:568-575`).
* **Why safe**: classic CVS semantics are per-directory read locks anyway (that's exactly what `Reader_Lock` does in non-lockserver mode); per-file read locks are *stronger than needed*. Writers (commit/tag) still take per-file write locks that conflict with the directory read lease.
* **Caveat**: `RCS_getbranch`/`RCS_head` assert `rcs->rcsbuf.lockId` (`src/rcs.cpp:2531, 2815`) and use it for `do_lock_version` under `atomic_checkouts` — keep a real (shared) id so the asserts hold; `do_lock_version` only fires when `atomic_checkouts` is on.
* **Impact**: for N=300k, removes ~600k synchronous loopback RTs. Even at 50 µs each this is ~30 s; on Windows TCP loopback with a busy lockserver it is often minutes.

### P2 — lockserver batching for tag/branch

* Same lease idea with Write granularity per directory: take one `Lock Write|dir` before iterating a directory's files in the tag pass; per-file `rcs_internal_lockfile`/`rcsbuf_open` reuse it. Files in a directory are tagged back-to-back, so the hold time doesn't change materially vs. today's sequential loop.
* Alternative (less invasive, smaller win): make `do_lock_file`/`do_unlock_file` pipelined — send `Unlock` without waiting for the reply (fire-and-forget with lazy error check on next command), halving RTs. `lock_server_command` (`src/lock.cpp:228-251`) is a strict send+recv today.
* Files: `src/lock.cpp` (`do_lock_file`, `do_unlock_file`, new lease bookkeeping), `src/rcs.cpp:905, 7057`.

### P3 — lazy delta parsing in `RCS_parse`

* **Change**: split `RCS_reparsercsfile` (`src/rcs.cpp:361-565`) so `RCS_parsercsfile_i` (`:326-352`) stops after the admin keys (head/branch/expand/symbols/properties) — i.e. break out at the first revision key instead of running the `getdelta` loop (`:533-544`). Parse deltas on first access via a `PARTIAL` flag checked in `RCS_getversion`/`RCS_gettag`/`RCS_isdead`/`RCS_getrevtime`/`RCS_getbranch`/`findnode(rcs->versions,…)` call sites (this is exactly how upstream CVS 1.11/1.12 works — the comment at `:336-340` documents the intended design).
* **What still forces a delta parse today** in the unchanged-file path: `RCS_getversion(HEAD)`→`RCS_getbranch` walks `versions` only when a branch is set; `RCS_isdead` looks up the target rev; `RCS_getrevtime` needs the delta date (`src/vers_ts.cpp:300`); `RCS_getexpand` needs per-rev kopt (`src/rcs.cpp:3691`). For trunk-HEAD classification only the *head* delta is needed — parse deltas until the head's node is seen, then stop (head is the first delta in the file). That makes the common case O(1 delta) instead of O(all deltas).
* **Impact**: server CPU + read volume drops by the average delta-header-section size; biggest for old files with hundreds of revisions. Helps update, tag pass 1, `cvs ls`, status.
* **Risk**: medium — audit every direct `rcs->versions` access (`grep -n "->versions" src/*.cpp`), Attic edge cases; sanity suite exists.

### P4 — per-directory RCS state cache (mtime-validated)

* **Change**: a single cache file per repository directory (e.g. `CVSREP/.rcs_state` or alongside `fileattr.xml`), containing per `,v`: `name, size, mtime64, head_rev, head_kopt, head_date, dead_flag, symbols_digest (+ optionally full symbol list)`. On update: after `readdir` (`find_rcs`, `src/find_names.cpp:88`), `stat` each `,v` (one stat, already nearly free vs. today's open+parse+2 lock RTs); if (size,mtime) match the cache entry and classification only needs head/kopt (HEAD update, or `-r tag` with cached symbol), skip `RCS_parse` completely for `T_UPTODATE` decisions (`vn_rcs == entry version && ts unchanged`, `src/classify.cpp:263-305`). Any miss/mismatch → normal path, then update the cache (write once per directory at `filesdone` time, temp+rename).
* Cache writes must happen under the directory write lock or be advisory-only (safe: a stale/absent cache only costs the old slow path). Invalidation = (size,mtime64) of the `,v`; tag/commit rewrite the file via rename so mtime+size always change.
* **Impact**: unchanged file cost becomes ~1 stat + hash lookup; combined with P1 this collapses the server per-file cost by an order of magnitude. Also accelerates `-r BRANCH` updates (symbols in cache).
* **Risk**: medium — correctness depends on strict invalidation; use mtime with 100 ns resolution + size + (on Unix) inode; keep a global "cache generation" kill-switch in CVSROOT/config.

### P5 — don't re-parse after `RCS_rewrite` when the result is discarded

* **Change**: add `bool want_reparse=true` to `RCS_rewrite` (`src/rcs.cpp:7154`); `tag_fileproc`/`rtag_fileproc`/`rtag_delete` pass `false` (they free the node right after: `src/tag.cpp:1204-1218, 748-750, 893`). Leave `true` for commit paths that keep using the node.
* **Impact**: removes 1 of the 3 full parses per tagged file — pure win, ~zero risk (the state after `free_rcsnode_contents` without reparse must simply never be touched; assert `refcount==1`).

### P6 — single-pass tag (or carry pass-1 results into pass 2)

* Options: (a) when no `pretag` trigger is configured and `check_uptodate` is off, skip the check pass entirely and do permission checks + tlist building inside the tag pass (`rtag_proc`, `src/tag.cpp:408-434`); (b) keep two passes but cache `(file → RCSNode)` from pass 1 in a per-directory map keyed by `,v` path, reusing nodes in pass 2 (their lock is a write lock already; keep it held between passes — note this lengthens lock hold time). (a) is simpler and safe: triggers are per-directory (`check_filesdoneproc`, `src/tag.cpp:569-602`), so when `cb->pretag` exists the two-pass structure must stay.
* **Impact**: halves the remaining parse+lock cost of tag when no pretag trigger is installed.

### P7 — parallelize the server-side per-file loop

* The per-file work in tag (parse → settag → rewrite → rename) touches independent files; ordering constraints are only: output (`cvs_output`, already mutex-guarded, `src/server.cpp:6413-6414`), `history_write` append, and lockserver commands (socket shared — needs a mutex or per-thread connections).
* **Sketch**: in `do_recursion` (`src/recurse.cpp:826` `walklist(filelist, do_file_proc, …)`), when `frame->fileproc == tag_fileproc/rtag_fileproc` (or a new `FILEPROC_PARALLEL` capability flag), collect nodes and feed them to a pool of ~4-8 threads reusing the existing `concurrent_queue.h`. Thread-unsafe globals to audit: `tag_set_ok`, statics in `tag.cpp`, `rcs_lockfile/rcs_lockfd` statics (`src/rcs.cpp:218-219` — must become per-thread), `lock_server_socket` (per-thread connections or mutex), `error()` buffers. This is the riskiest proposal but the only one that scales tag wall-time with cores/disk queue depth.
* For update, a cheaper variant: overlap "classify next file" (parse) with "send previous file" using a 2-stage pipeline; classification is read-only.

### P8 — Windows stat cost (client & Windows servers)

* Default `use_ntea=1` makes every stat of an existing file open it (`windows-NT/win32.cpp:362-363, 2203-2213, 1947`). The EA only carries a fake Unix mode; working-copy flows only need mtime/size/readonly.
* **Change**: flip the default to no-EA for the *client* role (keep opt-in via `CVSNT=ntea`), or add a fast path in `wnt_stat`/`wnt_lstat` that skips `GetUnixFileModeNtEA` when the caller only needs timestamps (e.g. a `CVS_LSTAT_FAST` used by `time_stamp`, `src/vers_ts.cpp:389,487`). Note: today users can already set env `CVSNT=nontea` (parsed at `windows-NT/win32.cpp:383-395`) — an immediate zero-code mitigation worth documenting.
* **Impact**: removes a `CreateFile`+`NtQueryEaFile`+`CloseHandle` per file per update on Windows clients (AV/filter drivers make CreateFile the most expensive syscall in the whole client). Risk: executable-bit fidelity for cygwin interop — acceptable default change for a Windows-only game studio.

### P9 — batch client Entries.Log writes

* `Register` reopens/closes both log files per file (`src/entries.cpp:394-429`). `call_in_directory` already keeps `last_entries` open across responses for the same directory (`src/client.cpp:909-916`) — keep `FILE*` handles for `Entries.Log`/`Entries.Extra.Log` in the same struct, append per Register, close on directory switch / `Entries_Close`. 6 syscalls/file → amortized ~0.
* Files: `src/entries.cpp` (Register/Scratch_Entry/Rename_Entry), lifecycle tied to `Entries_Close` (`:965-981`).

### P10 — server sandbox lifecycle

* `server_cleanup` deletes the whole temp sandbox **before** `buf_shutdown(buf_to_net)` sends the compression trailer (`src/server.cpp:5100-5121`) — clients (and scripts timing `cvs update`) wait for the `rm -rf` of M×~5 files. Reorder: shutdown/flush `buf_to_net` first, then delete; or spawn the deletion detached.
* Longer term: for `Entry`+`Unchanged`-only directories the sandbox brings no information the in-memory entries list didn't already have — the per-dir `CVS/*` files are written and re-read within the same process (`dirswitch` writes, `Entries_Open` reads back). An in-memory VFS for the sandbox (or trusting the entries list directly) would remove O(M·depth) mkdir/create plus the re-read, but it is a larger refactor of `server.c`'s "fake working directory" design.

### P11 — cache `open_directory`

* `update_predirent_proc` opens/closes the directory-version state twice back-to-back and `do_dir_proc` a third time (`src/update.cpp:1145-1156`, `src/recurse.cpp:1265`); each open re-parses and re-checks-out `.directory_history,v` when present (`src/mapping.cpp:1056-1119`). Memoize by `(repository, tag, date, version)` within `directory_data` for the current directory, or plumb the already-open handle from predirent into do_dir_proc. The FIXME at `src/update.cpp:1144` acknowledges this.

### P12 — protocol: directory manifest short-circuit (the big one, needs negotiation)

* **Idea**: client sends per directory a digest of its entries state (`Manifest <dir> <blake3(entries-lines + tag/date)>`) instead of N `Entry`+`Unchanged` pairs (still sending them for modified files only). Server computes/caches the same digest for (directory, tag/date, head state) — cheap once P4's cache exists — and answers "directory up to date" without touching a single `,v`.
* Gains: request phase shrinks from O(N) lines to O(M); server command phase skips whole directories; `rm -rf` sandbox shrinks.
* Compat: strictly opt-in via `Valid-requests` (old client never sends it; old server never advertises it; the client only sends when advertised — mirrors existing `supported_request("EntryExtra")` pattern, `src/client.cpp:5358`).
* Risk: high (protocol + cache correctness under concurrent commits — digest must be validated under the directory read lease from P1).

### P13 — blob pool tuning

* Raise the hard cap of 8 (`src/download_blob_to.cpp:241`) for LAN servers (make it a first-class config, not just env), and consider issuing the next request before fully draining the previous response on a connection (simple pipelining) for many-small-blob workloads. Connection setup already amortized; risk low.

---

## Suggested order of attack

1. **P8** (env `CVSNT=nontea` today; flip default in a patch) — instant Windows client win, trivially safe.
2. **P5** (no reparse after tag rewrite) + **P11** (open_directory memo) — small, contained patches.
3. **P1/P2** (lockserver leasing/batching) — biggest architectural win for both update and tag; touches `lock.cpp` + 2 call sites in `rcs.cpp`.
4. **P3** (lazy parse) then **P4** (mtime cache) — server CPU/I-O; P4 builds on the invariants P3 clarifies.
5. **P6/P7** (single-pass + parallel tag) — after locks are batched, parallelism gives tag near-linear scaling.
6. **P12** (manifest protocol) — end-game for "nothing changed" updates; needs P1+P4 foundations.
