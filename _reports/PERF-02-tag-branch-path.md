---
id: PERF-02
area: tag / rtag / branch
---

# Why `cvs tag` and branch creation scale badly with file count

Scope: `src/tag.cpp`, `src/rcs.cpp`, `src/lock.cpp`, `src/recurse.cpp`,
`src/RecurseRepository.cpp`, `src/rcs_checkin.cpp`, `src/history.cpp`,
`src/server.cpp` (+ `src/parseinfo.cpp`, `triggers/info_trigger.cpp` for the
trigger question). All paths are relative to
`cvsnt/cvsnt-2.5.05.3744/`. Line numbers were read out of the working tree at
commit `2d4b4db`.

## Executive summary

1. **Every tagged file rewrites its entire `,v` file.** `RCS_rewrite`
   (`src/rcs.cpp:7154`) always re-serialises the whole admin header and then
   byte-copies the *entire* deltatext section into a temp file and renames it
   over the original — even though the only change is one line in `symbols`.
   Cost per file is O(size of `,v`), not O(1). (F1)
2. **The tree is walked and every `,v` header is parsed three times per tag.**
   Two full `start_recursion` passes (`src/tag.cpp:409` and `src/tag.cpp:431`),
   both with `dosrcs=1`, plus a third parse inside `RCS_rewrite`
   (`src/rcs.cpp:7199-7200`) whose result is immediately discarded. (F2)
3. **Six synchronous lock-server round trips per file.** `lock_server` defaults
   to `localhost:2402` (`src/main.cpp:551-560`); `rcsbuf_open`
   (`src/rcs.cpp:905`) takes a *write* lock on every file in *both* passes, and
   `rcs_internal_lockfile` (`src/rcs.cpp:7057`) takes a *second, redundant*
   write lock on a file already locked. (F3)
4. **The new per-file `history_write` (commit `4e25b17`) costs a stat + open +
   append + close on one global file, plus a full trigger dispatch, per tagged
   file** (`src/tag.cpp:1196` -> `src/history.cpp:665`). If `CVSROOT/historyinfo`
   has any matching line this becomes **one process spawn per file**
   (`triggers/info_trigger.cpp:1252`). It also grows `CVSROOT/history` by
   ~60 MB per 500k-file tag. (F4)
5. **Total work is O(files x tags), and the "tags" factor grows monotonically.**
   The symbols block is parsed (`do_symbols`, `src/rcs.cpp:1702`) twice and
   re-serialised (`src/rcs.cpp:6600`) once per file per tag. Every tag you
   create makes the *next* tag slower, forever. (F5)
6. **Branch creation (`-b`) has an extra, self-inflicted O(branches x symbols)
   loop.** `RCS_magicrev` has two stacked `for` headers
   (`src/rcs.cpp:2257-2259`); the inner one resets `rev_num = 2`, throwing away
   the `findnextmagicrev` result computed on the line above and forcing a linear
   rescan with a full symbol-list walk per candidate. (F6)
7. **I/O is done in tiny chunks.** `RCSBUF_BUFSIZE` is `BUFSIZ*10`
   (`src/rcs.cpp:864`) = 5120 bytes on MSVC; the deltatext copy loop uses an
   8192-byte stack buffer (`src/rcs.cpp:6817`); there is no `setvbuf` anywhere
   in `src/`. (F7)
8. **`RCS_putdtree` calls `fflush(fp)` at the end of every recursive
   invocation** (`src/rcs.cpp:6734`) — one forced partial-buffer write syscall
   per branch node per file. (F8)
9. **The whole operation is strictly serial**, and global state
   (`rcs_lockfile` / `rcs_lockfd`, `src/rcs.cpp:218-219`, asserted single-use at
   `src/rcs.cpp:7052-7053`) actively prevents overlapping two rewrites. The
   codebase already has a worker-pool pattern in `src/download_blob_to.cpp:241`
   that is not used here. (F9)
10. **Good news: tagging never touches the CAFS blob store, never calls
    `RCS_fully_parse`, never `fsync`s, and never runs `taginfo` per file.** Four
    of the stated hypotheses are refuted — see "Hypotheses refuted". The blob
    store keeps `-kB` `,v` files small (71-byte references), so F1 is *not*
    catastrophic for binaries; it *is* catastrophic for text `,v` files, which
    still carry full inline delta history.

## Per-file cost model

Notation: `H` = bytes of admin header + delta tree (everything before
`delta_pos`), `D` = bytes of the deltatext section, `T = H + D` = size of the
`,v`. `M` = revisions in the file, `S` = symbols (tags) in the file, `B` =
branch nodes in the file. "RTT" = one synchronous lock-server send+recv
(`lock_server_command`, `src/lock.cpp:228`).

### Pass 1 — `check_fileproc` (`src/tag.cpp:409-412`, `dosrcs=1`, `readlock=1`)

| step | location | syscalls / file | bytes / file | complexity |
|---|---|---|---|---|
| `RCS_parse` -> `rcsbuf_open` | `src/recurse.cpp:914`, `src/rcs.cpp:341,869` | 1 `open` (POSIX: 2 `open` + 1 `close`, `src/rcs.cpp:918-934`) + **1 RTT** (`src/rcs.cpp:905`) | — | O(1) |
| header parse `RCS_reparsercsfile` | `src/rcs.cpp:349,361` | `ceil(H / 5120)` `read` | `H` read | O(M) `getdelta` (`src/rcs.cpp:533`) |
| `Version_TS` + `RCS_getversion(symtag)` | `src/tag.cpp:471,528` | 0 | — | **O(S)** (`do_symbols`, `src/rcs.cpp:1702`) |
| `verify_tag` -> `verify_perm` | `src/recurse.cpp:941`, `src/perms.cpp:585` | 0 (dir cache) | — | O(dir depth) map lookups |
| `freercsnode` | `src/recurse.cpp:958`, `src/rcs.cpp:766-771` | 1 `close` + **1 RTT** | — | O(M+S) frees |
| `cvs_flushout` | `src/recurse.cpp:968` | <=1 `send` | ~0 | O(1) |
| tlist node retained | `src/tag.cpp:497-499` | 0 | ~120 B **held until end of command** | O(1) |

### Pass 2 — `tag_fileproc` / `rtag_fileproc` (`src/tag.cpp:431-434`)

| step | location | syscalls / file | bytes / file | complexity |
|---|---|---|---|---|
| `RCS_parse` (again) | `src/recurse.cpp:914` | 1-2 `open` + **1 RTT** | `H` read | O(M) |
| `RCS_getversion(numtag)` + `RCS_getversion(symtag)` | `src/tag.cpp:961,1136` | 0 | — | **O(S)** |
| `RCS_magicrev` (only with `-b`) | `src/tag.cpp:1132`, `src/rcs.cpp:2243` | 0 (+ B RTT if `AtomicCheckouts=1`, `src/rcs.cpp:2544`) | — | **O(B*S)** — see F6 |
| `history_write` (**`tag` only**) | `src/tag.cpp:1196`, `src/history.cpp:665` | 1 `stat` (`:689`) + 1 `open` (`:692`) + 1 `write` (`:818`) + 1 `close` (`:820`) + 2 `getcwd` (`triggers/info_trigger.cpp:910,918`) **+ 1 process spawn if `historyinfo` matches** (`:1252`) | ~120 B appended to one global file | O(1) syscalls, unbounded file growth |
| `RCS_settag` | `src/rcs.cpp:4786` | 0 | — | **O(1)** (hash insert) |
| `RCS_rewrite` -> `resolve_symlink` | `src/rcs.cpp:7165`, `src/subr.cpp:869` | 1 `lstat` | — | O(1) |
| `rcs_internal_lockfile` | `src/rcs.cpp:7023` | **1 RTT** (`:7057`) + 1 `unlink` (Win32, `:7074`) + 1 `open(O_CREAT\|O_EXCL)` (`:7079`) | — | O(1) |
| `RCS_putadmin` | `src/rcs.cpp:6574` | — | `H_sym` written | **O(S)** stdio calls (`:6600`) |
| `RCS_putdtree` | `src/rcs.cpp:6687` | **B `fflush`** (`:6734`) | `H_delta` written | O(M) x ~9 stdio calls/rev |
| `rcsbuf_setpos_to_delta_base` | `src/rcs.cpp:6550` | 1 `lseek` + 1 `read` | — | O(1) |
| `RCS_copydeltas` (tag case) | `src/rcs.cpp:6808` | `~2 * ceil(D / 8192)` (`:6926`) | **`D` read + `D` written** | **O(D)** |
| `rcsbuf_close` | `src/rcs.cpp:7196` | 1 `close` | — | O(1) |
| `rcs_internal_unlockfile` | `src/rcs.cpp:7098` | 1 `close` + 1 `rename` (`:7113`) + **1 RTT** (`:7115`) | — | O(1) |
| `free_rcsnode_contents` + `RCS_reparsercsfile` | `src/rcs.cpp:7199-7200` | 1 `open` | **`H` read again** | **O(M) — pure waste** |
| `freercsnode` | `src/recurse.cpp:958` | 1 `close` + **1 RTT** | — | O(M+S) |
| `cvs_output("T ...")` + `cvs_flushout` | `src/tag.cpp:1207-1211` | <=1 `send` | ~50 B protocol | O(1) |

### Totals per tagged file

* **File-descriptor syscalls:** ~14 (Windows) / ~16 (POSIX, because of the
  double-open at `src/rcs.cpp:918-934`), *plus* `2*ceil(D/8192)` for the delta
  copy, *plus* `B` forced flushes.
* **Lock-server round trips: 6** (2 in pass 1, 4 in pass 2). Plus `B+1` more per
  file in the `-b` path if `AtomicCheckouts` is enabled.
* **Bytes read: `3H + D`.  Bytes written: `H + D`.**
* **Allocations:** ~`3*(4M + 2S)` (three header parses, each building an M-node
  version list with 4-6 allocations per `getdelta`, plus up to two `do_symbols`
  passes), plus one 1208-byte `hasharray` memset per `getlist()`
  (`src/hash.cpp` `getlist`, `HASHSIZE 151` in `src/hash.h:15`).

### Worked example

500 000 files; text-heavy repo tagged nightly for three years, so `S ~= 1000`
(~30 KB of symbols), `M ~= 40` (~6 KB of delta tree), `D ~= 40 KB`:

* reads `3*36 KB + 40 KB ~= 148 KB` per file -> **74 GB**
* writes `36 KB + 40 KB ~= 76 KB` per file -> **38 GB**
* **3 000 000 lock-server round trips** (at a realistic 40 us loopback RTT on
  Windows with a context switch each way => **~2 minutes of pure IPC**, more
  under load)
* **~7 000 000 file syscalls** + ~5 000 000 extra `read`/`write` for the delta
  copy
* **60 MB appended to `CVSROOT/history`** in 500 000 separate open/append/close
  cycles
* if `CVSROOT/historyinfo` matches: **500 000 process spawns** => at 8 ms each,
  **1 h 07 m** on its own.

## Findings

### F1: `RCS_rewrite` rewrites the entire `,v` to add one symbol line

* **Location:** `src/rcs.cpp:7154` (`RCS_rewrite`), `src/rcs.cpp:6808`
  (`RCS_copydeltas`), `src/rcs.cpp:7023` / `:7098`
  (`rcs_internal_lockfile` / `rcs_internal_unlockfile`).
* **Complexity:** O(T) bytes and O(M+S) stdio calls per file, i.e. **O(F*T)**
  for a whole-tree tag, where a plain tag semantically changes ~30 bytes.
* **Evidence.** `RCS_rewrite` unconditionally opens a temp file, re-emits the
  whole header, then copies everything else:

  ```c
  fout = rcs_internal_lockfile (rcs->path, &lockId_temp);   // rcs.cpp:7167
  RCS_putadmin (rcs, fout);                                  // 7169
  RCS_putdtree (rcs, rcs->head, fout);                       // 7170
  RCS_putdesc (rcs, fout);                                   // 7171
  ...
  RCS_copydeltas (rcs, fout, newdtext, insertpt, compress_new_delta); // 7184
  ...
  rcs_internal_unlockfile (fout, rcs->path, lockId_temp);    // 7197
  ```

  For a tag, `newdtext == NULL` and `insertpt == NULL`, and
  `count_delta_actions` (`src/rcs.cpp:6946`) returns 0 for every revision
  (nothing is `outdated`, no `->text`), so `actions == 0` and the parse loop at
  `src/rcs.cpp:6828` is skipped entirely. Control falls straight into the raw
  copy:

  ```c
  char buf[8192];                                            // rcs.cpp:6817
  ...
  while ((got = fread (buf, 1, sizeof buf, rcs->rcsbuf.fp)) != 0)   // 6926
  { ... fwrite (buf, 1, got, fout); ... }                    // 6936
  ```

  Then the temp file is renamed over the original:
  `rename_file (rcs_lockfile, rcsfile);` (`src/rcs.cpp:7113`).
* **Call chain:** `serve_tag` (`src/server.cpp:3833`) -> `do_cvs_command`
  (`:3835`) -> `server_main` (`:6807`) -> `cvstag` (`src/tag.cpp:128`) ->
  `rtag_proc` (`:315`) -> `start_recursion` (`:431`) -> `do_recursion`
  (`src/recurse.cpp:601`) -> `do_file_proc` (`:884`) -> `tag_fileproc`
  (`src/tag.cpp:903`) / `rtag_fileproc` (`:667`) -> `RCS_rewrite`
  (`src/tag.cpp:1204` / `:747`, `:800`).
* **Impact.** For `-kB` (CAFS) files the deltatext is a 71-byte blob reference
  (`RCS_write_binary_rev_data_blob`, `src/rcs_cvt_kB.cpp:6-18`;
  `blob_reference_size = 71`, `src/sha_blob_reference.h:15`), so `D` stays
  small — a few KB. For **text** files (`-kv`/`-kb`, which skip the blob path,
  see the `KFLAG_BINARY_DELTA` gate at `src/rcs_checkin.cpp:943`) the whole
  inline delta history is copied: a 40 MB `,v` costs 40 MB read + 40 MB write
  *per tag*. The `rename` also dirties directory metadata for every file, which
  on NTFS serialises through the MFT/journal.
* **Proposal (staged).**
  * **(a) Cheap, safe:** replace the 8192-byte `fread`/`fwrite` loop with a
    1 MB heap buffer, and `setvbuf` both `FILE*`s to 1 MB in `RCS_rewrite`.
    Optionally use `CopyFileEx`-style / `copy_file_range` for the tail. Removes
    ~128x of the syscalls in the copy and most of the userspace memcpy.
  * **(b) Structural, risky:** in-place header patching. The RCS grammar has a
    `newphrase` extension point in the admin section, and this codebase already
    round-trips unknown admin keys (parsed into `rdata->other`,
    `src/rcs.cpp:513-524`; written back by `putrcsfield_proc` at
    `src/rcs.cpp:6632`). Add a `pad @<N spaces>@;` newphrase sized so the header
    has slack. On `RCS_settag`, if the new `\n\t<tag>:<rev>` fits in the slack,
    shrink the pad by exactly that many bytes, keep the **total header length
    byte-identical** (so `delta_pos` is unchanged) and `pwrite` only bytes
    `[0, delta_pos)`. The deltatexts are never read or written. When the pad is
    exhausted, fall back to a full rewrite that re-pads.
* **Estimated LoC:** (a) ~20. (b) ~250 in `rcs.cpp` + ~40 in `rcs_checkin.cpp`
  (to emit the pad on creation/commit) + a repository upgrade path.
* **Risk:** (a) **low**. (b) **high**.
* **Risk detail:** (a) changes only buffering; the temp-file + `rename`
  atomicity is untouched, so a crash still leaves the original `,v` intact.
  (b) **direct repository-corruption risk**: an in-place `pwrite` is *not*
  atomic — a crash or a full disk mid-write leaves a torn header with no
  fallback copy, and the file is unrecoverable without a backup. It also
  requires that every other reader of these `,v` files (GNU RCS, other CVSNT
  builds, `rcs_convert/`, `cvsdelta/`, any in-house tooling) tolerates the new
  newphrase. Do (a) first and measure; only consider (b) if profiling shows the
  byte copy actually dominates after F2-F5 are fixed.

### F2: The tree is walked twice and every `,v` header is parsed three times

* **Location:** `src/tag.cpp:409-412` (pass 1), `src/tag.cpp:431-434`
  (pass 2), `src/rcs.cpp:7199-7200` (third parse).
* **Complexity:** 3 x O(F*(M+S)) parse work + 2 x O(directories) `readdir`
  scans, where 1 x would do.
* **Evidence.** Both recursions pass `dosrcs = 1` (the 15th argument of
  `start_recursion`, see the signature at `src/recurse.cpp:144-181`):

  ```c
  err = start_recursion (check_fileproc, check_filesdoneproc,
                         (PREDIRENTPROC) NULL, (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
                         argc - 1, argv + 1, local, which, 0, 1,
                         where, repository, 1, verify_tag, numtag);   // tag.cpp:409-412
  ...
  err = start_recursion (is_rtag ? rtag_fileproc : tag_fileproc,
                         (FILESDONEPROC) NULL, (PREDIRENTPROC) NULL, tag_dirproc,
                         (DIRLEAVEPROC) NULL, NULL, argc - 1, argv + 1,
                         local, which, 0, 0, where, repository, 1, verify_tag, numtag); // tag.cpp:431-434
  ```

  and `do_file_proc` parses on `dosrcs`:

  ```c
  if (frfile->frame->dosrcs && mapped_file_repository)
      finfo->rcs = RCS_parse (finfo->mapped_file, mapped_file_repository);  // recurse.cpp:912-914
  ```

  `RCS_parse` -> `RCS_parsercsfile_i` -> **always** `RCS_reparsercsfile(rdata)`
  (`src/rcs.cpp:349`) — despite the comment two lines above claiming the
  opposite. `RCS_reparsercsfile` builds the *full* version list
  (`while ((vnode = getdelta (...)) != NULL)`, `src/rcs.cpp:533`).

  The third parse is at the very end of `RCS_rewrite`:

  ```c
  free_rcsnode_contents(rcs);      // rcs.cpp:7199
  RCS_reparsercsfile(rcs);         // rcs.cpp:7200
  ```

  In `tag_fileproc` nothing reads `vers->srcfile` after `RCS_rewrite` — the
  function only prints `T <name>` and calls `freevers_ts`
  (`src/tag.cpp:1206-1216`). In `rtag_fileproc` it is likewise the last use
  (`src/tag.cpp:747`, `:800`). The re-parse is pure waste (it also re-`open`s
  the file: `rcsbuf_open` at `src/rcs.cpp:375`, because `rcsbuf_close` at
  `:7196` nulled `fp`).
* **Impact.** ~2/3 of all header-parsing work, ~2/3 of all `RCSVers`
  allocations, one extra `open`+`read`+parse per file, and a second full
  `readdir`/`sortlist` sweep of every directory (`Find_Names`,
  `src/find_names.cpp:55`; `find_rcs`, `:266`).
* **Proposal.**
  1. Guard the post-rewrite reparse behind a flag (`RCS_rewrite_no_reparse`) or
     a lazy "dirty" marker, and set it from the tag path. Cheapest single win in
     this report per line changed.
  2. Fuse the two passes. `check_fileproc` already computes exactly the answer
     pass 2 needs (`p->data` is the revision to tag, `NULL` = skip,
     `src/tag.cpp:512-559`). Either (i) carry `finfo->rcs` forward, or
     (ii) skip pass 1 entirely when `!check_uptodate` and no loaded trigger
     implements `pretag` — the only other thing pass 1 does is feed
     `check_filesdoneproc`'s `pretag` call (`src/tag.cpp:598-602`).
* **Estimated LoC:** (1) ~8. (2) ~60-120.
* **Risk:** (1) **low**. (2) **medium**.
* **Risk detail:** (1) the only consumers of a re-parsed node after
  `RCS_rewrite` are elsewhere (`admin.cpp`, `rcs_checkin.cpp`), so gate on an
  explicit parameter rather than removing the call — removing it outright would
  hand stale `RCSNode`s to the commit path, which *can* corrupt a `,v` on the
  next `RCS_rewrite` (wrong `delta_pos`). No repository-corruption risk in the
  tag path itself. (2) skipping pass 1 changes the semantics of "correct all
  errors first" — today a permission failure or `nothing known about X`
  anywhere in the tree aborts before *any* file is modified
  (`src/tag.cpp:414-417`). Fusing the passes means a mid-tree failure leaves a
  half-applied tag. That is a behaviour change users may rely on; keep the
  two-pass structure and just cache the parsed nodes, or make the fusion
  opt-in.

### F3: Six synchronous lock-server round trips per file

* **Location:** `src/rcs.cpp:905` (parse-time lock), `src/rcs.cpp:7057`
  (rewrite-time lock — redundant), `src/rcs.cpp:7115` and `src/rcs.cpp:767`
  (unlocks); transport at `src/lock.cpp:228-247` and `src/lock.cpp:254-354`.
* **Complexity:** O(F) blocking network round trips, each a `send()` +
  `recv()` on a TCP (or, on Linux, unix-domain) socket to a separate process.
* **Evidence.** The lock server is the default:

  ```c
  if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","LockServer",buffer,sizeof(buffer)))
  { ... }
  else
      lock_server = xstrdup("localhost:2402");     // main.cpp:551-560
  ```

  `cvstag` turns on write-locking for the whole operation:

  ```c
  lock_for_write = 1;      // tag.cpp:282
  ```

  so *every* `rcsbuf_open` — including all of pass 1 — takes an exclusive write
  lock:

  ```c
  rcsbuf->lockId=do_lock_file(filename, NULL, lock_for_write, 1);   // rcs.cpp:905
  ```

  and `RCS_rewrite` then takes a **second** write lock on the same object while
  the first is still held:

  ```c
  *lockId = do_lock_file(rcsfile,NULL,1, 1); /* Ask lockserver for an exclusive write lock */  // rcs.cpp:7057
  ```

  Each of these is a blocking request/response:

  ```c
  if(send(lock_server_socket,line,strlen(line),0)<=0) ...
  if((l=recv(lock_server_socket,line,line_len,0))<=0) ...   // lock.cpp:236-243
  ```

  Per file: Lock(pass 1) + Unlock(pass 1) + Lock(pass 2) + Lock(rewrite) +
  Unlock(rewrite) + Unlock(pass 2) = **6**. With `AtomicCheckouts=1`
  (`src/main.cpp:549`) add one `Version` round trip per `RCS_getbranch`
  (`src/rcs.cpp:2544`) and per `RCS_head` (`src/rcs.cpp:2820`), which in the
  `-b` path is `B+1` more per file (F6).
* **Impact.** 3M round trips for a 500k-file tag. `TCP_NODELAY` *is* set
  (`src/subr.cpp:1302`) so there is no 40 ms Nagle penalty, but each round trip
  still costs two syscalls plus two process context switches — 20-100 us on
  Windows loopback. It also means a whole-tree tag takes and releases an
  exclusive lock on every file in the repository, twice, blocking concurrent
  commits file by file for the duration.
* **Proposal.**
  1. Drop the redundant lock in `rcs_internal_lockfile`: if
     `rcs->rcsbuf.lockId` is already a write lock on this path, reuse it.
     (-2 RTT/file.)
  2. Make pass 1 take *read* locks (it never writes): pass an explicit
     `lock_for_write` value per recursion instead of a file-scope global.
  3. Add a batched lock verb to the protocol (`Lock Write|dir/*`), or take one
     directory-level write lock per directory in the tag path, replacing F
     round trips with D. The protocol already has a directory concept
     (`do_lock_server(object, directory, flags, wait)`, `src/lock.cpp:254`) and
     `lock_dir_for_write` has a commented-out directory-advisory call at
     `src/lock.cpp:1261`.
* **Estimated LoC:** (1) ~10. (2) ~25. (3) ~120 in `src/lock.cpp` +
  `lockservice/` protocol changes.
* **Risk:** (1) **medium**. (2) **low**. (3) **high**.
* **Risk detail:** (1) the two locks are on the same object name, so reuse is
  semantically identical *provided* the lock server is not counting
  acquisitions; verify against `lockservice/` before changing, because getting
  it wrong means `RCS_rewrite` runs unlocked and a concurrent commit can
  interleave — **that corrupts the `,v`**. (2) safe; pass 1 genuinely only
  reads. (3) coarsening to directory locks changes the concurrency contract for
  every other command that takes per-file locks; a mismatch between a
  directory-lock holder and a file-lock holder is exactly the scenario that
  produces lost updates. Needs the lock server changed in lockstep and a soak
  test with concurrent commit + tag.

### F4: `history_write` per tagged file — 4 syscalls + a trigger dispatch, unbounded file growth

* **Location:** `src/tag.cpp:1196` (added by commit `4e25b17`, "history: add
  history record (type T) for each tagged file"), implementation at
  `src/history.cpp:665-836`.
* **Complexity:** O(F) `stat`+`open`+`write`+`close` on a *single shared file*,
  O(F) trigger dispatches, O(F) bytes appended to `CVSROOT/history`.
* **Evidence.**

  ```c
  history_write ('T', finfo->update_dir, rev, finfo->file, finfo->repository, NULL, NULL);  // tag.cpp:1196
  ```

  and inside:

  ```c
  if (CFileAccess::exists(fname.c_str()))          // history.cpp:689   -> stat / GetFileAttributesW
  {
      if(!acc.open(fname.c_str(),"a+"))            // history.cpp:692   -> fopen
  ...
      if(!acc.write(line.c_str(),line.length()))   // history.cpp:818
      if(!acc.close())                             // history.cpp:820   -> fclose
  ...
  if (run_trigger (&args, historyinfo_proc) > 0)   // history.cpp:834
  ```

  `CFileAccess::exists` is `stat` on POSIX (`cvsapi/unix/FileAccess.cpp:316`)
  and `GetFileAttributesW` on Win32 (`cvsapi/win32/FileAccess.cpp:530`);
  `open`/`close` are `fopen`/`fclose` (`cvsapi/unix/FileAccess.cpp:43,59`).

  `run_trigger` (`src/parseinfo.cpp:20`) does *not* spawn a process itself —
  trigger DLLs are loaded once and cached via the `tf_loaded` static
  (`src/parseinfo.cpp:39,151`), and the per-call cost is just
  `EnumLoadedTriggers` + `callproc` (`src/parseinfo.cpp:155-160`). But the
  default `info` trigger's `history` handler is:

  ```c
  return parse_info(CVSROOT_HISTORYINFO,"%t|%d|%u|%w|%s|%v","",NULL,generic_options,history_options);  // triggers/info_trigger.cpp:741
  ```

  and `parse_info` does two `getcwd()` syscalls per call
  (`triggers/info_trigger.cpp:910,918`) plus two `std::map<cvs::filename,...>`
  lookups (`:907-908`), and **if any line matches**, `parse_info_line` runs an
  external program:

  ```c
  CRunFile rf;                       // triggers/info_trigger.cpp:1252
  ...
  if(!rf.run(NULL))                  // triggers/info_trigger.cpp:1264
  ```

  If `audit_trigger` is installed, `historyaudit`
  (`triggers/audit_trigger.cpp:331-352`) issues **one SQL `INSERT` per tagged
  file**. If `script_trigger` is installed, `history`
  (`triggers/script_trigger.cpp:449-465`) does one COM `IDispatch` call per
  file.

  Note the asymmetry: `rtag` writes **one** history record per module
  (`src/tag.cpp:291`), `tag` writes **one per file** (`src/tag.cpp:1196`).
* **Impact.** At 500k files: 2M syscalls on one contended file, 1M `getcwd`
  calls, ~60 MB appended to `CVSROOT/history` **per tag operation**. That file
  is read *in full* by every later `cvs history` invocation
  (`read_hrecs`, `src/history.cpp:969-1002`, which `push_back`s every selected
  record into a `std::vector`), so the cost compounds. With a non-empty
  `historyinfo`, add one process spawn per file — on Windows ~5-20 ms each,
  i.e. **40 minutes to 3 hours** for a single tag.
* **Proposal.**
  1. Hold the history file open for the duration of the command (open on first
     `history_write`, close in `Lock_Cleanup`/`main` exit). -3 syscalls/file.
  2. Hoist the trigger out of the per-file loop: accumulate `T` records and
     fire **one** `historyinfo` call per directory (mirroring how `pretag` is
     already done per directory at `src/tag.cpp:569-602`) with a name/version
     vector, exactly like the `pretag` callback signature
     (`pretag_list`/`pretag_version_list`, `src/tag.cpp:643-664`).
  3. Make the per-file `T` record opt-in via `CVSROOT/config`
     (`LogHistory`-style — `logHistory` already exists,
     `src/parseinfo.cpp:14`), defaulting to the pre-`4e25b17` behaviour for
     `tag`. Users who need per-file audit turn it on knowingly.
* **Estimated LoC:** (1) ~30. (2) ~80 (needs a new `historyinfo` batch entry
  point in `triggers/info_trigger.cpp` and the trigger ABI). (3) ~15.
* **Risk:** (1) **low**. (2) **medium**. (3) **low**.
* **Risk detail:** (1) a long-lived append handle means a crash loses buffered
  records — use unbuffered/`O_APPEND` writes so each record is still atomic; no
  repository-corruption risk (`CVSROOT/history` is not versioned data).
  (2) changes the trigger ABI; third-party triggers compiled against the old
  interface must keep working (the `trigger_interface` struct is versioned —
  see `triggers/info_trigger.cpp:1751` where `history` is slotted into the
  vtable, so add a *new* slot rather than change that one). (3) none beyond the
  history file no longer containing per-file `T` records unless enabled —
  document it.

### F5: Total cost is O(files x tags), and the tag count only ever grows

* **Location:** `do_symbols` `src/rcs.cpp:1702`; `RCS_symbols`
  `src/rcs.cpp:3184`; `RCS_putadmin` symbol write `src/rcs.cpp:6591-6601`;
  `putsymbol_proc` `src/rcs.cpp:6463`.
* **Complexity:** per file per tag: **2 x O(S) parse + 1 x O(S) serialise**,
  with `S` = number of symbols already in that file. Whole operation:
  **O(F*S)**, and `S <- S+1` after every tag.
* **Evidence.** `RCS_symbols` lazily explodes the raw `symbols_data` string into
  a hash list; `do_symbols` allocates a `Node` and two `xstrdup`s per symbol:

  ```c
  p = getnode ();
  p->key = xstrdup (tag);
  p->data = xstrdup (rev);
  (void) addnode (list, p);          // rcs.cpp:1754-1757
  ```

  The tag path forces this on *every* file, in *both* passes, because it always
  resolves the tag name: `RCS_getversion(vers->srcfile, symtag, ...)` at
  `src/tag.cpp:528` (pass 1) and `src/tag.cpp:1136` (pass 2) -> `RCS_gettag`
  (`src/rcs.cpp:2042`) -> `translate_symtag` (`src/rcs.cpp:3216`) ->
  `RCS_symbols`.

  On write, because `rcs->symbols` is now non-NULL, `RCS_putadmin` takes the
  slow branch:

  ```c
  if (rcs->symbols == NULL && rcs->symbols_data != NULL)
  { fputs ("\n\t", fp); fputs (rcs->symbols_data, fp); }        // rcs.cpp:6594-6597
  else
      walklist (RCS_symbols (rcs), putsymbol_proc, (void *) fp); // rcs.cpp:6600
  ```

  The upstream comment on `putsymbol_proc` concedes the point: "in an old
  repository with hundreds of tags this can get called hundreds of thousands of
  times when doing a cvs tag" (`src/rcs.cpp:6467-6470`).
* **Impact.** This is the mechanism behind "it used to be fast, now it takes
  hours". With 1000 tags at ~30 bytes each, the symbols block alone is 30 KB —
  typically **larger than the delta tree and larger than the deltatexts for a
  CAFS binary**. It is read twice and written once per file per tag.
* **Proposal.**
  * Keep the symbol table as the raw string and splice the new entry
    textually: `RCS_settag` only needs to (a) find `"\n\t<tag>:"` in
    `symbols_data` and replace the value, or (b) prepend `"\n\t<tag>:<rev>"`.
    Then `RCS_putadmin` can take the fast `symbols_data` branch
    (`src/rcs.cpp:6594-6597`) and never build the list at all. A small
    tag->offset index built once per file would keep lookup O(1) without the
    per-symbol `getnode`+2x`xstrdup`.
  * Independently: where the goal is only to detect "does this tag already
    exist", a `strstr(symbols_data, "\n\t<tag>:")` on the raw buffer answers it
    without exploding the list at all.
* **Estimated LoC:** ~120 in `src/rcs.cpp`.
* **Risk:** **medium-high**.
* **Risk detail:** `do_symbols` parses a four-field form
  `tag:rev:tagdate:tagcomment` (`src/rcs.cpp:1725-1745`) but `putsymbol_proc`
  writes only `tag:rev` (`src/rcs.cpp:6472-6476`) — **today every rewrite
  already silently discards tag dates and tag comments**, and `RCS_settag`'s
  `date` parameter (`src/rcs.cpp:4786`) is never referenced in the function
  body. A textual splice would *preserve* those fields, which is a behaviour
  change (arguably a fix). Getting the splice wrong writes a malformed
  `symbols` block, which is **repository corruption** — the parser would then
  fail on every subsequent operation on that file. Requires a byte-exact
  round-trip test over a corpus of real `,v` files before deployment.

### F6: `RCS_magicrev` has a dead outer loop that discards its own optimisation — O(B*S) per file on `-b`

* **Location:** `src/rcs.cpp:2243-2279`, specifically lines **2254-2259**.
* **Complexity:** O(B*S) per file, where `B` is the number of existing branches
  on the target revision and `S` the symbol count — instead of the intended
  O(S + log).
* **Evidence.**

  ```c
  rev_num = findnextmagicrev (rcs, rev, 2);      // rcs.cpp:2254

   /* only look at even numbered branches */
  for (; ; rev_num += 2)                          // rcs.cpp:2257   <-- outer; its body is the next for
   /* only look at even numbered branches */
  for (rev_num = 2; ; rev_num += 2)               // rcs.cpp:2259   <-- resets rev_num to 2
  {
      (void) sprintf (xrev, "%s.%d", rev, rev_num);
      test_branch = RCS_getbranch(rcs, xrev, 2);  // rcs.cpp:2263
      if (test_branch != NULL) { xfree (test_branch); continue; }
      (void) sprintf (xrev, "%s.%d.%d", rev, RCS_MAGIC_BRANCH, rev_num);
      if (walklist (RCS_symbols(rcs), checkmagic_proc, NULL) != 0)   // rcs.cpp:2274
          continue;
      return (xrev);
  }
  ```

  Two `for` headers are stacked with no braces between them, so the *inner*
  loop is the *body* of the outer one. The inner loop's init clause
  `rev_num = 2` executes on entry and throws away the value
  `findnextmagicrev` just computed. Since the inner loop only exits via
  `return`, the outer loop is unreachable dead code, and the "prime the pump"
  work at line 2254 — which itself walks the entire symbol list
  (`walklist (RCS_symbols (rcs), findnextmagicrev_proc, &info)`,
  `src/rcs.cpp:7355`), sorts a list, and frees it — is pure waste.

  Each iteration then costs `RCS_getbranch` (which, with
  `AtomicCheckouts=1`, adds a **lock-server round trip**:
  `do_lock_version(rcs->rcsbuf.lockId, branch, &v)`, `src/rcs.cpp:2544`) plus a
  **full linear walk of the symbol list** (`src/rcs.cpp:2274`).
* **Call chain:** `tag_fileproc` (`src/tag.cpp:1132`) / `rtag_fileproc`
  (`src/tag.cpp:739`, `:765`) -> `RCS_magicrev`, taken whenever
  `!alias_branch && branch_mode`, i.e. every `cvs tag -b` / `cvs rtag -b`.
* **Impact.** Branch creation is strictly more expensive than a plain tag by
  O(B*S) per file. On a file that has accumulated 50 branches in a repo with
  1000 tags, that is 50 000 extra string compares plus 50 `RCS_getbranch` calls
  (and 50 lock RTTs if atomic checkouts are on) — **per file**.
* **Proposal.** Delete line 2257 (the dead outer `for`) and drop line 2259's
  `rev_num = 2` init clause, so the `findnextmagicrev` result is actually used:

  ```c
  rev_num = findnextmagicrev (rcs, rev, 2);
  /* only look at even numbered branches */
  for (; ; rev_num += 2)
  { ... }
  ```
* **Estimated LoC:** 2.
* **Risk:** **medium**.
* **Risk detail:** `findnextmagicrev` returns `defaultrv` (2) when it cannot
  determine anything (`src/rcs.cpp:7332-7383`), so the fallback is the current
  behaviour. Each candidate is still validated with `RCS_getbranch` and
  `checkmagic_proc`, so a too-low value self-corrects and a too-high value
  merely skips a free magic-branch number without corrupting anything. The real
  risk is that this loop has been dead since it was written, so
  `findnextmagicrev` has effectively never run in production — it needs its own
  test coverage (files with 0, 1, many, and non-contiguous branch numbers)
  before enabling. **No repository-corruption risk**: the chosen revision is
  validated before use.

### F7: Tiny I/O buffers and repeated buffer reallocation

* **Location:** `src/rcs.cpp:864` (`#define RCSBUF_BUFSIZE (BUFSIZ*10)`),
  `src/rcs.cpp:1415-1474` (`rcsbuf_fill`), `src/rcs.cpp:6817`
  (`char buf[8192]`). No `setvbuf`/`setbuf` anywhere in `src/`.
* **Complexity:** O(H/5120) `read` syscalls for the header on MSVC (where
  `BUFSIZ == 512`), O(D/8192)x2 syscalls for the delta copy, plus
  O(log(H/512)) `xrealloc`s each of which runs a pointer-relocation loop.
* **Evidence.**

  ```c
  #define RCSBUF_BUFSIZE (BUFSIZ*10)             // rcs.cpp:864
  ...
  got = fread (rcsbuf->ptrend, 1, RCSBUF_BUFSIZE, rcsbuf->fp);   // rcs.cpp:1463
  ```

  `rcsbuf_fill` never recycles: it always grows the buffer and appends —

  ```c
  if (rcsbuf->ptrend - rcsbuf->buffer + RCSBUF_BUFSIZE > rcsbuf->buffer_size)
  {
      expand_string (&rcsbuf->buffer, &rcsbuf->buffer_size,
                     rcsbuf->buffer_size + RCSBUF_BUFSIZE);       // rcs.cpp:1424-1432
      ...
      for (char ***cp = rcsbuf->reloc_ptr_base; cp < ...; cp++)   // rcs.cpp:1445
  ```

  `expand_string` (`src/subr.cpp:166-184`) starts at `MIN_INCR = BUFSIZ`
  (512 on MSVC, `src/subr.cpp:159`) and doubles, so a 60 KB header triggers ~7
  `xrealloc` + memcpy + pointer-fixup cycles per file.

  The delta copy uses an 8 KB stack buffer against a `FILE*` whose default
  internal buffer is 4096 on MSVC (`src/rcs.cpp:6817`, `:6926`).
* **Impact.** On Windows this multiplies the syscall count for the read side by
  ~10x versus a 64 KB buffer, and adds ~7 realloc+copy of the growing header
  buffer per file per parse (x3 parses, per F2).
* **Proposal.** Define `RCSBUF_BUFSIZE` as a fixed 64 KB (not `BUFSIZ`-derived),
  raise `MIN_INCR` for this buffer (or pre-size `rcsbuf->buffer` from the
  file's `st_size` at `rcsbuf_open`), give the copy loop a 1 MB heap buffer, and
  `setvbuf` the read and write `FILE*`s in `RCS_rewrite`.
* **Estimated LoC:** ~25.
* **Risk:** **low**.
* **Risk detail:** memory footprint rises by ~1 MB per concurrently-open `,v`
  (which is 1, given F9). `rcsbuf_setpos_to_delta_base`
  (`src/rcs.cpp:6550-6561`) reads `buffer_size - delta_pos` bytes and calls
  `error(1,...)` if that returns 0 — pre-sizing the buffer larger makes that
  read larger, which is fine, but pre-sizing must never make
  `buffer_size <= delta_pos`. No repository-corruption risk; this is read-side
  buffering plus write-side buffering behind the same temp-file+rename.

### F8: `fflush()` inside recursive `RCS_putdtree`

* **Location:** `src/rcs.cpp:6734`.
* **Complexity:** O(B) forced partial-buffer writes per file rewrite.
* **Evidence.** `RCS_putdtree` recurses once per branch
  (`RCS_putdtree (rcs, q->key, fp);`, `src/rcs.cpp:6731`) and every invocation
  ends with:

  ```c
  dellist(&revs);
  fflush(fp);        // rcs.cpp:6734
  ```

  Each `fflush` on a partially-filled stdio buffer emits a short `write`.
  `RCS_rewrite` already does its own `fflush` before `CVS_FTELL`
  (`src/rcs.cpp:7178-7180`), so the one inside `RCS_putdtree` serves no purpose
  there. Each recursive call also allocates a fresh `List` via `getlist()`
  (`src/rcs.cpp:6700`) -> a 1208-byte `memset` per branch node.
* **Impact.** On a file with 200 branches, 200 short writes per rewrite instead
  of `H/bufsize` full-size writes.
* **Proposal.** Move the `fflush` out of `RCS_putdtree` — split into a
  recursive worker plus a thin wrapper that flushes once.
* **Estimated LoC:** ~10.
* **Risk:** **low**, *but only with the companion change below.*
* **Risk detail:** the correctness requirement is that the stream is flushed
  before `CVS_FTELL(fout)` computes `delta_pos` — a stale `delta_pos` written
  into the `,v` **is repository corruption** (subsequent reads seek to the wrong
  offset). `RCS_rewrite` already flushes explicitly at `src/rcs.cpp:7178` before
  `:7180`. The other caller, `RCS_checkin`, calls
  `rcs->delta_pos = CVS_FTELL (fout);` at `src/rcs_checkin.cpp:952` **with no
  intervening explicit flush**, relying today on `RCS_putdtree`'s internal one.
  That call site must gain an explicit `fflush` in the same commit, or this
  becomes a corruption bug.

### F9: No batching, no parallelism, and global state that prevents it

* **Location:** `src/recurse.cpp:820` (`walklist (filelist, do_file_proc, ...)`),
  `src/recurse.cpp:857` (`walklist (dirlist, do_dir_proc, ...)`),
  `src/rcs.cpp:218-219` + `:7052-7053` (global lockfile state).
* **Complexity:** wall time = sum of all per-file latencies; zero I/O overlap.
* **Evidence.** The recursion is a plain serial `walklist`; there is no
  threading anywhere in the tag path. Threading *does* exist in this fork, but
  only for blob download:

  ```
  src/download_blob_to.cpp:241: int threads_count = std::min(8, std::max(1, (int)std::thread::hardware_concurrency()-1));
  ```

  and `RCS_rewrite` cannot currently be run concurrently at all:

  ```c
  static char *rcs_lockfile;      // rcs.cpp:218
  static int rcs_lockfd = -1;     // rcs.cpp:219
  ...
  assert (rcs_lockfile == NULL);  // rcs.cpp:7052
  assert (rcs_lockfd < 0);        // rcs.cpp:7053
  ```

  Other per-command globals in the path: `repository`, `update_dir`,
  `mapped_repository`, `filelist`, `dirlist` (`src/recurse.cpp:22-46`),
  `current_directory` (`src/mapping.cpp`), `lock_server_socket`
  (`src/lock.cpp:157`).
* **Impact.** With 6 blocking lock-server RTTs and ~14 blocking file syscalls
  per file, the CPU is idle most of the time. A modest 8-way pipeline would cut
  wall time several-fold with no algorithmic change.
* **Proposal.**
  1. **Cheap:** a read-ahead thread that `open`s + `read`s the next N `,v`
     headers into memory while the main thread works, warming the page cache.
     No shared mutable state.
  2. **Medium:** parallelise at the *directory* level with a worker pool (the
     `concurrent_queue` in `src/concurrent_queue.h` already exists), after
     promoting `rcs_lockfile`/`rcs_lockfd` to locals of `rcs_internal_lockfile`
     threaded through to `rcs_internal_unlockfile` (they are only global so
     `rcs_cleanup`, `src/rcs.cpp:6968`, can unlink on signal — use a
     thread-local or an intrusive list instead), plus one lock-server socket per
     worker.
* **Estimated LoC:** (1) ~80. (2) ~400+ across `rcs.cpp`, `recurse.cpp`,
  `lock.cpp`, `mapping.cpp`.
* **Risk:** (1) **low**. (2) **high**.
* **Risk detail:** (1) read-only prefetch cannot corrupt anything; worst case it
  wastes I/O bandwidth. (2) **serious repository-corruption risk.** The signal
  handler `rcs_cleanup` unlinks `rcs_lockfile` on SIGINT/SIGTERM
  (`src/rcs.cpp:6976-6986`); with N in-flight rewrites it would have to unlink N
  temp files, and getting that wrong on a signal leaves stray `,file,` lockfiles
  that block all future writes to those paths. Output ordering (`cvs_output`
  `"T <file>"`) also becomes nondeterministic, which breaks scripts that parse
  it. Do not attempt (2) until F1-F5 are done and measured; the serial path may
  already be fast enough.

### F10: Whole-tree state held in memory for the duration

* **Location:** `src/tag.cpp:407` (`mtlist = getlist();`),
  `src/tag.cpp:478-497` (per-directory `tlist` + per-file node),
  `src/tag.cpp:437` (`dellist (&mtlist);` — only after *both* passes).
* **Complexity:** O(F) nodes + O(directories) hash arrays, live for the whole
  command.
* **Evidence.** `check_fileproc` creates one `List` per directory and one `Node`
  per file, with `xstrdup`ed key and data:

  ```c
  tlist = getlist ();                 // tag.cpp:485
  ...
  p = getnode ();
  p->key = xstrdup (finfo->file);     // tag.cpp:497
  ...
  p->data = RCS_getversion (...);     // tag.cpp:522
  ...
  (void) addnode (tlist, p);          // tag.cpp:562
  ```

  Every `getlist()` carries a `Node *hasharray[HASHSIZE]` with
  `HASHSIZE 151` (`src/hash.h:15,39`) = 1208 bytes, `memset` on each
  allocation.
* **Impact.** 500k files / 50k directories => ~60 MB of hash arrays plus
  ~100 MB of nodes and strings, none of it released until `dellist (&mtlist)`
  at `src/tag.cpp:437`. On a 32-bit server build this is an OOM; on 64-bit it
  is cache pressure and allocator churn during the phase that matters most.
* **Proposal.** Free each directory's `tlist` in `check_filesdoneproc` right
  after the `pretag` trigger has consumed it (`src/tag.cpp:569-602`) instead of
  keeping the whole `mtlist` alive. Or drop pass 1 entirely per F2(2).
* **Estimated LoC:** ~20.
* **Risk:** **low**.
* **Risk detail:** `pretag_proc` (`src/tag.cpp:604`) walks `tlist` and
  `check_filesdoneproc` re-finds it via `findnode_fn(mtlist, update_dir)`
  (`src/tag.cpp:574`); freeing after that point is safe as long as nothing else
  looks the directory up later — `mtlist` has no other reader in the file
  (`src/tag.cpp:407, 478, 493, 574, 437`). No repository-corruption risk.

## Hypotheses refuted

* **"Full parse when only the header is needed — the tag path calls
  `RCS_fully_parse`."** **Refuted.** `RCS_fully_parse` (`src/rcs.cpp:579`,
  declared `src/rcs.h:256`) has exactly one caller in the whole tree:
  `src/log.cpp:924`. The tag path only ever reaches `RCS_reparsercsfile`
  (`src/rcs.cpp:349`), which stops at `delta_pos` and never reads a deltatext.
  The real problem is that it does the header-only parse **three times** (F2),
  not that it does the wrong kind of parse.

* **"For a multi-hundred-MB `,v` this would be catastrophic."** **Partially
  refuted for binaries.** CAFS-backed (`-kB`) revisions store a 71-byte
  reference in the deltatext, not the content:
  `blob_reference_size = hash_type_magic_len + hash_encoded_size` = 7 + 64
  (`src/sha_blob_reference.h:15`), written by
  `RCS_write_binary_rev_data_blob` (`src/rcs_cvt_kB.cpp:6-18`). So a binary
  file's `,v` is a few tens of KB, not hundreds of MB, and F1's byte copy is
  bounded. **Confirmed for text**, though: the blob path is gated on
  `kf.flags & KFLAG_BINARY_DELTA` (`src/rcs_checkin.cpp:943`), so `-kv`/`-kb`
  files still carry their entire inline delta history and *are* copied in full
  on every tag.

* **"fsync/flush per file on rewrite."** **Refuted.** There is no `fsync`,
  `FlushFileBuffers`, or `_commit` anywhere in `src/rcs.cpp`. The only `fsync`
  calls in `src/` are in the `copy_file` family (`src/filesubr.cpp:135, 274,
  432`), which the tag path never enters. `rename_file`
  (`src/filesubr.cpp:758-770`) is a bare `rename(2)`. The per-file `rename` is
  still a metadata operation that must be journalled, but there is no
  durability barrier per file.

* **"Blob interaction — does tagging touch the blob store?"** **Refuted.** It
  does not. `src/rcs.cpp` references CAFS only via
  `get_binary_blob_ver_file_path` (`src/rcs.cpp:4288`), whose sole caller is
  `src/rcs_checkin.cpp:1474` (the commit path). `RCS_copydeltas` in the tag case
  copies raw bytes without interpreting them (`src/rcs.cpp:6926-6941`), so blob
  references pass through untouched. No blob is read, hashed, or
  reference-counted by `tag`, `rtag`, or `tag -b`. **This is correct
  behaviour** — no change needed.

* **"`RCS_settag` symbol-table cost — is the symbols list a linear list, making
  insert O(symbols)?"** **Refuted for the insert.** `List` is a doubly-linked
  list *plus* a 151-bucket hash (`src/hash.h:29-45`), so `findnode`
  (`src/hash.cpp:331`) and `addnode_at_front` are O(1) average. `RCS_settag`
  itself (`src/rcs.cpp:4786-4844`) is O(1). **But** the surrounding parse
  (`do_symbols`, O(S)) and serialise (`walklist`+`putsymbol_proc`, O(S)) *are*
  linear, which is the actual O(F*S) cost — see F5.

* **"Trigger/notify overhead per file — process spawn per file."** **Refuted
  for `taginfo`, confirmed for `historyinfo`.** `pretag` is invoked once per
  *directory* from `check_filesdoneproc` (`src/tag.cpp:569-602`) with the whole
  directory's file/version vectors, and `run_trigger` caches loaded trigger DLLs
  behind the `tf_loaded` static (`src/parseinfo.cpp:39, 151`), so there is no
  per-file DLL load. `parse_info` also caches each info file's contents in a
  `static std::map` (`triggers/info_trigger.cpp:904-908`). **However**,
  `history_write` invokes `run_trigger`/`historyinfo` *per file*
  (`src/history.cpp:834`), and if `CVSROOT/historyinfo` has a matching line,
  `parse_info_line` runs `CRunFile::run` — a real process spawn
  (`triggers/info_trigger.cpp:1252-1264`). See F4.

* **"Lock churn — is a lock taken per directory or per file? What is the syscall
  cost (create dir, write file, unlink)?"** **The filesystem-lock hypothesis is
  refuted in the default configuration.** With `lock_server` set (the default,
  `src/main.cpp:558`), `Reader_Lock` returns immediately
  (`src/lock.cpp:702-707`), `lock_tree_for_write` returns immediately
  (`src/lock.cpp:1238-1239`), and `lock_dir_for_write` returns immediately
  (`src/lock.cpp:1256-1261`) — so there is **no** `CVSLCK` mkdir / `#cvs.rfl`
  create / unlink per directory. The cost has simply moved: it is now **six
  network round trips per file** (F3). The `set_lock`/`clear_lock` mkdir dance
  (`src/lock.cpp:1064-1163`) only runs in the `LockServer=none` fallback.

* **"`lock_tree_for_write` costs an extra full recursion."** **Refuted in the
  default configuration.** `src/tag.cpp:426` calls it, but the first statement
  is `if(lock_server) return;` (`src/lock.cpp:1238-1239`). It *would* cost a
  third full `start_recursion` over the tree (`src/lock.cpp:1241-1246`) with
  `LockServer=none`.

* **"`tag_check_valid` scans the whole repository for `-r <tag>`."**
  **Refuted.** It short-circuits before the scan with an explicit comment —
  "val-tags sucks... Scanning the entire repository for a flippin' tag is just
  stupid" — and returns at `src/tag.cpp:1400`. The `val_fileproc` recursion
  below it (`src/tag.cpp:1275-1323`) is dead code.

* **"`src/rcs_checkin.cpp` participates in the tag path."** **Refuted.**
  `RCS_checkin` (`src/rcs_checkin.cpp:486`) is reachable only from `commit` /
  `import` / `add`. `src/tag.cpp` never references it. `rcs_checkin.cpp` is
  relevant here only because it is the *other* caller of the shared
  `rcs_internal_lockfile` / `RCS_putadmin` / `RCS_putdtree` machinery, which
  constrains the F1/F8 fixes (see F8's risk detail).

* **"`src/RecurseRepository.cpp` is part of the walk."** **Refuted.**
  `CRecurseRepository` is referenced only by its own header
  (`src/RecurseRepository.h:21-25`) and nothing instantiates it. It is
  unfinished experimental code (see the design sketch in the comment block at
  `src/RecurseRepository.cpp:60-78`) and contributes nothing at runtime. It also
  contains a latent bug — `cvs::sprintf(ent.logical_name,256,"%s/%s", a, "/",
  b)` (`src/RecurseRepository.cpp:102-103`) passes three arguments to a
  two-specifier format — which is further evidence it has never been executed.

* **"`src/run.cpp` / `src/logmsg.cpp` add per-file overhead."** **Refuted.**
  Neither is on the tag path: `src/tag.cpp` calls nothing from `logmsg.cpp`
  (no `Update_Logfile` / `do_verify`), and `run_exec` (`src/run.cpp:57`) is not
  reached — the trigger libraries spawn processes through their own `CRunFile`
  (`triggers/info_trigger.cpp:1252`), not through `src/run.cpp`.

* **"`server.cpp` forks a child per command."** **Refuted.** `serve_tag`
  (`src/server.cpp:3833`) -> `do_cvs_command` (`:3226`) -> `server_main`
  (`:6807`) calls `(*command)(argument_count, argument_vector)` in-process. The
  only per-command extras are the `precommand` / `postcommand` triggers
  (`src/server.cpp:6831, 6839`), fired once each.

## Recommended order of work

Ordered cheapest-highest-win first. Items 1-5 are **safe** (no
repository-corruption exposure, with the one companion change noted in 4) and
should land together as one measurable step; 6-8 need real testing; 9-10 need
design work and a soak test.

1. **Drop the post-rewrite re-parse.** Gate `free_rcsnode_contents` +
   `RCS_reparsercsfile` at `src/rcs.cpp:7199-7200` behind a new parameter and
   pass "don't re-parse" from the tag path. ~8 LoC, **low risk**. Removes one
   full `open` + `read` + `getdelta` sweep per file — ~1/3 of all parse work.
   *(F2.1)*

2. **Fix `RCS_magicrev`'s dead loop.** Delete `src/rcs.cpp:2257` and drop the
   `rev_num = 2` init at `:2259`. 2 LoC, **medium risk** (needs branch-numbering
   tests — the optimised path has never run). Removes an O(B*S) inner loop from
   every file in every `tag -b` / `rtag -b`. *(F6)*

3. **Fix the buffering.** Fixed 64 KB `RCSBUF_BUFSIZE`, pre-size the parse
   buffer from `st_size`, 1 MB copy buffer in `RCS_copydeltas`, `setvbuf` both
   streams in `RCS_rewrite`. ~25 LoC, **low risk**. Cuts read/write syscalls by
   ~10x on the header and ~128x on the delta copy. *(F1a, F7)*

4. **Remove the `fflush` from recursive `RCS_putdtree`** (`src/rcs.cpp:6734`),
   **and in the same commit** add an explicit `fflush(fout)` before
   `CVS_FTELL(fout)` at `src/rcs_checkin.cpp:952` so `delta_pos` is still
   computed against a flushed stream. ~10 LoC, **low risk given that companion
   change** — omit it and you get a wrong `delta_pos`, which is corruption.
   *(F8)*

5. **Make pass 1 take read locks, not write locks** — replace the
   `lock_for_write` global (`src/tag.cpp:282`) with an explicit per-recursion
   value. ~25 LoC, **low risk**. Halves lock contention against concurrent
   commits and stops a read-only pass from exclusively locking the whole repo.
   *(F3.2)*

6. **Batch the history writes.** Keep `CVSROOT/history` open for the command,
   and move the `historyinfo` trigger from per-file to per-directory (mirroring
   `pretag`). Add a `config` switch to restore the pre-`4e25b17` behaviour.
   ~125 LoC total, **medium risk** (trigger ABI — add a new vtable slot rather
   than change `history`'s). Biggest single win if `CVSROOT/historyinfo` is
   non-empty or `audit_trigger` is installed: removes up to 500 000 process
   spawns / SQL inserts. *(F4)*

7. **Remove the redundant rewrite-time lock.** In `rcs_internal_lockfile`
   (`src/rcs.cpp:7057`), reuse `rcs->rcsbuf.lockId` when it is already a write
   lock on the same path. ~10 LoC, **medium risk** — must be validated against
   `lockservice/` semantics, because getting it wrong means rewriting a `,v`
   without a lock. Removes 2 of 6 round trips per file. *(F3.1)*

8. **Stop exploding the symbol table.** Textual splice in `RCS_settag` +
   `strstr`-based "does this tag exist" so `RCS_putadmin` can take the fast
   `symbols_data` path. ~120 LoC, **medium-high risk** — needs a byte-exact
   round-trip test over a corpus of real `,v` files, and it changes today's
   (buggy) discarding of tag dates/comments. This is the fix that stops tag
   time growing with the tag count. *(F5)*

9. **Fuse or elide pass 1.** Skip the check recursion entirely when
   `!check_uptodate` and no loaded trigger implements `pretag`; otherwise carry
   the parsed `RCSNode`s forward. ~60-120 LoC, **medium risk** — it changes the
   all-or-nothing failure semantics documented at `src/tag.cpp:414-417`, so make
   it opt-in or keep an explicit pre-flight for the error cases only. Halves the
   remaining parse work and the directory scans. Also free per-directory
   `tlist`s eagerly (~20 LoC, low risk). *(F2.2, F10)*

10. **Only then consider structural changes.** Either (a) in-place header
    patching with a padding newphrase — **high risk, direct
    repository-corruption exposure**, needs a `,v` format upgrade path and
    compatibility testing against every other reader — or (b) parallelising the
    walk, which first requires de-globalising `rcs_lockfile` / `rcs_lockfd`
    (`src/rcs.cpp:218-219`), `lock_server_socket` (`src/lock.cpp:157`), and the
    `recurse.cpp` frame globals, and rethinking `rcs_cleanup`'s signal-time
    unlink. Do not start either until 1-9 have been measured; the combination of
    1, 3, 4, 6 and 8 removes most of the per-file constant factor and may make
    the structural work unnecessary.

**Operational note, no code required:** `rtag` writes one history record per
module (`src/tag.cpp:291`) whereas `tag` writes one per file
(`src/tag.cpp:1196`), and `rtag` needs no client-side working-copy walk or
`Entry` / `Directory` upload at all (`src/tag.cpp:281-296` vs `:262-272`). For
whole-tree tagging, `cvs rtag` is already substantially cheaper than `cvs tag`
today — worth telling users while the above lands.
