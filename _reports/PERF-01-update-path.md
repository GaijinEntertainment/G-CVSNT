---
id: PERF-01
area: update path (client + server)
---

# Why `cvs update` scales badly with file count

> All paths are relative to `/d/another/G-CVSNT/`. Source root is `cvsnt/cvsnt-2.5.05.3744/`.
> Read at commit `2d4b4db` (branch `master`).

## Executive summary

1. **Two synchronous lock-server round-trips per file, on every file, even up-to-date ones.**
   `rcsbuf_open()` takes a lock for *every* `,v` file it opens (`src/rcs.cpp:905`), and
   `freercsnode()` releases it (`src/rcs.cpp:766`). `lock_server_command()` is a blocking
   `send()`+`recv()` pair (`src/lock.cpp:236-243`). 300 k files ⇒ **600 k blocking RTTs**
   serialised on one socket. This is the single biggest file-count-linear cost.
2. **The "lazy RCS parse" optimisation has been deleted.** `RCS_parsercsfile_i()`
   unconditionally calls `RCS_reparsercsfile()` (`src/rcs.cpp:349`), which parses the whole
   admin block *and every delta node* (`src/rcs.cpp:527-537`). Update only ever needs
   head/expand/one revision. The `PARTIAL` flag survives only as a stale comment
   (`src/rcs.h:97`).
3. **`rcsbuf_valfree()` is O(revisions²) per file.** It linearly scans the whole relocation
   array (`src/rcs.cpp:1547`) and is called 4× per revision from `free_rcsvers_contents()`
   (`src/rcs.cpp:826-837`). A file with 2 000 revisions costs ~64 M pointer compares *to free*.
4. **`rcsbuf_fill()` grows the RCS buffer linearly, not geometrically** (`src/rcs.cpp:1431`,
   `MAX_INCR = 2 MiB` in `src/subr.cpp:160`) ⇒ O(size²/2 MiB) memcpy per big `,v`, and the whole
   `,v` is held in RAM.
5. **`Register()` does 2 × `fopen`+`fclose` per file and writes the Entries line twice**
   (`src/entries.cpp:399-427`; the duplicate comes from calling both `write_ent_proc` and
   `write_ent_ex_proc`, which itself re-writes `entfile` at `src/entries.cpp:129`).
   Client *and* server pay this. ~4 syscalls + 2× the log bytes per updated file.
6. **`write_entries()` byte-copies + `fsync()`s the Entries files twice per directory**
   (`src/entries.cpp:216-219` → `copy_file()` with `fsync` at `src/filesubr.cpp:135`).
   20 k directories ⇒ 40 k fsyncs on POSIX servers/clients.
7. **One `write()` syscall (and TCP segment) per output line on the server.** `cvs_output()`
   flushes whenever the string ends in `\n` (`src/server.cpp:6459-6461`), and `do_file_proc()`
   additionally calls `cvs_flushout()` per file (`src/recurse.cpp:967`).
8. **A libxml2 XPath is compiled and evaluated per checked-out file** to answer "is this file
   watched?" (`src/update.cpp:1724`, `2085`, `2147`, `2396`, `3214` →
   `cvsapi/XmlNode.cpp:172-196`, new context + namespace + function + variable registration +
   `xmlXPathEvalExpression` *every call*). O(files × fileattr-entries) per directory.
9. **`open_directory()` parses and checks out `.directory_history,v` per directory**
   (`src/mapping.cpp:1056-1116`) — another full RCS parse plus 2 lock RTTs per directory,
   plus 2–4 speculative `stat`s.
10. **Fixed 151-bucket hash + double duplicate-check on insert.** `HASHSIZE 151`
    (`src/hash.h:14`); `insert_before()` walks the bucket to reject duplicates
    (`src/hash.cpp:285-290`) and callers *already* did a `findnode_fn` first
    (`src/find_names.cpp:297-301`, `src/entries.cpp:1004`). Per directory this is
    O(n²/151); it only bites directories with thousands of files, and the extra scan is
    pure waste. `find_rcs()` also **leaks** the node when the name is already present.

## Cost model

Per-file / per-directory costs for a **no-op** `cvs update` of a checked-out tree
(`F` = files, `D` = directories, `R` = revisions in a `,v`, `S` = size of a `,v`).
"warm" = filesystem cache hot.

| Phase | Where | Complexity | Per-file syscalls / RTTs | Dominant cost |
|---|---|---|---|---|
| Client: walk sandbox | `send_files` → `start_recursion` (`src/client.cpp:5960`) | O(F) | 1 `lstat` (`src/vers_ts.cpp:389`) | protocol string building (~30 `send_to_server` calls/file, `src/client.cpp:5319-5375`) |
| Client: emit Entry/EntryExtra/Unchanged | `send_fileproc` (`src/client.cpp:5297`) | O(F) | 0 (batched, flush every 2×80 KiB — `src/client.cpp:4127-4136`) | 3 protocol lines/file |
| Client: `? file` detection | `ignore_files` (`src/ignore.cpp:377`) | O(D) opendir + O(unknown) `lstat` + O(unknown × ign_patterns) `fnmatch` | 1 `lstat` per *unknown* file | extra full readdir per directory |
| Server: `Directory` request | `dirswitch` (`src/server.cpp:1304`) | O(D) | `mkdir_p` + `create_adm_p` + `chdir` + `mkdir` + 3× (`fopen`+`fclose`) | materialising a shadow sandbox |
| Server: `Entry`/`Unchanged` dispatch | `src/server.cpp:5341-5342` | O(F × 90) `strlen`+`strncmp` | 0 | `strlen(rq->name)` recomputed inside the loop |
| Server: enumerate names | `Find_Names` (`src/find_names.cpp:55`) | O(D) × 2 readdir (repo + Attic); O(n²/151) inserts | 2 `opendir` per dir | `fnmatch("*,v")` per dirent; double dup-check |
| Server: per-directory open | `open_directory` (`src/mapping.cpp:1009`) | O(D) | 1–2 `fopen` + **2 lock RTTs** + full `.directory_history,v` parse + `RCS_checkout` + 2–4 `stat` | RCS parse + lock RTTs |
| **Server: per-file RCS open** | `RCS_parse` → `rcsbuf_open` (`src/rcs.cpp:869`) | O(F) | **2 lock RTTs**, 1–3 `fopen`, 1–2 `fclose` (2nd `fopen` is POSIX-only, `src/rcs.cpp:921`) | **lock RTT latency** |
| **Server: per-file RCS parse** | `RCS_reparsercsfile` (`src/rcs.cpp:361`) | O(S) read + O(R²/151) inserts + O(S²/2 MiB) realloc-copy | ⌈S/80 KiB⌉ `fread` | full delta-tree materialisation |
| Server: timestamp | `time_stamp_server` (`src/vers_ts.cpp:416`) | O(F) | 1 `lstat` | — |
| **Server: per-file RCS free** | `freercsnode` → `free_rcsvers_contents` (`src/rcs.cpp:820`) | **O(R²)** | 1 `fclose` + 1 lock RTT | `rcsbuf_valfree` linear scan |
| Server: per-changed-file checkout | `checkout_file` (`src/update.cpp:1585`) | O(changed) | 1 XPath compile+eval, 1 `stat`+`fopen`+`write`+`fclose` on `CVSROOT/history` (`src/history.cpp`) | XPath + shared history file |
| Server: per-changed-file response | `server_updated` (`src/server.cpp:4356`) | O(changed) | 1 `write()` per output line (`src/server.cpp:6460`) | small TCP segments |
| Client: apply | `update_entries` → `Register` (`src/client.cpp:2597`) | O(changed) | 2 `fopen` + 2 `fclose` + 2 `write` | Entries.Log written twice |
| Client: blob fetch | `add_download_queue` (`src/download_blob_to.cpp:334`) | O(changed) / ≤8 threads | 1 `getcwd` + `fopen`/`fwrite`/`fclose`/`chmod`/`rename`/`utime`/`stat` (+1 `stat` if validating) | ~7 syscalls + 1 HTTP req per blob |
| Both: leave directory | `Entries_Close` → `write_entries` (`src/entries.cpp:141`) | O(D) | 2 full file copies + **2 `fsync`** + 2 `rename` + 2 `unlink` | fsync latency |

**Bottom line for a no-op update of 300 000 files in 20 000 directories (Linux server,
lock server enabled):** ≈ 640 000 lock-server round-trips, ≈ 900 000 `open`/`close`,
≈ 300 000 `lstat`, 40 000 `fsync`, and every `,v` file in the repository read and fully
parsed — before a single byte of user data moves.

---

## Findings

### F1: Two blocking lock-server round-trips for **every** RCS file opened

- **Location:** `cvsnt/cvsnt-2.5.05.3744/src/rcs.cpp:905` (lock) and
  `cvsnt/cvsnt-2.5.05.3744/src/rcs.cpp:766` (unlock);
  transport at `cvsnt/cvsnt-2.5.05.3744/src/lock.cpp:227-249`.
- **Complexity:** O(F) synchronous request/response round-trips, serialised on one socket.
  Zero parallelism, zero batching, zero pipelining.
- **Evidence:**

  `src/rcs.cpp:869-937`, inside `rcsbuf_open()`:
  ```c
  fp = CVS_FOPEN(filename, FOPEN_BINARY_READ);
  ...
  if(!orig_lockId)
  {
      rcsbuf->lockId=do_lock_file(filename, NULL, lock_for_write, 1);
      if(!rcsbuf->lockId) { rcsbuf_close(rcsbuf); return 0; }
  #ifndef _WIN32
      fp = CVS_FOPEN(filename, FOPEN_BINARY_READ);   /* second open, POSIX only */
      ...
      fclose(rcsbuf->fp);
      rcsbuf->fp = fp;
  #endif
  }
  ```
  `src/lock.cpp:362-366`:
  ```c
  size_t do_lock_file(const char *file, const char *repository, int write, int wait)
  {
      if(!lock_server) return (size_t)-1;
      return do_lock_server(file,repository,write?"Write":"Read", wait);
  }
  ```
  `src/lock.cpp:227-249`, `lock_server_command()`:
  ```c
  if(send(lock_server_socket,line,strlen(line),0)<=0) ...
  if((l=recv(lock_server_socket,line,line_len,0))<=0) ...
  ```
  Release, `src/rcs.cpp:764-767` in `freercsnode()`:
  ```c
  if((*rnodep)->rcsbuf.lockId)
      do_unlock_file((*rnodep)->rcsbuf.lockId);
  ```

  Call chain: `update()` → `do_update()` → `start_recursion()` (`src/update.cpp:651`) →
  `do_recursion()` → `walklist(filelist, do_file_proc)` (`src/recurse.cpp:806`) →
  `do_file_proc()` → `RCS_parse(finfo->mapped_file, mapped_file_repository)`
  (`src/recurse.cpp:915`) → `RCS_parsercsfile_i()` → `rcsbuf_open()`.
  `freercsnode(&finfo->rcs)` at `src/recurse.cpp:957` closes the loop.
- **Impact:** Exactly 2 network round-trips per file, unconditionally, including files that
  are already up to date and produce no output at all. At a 100 µs loopback RTT (UDS on
  Linux, `src/lock.cpp:180-183`) that is 60 s of pure latency for 300 k files; over TCP to
  another host it is minutes. It is entirely file-count-linear and entirely latency-bound —
  it does not shrink with faster disks or more data-volume optimisation.
  Note the perverse trade: with `lock_server` set, the *per-directory* read lock is skipped
  outright (`Reader_Lock` returns 0 at `src/lock.cpp:705-709`), so the design replaced
  1 lock per directory with 2 locks per file.
- **Proposal:**
  (a) Short term: skip file locking entirely for read-only recursions. Add a
  `rcsbuf_open(..., bool need_lock)` parameter and pass `false` from
  `RCS_parsercsfile_i()` when a per-directory read lock is already held or when
  `command_name` is a pure reader (`update`/`checkout`/`status`/`diff`/`log`). Take the
  cheap per-directory `Reader_Lock` instead.
  (b) Medium term: add a batched `LockMany <dir>|<f1>|<f2>|…` verb to the lock service and
  acquire one directory's worth of locks in one round-trip, plus a matching `UnlockMany`.
  (c) Either way, pipeline: issue the Lock for file *n+1* before processing file *n*.
- **Estimated LoC:** (a) ~40 in `src/rcs.cpp` + `src/lock.cpp`; (b) ~150 across
  `src/lock.cpp` and `lockservice/`.
- **Risk:** medium.
- **Risk detail:** These locks exist to stop a concurrent `commit` swapping the `,v` between
  `Find_Names` and `RCS_parse`, and to stop reading a half-written `,v` (see the rationale at
  `src/lock.cpp:28-40`). Dropping them without substituting a directory-level read lock
  re-opens that race. Option (a) is only safe if `Reader_Lock` is re-enabled for the
  directory when `lock_server` is set — currently it is a no-op. Option (b) preserves
  semantics exactly and is the safer of the two, at the cost of touching the lock daemon.

### F2: `RCS_parse` always fully parses the delta tree; the lazy path was removed

- **Location:** `cvsnt/cvsnt-2.5.05.3744/src/rcs.cpp:349` (unconditional call);
  the parse loop at `cvsnt/cvsnt-2.5.05.3744/src/rcs.cpp:527-537`.
- **Complexity:** O(size of `,v`) I/O and O(R) allocations per file, where update needs
  only `head`, `expand`, and one `RCSVers` node. Plus O(R²/151) for the `versions` list
  inserts (see F5).
- **Evidence:**

  `src/rcs.cpp:335-351` — the comment still describes the lazy design, the code no longer
  implements it:
  ```c
  /* Process HEAD, BRANCH, and EXPAND keywords from the RCS header.

     Most cvs operations on the main branch don't need any more
     information.  Those that do call RCS_reparsercsfile to parse
     the rest of the header and the deltas.  */
  if(!rcsbuf_open(&rdata->rcsbuf, rcsfile))
  { ... return NULL; }

  RCS_reparsercsfile(rdata);      /* <-- unconditional */
  ```
  `src/rcs.cpp:527-537` — every delta node is materialised:
  ```c
  while ((vnode = getdelta (&rdata->rcsbuf, rcsfiledatapath, &key, &value)) != NULL)
  {
      q = getnode ();
      q->type = RCSVERS;
      q->delproc = rcsvers_delproc;
      q->data = (char *) vnode;
      q->key = vnode->version;
      addnode (rdata->versions, q);
  }
  ```
  `getdelta()` (`src/rcs.cpp:6148`) reads `date`, `author`, `state`, `branches`,
  `properties`, `next` and every newphrase for each revision.
  The `PARTIAL` flag it used to be gated on no longer exists — the only occurrence in the
  tree is the stale comment at `src/rcs.h:97`; `src/rcs.h:39-40` define only `VALID` and
  `INATTIC`.

  Consumers on the update path that genuinely need `rcs->versions`:
  `RCS_isdead()` (`src/rcs.cpp:3673`), `RCS_getrevtime()` (`src/rcs.cpp:3116`),
  `RCS_getexpand()` (`src/rcs.cpp:3691`), `RCS_getproplist()` (`src/rcs.cpp:3017`) —
  all called from `Version_TS()` (`src/vers_ts.cpp:296-312`) and `Classify_File()`
  (`src/classify.cpp:90`). Each looks up **exactly one** revision.
- **Impact:** File-count-linear: the update reads and parses *every* `,v` in the tree,
  not just the ones that changed. In a CAFS repository the `,v` payload is a 64-char blob
  reference, so the `,v` file is nearly all delta metadata and log text — i.e. almost the
  entire file is parsed for nothing. For long-lived game assets with thousands of
  revisions the per-file constant is large and grows over the project's lifetime, which is
  exactly the "gets slower every year" symptom.
- **Proposal:** Reinstate a two-phase parse. Phase 1 (`RCS_parse`) reads only until the
  first revision key and records `delta_pos` (the code already stores it,
  `src/rcs.cpp:558`). Set a `PARTIAL` flag. Phase 2 (`RCS_reparsercsfile`) is called
  on demand from a new `rcs_ensure_deltas(RCSNode*)` guard placed at the top of
  `RCS_isdead`, `RCS_getrevtime`, `RCS_getproplist`, `RCS_getexpand`, `RCS_getversion`,
  `RCS_checkout`, `RCS_symbols`. For the overwhelmingly common
  "no tag, HEAD, unchanged" case, phase 2 never runs.
  Bigger win, later: keep an in-`,v` index (offset table) so a single revision can be
  seeked to without walking all deltas.
- **Estimated LoC:** ~120 in `src/rcs.cpp` (+ a one-line guard in ~8 accessors).
- **Risk:** medium-high.
- **Risk detail:** ~80 call sites touch `rcs->versions`, `rcs->symbols`, `rcs->locks`,
  `rcs->other` directly rather than through accessors (`src/rcs.cpp`, `src/log.cpp`,
  `src/tag.cpp`, `src/admin.cpp`, `src/annotate.cpp`, `src/commit.cpp`). Any one that is
  missed reads a `NULL`/empty `versions` list and silently produces wrong output (e.g.
  "file is dead" for a live file). This needs an assert in `findnode(rcs->versions, …)`
  paths during a soak period, plus the full `testcvs/sanity.sh` suite. Do F1 and F5 first;
  they are cheaper and lower-risk.

### F3: `Register()` opens and closes two files per registered file, and writes the entry twice

- **Location:** `cvsnt/cvsnt-2.5.05.3744/src/entries.cpp:394-428`.
- **Complexity:** O(changed files) × (2 `open` + 2 `write` + 2 `close`), and 2× the
  intended Entries.Log volume — which then costs 2× in `AddEntryNode` on the next
  `Entries_Open`.
- **Evidence:** `src/entries.cpp:394-428`:
  ```c
  entfilename = CVSADM_ENTLOG;
  entexfilename = CVSADM_ENTEXTLOG;
  entfile = CVS_FOPEN (entfilename, "a");
  entexfile = CVS_FOPEN (entexfilename, "a");
  ...
  if (fprintf (entfile, "A ") < 0) ...
  if (fprintf (entexfile, "A ") < 0) ...

  write_ent_proc (node, NULL);        /* -> fputentent(entfile, …)  entries.cpp:111 */
  write_ent_ex_proc (node, NULL);     /* -> fputentent(entfile, …)  entries.cpp:129
                                         AND fputententex(entexfile, …) entries.cpp:131 */
  if (fclose (entfile) == EOF) ...
  if (fclose (entexfile) == EOF) ...
  ```
  `write_ent_ex_proc` (`src/entries.cpp:120-136`) writes **both** files; `write_ent_proc`
  (`src/entries.cpp:102-117`) writes `entfile` only. Calling both means the `/name/rev/ts//`
  line lands in `CVS/Entries.Log` twice — once prefixed `A `, once bare. `fgetentent`
  treats a bare line as an implicit `A` (`src/entries.cpp:470-478`), so it is idempotent
  and has gone unnoticed; it is pure waste.

  Call chain (client): `get_responses_and_close()` → `call_in_directory()` →
  `update_entries()` (`src/client.cpp:1436`) → `Register()` (`src/client.cpp:2092`),
  and `update_blob_ref_entries()` → `Register()` (`src/client.cpp:2597`).
  Call chain (server): `update_fileproc()` → `checkout_file()` → `Register()`
  (`src/update.cpp:1836` region) and `src/update.cpp:900`.
  The same open/close-per-call pattern also exists in `Scratch_Entry`
  (`src/entries.cpp:249`), `Rename_Entry` (`src/entries.cpp:289`, `300`, `311`) and
  `Subdir_Register`/`Subdir_Deregister` (`src/entries.cpp:1338`).
- **Impact:** Every changed file costs 4 file-handle operations on both peers. On Windows
  clients `CreateFile`/`CloseHandle` are ~10–50 µs each with an AV filter in the path, so a
  300 k-file checkout burns ~30–60 s in nothing but opening and closing `CVS/Entries.Log`.
- **Proposal:** Keep `entfile`/`entexfile` open across the directory. Add
  `entries_log_open(void)` / `entries_log_close(void)` and call them from
  `call_in_directory()` (client, `src/client.cpp:909-1160`) and from `do_recursion()`
  around the `walklist(filelist, do_file_proc)` (server, `src/recurse.cpp:806`); make
  `Register`/`Scratch_Entry`/`Rename_Entry` reuse the handle. Separately, delete the
  redundant `write_ent_proc (node, NULL);` at `src/entries.cpp:422`.
- **Estimated LoC:** ~50 (plus a 1-line deletion for the duplicate write).
- **Risk:** low for the duplicate-write deletion; medium for the handle caching.
- **Risk detail:** `Register()` is also reached on abort paths and from
  `server_register()`; a leaked or unflushed handle at `error_exit()` would lose the
  Entries.Log and leave the sandbox inconsistent. Mitigate by flushing (not closing) in the
  existing `Lock_Cleanup`/`server_cleanup` hooks, and by keeping the current
  fopen-if-not-open fallback so an unpaired `Register` still works.

### F4: `write_entries()` byte-copies and `fsync()`s the Entries files twice per directory

- **Location:** `cvsnt/cvsnt-2.5.05.3744/src/entries.cpp:216-219`;
  the copy at `cvsnt/cvsnt-2.5.05.3744/src/filesubr.cpp:36-150` (`fsync` at line 135).
- **Complexity:** O(D) × (2 × full-file read+write + 2 `fsync` + 2 `rename` + 2 `unlink`
  + 4 `lstat`).
- **Evidence:** `src/entries.cpp:213-219`:
  ```c
  /* First make a copy in the .Old files (note we don't rename so that the
     entries files always exist) */
  copy_file (CVSADM_ENT, CVSADM_ENTOLD, 1, 0);
  rename_file (entfilename, CVSADM_ENT);
  copy_file (CVSADM_ENTEXT, CVSADM_ENTEXTOLD, 1, 0);
  rename_file (entexfilename, CVSADM_ENTEXT);
  ```
  `src/filesubr.cpp:51-57` (`islink`) + `:60` (`isdevice`) = 2 `lstat` before each copy;
  `:111-125` copies in `BUFSIZ` (8 KiB) chunks; `:134-136`:
  ```c
  #ifdef HAVE_FSYNC
      if (fsync (fdout))
          error (1, errno, "cannot fsync file %s after copying", fn_root(to));
  #endif
  ```
  `HAVE_FSYNC` is **on** for POSIX builds (`config.h:141`) and off for Windows
  (`windows-NT/config.h:121`).
  Trigger: `Entries_Close()` (`src/entries.cpp:965-985`) calls `write_entries()` whenever
  `CVS/Entries.Log` exists — i.e. whenever any file in that directory was `Register`ed.
  Reached per directory from `do_recursion()` (`src/recurse.cpp:867`) on the server and
  from `call_in_directory()` (`src/client.cpp:915`) on the client.
- **Impact:** Directory-count-linear, and file-count-linear in bytes copied (Entries grows
  with files-per-directory). On a Linux server, 20 000 directories ⇒ 40 000 `fsync`s;
  at 1–10 ms each that alone is 40–400 s of wall time that no amount of I/O parallelism
  hides, because it is issued serially on the recursion thread.
- **Proposal:** (a) Drop the `.Old` copies entirely — they are a belt-and-braces backup of a
  file that is already being replaced atomically by `rename`; if they must stay, replace
  `copy_file` with `link()`/`CreateHardLink` (O(1), no fsync). (b) Add a
  `copy_file_nosync()` variant, or thread a `sync` flag through `copy_file`, and use it
  here — the `rename` provides the atomicity that matters, the `fsync` of a *backup* buys
  nothing.
- **Estimated LoC:** ~25.
- **Risk:** low.
- **Risk detail:** `CVS/Entries.Old` is a manual-recovery aid only; nothing in the tree
  reads it (`grep -n CVSADM_ENTOLD src/` shows only `entries.cpp:216`). Removing the
  `fsync` slightly widens the window in which a power loss leaves `Entries.Old` stale —
  acceptable for a backup. Hard-linking changes behaviour if a user edits `Entries` in
  place expecting `Entries.Old` to be independent; the fsync-only change avoids that.

### F5: `rcsbuf_valfree()` is a linear scan called O(R) times ⇒ O(R²) to free one RCS file

- **Location:** `cvsnt/cvsnt-2.5.05.3744/src/rcs.cpp:1540-1553`, called from
  `cvsnt/cvsnt-2.5.05.3744/src/rcs.cpp:826-837`.
- **Complexity:** O(R²) pointer comparisons per file, where R = revisions in the `,v`.
- **Evidence:** the array is appended to once per in-place value copy
  (`src/rcs.cpp:1512-1521`):
  ```c
  if(valp && !malloc)
  {
      if(rcsbuf->reloc_ptr_count>=rcsbuf->reloc_ptr_size)
      {
          rcsbuf->reloc_ptr_size+=128;
          rcsbuf->reloc_ptr_base=(char***)xrealloc(...);
      }
      rcsbuf->reloc_ptr_base[rcsbuf->reloc_ptr_count++]=valp;
      *valp=ret;
  }
  ```
  and searched linearly on every free (`src/rcs.cpp:1540-1553`):
  ```c
  static void rcsbuf_valfree(struct rcsbuffer *rcsbuf, char **valp)
  {
      ...
      for(char ***cp=rcsbuf->reloc_ptr_base;cp<rcsbuf->reloc_ptr_base+rcsbuf->reloc_ptr_count;cp++)
          if(*cp==valp)
              *cp=NULL;
      *valp=NULL;
  }
  ```
  `getdelta()` registers ~4–6 entries per revision (`author`, `state`, `next`, `type`, plus
  each newphrase), so `reloc_ptr_count ≈ 5R`. `free_rcsvers_contents()`
  (`src/rcs.cpp:820-845`) then calls `rcsbuf_valfree` 4× per revision:
  ```c
  rcsbuf_valfree(rnode->rcsbuf,&rnode->next);
  rcsbuf_valfree(rnode->rcsbuf,&rnode->author);
  if(rnode->rcsbuf) rcsbuf_valfree(rnode->rcsbuf,&rnode->state);
  else if(rnode->rcsbuf) rcsbuf_valfree(rnode->rcsbuf,&rnode->type);
  ```
  Call chain: `do_file_proc()` → `freercsnode(&finfo->rcs)` (`src/recurse.cpp:957`) →
  `free_rcsnode_contents()` → `dellist(&rnode->versions)` (`src/rcs.cpp:786`) →
  `delnode` → `rcsvers_delproc` → `free_rcsvers_contents`.
- **Impact:** 4R × 5R = 20R² compares **per file, on teardown**. R = 500 ⇒ 5 M;
  R = 2 000 ⇒ 80 M; R = 5 000 ⇒ 500 M — per file. This is the reason the same tree gets
  slower every year even when the file count is constant, and it multiplies the file-count
  cost by a factor that grows with repository age.
- **Proposal:** During `freercsnode()` nothing can call `rcsbuf_fill()`, so the relocation
  table is dead. Add `bool tearing_down` to `struct rcsbuffer`; set it at the top of
  `free_rcsnode_contents()` (`src/rcs.cpp:782`) and make `rcsbuf_valfree()` do only
  `*valp = NULL` when it is set. Reset `reloc_ptr_count = 0` once, after the teardown.
  (Alternative, if a non-teardown caller ever needs the removal: store the slot index next
  to the value, or use an open-addressed set keyed on `valp`.)
- **Estimated LoC:** ~15.
- **Risk:** low.
- **Risk detail:** The only hazard is a `rcsbuf_fill()` (i.e. a further `fread`) happening
  between setting the flag and clearing the table, which would relocate a stale pointer.
  `free_rcsnode_contents()` does no reads — it only frees — and `rcsbuf_close()` is called
  before it in `freercsnode()` (`src/rcs.cpp:769`), which already sets `fp = NULL` and
  drops the buffer. Add an `assert(!rcsbuf->fp)` alongside the flag to make that invariant
  explicit.

### F6: `rcsbuf_fill()` grows the RCS buffer by a constant, and never releases it

- **Location:** `cvsnt/cvsnt-2.5.05.3744/src/rcs.cpp:1424-1478`; `expand_string`
  at `cvsnt/cvsnt-2.5.05.3744/src/subr.cpp:166-185` with
  `MAX_INCR = 2*1024*1024` (`src/subr.cpp:160`).
- **Complexity:** O(S²/2 MiB) bytes memcpy'd per `,v` of size S, plus
  O(reloc_ptr_count) pointer fixups per realloc. Peak RSS = size of the largest `,v`.
- **Evidence:** `src/rcs.cpp:1424-1432`:
  ```c
  if (rcsbuf->ptrend - rcsbuf->buffer + RCSBUF_BUFSIZE > rcsbuf->buffer_size)
  {
      ...
      expand_string (&rcsbuf->buffer, &rcsbuf->buffer_size,
                     rcsbuf->buffer_size + RCSBUF_BUFSIZE);
  ```
  `RCSBUF_BUFSIZE = BUFSIZ*10` (`src/rcs.cpp:864`) ≈ 80 KiB.
  `expand_string` (`src/subr.cpp:166-185`) doubles only while `*n < MAX_INCR`; above
  2 MiB it does `*n += MAX_INCR`. Since the requested `newsize` is only
  `buffer_size + 80 KiB`, each call grows the buffer by exactly one `MAX_INCR` step ⇒
  **linear growth**, and `xrealloc` copies the whole buffer each time it cannot extend
  in place. On top of that, every move runs the relocation loop
  (`src/rcs.cpp:1438-1450`), which is O(reloc_ptr_count) = O(R).
  The buffer is never compacted: `rcsbuf_fill` only appends (`rcsbuf->ptrend += got`), so
  the entire `,v` ends up resident.
- **Impact:** A 40 MiB `,v` (very possible for a heavily-revised asset with long log
  messages) costs 20 reallocs and ~400 MiB of memcpy, and 40 MiB of RSS held for the
  duration of that one file. This is a per-file constant, so it multiplies the file-count
  problem. It also interacts with F5: the relocation loop is O(R) per realloc.
- **Proposal:** Two independent fixes.
  (a) Make the growth geometric: pass `rcsbuf->buffer_size * 2` (floored at
  `buffer_size + RCSBUF_BUFSIZE`) to `expand_string`, or raise `MAX_INCR`.
  (b) Better: pre-size the buffer once from the file size. `rcsbuf_open()` already has the
  `FILE*`; `fstat(fileno(fp))` and `expand_string(..., sb.st_size + 1)` eliminates all
  reallocs and all relocation loops for the common case.
- **Estimated LoC:** ~15.
- **Risk:** low.
- **Risk detail:** (b) increases peak RSS for pathological `,v` files that were previously
  read incrementally — but they were fully buffered anyway, so the ceiling is unchanged;
  only the allocation is earlier. Cap the pre-size (e.g. at 64 MiB) and fall back to
  incremental growth beyond that. Watch out that `expand_string` moving the buffer must
  still run the relocation loop — pre-sizing before any `rcsbuf_valcopy` runs means
  `reloc_ptr_count == 0` at that point, so it is a no-op.

### F7: One `write()`/TCP segment per output line on the server

- **Location:** `cvsnt/cvsnt-2.5.05.3744/src/server.cpp:6459-6461`;
  per-file flush at `cvsnt/cvsnt-2.5.05.3744/src/recurse.cpp:967`.
- **Complexity:** O(reported files) `write()` syscalls, each carrying ~20–200 bytes.
- **Evidence:** `src/server.cpp:6459-6461`, in `cvs_output()`:
  ```c
  buf_output (stdout_buf?stdout_buf:buf_to_net, str, len);
  if(str[len-1]=='\n')
      buf_send_output(stdout_buf?stdout_buf:buf_to_net);
  ```
  `buf_send_output()` (`src/buffer.cpp:249-302`) calls `buf->output(...)` immediately;
  for the wrapped stdout buffer that recurses into
  `buf_send_output(pb->buf)` on `buf_to_net` (`src/buffer.cpp:1741`), i.e. a real
  `fd_buffer_output` → `write()`.
  `cvs_output()` also calls `cvs_flusherr()` first (`src/server.cpp:6416`).
  Producers: `write_letter()` (`src/update.cpp:2258-2301`) emits `U <path>\n` per file via
  `cvs_output_tagged`; `do_file_proc()` calls `cvs_flushout()` unconditionally per file
  (`src/recurse.cpp:967`), and the request dispatcher calls
  `buf_send_output(stderr_buf); buf_send_output(stdout_buf);` after **every** protocol
  request (`src/server.cpp:5401-5402`) — i.e. 3× per file for Entry/EntryExtra/Unchanged.
- **Impact:** For a checkout that reports 300 k files, ~300 k small `write()` calls and
  ~300 k TCP segments. With Nagle disabled this saturates the packet rate rather than the
  bandwidth; with Nagle on it adds per-file latency. The comment at `src/recurse.cpp:964`
  ("Doing this once per file should be no big deal") was written for repositories three
  orders of magnitude smaller.
- **Proposal:** Change the `cvs_output` flush condition from "ends in newline" to
  "buffered bytes ≥ 8 KiB, or an explicit flush point". Keep explicit flushes at
  end-of-directory (`update_filesdone_proc`, `src/update.cpp:1021`), before any read from
  the client, and at command end. Delete the unconditional `cvs_flushout()` in
  `do_file_proc` (`src/recurse.cpp:967`) or make it a counter-gated flush every N files.
- **Estimated LoC:** ~30.
- **Risk:** medium.
- **Risk detail:** The protocol requires the server to have flushed before it blocks on a
  client read (`serve_valid_requests` already does this explicitly at
  `src/server.cpp:5001`) — miss one such point and the session deadlocks. Audit every
  `server_read_line`/`buf_read_line` call in `src/server.cpp` and ensure a flush precedes
  it. Also, the per-file flush is what makes `tail -f`-style progress work; users will see
  output arrive in bursts. Gate on a byte threshold rather than removing flushing so
  progress still appears.

### F8: A libxml2 XPath is compiled and evaluated per file to answer "is this file watched?"

- **Location:** `cvsnt/cvsnt-2.5.05.3744/src/update.cpp:1724-1732` (`checkout_file`),
  `:2085-2090` and `:2147-2153` (`patch_file`), `:2396-2402` (`merge_file`),
  `:3214-3221` (`join_file`);
  implementation at `cvsnt/cvsnt-2.5.05.3744/cvsapi/XmlNode.cpp:172-196`.
- **Complexity:** O(changed files) XPath compilations, each O(entries in
  `CVS/fileattr.xml` for that directory) to evaluate ⇒ O(n²) per directory when
  fileattr has an entry per file.
- **Evidence:** `src/update.cpp:1724-1732`:
  ```c
  CXmlNodePtr node = fileattr_getroot();
  node->xpathVariable("name",xfile);

  if (!is_rcs
      && cvswrite
      && !file_is_dead
      && (!node->Lookup("file[cvs:filename(@name,$name)]/watched") || !node->XPathResultNext())
      && !(kftmp.flags&KFLAG_RESERVED_EDIT))
  ```
  `cvsapi/XmlNode.cpp:172-196`, `CXmlNode::Lookup()`:
  ```c
  xpathCtx = xmlXPathNewContext(m_tree->m_doc);           /* new context every call   */
  ...
  r1 = xmlXPathRegisterNs(xpathCtx, ... "cvs" ...);       /* re-register namespace    */
  r2 = xmlXPathRegisterFuncNS(xpathCtx, ... "filename" ...);
  r3 = xmlXPathRegisterFuncNS(xpathCtx, ... "username" ...);
  for(... i = m_xpathVars.begin(); i!=m_xpathVars.end(); ++i)
      xmlXPathRegisterVariable(xpathCtx, ..., xmlXPathNewCString(i->second.c_str()));
  xpathObj = xmlXPathEvalExpression((const xmlChar *)path, xpathCtx);
  ```
  The expression is re-*parsed* from a string on every call (no `xmlXPathCompile` cache),
  and `cvs:filename()` is a user-defined function, which forces libxml2 to call back into
  CVSNT for every candidate node.
  `fileattr_getroot()` (`src/fileattr.cpp:80-87`) additionally `Clone()`s the root node per
  call. The XML file itself *is* read only once per directory (`fileattr_read` is lazy,
  `src/fileattr.cpp:270`, and `fileattr_startdir`/`fileattr_write`/`fileattr_free` bracket
  the directory at `src/recurse.cpp:724`/`848-849` and `:1272`/`1380-1381`) — so the file
  I/O is fine; it is the *query* that is per-file.
- **Impact:** Only on files that are actually checked out / patched / merged, so a no-op
  update does not pay it — but a first checkout or a large sync pays it on every file. In a
  directory with 5 000 files and a `fileattr.xml` listing 5 000 of them, this is 25 M node
  visits plus 5 000 XPath compilations, per directory.
- **Proposal:** Hoist the query out of the per-file loop. Add
  `fileattr_watched_set()` that runs **one** XPath (`file/watched/..`) per directory in
  `fileattr_startdir()`, materialises the watched filenames into a `List` (or
  `std::unordered_set` with the `fncmp` folding rule), and expose
  `bool fileattr_is_watched(const char *name)`. Replace all five `Lookup(...)/watched`
  sites with that O(1) test. As a cheap intermediate step, cache the compiled expression
  with `xmlXPathCompile`/`xmlXPathCompiledEval` and reuse one `xmlXPathContext` per
  directory.
- **Estimated LoC:** ~70 (`src/fileattr.cpp` + `src/fileattr.h` + 5 call sites).
- **Risk:** low-medium.
- **Risk detail:** `cvs:filename()` implements CVSNT's case-folding/encoding-aware filename
  comparison (`XpathFilename` in `cvsapi/XmlNode.cpp`); the replacement lookup must use the
  same `fncmp` semantics or files on case-insensitive filesystems will stop being reported
  as watched, silently making files writable that should not be. The set must also be
  invalidated if `fileattr_setvalue`/`fileattr_delete` mutate the tree mid-directory
  (`edit.cpp` does this) — guard with the existing `modified` flag
  (`src/fileattr.cpp:21`).

### F9: `open_directory()` costs a full RCS parse + 2 lock round-trips per directory

- **Location:** `cvsnt/cvsnt-2.5.05.3744/src/mapping.cpp:1009-1196`, called from
  `cvsnt/cvsnt-2.5.05.3744/src/recurse.cpp:1259`.
- **Complexity:** O(D) × (1–2 `fopen` + 2 lock RTTs + full parse of
  `.directory_history,v` + one `RCS_checkout` + 2–4 `stat`).
- **Evidence:** `src/mapping.cpp:1056-1116`:
  ```c
  current_directory->repository_rcsfile = RCS_parse(RCSREPOVERSION,repository);
  ...
      rev = RCS_head(current_directory->repository_rcsfile);
  ...
  retcode = RCS_checkout(current_directory->repository_rcsfile, NULL, (char*)rev,
                         (char*)tag, NULL, NULL, repository_checkoutproc, NULL, NULL);
  ```
  `RCS_parse` (`src/rcs.cpp:256`) tries `repos/.directory_history,v` then
  `repos/Attic/.directory_history,v` — so in repositories that do not use directory
  versioning it is **two failed `fopen`s per directory**; where it does exist it is a full
  `rcsbuf_open` (F1: 2 lock RTTs) plus a full `RCS_reparsercsfile` (F2) plus a checkout.
  Then `src/mapping.cpp:1122-1188` does `isfile(dir/CVS/Renamed)` and
  `isfile(dir/CVS/VirtualRepository)` — 2 more `stat`s.
  Around it, `do_dir_proc` (`src/recurse.cpp:1118-1160`) does 3 more `stat`s
  (`dir/CVS`, `dir/CVS/Repository`, `dir/CVS/Entries`), plus `isdir(dir)`
  (`src/recurse.cpp:1237`), plus `isfile(dir/CVS/Repository)` again at
  `src/recurse.cpp:1207`; and `CVS/Tag` is opened up to three times per directory
  (`ParseTag` at `src/recurse.cpp:1211`, `ParseTag_Dir` at `:1258`, `ParseTag` inside
  `Entries_Open` at `src/entries.cpp:821`).
- **Impact:** Directory-count-linear rather than file-count-linear, but in a game repo the
  directory count is itself in the tens of thousands, and the `.directory_history,v` file
  accumulates one revision per structural change — so its parse cost grows monotonically
  with repository age (compounded by F5's O(R²)).
- **Proposal:** (a) Cache a per-repository "this repo has no `.directory_history`" flag so
  the two failed `fopen`s become one cheap check (or `stat` the directory listing once in
  `Find_Names`, which already `readdir`s the repository directory, and record whether
  `.directory_history,v` was seen). (b) Memoise `ParseTag` per directory — read `CVS/Tag`
  once in `do_dir_proc` and pass the result down instead of re-opening it in
  `Entries_Open`. (c) Collapse the duplicated `stat`s in `do_dir_proc` into a single
  `opendir`+`readdir` of `dir/CVS`.
- **Estimated LoC:** ~90.
- **Risk:** low-medium.
- **Risk detail:** (a) is safe as long as the flag is per-repository-directory and not
  global (a concurrent `cvs rename`/`add` can create `.directory_history,v` mid-session —
  but `open_directory` is already racy against that and the read lock covers it).
  (b) changes when a sticky tag written by `WriteTag` during the same recursion becomes
  visible; `update_filesdone_proc` (`src/update.cpp:1021`) writes `CVS/Tag` *after* the
  files are processed, so re-reading it later in the same directory must be preserved —
  invalidate the memo in `WriteTag`.

### F10: Fixed 151-bucket hash, double duplicate-check on insert, and a leak in `find_rcs`

- **Location:** `cvsnt/cvsnt-2.5.05.3744/src/hash.h:14`,
  `cvsnt/cvsnt-2.5.05.3744/src/hash.cpp:269-304`,
  `cvsnt/cvsnt-2.5.05.3744/src/find_names.cpp:294-301`,
  `cvsnt/cvsnt-2.5.05.3744/src/entries.cpp:1000-1050`.
- **Complexity:** O(n²/151) per directory for building the Entries list and the file list,
  where n = files in that directory. Constant-factor 2× from the redundant lookup.
- **Evidence:** `src/hash.h:14`:
  ```c
  #define HASHSIZE	151
  ```
  `src/hash.cpp:284-296`, inside `insert_before()` (which `addnode()` always calls):
  ```c
  /* put it into the hash list if it's not already there */
  for (q = list->hasharray[hashval]->hashnext;
       q != list->hasharray[hashval]; q = q->hashnext)
  {
      if (fncmp (p->key, q->key) == 0)
          return (-1);
  }
  ```
  Callers do the same scan again first — `src/find_names.cpp:294-301`:
  ```c
  p = getnode ();
  p->type = FILES;
  p->key = xstrdup (q);
  if(!findnode_fn(list,p->key))       /* scan #1 */
  {
      if(addnode (list, p) != 0)      /* scan #2, inside insert_before */
          freenode (p);
  }
  ```
  Note the **memory leak**: if `findnode_fn` finds the name (the normal case for every file
  that is both in `CVS/Entries` and in the repository, i.e. essentially all of them),
  `p` and `p->key` are never freed. One `Node` + one `strdup` leaked per file per
  directory pass.
  Same double-scan in `AddEntryNode` (`src/entries.cpp:1004` `findnode_fn`, then
  `src/entries.cpp:1046` `addnode`), and in `find_virtual_rcs`/`find_rename_rcs`
  (`src/mapping.cpp:836`, `:876`).
- **Impact:** With 151 buckets and 10 000 files in one directory the average chain is 66
  nodes, so building the list costs 10 000 × 2 × 33 ≈ 660 k `fncmp` calls, and every
  subsequent `findnode_fn` (one per file in `do_file_proc`, `src/recurse.cpp:934`; one in
  `Version_TS`, `src/vers_ts.cpp:134`; one in `server_updated`, `src/server.cpp:4485`)
  costs 33 more. It is a genuine quadratic, but it only dominates for
  pathologically flat directories — for a typical 50–500 files/directory layout the
  chain is ≤ 4 and this is *not* the main problem. Listed because the brief asks, and
  because the redundant scan and the leak are free to fix.
- **Proposal:** (a) Delete the caller-side `findnode_fn` where `addnode`'s return value is
  already checked, and free the node on the `-1` path — this fixes the leak and halves the
  scans: at `src/find_names.cpp:297` replace with
  `if (addnode(list, p) != 0) freenode(p);`.
  (b) Size the bucket array dynamically: keep `HASHSIZE` as the initial size and rehash at
  load factor 4 (`getlist` already memsets the array; add a `grow_hash()` in
  `insert_before`). Or simply raise `HASHSIZE` to 1021 — a 4 KiB `hasharray` per `List` is
  still cheap given lists are cached (`src/hash.cpp:55-79`).
- **Estimated LoC:** (a) ~10; (b) ~60 for dynamic rehash, ~1 for the constant bump.
- **Risk:** (a) low, (b) low-medium.
- **Risk detail:** (a) changes nothing semantically — `insert_before` already returns -1 on
  duplicate. (b) `dellist()` iterates all `HASHSIZE` buckets to recycle headers
  (`src/hash.cpp:104-113`), and `getlist()` `memset`s the whole array
  (`src/hash.cpp:77`) — both become more expensive per list with a larger constant, which
  matters because a `List` is created and destroyed per directory *and* per RCS file
  (`rdata->versions`, `rdata->other`, `vnode->branches`, `vnode->properties`). Bumping
  `HASHSIZE` to 1021 makes `getlist`/`dellist` ~7× more expensive; measure before
  committing. Dynamic growth avoids that and is the better answer.

### F11: Micro-costs on the per-file path (grouped)

- **Locations and evidence:**
  - `cvsnt/cvsnt-2.5.05.3744/src/server.cpp:5341-5342` — request dispatch is a linear scan
    of a 90-entry table with `strlen(rq->name)` **recomputed inside the loop**:
    ```c
    for (rq = requests; rq->name != NULL; ++rq)
        if (strncmp (cmd, rq->name, strlen (rq->name)) == 0)
    ```
    `Entry` is entry #8, `EntryExtra` #9, `Unchanged` #19 ⇒ ~36 `strlen`+`strncmp` per file.
  - `cvsnt/cvsnt-2.5.05.3744/src/client.cpp:5319-5375` — ~30 separate `send_to_server()`
    calls per file to assemble the `Entry` and `EntryExtra` lines, each doing a `strlen`
    (`src/client.cpp:4123-4124`) and a `buf_output`.
  - `cvsnt/cvsnt-2.5.05.3744/src/entries.cpp:447-461` and `:578-592` — `fgetentent` and
    `fgetententex` allocate and free a fresh `getline` buffer **per Entries line**
    (`line = NULL; line_chars_allocated = 0;` … `xfree (line);`) and return after one
    record ⇒ 2 malloc/free pairs per file at every `Entries_Open`.
  - `cvsnt/cvsnt-2.5.05.3744/src/mapping.cpp:566-581` — when `directory_mappings` is
    non-empty, `lookup_module2()` walks the **entire** mapping list on every lookup, and
    `map_filename()` is called once per file from `do_file_proc`
    (`src/recurse.cpp:894`) ⇒ O(files × mappings) per directory.
  - `cvsnt/cvsnt-2.5.05.3744/src/mapping.cpp:330-341` — `lookup_repository_directory()`
    falls back to a linear scan over all module entries with **five `strlen` calls inside
    the loop body**, and `_lookup_module2()` calls it once per path component
    (`src/mapping.cpp:395-436`) ⇒ O(depth × modules × strlen) per file. Only bites
    installations that actually use `CVSROOT/modules2`.
  - `cvsnt/cvsnt-2.5.05.3744/src/history.cpp` (`history_write`) — `CFileAccess::exists()`
    (`stat`) + `open("a+")` + `write` + `close` of the single global
    `$CVSROOT/CVSROOT/history` per checked-out file (called from `src/update.cpp:1825`,
    `:2194`, `:2372`, `:2471`, `:2494`, `:1544`), plus a `run_trigger` call.
  - `cvsnt/cvsnt-2.5.05.3744/src/download_blob_to.cpp:334-343` — `add_download_queue`
    calls `xgetwd()` (a `getcwd` syscall) and inserts into `download_dirs` for **every**
    blob, though the cwd changes only per directory.
- **Complexity:** all O(F) with modest constants; together they are perhaps 10–20 % of a
  no-op update, more on Windows where syscalls are dearer.
- **Impact:** None of these is the headline problem, but they are cheap to fix and they
  compound. `history_write` in particular serialises all concurrent server processes on one
  append-only file.
- **Proposal:**
  - Precompute request-name lengths once (static table init) and/or dispatch on
    `cmd[0]` first; ~15 LoC.
  - Build the `Entry`/`EntryExtra` line into a stack buffer with one `snprintf` and one
    `send_to_server`; ~40 LoC.
  - Hoist the `getline` buffer into `Entries_Open` and pass it into
    `fgetentent`/`fgetententex`; ~25 LoC.
  - Index `directory_mappings` by key (it is already a hashed `List` — use `findnode_fn`
    on a reverse map instead of the `p->data` linear walk); ~40 LoC.
  - Cache `strlen(dir->directory)` in `modules2_module_struct` at load time; ~10 LoC.
  - Keep the history file open for the life of the command (open lazily on first
    `history_write`, close in `server_cleanup`); ~25 LoC.
  - Cache `xgetwd()` per directory in `add_download_queue`; ~10 LoC.
- **Estimated LoC:** ~165 total.  **Risk:** low.
- **Risk detail:** The history-file handle must be flushed before the process can be killed
  or the audit trail is lost — flush after each record, close at exit. The `Entry`-line
  `snprintf` must preserve the exact byte sequence including the codepage translation in
  `send_to_server` (`src/client.cpp:4088-4111`); translate once on the assembled line
  rather than per fragment, which is also more correct for multi-byte encodings that a
  fragment boundary could split.

---

## Hypotheses refuted

- **"The client flushes the socket per file."** *Refuted for the client→server direction.*
  `send_to_server_untranslated()` accumulates and only flushes at
  `2 * BUFFER_DATA_SIZE` = 2 × `BUFSIZ*10` ≈ 160 KiB
  (`src/client.cpp:4127-4136`, `src/buffer.h:95`). The Entry/Unchanged stream is properly
  batched. *Confirmed for the server→client direction* — see F7
  (`src/server.cpp:6459-6461`).

- **"Blobs are downloaded one at a time, serially, with connection setup per blob."**
  *Refuted.* `BackgroundProcessor::init()` (`src/download_blob_to.cpp:233-296`) starts
  `min(8, hardware_concurrency()-1)` worker threads, each with its **own persistent**
  `BlobNetworkProcessor` (`download_clients[ti]`), fed by a `concurrent_queue`
  (`src/download_blob_to.cpp:82-124`). `add_download_queue` enqueues without blocking
  (`:334`), and `wait_threads()` is called once at command end
  (`src/main.cpp:1716-1717`, `src/client.cpp:5896-5897`) — **not** per file or per
  directory. Connections are reused across blobs and round-robin over
  public/private/master URLs (`:198-227`). The remaining per-blob cost is filesystem, not
  network: temp `fopen`/`fwrite`/`fclose`, `change_mode`, `rename_file`, `change_utime`,
  and one or two `get_file_size` validations (`src/download_blob_to.cpp:389-460`) ≈ 7
  syscalls per blob.

- **"`CVS/fileattr` is re-read per file or per directory pass."** *Refuted for the read.*
  `fileattr_read()` is lazy and memoised in `stored_root` — every accessor guards with
  `if(!stored_root) fileattr_read();` (`src/fileattr.cpp:62, 84, 94, 106, 147, 161, 184,
  215`), and `fileattr_startdir`/`fileattr_free` bracket exactly one directory
  (`src/recurse.cpp:724`/`848-849`, `:1272`/`1380-1381`). `fileattr_write()` is a no-op
  unless `modified` is set (`src/fileattr.cpp:241`). The *file* is read once per directory.
  What is per-file is the XPath **query** against the parsed tree — see F8.

- **"The result of `find_names` is sorted with an O(n²) insertion."** *Refuted.*
  `sortlist()` (`src/hash.cpp:415-452`) counts the nodes, copies them into an array, and
  calls libc `qsort` — O(n log n) with one allocation. `Find_Names` calls it once per
  directory (`src/find_names.cpp:121`).

- **"The directory is read more than once in `find_names`."** *Partly refuted, partly
  confirmed.* `find_rcs()` opens the repository directory once and the Attic once — two
  necessary scans, not a redundant re-read (`src/find_names.cpp:88-105`). It does **not**
  `stat` the `,v` entries: `find_dirs()` explicitly skips them
  (`src/find_names.cpp:368-370`, "don't bother stating ,v files"). On the client
  (`W_LOCAL` only) `Find_Names` does no `readdir` at all — it walks `CVS/Entries`
  (`src/find_names.cpp:68-83`). *However*, an extra full `readdir` per directory does
  happen later, in `ignore_files()` (`src/ignore.cpp:405`) for `? file` detection, and a
  third when `-P` is used, in `isemptydir()` (`src/update.cpp:1468`).

- **"The server takes a read lock per directory, and lock acquisition scales with
  directory count."** *Refuted as stated, and the reality is worse.* When a lock server is
  configured, `Reader_Lock()` returns immediately (`src/lock.cpp:705-709`,
  `/* No recursive locks */`), so there is **no** per-directory lock. The cost moved to
  **per-file** locks in `rcsbuf_open` — F1. Only in the file-lock (no `lock_server`)
  configuration does the per-directory `set_lock`/`readers_exist` path run.

- **"`stat`/`lstat` is called several times per file across `find_names`, `Classify_File`
  and `vers_ts`."** *Mostly refuted.* On the server the per-file stat count is **one**
  (`CVS_LSTAT` in `time_stamp_server`, `src/vers_ts.cpp:416`); on the client it is also
  **one** (`CVS_LSTAT` in `time_stamp`, `src/vers_ts.cpp:487`, reached from
  `Version_TS` at `:389`). `Classify_File` (`src/classify.cpp:29`) adds no stat of its own —
  it consumes `vers->ts_user`. `find_rcs` adds none. The legacy-timestamp conversion in
  `fgetentent` (`src/entries.cpp:544-559`) would add one `CVS_STAT` + one `time_stamp` per
  entry, but it is gated on `strlen(ts) > 30` and a normal `ctime` timestamp is 24
  characters, so it does not fire. The per-file *syscall* storm is real but it is `open`
  and lock-RTT, not `stat`.

- **"`RCS_fully_parse` is called on the update path."** *Refuted.* `RCS_fully_parse`
  (`src/rcs.cpp:579`) is only reached from `log_fileproc` — `grep -n RCS_fully_parse
  src/*.cpp` shows definition plus `src/log.cpp` usage only. Update never fetches
  deltatexts for the metadata phase. The problem is `RCS_reparsercsfile` (F2), which parses
  every delta *node* though not every delta *text*.

- **"`realloc`-by-1 growth patterns."** *Refuted in the generic helper, confirmed in one
  place.* `expand_string` (`src/subr.cpp:166`) doubles up to `MAX_INCR`; the modules2
  loader doubles (`src/mapping.cpp:151`, `:176`); the rename-script loader doubles
  (`src/mapping.cpp:1152`); `directory_stack` doubles (`src/mapping.cpp:1035`). The
  exception is `reloc_ptr_base`, which grows by a fixed 128 (`src/rcs.cpp:1516`) — benign,
  since the array is small relative to the O(R²) scan that reads it (F5). The genuine
  linear-growth bug is `rcsbuf_fill` above `MAX_INCR` (F6).

- **`src/RecurseRepository.cpp` is on the hot path.** *Refuted — it is dead code.*
  `CRecurseRepository` is referenced only by its own header
  (`grep -rn CRecurseRepository src/` → `RecurseRepository.h:21,24,25` only);
  `BeginRecursion()` is never called. It also contains two latent bugs
  (`cvs::sprintf(ent.logical_name,256,"%s/%s", a, "/", b)` at
  `src/RecurseRepository.cpp:102-103` passes three arguments to a two-placeholder format).
  Recommend deleting it rather than optimising it.

---

## Recommended order of work

1. **F5 — `rcsbuf_valfree` teardown flag** (~15 LoC, low risk). Removes an O(R²) term with
   a one-flag change and no protocol or on-disk impact. Do this first; it is the best
   effort-to-win ratio in the report and it makes every later measurement cleaner.
2. **F6 — pre-size the RCS buffer from `fstat`** (~15 LoC, low risk). Removes the
   O(S²/2 MiB) memcpy and all relocation-loop work in one change. Pairs naturally with F5.
3. **F3 — delete the duplicate `write_ent_proc` call** (~1 LoC, low risk) and
   **F10(a) — fix the `find_rcs` double-lookup and leak** (~10 LoC, low risk).
   Two trivial, obviously-correct edits that cut Entries.Log volume in half and stop a
   per-file leak.
4. **F4 — stop `fsync`ing / copying the Entries `.Old` backups** (~25 LoC, low risk).
   On a Linux server this alone can remove tens of seconds per large update, and it is
   isolated to one function.
5. **F1(a) — suppress per-file locks for read-only recursions, restore the per-directory
   read lock** (~40 LoC, medium risk). This is the single biggest win. Stage it behind a
   config switch (`LockLevel`-style) so it can be rolled back in production, and validate
   against a concurrent-commit soak test before enabling by default.
6. **F11 — the micro-cost batch** (~165 LoC, low risk). Do it while F1's soak test runs;
   independent, mechanical, and individually revertable.
7. **F7 — byte-threshold flushing instead of newline flushing** (~30 LoC, medium risk).
   Needs a careful audit of flush-before-read points; worth doing before F2 so the
   protocol behaviour is settled.
8. **F9 — per-directory memoisation (`.directory_history` presence, `CVS/Tag`, `CVS/*`
   stats)** (~90 LoC, low-medium risk). Attacks the directory-count term, which becomes
   proportionally more visible once F1 and F2 have removed the file-count terms.
9. **F8 — hoist the watched-file XPath to one query per directory** (~70 LoC, low-medium
   risk). Only matters for checkouts and large syncs, not for no-op updates — schedule it
   after the no-op path is fixed, and verify with a case-insensitive-filesystem test.
10. **F2 — reinstate the lazy RCS parse** (~120 LoC, medium-high risk). The largest
    structural win left, but it touches ~80 direct `rcs->versions` users. Do it last, with
    an assert-on-unparsed guard shipped first (in a debug build) to find every missing
    `rcs_ensure_deltas()` call site before the optimisation is switched on.
11. **F1(b) — batched `LockMany`/`UnlockMany` in the lock service** (~150 LoC, medium
    risk). Only if F1(a) proves insufficient or cannot be made safe; it preserves lock
    semantics exactly but requires deploying a new lock daemon in lockstep with servers.
12. Housekeeping: delete `src/RecurseRepository.cpp` / `.h` (dead code, contains latent
    format-string bugs).
