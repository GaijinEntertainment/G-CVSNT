---
id: PERF-03
area: locking / cross-command contention
---

# Why `cvs tag` blocks behind a concurrent `cvs update`

*All paths below are relative to `cvsnt/cvsnt-2.5.05.3744/`.*

## Answer in three sentences

`cvs update` **does** take a lock on every `,v` file it opens — a **shared/read** lock from
`rcsbuf_open` (`src/rcs.cpp:908`) — and `cvs tag`/`cvs rtag` take an **exclusive/write** lock on
every `,v` they open, because `tag.cpp:282` sets the global `lock_for_write = 1` for the whole
command, so the lock server refuses the tag's Write while the update's Read is outstanding
(`lockservice/LockParse.cpp:868`). The waiting side does not queue: it gets an immediate
`002 busy`, then polls in a hand-rolled loop that sleeps **at least one second per retry and gives
up fatally after twenty** (`src/lock.cpp:339-356`), so every single-file collision costs the tag a
whole second and a collision that persists ~39 s kills the command outright with
`Failed to obtain lock on …`. A slow client does not lengthen any individual `,v` lock much
(the bulk write to the socket happens *after* `freercsnode`, at `src/recurse.cpp:967`), but it
stretches the update's walk from seconds to many minutes so the two walkers stay adjacent in the
same file list for that entire time — and the one lock that genuinely *is* held across the client
writes, the per-directory `.directory_history,v` node taken in `open_directory`
(`src/mapping.cpp:1057`) and not released until `close_directory` (`src/mapping.cpp:1392`), is
exactly the object `cvs tag` must take a **write** lock on to enter that directory at all.

## Does update lock?

**Yes. The operator's premise "cvs update should not do a lock on files, but it does" is correct as
stated.** The refinements they will care about:

* The lock is **shared (Read)**, not exclusive. `lock_for_write` (`src/rcs.cpp:35`) is a plain
  zero-initialised global; the *only* writer is `src/tag.cpp:282` (`= 1`) / `:305` (`= 0`).
  During `update`, `serve_update`, `checkout`, `diff`, `log`, `status` it is therefore `0`, so
  `rcsbuf_open` calls `do_lock_file(filename, NULL, 0, 1)` → `do_lock_server(..., "Read", 1)`
  (`src/lock.cpp:362-367`). Two shared readers never block each other.
* It is nevertheless a real lock that a **writer** must wait for, and `cvs tag` is a writer on
  every file it merely *inspects*.
* The lock is taken **only when a lock server is configured** — `do_lock_file` returns the
  non-zero sentinel `(size_t)-1` immediately if `!lock_server` (`src/lock.cpp:364-365`).
* On a server, a lock server **is** configured by default: `read_global_config()` defaults
  `lock_server` to `"localhost:2402"` (`src/main.cpp:551-560`) and then, for `server_active`,
  force-sets `"127.0.0.1:2402"` if it is still unset (`src/main.cpp:587-591`). Only
  `LockServer=none` in `CVSROOT/config` (`src/parseinfo.cpp:329-334`) turns it off.
* Client/server vs local makes **no difference to the locking**: `cvstag()` returns early for
  `current_parsed_root->isremote` (`src/tag.cpp:223-278`), so the server-side re-invocation is the
  one that reaches `lock_for_write = 1`. Same for `update`.

Evidence:

```c
/* src/rcs.cpp:905-912  (rcsbuf_open) */
    if(!orig_lockId)
    {
        rcsbuf->lockId=do_lock_file(filename, NULL, lock_for_write, 1);

/* src/rcs.cpp:766-767  (freercsnode) */
    if((*rnodep)->rcsbuf.lockId)
        do_unlock_file((*rnodep)->rcsbuf.lockId);

/* src/lock.cpp:362-367 */
size_t do_lock_file(const char *file, const char *repository, int write, int wait)
{
    if(!lock_server)
        return (size_t)-1;
    return do_lock_server(file,repository,write?"Write":"Read", wait);
}
```

Call chain for a server-side update, per file:

```
serve_update → do_update (src/update.cpp:650)
  → start_recursion(update_fileproc, …, readlock=1, dosrcs=1)
    → do_recursion (src/recurse.cpp:806  Reader_Lock — no-op when lock_server)
      → walklist(filelist, do_file_proc)        src/recurse.cpp:826
        → do_file_proc                           src/recurse.cpp:224
            RCS_parse(finfo->mapped_file, …)     src/recurse.cpp:915
              → RCS_parsercsfile_i               src/rcs.cpp:326
                → rcsbuf_open                    src/rcs.cpp:872
                  → do_lock_file(path,NULL,0,1)  src/rcs.cpp:908   ← READ LOCK ACQUIRED
            fileproc = update_fileproc           src/recurse.cpp:954/957
            freercsnode(&finfo->rcs)             src/recurse.cpp:959 ← READ LOCK RELEASED
            cvs_flushout()                       src/recurse.cpp:967 ← blocking socket write
```

## The blocking chain

With the default (lock-server) configuration:

| step | who | lock | where taken | held across | released |
| --- | --- | --- | --- | --- | --- |
| 1 | update | **Read** on `<repos>/<dir>/.directory_history,v` | `src/mapping.cpp:1057` via `open_directory` | the **whole directory subtree**, every file, every blocking flush to the client | `src/mapping.cpp:1392` (`close_directory`) |
| 2 | update | **Read** on `<repos>/<dir>/<file>,v` | `src/rcs.cpp:908` | `update_fileproc` → `checkout_file` → `RCS_checkout` → `server_updated` (buffer-only) | `src/recurse.cpp:959` |
| 3 | tag pass 1 | **Write** on `<file>,v` | `src/rcs.cpp:908` with `lock_for_write==1` | `check_fileproc` (read-only work!) | `src/recurse.cpp:959` |
| 4 | tag pass 2 | **Write** on `<dir>/.directory_history,v` | `src/mapping.cpp:1057` | the whole subtree | `src/mapping.cpp:1392` |
| 5 | tag pass 2 | **Write** on `<file>,v` | `src/rcs.cpp:908` | `tag_fileproc` | `src/recurse.cpp:959` |
| 6 | tag pass 2 | **Write #2** on the same `<file>,v` | `src/rcs.cpp:7097` (`rcs_internal_lockfile`, from `RCS_rewrite` `src/rcs.cpp:7213`) | the `,foo,` rewrite | `src/rcs.cpp:7155` |

Step 3 or 5 collides with step 2; step 4 collides with step 1. The refusal happens here:

```c
/* lockservice/LockParse.cpp:868-884 */
inline bool can_get_lock(LockMapType::const_iterator i, uint32_t client, const char *path, hash_t hash, unsigned flags)
{
    if((hash == i->second.hash && !strcmp(path,i->second.path.c_str())))
    {
        if(flags&lfWrite)
        {
            /* Mixed Write lock only possible with the same client */
            if(i->second.owner!=client)
                return false;          //  ← tag's Write refused because update holds ANY lock
        }
        else /* read lock */
        {
            if((i->second.flags&lfWrite) && i->second.owner!=client)
                return false;          //  ← update's Read refused while tag holds Write
        }
    }
    return true;
}
```

`DoLock` then answers `002 busy|<user>|<host>|<path>` (`lockservice/LockParse.cpp:632`) and returns
— **the lock server keeps no wait queue and never blocks**. The waiting is entirely in the CVS
server process:

```c
/* src/lock.cpp:337-357  (do_lock_server, case 2 == busy) */
        if(!(bWaited%5)) // No need to keep going on about it..
        {
            sleep(5);//increase amount of time waiting for obtain lock
            error(0,0,"[%s] waiting for %s on %s's lock in %s", …);
        }
        bWaited++;
        if(bWaited==20)
            error(1,0,"Failed to obtain lock on %s",fn_root(object));
        break;
    …
    // Only get here in case 2 == WAIT
    sleep(1);
} while(1);
```

Budget: `sleep(5)` on iterations 0, 5, 10, 15 plus `sleep(1)` on every iteration →
**≈39 seconds, then `error(1,…)` which is fatal**. The tag does not "wait for the update"; after
39 s it *dies*. That is the literal reading of "we just can't create or move cvs branches".

The same code runs on the update side, so a tag that gets there first kills the update after 39 s
too — the "affect each other" the operator describes is symmetric.

## Why a slow client makes it worse

Three distinct mechanisms, in decreasing order of confidence:

**(a) Phase-locking of two walkers over the same file list — the dominant effect.**
Both commands enumerate the same directory with `Find_Names` and walk it in the same sorted order.
Each collision costs the *waiter* a minimum of one second (`src/lock.cpp:356`) — there is no
sub-second retry and no notification. With a fast client the update sweeps a directory in
milliseconds per file and passes the tag almost immediately; the collision window is one or two
files. With a slow client the update's per-file cycle is dominated by the blocking drain at
`src/recurse.cpp:967` and becomes seconds long, which is the *same order as the tag's retry
granularity*. The two walkers therefore advance at comparable speed and stay adjacent in the list
for the whole update, colliding repeatedly, each collision costing ≥1 s and each one carrying a
1-in-20-retries chance of killing the command.

**(b) One lock genuinely IS held across writes to the client socket.**
`open_directory` parses `.directory_history,v` into `current_directory->repository_rcsfile`
(`src/mapping.cpp:1057`) — with `RCS_parse`, therefore with a `rcsbuf_open` lock — and does not
free it until `close_directory` (`src/mapping.cpp:1392`), i.e. **after the entire subtree has been
processed and pushed to the client**. `directory_stack` is a stack, so a recursive update holds one
such lock per level of the current path simultaneously. `update.cpp:1145-1146` opens the directory
**twice** (once for the sticky version, once for `"_H_"`), so two locks on the same path. `cvs tag`
needs a **Write** lock on the same object to enter the directory (its own `open_directory`, same
line, with `lock_for_write==1`), and `tag_dirproc` (`src/tag.cpp:1247-1253`) then deliberately tags
the mapping file itself via `get_directory_finfo` → `tag_fileproc` → `RCS_settag` + `RCS_rewrite`.
So on any repository that has directory-version files, **a tag cannot even enter a directory an
update is anywhere inside of, and the update's hold on that object is exactly as long as the slow
client takes.**
Caveat, stated honestly: if `<repos>/<dir>/.directory_history,v` does not exist (a repository that
has never used CVSNT directory renames), `RCS_parse` returns NULL before any lock is taken —
`rcsbuf_open` locks only *after* a successful `CVS_FOPEN` (`src/rcs.cpp:889-908`) — and mechanism
(b) does not apply. Check for the file before assuming it.

**(c) Occasional in-window flushes.** `cvs_output` flushes synchronously on any newline-terminated
string:

```c
/* src/server.cpp:6458-6460 */
        buf_output (stdout_buf?stdout_buf:buf_to_net, str, len);
        if(str[len-1]=='\n')
            buf_send_output(stdout_buf?stdout_buf:buf_to_net);
```

and `buf_send_output` on the wrapper ends with `return buf_send_output (pb->buf);`
(`src/buffer.cpp:1676`), i.e. a **blocking** `write()` on `buf_to_net` — `buf_to_net` is
`fd_buffer_initialize(STDOUT_FILENO, 0, …)` (`src/server.cpp:5148`) and `buf->nonblocking` is left
at 0 (`src/buffer.cpp:30`); `set_block` is only ever called on shutdown paths (`server.cpp:3303,
3315, 5072`). Any `error(0,…)` emitted for a file reaches `cvs_flushout()` (`src/error.cpp:194`)
and drains the whole pending `buf_to_net` — **while the current file's `,v` lock is held**. On the
tag side this is not occasional: `tag_fileproc` prints its result with bare `cvs_output` calls
ending in `cvs_output ("\n", 1)` (`src/tag.cpp:1160-1172`, `1206-1211`), so **every tagged file
does a blocking socket write while holding that file's write lock plus the directory's**.

**What does *not* happen** (I checked, because it is the obvious hypothesis): the bulk file body is
**not** written to the socket under the per-file lock. `server_updated` only appends to memory —
`buf_output` / `buf_append_data` never flush (`src/buffer.cpp:160-207`, `:225-238`), the
buffer-data pool grows without bound, and the one draining flush per file is `cvs_flushout()` at
`src/recurse.cpp:967`, **eight lines after** `freercsnode` at `:959`. With an `MT`-capable client
(every modern client) `write_letter` goes through `cvs_output_tagged`, which for `MT` uses
`buf_output0` with no flush (`src/server.cpp:6785-6795`), so even the "U file" line does not block.
The `-kB`/blob path is *better*, not worse: `server_updated` collapses the response to
`SERVER_BLOB_REF` and sends a ~70-byte reference (`src/server.cpp:4429-4455`); the client fetches
the content out of band from the CAFS server on its own connections, with the CVS server no longer
in the loop and holding nothing. The server-side blob resolution for legacy clients
(`pull_at_once`, `src/rcs_cvt_kB.cpp:23-56`) reads the **local** content-addressed store, not a
remote one.

## Findings

### F1: `cvs update` takes a per-`,v` shared lock on every file it opens
- **Location:** `src/rcs.cpp:908` (acquire), `src/rcs.cpp:766-767` (release), `src/lock.cpp:362`
- **Mechanism:** `rcsbuf_open` unconditionally calls `do_lock_file(filename, NULL, lock_for_write, 1)`
  for every `,v` it successfully opens. `lock_for_write` is 0 outside `tag`, so the request is
  `Lock Read|<path>`. `freercsnode` releases it. That is **two blocking lock-server round trips per
  file**, on the single global `lock_server_socket` (`src/lock.cpp:156`), even for files that turn
  out to be up to date and are never sent.
- **Evidence:** call chain in "Does update lock?" above; `do_lock_file` is also reached from
  `Version_TS` (`src/vers_ts.cpp:228`) when `finfo->rcs` is NULL, and refcounting
  (`src/vers_ts.cpp:221-224`) means the same node/lock is shared, not doubled.
- **Symptom it explains:** the operator's core claim. It is true, and it is the object the tag waits on.

### F2: `cvs tag`/`cvs rtag` upgrade *every* `,v` open to an exclusive lock, in both passes
- **Location:** `src/tag.cpp:282` (`lock_for_write = 1`), `:305` (`= 0`), `src/rcs.cpp:35`
- **Mechanism:** `lock_for_write` is a process-global consulted inside `rcsbuf_open`. It is set once
  for the entire `cvstag()` body, which covers **both** `start_recursion` passes
  (`src/tag.cpp:409` and `:431`) *and* every incidental `RCS_parse` in between —
  `tag_check_valid` (`src/tag.cpp:401`), `open_directory`'s mapping-file parse, `RCS_parsercsfile`
  from module expansion. Pass 1 (`check_fileproc`, `src/tag.cpp:452`) does nothing but call
  `Version_TS` and `RCS_getversion`; it is **purely read-only yet takes exclusive locks on the whole
  module.**
- **Evidence:**
  ```c
  /* src/tag.cpp:282-305 */
      lock_for_write = 1;
      if (is_rtag) { … do_module(…, rtag_proc, …) … }
      else         { err = rtag_proc (…); }
      lock_for_write = 0;
  ```
  and `rcsbuf_open` passes that global straight through at `src/rcs.cpp:908`.
- **Symptom it explains:** why a *tag* — which the operator reasonably thinks of as a metadata
  operation — is blocked by a *reader*, and why it is blocked for roughly twice as long as
  necessary (two full passes over the module, each write-locking every file).

### F3: the busy path is a fixed-schedule poll with a hard 20-retry fatal, not a wait
- **Location:** `src/lock.cpp:254-360` (`do_lock_server`), specifically `:339-356`
- **Mechanism:** the lock server answers `002 busy` and returns (`lockservice/LockParse.cpp:632`).
  The CVS server then sleeps ≥1 s (5+1 s on retries 0/5/10/15) and re-issues the *same* `Lock`
  command. There is no queue, no ticket, no notification, no exponential backoff, and no
  configuration knob for either the count or the sleep. On the 20th refusal it calls
  `error(1, …)` — **fatal**.
- **Evidence:** quoted in "The blocking chain". Total budget ≈ 4×5 + 19×1 = 39 s. Note the
  `sleep(5)` runs *before* the "waiting for …" message is printed, so the first diagnostic reaches
  the operator 5 s late.
- **Symptom it explains:** (i) "so slow" — every collision costs the *whole second*, however brief
  the actual conflict; (ii) "we just can't create or move cvs branches" — the tag does not merely
  wait, it aborts with `Failed to obtain lock on …` once one file stays contended for ~39 s.

### F4: no fairness, and starvation is structurally possible
- **Location:** `lockservice/LockParse.cpp:888-912` (`request_lock`), `src/lock.cpp:339-356`
- **Mechanism:** `request_lock` scans the existing locks for the path and grants or refuses on the
  spot; nothing records that a writer is waiting, so **arriving readers are never held back for a
  waiting writer**. The update re-acquires a Read lock on a fresh file every few hundred
  milliseconds in a tight loop (`src/recurse.cpp:826` → `:915`); the tag polls one file at a time at
  1 Hz. A second updater joining mid-flight can keep a file continuously read-locked and the tag
  will simply die at 39 s. Even with a single updater, the tag's throughput while contended is
  capped at ~1 file/s.
- **Evidence:** `request_lock` has no waiter registry; `LockMap`/`PathToLocks`
  (`lockservice/LockParse.cpp:192-195`) store only *held* locks. `DoLock`'s only "wait" response is
  the immediate `002`.
- **Symptom it explains:** why the tag can be arbitrarily delayed rather than merely slowed, and why
  running the tag "again in a minute" is not reliably better.

### F5: the directory mapping file is locked for the entire subtree — across all client I/O
- **Location:** `src/mapping.cpp:1057` (acquire), `src/mapping.cpp:1392` (release),
  `src/recurse.cpp:1265` (call site), `src/update.cpp:1145-1146`, `src/checkout.cpp:525-526`
- **Mechanism:** `open_directory` parses `.directory_history,v` into an `RCSNode` that lives on
  `directory_stack` until `close_directory`. The `rcsbuf.lockId` inside it is therefore held for the
  whole time the directory (and everything below it) is being processed, **including every blocking
  `buf_send_output` to the client**. Nesting means one held lock per path level; `update.cpp`
  doubles it per directory.
- **Evidence:**
  ```c
  /* src/mapping.cpp:1055-1058 */
      if(!remote)
      {
          current_directory->repository_rcsfile = RCS_parse(RCSREPOVERSION,repository);
  /* src/mapping.cpp:1391-1392 */
      if(current_directory->repository_rcsfile)
          freercsnode(&current_directory->repository_rcsfile);
  ```
  `RCSREPOVERSION` is `".directory_history"` (`src/cvs.h:220`). The tag's counterpart is the same
  line with `lock_for_write==1`, plus `tag_dirproc` → `get_directory_finfo`
  (`src/mapping.cpp:1491`) → `tag_fileproc` → `RCS_settag`/`RCS_rewrite`.
- **Symptom it explains:** the only mechanism in the code by which **client link speed directly
  extends a lock hold time**. It converts "the tag is slow" into "the tag cannot enter the
  directory at all". Applies only where `.directory_history,v` exists.

### F6: `RCS_rewrite` takes a second exclusive lock on a file it already holds
- **Location:** `src/rcs.cpp:7097` inside `rcs_internal_lockfile`, called from `src/rcs.cpp:7213`
- **Mechanism:** `*lockId = do_lock_file(rcsfile,NULL,1, 1);` on a path whose lock the caller's
  `RCSNode` already holds. Same client, so `can_get_lock` permits it — no deadlock — but it is two
  more round trips per rewritten file, and it briefly registers a **second** Write lock that any
  concurrent reader must also clear.
- **Evidence:** `can_get_lock`'s `if(i->second.owner!=client) return false;` is what saves it.
  Note this makes the "second lock" invisible in testing and easy to remove.
- **Symptom it explains:** constant factor only — 2 of the 6 round trips per tagged file. Matches
  `suggested_optimizations.md` item 17.

### F7: with the lock server on, *all* of the classic directory/tree locking is dead code
- **Location:** `src/lock.cpp:704-707` (`Reader_Lock`), `:1239-1240` (`lock_tree_for_write`),
  `:1256-1259` (`lock_dir_for_write`), `:572-574` (`Lock_Cleanup_Directory`)
- **Mechanism:** each begins with `if(lock_server) return …;`. So `#cvs.lock`, `#cvs.rfl.*`,
  `#cvs.wfl.*`, `set_lock`, `write_lock` and `readers_exist` (`src/lock.cpp:952`) never execute in
  the default server configuration. In particular `readers_exist`'s per-check `opendir`/`readdir`
  scan of the whole repository directory — a real cost with 10 000 `,v` files in a directory — is
  **not** part of this problem at all unless `LockServer=none` is set.
- **Evidence:** `src/tag.cpp:426` calls `lock_tree_for_write`, which returns immediately;
  `src/recurse.cpp:806` calls `Reader_Lock`, which returns 0 immediately.
- **Symptom it explains:** nothing by itself — but it rules out a whole family of wrong diagnoses,
  and it tells you which single config switch changes the entire failure mode (see below).

### F8: the file-based path fails differently — livelock instead of a fatal
- **Location:** `src/lock.cpp:797-857` (`Writer_Lock`), `:695-777` (`Reader_Lock`), `:1232-1249`
- **Mechanism:** with `LockServer=none`, per-file locks vanish entirely (`do_lock_file` returns the
  success sentinel at `:364`). Instead `update` takes one `#cvs.rfl.*` per directory at
  `src/recurse.cpp:806`, held across the **whole** `walklist(filelist, do_file_proc)` at `:826` —
  i.e. genuinely across all the slow client writes — and releases it at `:830`. `cvs tag` calls
  `lock_tree_for_write` (`src/tag.cpp:426`), which write-locks **every directory in the module at
  once, all-or-nothing**: `Writer_Lock` walks the list, and if `readers_exist` finds the update's
  read-lock file in *any* one directory it calls `remove_locks()`, `lock_wait()` (sleep
  `CVSLCKSLEEP`) and retries the whole tree — in a `for(;;)` **with no give-up**. Conversely, while
  the tag holds the tree, every directory has a `#cvs.lock` dir, so an update entering
  `Reader_Lock` → `set_lock(&global_readlock, 1)` waits — also forever.
- **Evidence:**
  ```c
  /* src/lock.cpp:806-846 */
      for (;;) { … (void) walklist (list, set_writelock_proc, NULL);
          switch (lock_error) {
            case L_LOCKED: remove_locks (); lock_wait (lock_error_repos); … continue;
  ```
  and `write_lock` → `if (readers_exist (lock->repository)) … return (L_LOCKED);`
  (`src/lock.cpp:903-915`).
- **Symptom it explains:** it does not explain the current symptom (the default is the lock-server
  path), but it defines the trade-off of the one no-code mitigation that actually changes the
  mechanism: hard failure at 39 s becomes an unbounded wait plus `readers_exist` directory scans.

### F9: `tag -b` costs more than plain `tag`, and "moving" a branch needs `-F -B`
- **Location:** `src/tag.cpp:1131`, `src/rcs.cpp:2274` (`RCS_magicrev`), `src/tag.cpp:1156`
- **Mechanism:** lock-wise `-b` adds nothing new — the exclusive lock is already taken by
  `rcsbuf_open` for every file in both passes regardless of `-b`. What `-b` adds is **work under
  that lock**: `RCS_magicrev` scans for a free magic branch number, calling `RCS_getbranch` and a
  full `walklist(RCS_symbols(rcs), checkmagic_proc)` per candidate. And the function contains two
  stacked `for` headers — `for (; ; rev_num += 2)` immediately followed by
  `for (rev_num = 2; ; rev_num += 2)` — so the inner loop resets to 2 and discards the
  `findnextmagicrev` result computed just above; the outer loop is dead. Every branch creation
  therefore rescans from 2 with a symbol-table walk per step (also logged as
  `_reports/BUG-server-14-magicrev-duplicated-for.md`, `suggested_optimizations.md` item 10).
  Separately: `if (!force_tag_move || (isbranch && !move_branch_tag))` at `src/tag.cpp:1156` means
  **moving an existing branch tag requires both `-F` and `-B`**; with `-F` alone the tag prints
  `NOT MOVING tag` and returns — but only *after* the exclusive lock has already been taken and the
  file parsed, so a no-op tag move costs the same contention as a real one.
- **Evidence:** `src/rcs.cpp:2284-2290`, `src/tag.cpp:1156-1178`.
- **Symptom it explains:** why branch operations specifically are the ones that time out — they hold
  the exclusive lock longer per file (magic-rev search + `RCS_settag` + full `,v` rewrite) and so
  present a larger target to the update's Read requests, which then also stall ≥1 s each.

### F10: lock keys are raw path strings, so Attic moves and differing callers can miss
- **Location:** `src/rcs.cpp:908` vs `src/commit.cpp:1115-1124`; `lockservice/LockParse.cpp:869`
- **Mechanism:** `rcsbuf_open` locks the **full path as opened**, which for an attic file is
  `<repos>/Attic/<file>,v` (`src/rcs.cpp:281`), while `commit` locks `<repos>` + `/` + `<file>,v`.
  `can_get_lock` compares with `strcmp` on the exact string. Two commands that disagree about
  attic-ness lock different objects and do not exclude each other. On Windows the
  `filenames_case_insensitive` branch (`src/lock.cpp:262-283`) normalises; on Linux nothing does.
- **Evidence:** `can_get_lock`: `if((hash == i->second.hash && !strcmp(path,i->second.path.c_str())))`.
- **Symptom it explains:** not the reported slowness — but it is a correctness gap worth knowing
  about before anyone "fixes" the contention by changing which string gets locked.

## What an operator can do today

Ordered by expected benefit, no code change.

1. **Convert the big binaries to `-kB` (blob) storage and make sure clients are blob-capable.**
   This is the highest-leverage no-code change. `server_updated` then sends a ~70-byte reference
   instead of the file body (`src/server.cpp:4429-4455`) and the client pulls content from the CAFS
   server on separate connections. The CVS server's walk stops being gated by the client link at
   all, which collapses the collision window that F5 and mechanism (a) depend on. Deploy a
   `cafs_proxy_server` at the slow site so the bulk transfer is client↔local proxy.
2. **Stop tagging the same subtree that a slow client is updating.** The collision is per exact
   `,v` path. Tagging a disjoint module, or tagging with `-l`/narrower arguments, has no
   contention at all. If the branch operations are scheduled, schedule them against a quiet window.
3. **Prefer `cvs rtag` to `cvs tag`** — but for the right reason. `rtag` uses the identical
   per-file `lock_for_write` write locks (`src/tag.cpp:282` covers both), so it does **not** reduce
   lock strength or granularity. It is faster because it skips the client-side working-copy walk and
   the `Directory`/`Entry`/`Modified` upload and writes one history record per module instead of one
   per file — a shorter command means a shorter exposure window and fewer collisions.
4. **Check whether `<repos>/<dir>/.directory_history,v` exists.** If it does, mechanism (b) is live
   and a tag cannot enter any directory an update is inside of. Look for the file before blaming
   anything else; on a repository that has never used CVSNT directory renames it will be absent and
   this whole class of blocking disappears.
5. **Consider `LockServer=none` in `CVSROOT/config` — with eyes open.** It switches to the classic
   file-based path (F7/F8): per-file locks disappear entirely, contention drops to per-directory,
   and the **39-second fatal becomes an unbounded wait**, which for "I cannot create a branch at
   all" is arguably an improvement. Costs: the tag's whole-tree all-or-nothing write lock can
   livelock against a continuous stream of updates; `readers_exist` (`src/lock.cpp:952`) does an
   `opendir`/`readdir` scan of each repository directory per attempt, which is expensive with
   thousands of `,v` per directory; and **`AtomicCheckouts` silently stops working** — the atomic
   version resolution at `src/rcs.cpp:2573` and `:2848` is implemented by the lock server's
   transaction list, and `do_lock_version` returns nothing without it (`src/lock.cpp:391-395`).
   `atomic_checkouts` defaults to 0 (`src/main.cpp:78`), so verify your `AtomicCheckouts` global
   setting before changing this. Test on a copy.
6. **`cvs -j N` helps, indirectly.** `-j` sets `blob_concurrency_download_level`
   (`src/main.cpp:1013-1014`), used only in `src/client.cpp:2177` — it is **client-side blob
   download concurrency**, not server-side parallelism, and it does not change lock hold time. It
   shortens the update wall-clock, which shortens the exposure window. Default is
   `min(8, cpu_count-1)`.
7. **There is no tunable for the retry budget.** The 20 retries and the 1 s / 5 s sleeps at
   `src/lock.cpp:339-356` are hardcoded, with no config or environment override. Do not go looking
   for one.
8. **Read the server's own message.** When the tag stalls it prints
   `[hh:mm:ss] waiting for <user> on <host>'s lock in <path>` — but only every 5th retry and only
   *after* the `sleep(5)`, so the first message appears 5 s late. That line names the exact `,v` and
   the exact competing user, which is the fastest way to confirm this diagnosis in production.

## Fixes, ranked

Root cause = removes the reason the tag waits at all. Constant factor = the tag still waits, just
less often or less long.

| # | What | Where | LoC | Risk | What could break | Protocol / format change? | Root cause or constant factor |
| --- | --- | --- | ---: | --- | --- | --- | --- |
| 1 | **Do not write-lock in tag pass 1.** Replace the `lock_for_write` global with a value carried on the recursion frame (or simply clear it around the `check_fileproc` pass) so the read-only validation pass takes Read locks. Halves the tag's exclusive footprint and lets pass 1 run concurrently with any number of updates. | `src/rcs.cpp:35`, `src/tag.cpp:282`, `src/recurse.cpp` frame | ~25 | low | any path relying on pass 1 pre-locking for pass 2 — there is none; locks are dropped per file at `recurse.cpp:959` regardless | no | **root cause** (for half the exposure) — `suggested_optimizations.md` item 9 |
| 2 | **Fix the retry loop:** bounded exponential backoff starting at ~50 ms instead of a 1 s floor, and make the 20-retry cap a configurable timeout defaulting much higher. The 1 s floor turns a 5 ms conflict into a 1 s stall; the hard cap turns a busy repository into `Failed to obtain lock`. | `src/lock.cpp:339-356` | ~20 | low | busier lock-server chatter under heavy contention; the "waiting for X's lock" cadence changes (it is already misleading — printed after the sleep) | no | **constant factor**, but the largest one, and it removes the *fatal* |
| 3 | **Suppress per-file locks for read-only recursions.** Add a "this recursion only reads" flag; when set, `rcsbuf_open` skips `do_lock_file` and the command relies on the per-directory read lock instead. Removes both round trips per file from `update`, `checkout`, `diff`, `log`, `status` — and removes the object the tag collides with. | `src/rcs.cpp:905-912`, `src/recurse.cpp`, `src/lock.cpp` | ~40 | medium | consistency the per-file lock provides against a concurrent commit mid-parse (the reason `rcsbuf_open` re-opens the file after locking on non-Windows, `src/rcs.cpp:917-937`); `AtomicCheckouts` asserts `rcs->rcsbuf.lockId` at `src/rcs.cpp:2561` and `:2846` | no — but stage behind a config switch and soak-test against concurrent commits | **root cause** — `suggested_optimizations.md` item 16, "the single biggest win available" |
| 4 | **Release the directory mapping-file lock early.** `open_directory` needs `.directory_history,v` only long enough to resolve the version and check out the mapping (`src/mapping.cpp:1063-1117`). Drop the lock once `directory_version`/`directory_mappings` are populated, keeping the parsed data. Also collapse the double `open_directory` at `src/update.cpp:1145-1146`. | `src/mapping.cpp:1057-1120`, `:1392` | ~35 | medium | `commit_directory`/`create_mapping_file` (`src/mapping.cpp:1296-1380`) reuse `repository_rcsfile` to *write* and must re-acquire; `tag_dirproc`'s `get_directory_finfo` (`src/mapping.cpp:1491`) hands the node straight to `tag_fileproc` | no | **root cause** — the only lock held across client network I/O |
| 5 | **Reuse the existing lockId in `rcs_internal_lockfile`** instead of taking a second Write lock on a file the `RCSNode` already holds. | `src/rcs.cpp:7097`, `:7155`, `:7213` | ~10 | medium | `RCS_rewrite` is also called on nodes from `RCS_fopen`/`RCS_parsercsfile` where `rcsbuf.lockId` may be stale or zero — must fall back; a wrong fallback leaves `RCS_rewrite` running **unlocked** | no | **constant factor** — 2 of 6 round trips per tagged file (item 17) |
| 6 | **Fix `RCS_magicrev`'s dead outer loop** (delete the stray `for (; ; rev_num += 2)` header so `findnextmagicrev`'s result is used). Shortens the exclusive-lock hold per branched file. | `src/rcs.cpp:2284-2290` | 2 | medium | branch numbering — the optimised path has never executed; needs magic-branch tests first | no | **constant factor**, specific to `tag -b` (item 10) |
| 7 | **Give the lock server a real wait queue.** Replace `002 busy` + client polling with a server-side wait list: register the waiter, grant on release, hold new Read grants back when a Write is queued (writer preference). Kills both the 1 s granularity and the starvation. | `lockservice/LockParse.cpp:545-635`, `:888-912`, `src/lock.cpp:254-360` | ~200 | high | the lock server is thread-per-connection with **one global mutex** (`lockservice/LockParse.cpp:79-128`, `lockservice/server.cpp:57-90`) and `DoLock` writes its reply with `s->printf` *while holding it* — a blocking wait must not be held under that mutex or the whole service stalls | **yes** — new/changed responses; version bump (`CVSLock 2.21`, `LockParse.cpp:306`) and the client gate at `src/lock.cpp:205-215` | **root cause** — correct long-term fix, largest |
| 8 | **Batch `LockMany`/`UnlockMany`.** One round trip per directory instead of two per file; fewer *moments* at which a collision can occur. | `src/lock.cpp`, `lockservice/LockParse.cpp` | ~150 | medium | partial-acquire semantics and rollback; interacts with #7 | **yes** — new commands + version bump | **constant factor** (item 21) — only if #3 proves insufficient |
| 9 | **Move the tag's own progress output out of the lock window.** `tag_fileproc` calls bare `cvs_output(…"\n", 1)` (`src/tag.cpp:1160-1172`, `:1206-1211`), which flushes synchronously to the client while the file's Write lock and the directory lock are held. Buffer it (use `cvs_output_tagged`, or drop the trailing-newline flush). | `src/tag.cpp:1160-1211`, or `src/server.cpp:6458-6460` | ~15 locally, ~30 for a general byte-threshold flush | low locally / medium generally | the general change needs a flush-before-read audit or the session deadlocks (item 12) | no | **root cause** for the tag-holds-lock-across-network case |
| 10 | **Normalise the lock key** (canonical `<repos>/<file>,v`, attic-independent) so all callers lock the same string. | `src/rcs.cpp:908`, `src/commit.cpp:1117`, `src/lock.cpp:262-283` | ~30 | medium | changes which operations exclude each other — could *introduce* contention that today silently does not exist | no | correctness, not performance |

**If only three things get done:** #1 (halve the tag's exclusive footprint), #2 (stop the fatal and
the 1 s floor), #3 (stop `update` locking at all). #1 and #2 together are ~45 LoC, low risk, no
protocol change, and turn "the tag dies" into "the tag is a bit slower".

## Refuted

* **"`cvs update` should not do a lock on files."** As a statement of what *ought* to happen it is
  defensible; as a description of the code it is a correct diagnosis, not a mistaken one. Update
  does lock, at `src/rcs.cpp:908`, on every `,v`, twice per file counting the release.
* **"The lock is held across the file transfer to the client."** Not for the per-file `,v` lock in
  the common case. `server_updated` only appends to memory (`buf_output`/`buf_append_data`,
  `src/buffer.cpp:160-238` — neither ever flushes), `freercsnode` releases at
  `src/recurse.cpp:959`, and the one draining, blocking flush is `cvs_flushout()` at
  `src/recurse.cpp:967`, *after* the release. With an `MT`-capable client even the "U file" line is
  buffered (`src/server.cpp:6785-6795`). **But the claim is true for two other locks:** the
  directory `.directory_history,v` node (F5), which spans every flush in the subtree, and — on the
  tag side — the per-file Write lock, because `tag_fileproc` prints with bare `cvs_output`, which
  does flush synchronously (mechanism (c), `src/server.cpp:6458-6460`).
* **"The blob/`-kB` path holds something while the client fetches from the CAFS server."** No. The
  server emits a `SERVER_BLOB_REF` of `blob_reference_size` bytes and moves on
  (`src/server.cpp:4429-4455`); the client's blob fetch is a separate connection with the CVS server
  holding nothing. The legacy-client fallback `pull_at_once` (`src/rcs_cvt_kB.cpp:23`) reads the
  **local** content-addressed store via `caddressed_fs::start_pull(get_default_ctx(), …)`, not a
  remote one. Blobs make this problem *better*; converting to them is the best no-code mitigation.
* **"`readers_exist`'s per-directory scan is what costs when a large update is in flight."** It is
  never called in the default configuration: `Reader_Lock`, `Writer_Lock`, `lock_tree_for_write` and
  `lock_dir_for_write` all short-circuit on `if(lock_server)` (`src/lock.cpp:704, 1239, 1256, 572`),
  and `lock_server` is force-defaulted to `127.0.0.1:2402` for `server_active`
  (`src/main.cpp:587-591`). The `#cvs.lock` / `#cvs.rfl.*` machinery is dead code on a stock server.
  It becomes live — and `readers_exist` really does become an `opendir` scan of a directory full of
  `,v` files per attempt — only if you set `LockServer=none`.
* **"The lock server serialises unrelated files behind one another."** Not meaningfully. It is
  thread-per-connection (`lockservice/server.cpp:57-90`) with one global mutex
  (`lockservice/LockParse.cpp:79-128`) held for the duration of a hash-map lookup in `request_lock`
  — microseconds. The genuine serialisation is per **CVS server process**: one global
  `lock_server_socket` (`src/lock.cpp:156`) driven strictly request/response, so one command's
  per-file lock operations are inherently sequential. That is a throughput cost (2 round trips
  × file count), not a cross-command contention cost.
* **"`rtag` avoids the locking."** It does not. `cvstag()` sets `lock_for_write = 1` before the
  `is_rtag` branch (`src/tag.cpp:282`), and `rtag_fileproc` reaches `RCS_settag`/`RCS_rewrite` the
  same way. `rtag` is still worth preferring, but because it is a shorter command, not a gentler one.
* **"`-j N` reduces server-side lock hold time."** `-j` sets `blob_concurrency_download_level`
  (`src/main.cpp:1013-1014`), referenced only in `src/client.cpp` (`:2106, 2177, 5231`). It is
  purely client-side blob-download parallelism. It helps only by finishing the update sooner.
* **"`rcs_internal_lockfile`'s second write lock self-deadlocks the tag."** It does not:
  `can_get_lock` exempts the same owner (`lockservice/LockParse.cpp:872-875`). It is pure overhead,
  not a hang.
* **"Deadlock between update and tag is possible."** Not by lock ordering. Both take the directory
  mapping lock in `open_directory` before any file lock in that directory, and both walk directories
  top-down, so there is no ABBA cycle. What happens is one-sided starvation ending in the 39 s
  fatal — which presents to the user as a hang, but is not one.
* **"The `-c`/uptodate check or the pretag trigger is what serialises."** `check_uptodate` only
  changes `check_fileproc`'s classification (`src/tag.cpp:456-467`) and `pretag` runs once per
  directory in `check_filesdoneproc` (`src/tag.cpp:569-600`), outside any per-file lock. Neither is
  in the blocking chain.
