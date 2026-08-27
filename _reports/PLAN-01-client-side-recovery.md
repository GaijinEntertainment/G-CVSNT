---
id: PLAN-01
area: client robustness / working-copy recovery
status: proposal
---

# Client-side working-copy recovery — implementation plan

All paths below are relative to `cvsnt/cvsnt-2.5.05.3744/`. Every claim about current behaviour is
anchored to a `file:line` that was read during this investigation. Message texts are quoted verbatim
from the source. This is a plan only: no code, no source changes.

**Method note.** This plan was produced by a research → alignment → framing pass run autonomously.
Where a design decision would normally be confirmed with a human, the decision is recorded together
with the alternatives considered (see "Decision log" and "Open questions"). Nothing here is final
until reviewed.

## 1. Problem statement

`cvs update` / `cvs checkout` on large trees routinely runs into a family of situations where the
client either aborts the whole run or leaves the working copy in a state that only converges after
several re-runs with manual (or scripted) repairs in between:

- an unversioned file sitting where the server wants to create a file makes that file undeliverable
  (`move away ...; it is in the way`), run after run, until someone deletes it;
- merges and `-C` leave `.#name.rev` backup files behind forever;
- on Windows, updating a `.exe`/`.dll` that some process has loaded makes the final rename fail and,
  after 10 seconds of retrying, aborts the whole update;
- server-side lock contention aborts the command after ~40 seconds with no way to lengthen the wait;
- directories deleted in the repository leave stale local trees behind (with a *successful* exit
  status, see §2.5), and a locally corrupted `CVS/Entries` aborts the entire update;
- there is no built-in way to remove files unknown to CVS (a `git clean` equivalent).

Each of these forces someone (or something) to delete or repair files by hand and run `cvs` again —
sometimes several times — before the tree converges. The goal of this plan is that the client,
under explicit opt-in switches, handles each situation itself — or fails cleanly and honestly — so
a single run either converges or reports a real, actionable error.

## 2. Current behaviour — verified map

| # | Symptom (verbatim message) | Emitting site(s) | Severity today |
|---|---|---|---|
| 1 | `move away %s; it is in the way` | `src/classify.cpp:111` (local/server classify); `src/client.cpp:1582`, `src/client.cpp:2433` (client, file arrives from server) | non-fatal per file; file never updated; exit 1 via `failure_exit` (`src/client.cpp:1318`, `src/client.cpp:4276`) |
| 2 | `.#<file>.<rev>` backup files | `BAKPREFIX ".#"` `src/cvs.h:269`; created at `src/update.cpp:804` (`backup_file`), `src/update.cpp:2325-2331` (`merge_file`), `src/update.cpp:3101-3108` (`join_file`), `src/client.cpp:5434-5446` (send path), `src/subr.cpp:915-937` (`backup_file`), client `Copy-file` handler `src/client.cpp:1190-1204` | litter accumulates |
| 3 | `Unable to rename file %s to %s for %d second%s, still trying...` / `...for 10 seconds, giving up...` | `windows-NT/filesubr.cpp:726`,`:722` (dead `CVS95` branch `:741`,`:745` equivalents at `:722/:726`; live loop `:729-746`) then fatal `cannot rename file %s to %s` at `windows-NT/filesubr.cpp:763-780` | fatal after 10 s |
| 4 | `Failed to obtain lock on %s[: %s]` | `src/lock.cpp:310` (lock-server FAIL), `src/lock.cpp:349` (20 busy retries exhausted), inside `do_lock_server` `src/lock.cpp:254` | fatal (`[server aborted]`) |
| 5 | `skipping directory %s` | `src/recurse.cpp:770` (only site) | non-fatal, **does not set a nonzero exit** |
| 6 | `cannot open directory %s` (+ errno text) | `src/find_names.cpp:90` (non-fatal; produces the NULL that triggers #5); fatal variants `src/find_names.cpp:103`, `:239` | warning / fatal |
| 7 | `while updating %s, %s is missing (%s). If intentional, create empty Entries to get all files.` | `src/recurse.cpp:1144-1157` (`CVS/Entries` missing) — contrast non-fatal `ignoring %s (%s missing)` for `CVS/Repository` at `src/recurse.cpp:1133-1141` | **fatal, aborts entire update** |
| 8 | `there is no version here; run '%s checkout' first` / `...do '%s checkout' first` | `src/recurse.cpp:274-280`; `src/repos.cpp:81-83` | fatal |
| 9 | `conflict: removed %s was modified by second party` | `src/classify.cpp:200-206` → `T_CONFLICT` | non-fatal, persists every run |
| 10 | (no command exists) unknown files only reported: `? ` via `serve_questionable` `src/server.cpp:2938-2963`, client `Questionable` send `src/client.cpp:5481-5490` | — | manual cleanup only |
| 11 | multiple passes needed to converge | see §3.11 analysis | — |
| 12 | command-line length | response file already exists: `src/main.cpp:690` (`@file`), `-F file` global option (`src/main.cpp:736` `F:`, `:1004-1005`), `append_args` `src/main.cpp:365-392` applied at `src/main.cpp:1072-1073` | solved upstream |

Option-table facts used throughout (all read directly):

- Global short options `src/main.cpp:736`: `+QqrwtnlvT:e:d:HfF:z:s:axyNRo::OL:C:cj:` — global `-C`
  takes an argument (config dir, `src/main.cpp:1025-1027`), global `-n` is `noexec`
  (`src/main.cpp` `case 'n'`), `-F` is the response-file option (`src/main.cpp:1004-1005`).
- Global long options `src/main.cpp:737-763`: `help`, `version`, `encrypt`, `authenticate`,
  `readonly`, `utf8`, `help-commands`, `help-synonyms`, `help-options`, `allow-root`, `crlf`, `lf`,
  `cr`, `testserver`, `win32_socket_io`, `debug`, `blob_url` (values 1-11 in use).
- `update` options `src/update.cpp:183`: `+AB:pCcPflRQqdnuk:r:D:j:bmI:W:3Stxe::i`; long table
  `src/update.cpp:175-179` contains only `blob_zero`. `update -n` clears `backup_local_files`
  (`src/update.cpp:204-205`, default 1 at `src/update.cpp:106`; documented at the usage text
  `src/update.cpp` "-n Do not backup local files(silently remove)...").
- `checkout` options `src/checkout.cpp:148`: `+ANnk:d:flRp::Qqcsr:D:j:Pbm3St` (`-n` there means
  "do not run module program", `src/checkout.cpp` `case 'n'` → `run_module_prog = 0`); `export`
  `src/checkout.cpp:142`: `+Nnk:d:flRQqr:D:`.
- Command table `src/main.cpp:130-212`: no command named `clean` or `sweep` exists (verified by
  grep); precedent for a no-connect command: `switch` with `CVS_CMD_NO_CONNECT`
  (`src/main.cpp:202`).
- `.cvsrc` supplies defaults both for global options (`read_cvsrc(&argc,&argv,"cvs")`
  `src/main.cpp:856`) and per command (`read_cvsrc(&argc,&argv,command_name)`
  `src/main.cpp:1649`), so any new option is `.cvsrc`-settable for free.
- Protocol negotiation: server request table `struct request requests[]` `src/server.cpp:4908`.
  Request flags (`src/server.h:158-176`): `RQ_ESSENTIAL`(1), `RQ_SUPPORTED`(2), `RQ_ENABLEME`(4),
  `RQ_ROOTLESS`(8), `RQ_SERVER_REQUEST`(16) — there is **no** "optional" flag; a non-essential
  request simply carries flags `0`. Fork marker `GAIJIN_RQ_ESSENTIAL` `src/server.cpp:4907`.
  Client response table `struct response responses[]` `src/client.cpp:4022` with
  `rs_essential`/`rs_optional` (fork marker `GAIJIN_rs_essential` `src/client.cpp:4020`); a
  client missing an *essential* response is disconnected by the server
  (`serve_valid_responses`, `src/server.cpp:983-1046`), while unadvertised optional responses
  are simply never sent. The server sends an optional response only after checking the client
  declared it — pattern: `if (!supported_response ("Copy-file")) return;` in `server_copy_file`
  `src/server.cpp:4322`. The client sends a non-essential request only after checking
  Valid-requests — pattern: `if (!supported_request ("Questionable"))` `src/client.cpp:5481`.
  The update client re-encodes options one by one (`send_arg("-C")` etc.,
  `src/update.cpp:331-378`); options are **not** forwarded wholesale, so a client-local option is
  simply never sent. Global options reach the server only through explicit `Global_option` lines,
  which the server re-parses in a six-letter switch (`serve_global_option`,
  `src/server.cpp:2899-2912`) — a new global long option is therefore inherently client-local.
- Existing deletion machinery (precedent for anything that removes trees): `release -d` deletes
  `CVS/` then the directory (`src/release.cpp:80-107`), `update -P` prunes empty dirs via
  `update_dirleave_proc`/`isemptydir` (`src/update.cpp:1414-1449`, `:1464-1475`);
  `unlink_file_dir` performs recursive deletion.
- Entries machinery: `Entries_Open` replays `CVS/Entries.Log` (`A `/`R ` records) and rewrites when
  needed (`src/entries.cpp:938-942`); `write_entries` renames backup files into place and removes
  the logs (`src/entries.cpp:212-231`); `Entries_Close` flushes a still-present log
  (`src/entries.cpp:965-981`). Ignore machinery: `ign_add_file` `src/ignore.cpp:143`, `ign_add`
  `src/ignore.cpp:239`, `ign_name` `src/ignore.cpp:310`.
- Windows rename: `CVS_RENAME` is `wnt_rename` (`windows-NT/config.h:284`); the Windows build
  compiles `windows-NT/filesubr.cpp` **instead of** `src/filesubr.cpp`; `rename_file` there wraps
  `wnt_rename` and is fatal by default (`windows-NT/filesubr.cpp:763-780`). Install path writes
  `_new_<file>` then renames (`src/client.cpp:1629-1630`, `:1807`, `:1907`).

## 3. Items

### 3.1 Unversioned file in the way of an incoming file

**What goes wrong today.** When the server sends a new file and an unversioned file already occupies
its path, the client prints `move away %s; it is in the way`, reports `C <file>`, discards the
received contents, sets `failure_exit = 1`, and leaves the obstruction untouched
(`src/client.cpp:1582` in `update_entries` and `src/client.cpp:2433` in the blob-reference variant
`update_blob_ref_entries`; guard
`data->existp == UPDATE_ENTRIES_NEW && !client_overwrite_existing && isfile (filename)` at
`src/client.cpp:2406`; exit at `src/client.cpp:4276`). In local (non-client/server) operation the
same text comes from classification: no entry + file on disk + contents differ →
`error (0, 0, "move away %s; it is in the way", ...)` and `T_CONFLICT`
(`src/classify.cpp:108-113`). The file is never updated; every subsequent run repeats the message.
Note `update -C` does *not* cover this case: `client_overwrite_existing` is only set for files that
have an entry and were locally modified (`src/client.cpp:5447`).

**Why the client should handle it.** The obstruction is by definition not under version control; the
user-visible result today is an update that can never succeed until a human deletes files by hand.
The client has all the information (path, incoming contents) to resolve it safely without deleting
anything.

**Proposed behaviour** (opt-in). When enabled and the in-the-way condition is hit:
1. Rename the obstructing file to `.#<name>.notversioned.<unix-timestamp>` in the same directory
   (append `.1`, `.2`, ... on collision). Rename, never delete — the user's bytes survive.
2. Print one line: `cvsnt client: moved <name> aside to <backup-name> (it was in the way)`.
3. Proceed with the normal install path (`_new_<file>` → rename, `src/client.cpp:1807`).
4. Do not set `failure_exit` for this file; report the file with its normal `U`/`P` letter.
On disk afterwards: the new versioned file at its path, plus the renamed original. The `.#` prefix
keeps the file inside the existing backup-name family (`BAKPREFIX`, `src/cvs.h:269`), which the
default ignore list already covers (`ign_default` contains `.#*`, `src/ignore.cpp:36-41`) — so it
never shows up as `?` noise and the `clean` command (item 10) treats it as disposable.
Local mode gets the same treatment where classification yields this specific state
(`src/classify.cpp:108-113`; the no-entry sub-case only, not other `T_CONFLICT` causes).

**Switch.** `update --move-in-the-way` (long-only, per-command; also added to `checkout`, which
shares the client install path). Collision check: not in `src/update.cpp:175-179` (only
`blob_zero`), not in the global long table `src/main.cpp:737-763`, no short letter consumed.
`.cvsrc`-settable per command via `src/main.cpp:1649`. Per-command rather than global because the
semantics are specific to update-style file installation.

**Default.** Off. It renames user files; that must be an explicit choice.

**Server-side implications.** None. The two client sites fire on ordinary `Created`/`Updated`
responses; the option is client-local and never transmitted (the update client only sends options
explicitly listed in `src/update.cpp:331-378`). Works against any server, old or new. The
server-side classify emission (`src/classify.cpp:111` running under `server_active`) continues to
cover the server's own temp-dir state and needs no change.

**Estimated LoC.** ~100-140 (two client sites + shared rename-aside helper + local-mode hook +
option plumbing in update/checkout + usage text).

**Risk.** Low-medium. Worst case: a wanted unversioned file gets renamed (never lost). A collision
storm (many timestamped backups) is possible on repeated runs — acceptable, they are inert. Care
needed to apply only in the `UPDATE_ENTRIES_NEW`+no-entry case, never for genuine merge conflicts.

**Testing.** Sandbox repo; place an unversioned file where the server will create one; run update
with and without the switch, local mode and pserver mode; assert exit status, on-disk backup
name, and that a second update is a no-op. Case-insensitivity edge (`src/client.cpp:2408-2415`
ambiguity branch) must remain untouched — regression test.

### 3.2 `.#` backup files accumulate

**What goes wrong today.** Three producers create `.#<file>.<rev>` copies:
1. `merge_file` unconditionally copies the working file to `.#file.rev` before merging
   (`src/update.cpp:2325-2331`);
2. `join_file` does the same for `-j` joins (`src/update.cpp:3101-3108`);
3. in client/server mode the server instructs the client to make that same copy via the `Copy-file`
   response (`server_copy_file` `src/server.cpp:4314-4330`), which the client executes in
   `copy_a_file` (`src/client.cpp:1190-1204`).
The fork already has a partial off-switch: `update -n` sets `backup_local_files = 0`
(`src/update.cpp:204-205`, default 1 at `:106`) and suppresses the `-C` backups at
`src/update.cpp:801-811` ("(Locally modified %s moved to %s)") and `src/client.cpp:5434-5446`. It
does **not** gate the three producers above — that is the gap. Nothing ever deletes these files
("so that it will stay around for a few days before being automatically removed by some cron
daemon", `src/update.cpp:2318-2323` — no such daemon exists on Windows).

**Why the client should handle it.** On build/CI working copies the backups are pure litter that
grows without bound and confuses tooling that inventories the tree.

**Proposed behaviour.** Complete the existing switch rather than invent a second one: when
`backup_local_files == 0`,
- `merge_file` and `join_file` skip the `copy_file` to the `.#` name (merge itself unchanged;
  conflict markers still land in the working file);
- `copy_a_file` accepts the `Copy-file` response but skips the copy (protocol stream stays in
  sync; nothing written).
What it prints: nothing new (the suppressed copies print nothing today). On disk: no `.#*` files.

**Switch.** Existing `update -n` (`src/update.cpp:204-205`) plus a new long alias
`update --no-backups` for readability. Collision check: `--no-backups` absent from
`src/update.cpp:175-179` and `src/main.cpp:737-763`; `-n` already taken with exactly this meaning
inside update (and means other things in `checkout` (`run_module_prog`, `src/checkout.cpp`) and
globally (`noexec`) — no change to either). `.cvsrc`-settable per command (`update -n` line);
recorded alternative: a brand-new flag leaving `-n` untouched (rejected: `-n`'s documented intent
is precisely "Do not backup local files", and two flags for one concept invites drift).

**Default.** Backups stay on (current behaviour). `-n` is already an explicit destructive opt-in
("Irreversibly deletes locally modified files", usage text in `src/update.cpp`).

**Server-side implications.** None required. `-n` is *not* forwarded to the server (absent from
`src/update.cpp:331-378`), and the fix consumes the `Copy-file` response client-side, so old
servers keep sending it and new clients simply skip the copy. The server's own `.#` copies during
c/s merges happen in its temp dir and never reach the client. Old client + new server: unchanged.

**Estimated LoC.** ~30-50.

**Risk.** Low. With the flag on, a failed merge loses the pre-merge snapshot — but the flag's
existing contract already declares local modifications forfeit. One subtlety: `merge_file` uses the
backup for restore-on-abnormal-merge-failure (`src/update.cpp:2416` renames it back); when
suppressing, that restore path must degrade to leaving the half-merged file with a warning —
implement and test explicitly.

**Testing.** Local merge, c/s merge (pserver loopback), `-j` join, `update -C`, each with and
without `-n`/`--no-backups`; assert zero `.#*` afterwards and unchanged behaviour without the flag;
force an `RCS_merge` failure to exercise the degraded restore path.

### 3.3 Destination file in use during rename (Windows)

**What goes wrong today.** The install step writes `_new_<file>` then renames over the destination
(`src/client.cpp:1629-1630`, `:1807`, `:1907`). On Windows the rename is
`wnt_rename` (`windows-NT/filesubr.cpp:669`; `CVS_RENAME` mapping `windows-NT/config.h:284`):
`MoveFileEx(from, to, MOVEFILE_COPY_ALLOWED|MOVEFILE_REPLACE_EXISTING)` in a loop
(`windows-NT/filesubr.cpp:729-746`) that retries only `ERROR_ACCESS_DENIED`, sleeping 100 ms per
try, printing every second `Unable to rename file %s to %s for %d second%s, still trying...`
(`:726`) and giving up after 100 tries (~10 s): `Unable to rename file %s to %s for 10 seconds,
giving up...` (`:722`). The enclosing `rename_file` then errors fatally: `cannot rename file %s to
%s` (`windows-NT/filesubr.cpp:763-780`) — the whole update aborts. This is exactly what happens
when the destination is an `.exe`/`.dll` currently loaded by a running process (the image file
cannot be replaced or deleted, but it *can* be renamed). There is no move-aside, no
`MOVEFILE_DELAY_UNTIL_REBOOT`, no `ReplaceFile` anywhere under `windows-NT/` (verified by search).

**Why the client should handle it.** Ten seconds of retry cannot outwait a running program; the
update dies mid-run, leaving a half-updated tree, and must be re-run after the process exits — or
the file must be renamed aside externally, which is precisely the recovery the client itself can
perform.

**Proposed behaviour** (opt-in). In `wnt_rename`, when the flag is set and the loop has seen
`ERROR_ACCESS_DENIED` for ~2 s (20 tries) with the destination existing:
1. Rename the destination to `.#<name>.inuse.<pid>.<unix-timestamp>` (same directory; NTFS permits
   renaming a mapped image).
2. Print `cvsnt client: renamed in-use <name> aside to <backup-name>`.
3. Continue the existing retry loop (which should now succeed immediately).
4. After overall success, attempt `DeleteFile` on the moved-aside copy once; if it fails (still
   mapped — expected), leave it and print nothing further.
Timeout and give-up path unchanged when the aside-rename itself fails.
On disk afterwards: the new file at the destination; possibly one `.#...inuse...` remnant, in the
`.#` family so item 10's `clean` and ignore conventions collect it later.

**Switch.** Global long option `--rename-in-use` (add to `src/main.cpp:737-763`, next free value
12), setting a global flag read by `wnt_rename`. Collision check: absent from the global long
table and from all per-command tables inspected (`src/update.cpp:183`, `src/checkout.cpp:142/:148`).
Global, not per-command, because the rename layer is command-agnostic (update, checkout, edit
paths all funnel through `rename_file`). `.cvsrc`-settable on the `cvs` line
(`src/main.cpp:856`). Recorded alternative: environment variable (rejected: invisible in command
lines and logs); per-command option (rejected: would need plumbing through every command).

**Default.** Off. It renames files that another process is actively using.

**Server-side implications.** None; entirely inside the client's platform layer. No protocol
surface: global options only reach a server through explicit `Global_option` lines
(`serve_global_option`, `src/server.cpp:2899-2912`), and this flag is never sent. Non-Windows
builds compile `src/filesubr.cpp` (no retry loop, `:759-771`) — the flag is accepted and ignored
there.

**Estimated LoC.** ~70-100 (all in `windows-NT/filesubr.cpp` + option plumbing).

**Risk.** Medium. Renaming a file another process is *writing* (not just executing) could confuse
that process — mitigated by triggering only on the exact retry pattern that today ends in an abort
anyway, and by opt-in default. `unlink_file` has no sharing-violation handling at all
(`windows-NT/filesubr.cpp:785-812`; `ERROR_SHARING_VIOLATION` is not even mapped by `_dosmaperr`,
`windows-NT/win32.cpp:77-124`) — the post-success cleanup must therefore tolerate failure silently.

**Testing.** Windows-only integration test: build a tiny DLL in the sandbox repo, `LoadLibrary` it
from a helper process, commit a new revision, update with and without the flag; assert abort
(without) vs success + aside-file (with). Verify the aside file is deletable after the helper
exits.

### 3.4 Server-side lock contention aborts after a fixed short wait

**What goes wrong today.** With a lock server configured (`LockServer` in `CVSROOT/config`,
`src/parseinfo.cpp:329-334`), `do_lock_server` (`src/lock.cpp:254`) reacts to a busy response by
sleeping 1 s per attempt (`src/lock.cpp:356`) plus 5 s on every 5th attempt (`:341`), printing
`[%s] waiting for %s's lock in %s` (`:343-345`); after a hardcoded `bWaited==20`
(`src/lock.cpp:348`) it aborts fatally: `Failed to obtain lock on %s` (`:349`) — roughly 40
seconds total. The FAIL response aborts immediately (`:310`). This runs in the server process, so
the client sees `cvsnt [server aborted]: Failed to obtain lock on <path>,v`. No configuration key,
environment variable, or option adjusts the 20-attempt cap (verified: the only lock-related config
keys are `LockDir`/`LockServer`, `src/parseinfo.cpp:320-334`; the filesystem-lock fallback waits
*forever* in 15 s steps instead — `lock_wait`, `src/lock.cpp:1164-1179`, `CVSLCKSLEEP`
`src/cvs.h:266`).

**Why this should be handled.** Long housekeeping operations (tagging, large commits) legitimately
hold locks for minutes. Aborting the entire update at ~40 s forces external re-runs; the process
already knows how to wait and report progress — the bound is just not adjustable.

**Proposed behaviour.** Make the bound configurable server-side: a `CVSROOT/config` key
`LockWaitMax=<seconds>` (0 = fail on first busy; absent = current default ≈40 s, preserved
exactly). `do_lock_server` converts seconds to its iteration budget; progress messages unchanged
(they already print periodically). What it prints: existing wait lines, then either the existing
success line `[%s] obtained lock in %s` (`src/lock.cpp:296`) or the existing failure. Nothing new
on disk.

**Switch.** Config key, not a command-line option: the wait happens in the server process; clients
cannot meaningfully carry per-repository lock policy, and `parse_config` is the established home
for exactly this kind of knob (pattern at `src/parseinfo.cpp:320-334`). Spelling `LockWaitMax`
collides with no existing key in the `parse_config` chain read during this investigation.

**Default.** Current behaviour (≈40 s) — no behavioural change unless the administrator opts in.

**Server-side implications.** Server-only change; zero protocol impact; old clients see identical
messages. The near-duplicate lock client in `tools/simpleLock.cpp.inc:185/:223` (compiled into
standalone maintenance tools) is deliberately left unchanged in the first pass — noted as a known
divergence.

**Estimated LoC.** ~30-50.

**Risk.** Low. Longer waits hold a server process and its connection open; bounded by the
configured value. A misconfigured huge value degrades to "hangs like the filesystem backend
already does", with progress messages.

**Testing.** Two concurrent sessions against a loopback pserver + lock server: holder takes a
write lock, second session updates; assert abort timing with no key, extended wait with
`LockWaitMax=120`, immediate failure with `0`.

### 3.5 + 3.6 Directory deleted in the repository leaves a stale local tree

**What goes wrong today.** For a local directory whose repository counterpart is gone, recursion
still descends (the hint checks only local existence, `src/recurse.cpp:1233-1244`), then
`Find_Names` fails to open the repository directory — `cannot open directory %s` (+ `No such file
or directory`, `src/find_names.cpp:88-92`, non-fatal) — returns NULL, and `do_recursion` prints
`skipping directory %s` (`src/recurse.cpp:770`) and jumps past all file and subdirectory
processing (`goto skip_directory`, `:774`). Crucially the error counter is not touched: **the
update exits 0** while leaving the entire stale tree (files + `CVS/` metadata) on disk, run after
run. In client/server mode both messages arrive prefixed `cvsnt server:`.

**Why the client should handle it.** The tree silently diverges from the repository; stale sources
keep being compiled; and because exit status is 0, nothing upstream even notices. Text-parsing the
`E` lines is not a viable client strategy — the fix belongs where the knowledge is: the server
knows the directory is gone; the client owns the local tree.

**Proposed behaviour.**
- **Protocol**: a new optional response `Removed-directory <update-dir>` sent by the server at the
  exact point it today prints the two messages (`src/recurse.cpp:768-775` under `server_active`),
  following the established gate pattern: `if (!supported_response ("Removed-directory")) return;`
  (pattern: `server_copy_file`, `src/server.cpp:4322`). Name collision verified against the
  response table: `Removed` (`src/client.cpp:4050`), `Remove-entry` (`:4052`), `Clear-*`
  (`:4057-4072`) exist; `Removed-directory` does not.
- **Client**: new handler records the directory; at command end, if `--prune-removed-dirs` is on,
  for each recorded directory: verify it has a `CVS/` admin dir; delete files listed in its
  `Entries` (+ replayed `Entries.Log`, machinery `src/entries.cpp:801-952`), recurse into
  registered subdirectories, remove `CVS/`, then remove each directory only if now empty
  (precedent: `release_delete_dirleaveproc`, `src/release.cpp:80-107`, and
  `update_dirleave_proc`/`isemptydir`, `src/update.cpp:1414-1449`/`:1464-1475`). Unversioned
  files are **left in place** and the directory is then kept, with a summary line
  `cvsnt client: directory <dir> was removed in the repository; kept <n> unversioned file(s)`.
- What it prints: one line per pruned directory (`cvsnt client: pruned <dir> (removed in
  repository)`); existing messages unchanged. On disk: stale versioned content gone; unversioned
  content untouched.

**Switch.** `update --prune-removed-dirs` (long, per-command update/checkout; client-local, never
transmitted). Collision check: absent from `src/update.cpp:175-179`, `src/main.cpp:737-763`.
`.cvsrc`-settable. Recorded alternative: piggyback on `-P` (rejected: `-P` today only prunes
*empty* dirs — silently upgrading it to delete file trees would be a semantic trap).

**Default.** Off (deletes versioned user data by design).

**Server-side implications.** New response, negotiated: new client + old server → client declares
`Removed-directory` but nothing sends it; nothing happens (today's behaviour). Old client + new
server → `supported_response` fails, server sends nothing new. Mark the response `rs_optional` in
the client table (`src/client.cpp:4022`). No new request; the *option* is consumed client-side
only, so no old-server getopt hazard (forwarding block `src/update.cpp:331-378` untouched).

**Estimated LoC.** ~180-250 across `server.cpp` (emit), `client.cpp` (handler + prune walk),
`update.cpp`/`checkout.cpp` (option), usage/docs.

**Risk.** High by nature (recursive deletion). Mitigations: entries-only deletion, keep-on-unknown
rule, never follow symlinks, off by default, dry-run mode (`--prune-removed-dirs` respects global
`-n` `noexec` to print what it would remove).

**Testing.** Loopback pserver: delete a directory in the repository (move the `,v` dir aside),
update with/without the flag, old-client-vs-new-server and new-client-vs-old-server matrix
(build both), unversioned-file-present case, nested-subdir case, exit codes, and `-n` dry-run.

### 3.7 Missing `CVS/Entries` aborts the entire update

**What goes wrong today.** A subdirectory with a `CVS/` dir but no `Entries` file kills the whole
run: `error (1, 0, "while updating %s, %s is missing (%s). If intentional, create empty Entries to
get all files.", ...)` (`src/recurse.cpp:1144-1157`). Contrast: missing `CVS/Repository` in the
same block is a *non-fatal* skip — `ignoring %s (%s missing)` + `R_SKIP_ALL`
(`src/recurse.cpp:1133-1141`). One corrupt subdirectory therefore prevents updating everything
else, and the message itself already tells the user the manual fix.

**Why the client should handle it.** The remedy the message prescribes (create an empty Entries) is
mechanical; aborting a 20-minute update on one corrupt admin file forces a full re-run after a
by-hand touch-up.

**Proposed behaviour** (opt-in). When enabled, at that site: write a minimal valid `Entries`
(single `D` terminator line, the format `write_entries` produces, `src/entries.cpp:141-231`),
print `cvsnt client: recreated missing %s in %s; all files will be refetched`, and continue
recursion instead of aborting. The server then re-sends everything in that directory as new files;
on-disk files that now conflict are exactly item 3.1's case — pairs naturally with
`--move-in-the-way` (files with local edits are preserved as `.#*.notversioned.*` copies rather
than silently overwritten). Without 3.1 enabled, in-the-way conflicts surface per file as today.
On disk: a fresh `CVS/Entries`; nothing deleted.

**Switch.** `update --recreate-entries` (long, per-command; client-local; `.cvsrc`-settable).
Collision check: absent from `src/update.cpp:175-179` and `src/main.cpp:737-763`. Recorded
alternative: make the site non-fatal-skip like the `CVS/Repository` case (rejected as the default:
it would silently stop updating that subtree; acceptable as a follow-up `--skip-bad-admin` if
wanted).

**Default.** Off (changes failure semantics; a missing Entries can indicate wider corruption worth
a human look).

**Server-side implications.** None; purely a client-recursion change on a path the server never
sees (`W_LOCAL` guard, `src/recurse.cpp:1116`).

**Estimated LoC.** ~40-60.

**Risk.** Low-medium: could mask real corruption; the loud message and off-default mitigate.
Dead-code note: the current `dir_return = R_SKIP_ALL;` after the fatal call (`src/recurse.cpp`) is
unreachable today and becomes live logic — review carefully.

**Testing.** Delete `CVS/Entries` in a subdir of a sandbox copy; update with/without the flag,
with/without `--move-in-the-way`, local and pserver modes; assert full-tree refetch and preserved
local edits.

### 3.9 Removed-locally vs modified-remotely conflict persists forever

**What goes wrong today.** Entry removed locally (`-` revision), file changed on the branch since:
classification prints `conflict: removed %s was modified by second party`
(`src/classify.cpp:200-206`), returns `T_CONFLICT`, and the run reports `C` for that file — every
run, forever, until a human resolves it (typically by resurrecting the file).

**Why the client should handle it.** In automation the only sane policy is deterministic: accept
the repository side (the removal lost the race). Today that requires manual `Entries` surgery or
directory deletion.

**Proposed behaviour** (opt-in). When enabled and this exact classify state is hit: drop the
removed entry (`Scratch_Entry`, `src/entries.cpp:236-273`), check out the head revision, print
`U <file>` plus one explanatory line `cvsnt: resurrected <file> (removed locally, modified in
repository)`. On disk: the repository version of the file; the pending removal is cancelled.

**Switch.** `update --resurrect-removed` (long, per-command). Collision check: absent from
`src/update.cpp:175-179` and `src/main.cpp:737-763`. Because classification runs **server-side**
in client/server mode, the server must know the option — and an old server's getopt would reject
an unknown argument. Gate transmission on capability: add a request `Resurrect-removed` to
`requests[]` (`src/server.cpp:4908`) with flags `0` — non-essential; there is no separate
"optional" request flag, absence of `RQ_ESSENTIAL` is the convention (`src/server.h:158-176`) —
whose handler just sets a flag, and have the client send it only
`if (supported_request ("Resurrect-removed"))` (pattern `src/client.cpp:5481`); otherwise print a
one-line notice and behave as today. Resolution itself
travels over existing essential responses (`Updated`/`Checked-in`), so no new response is needed.

**Default.** Off (it overrides a recorded user intention — the removal).

**Server-side implications.** New optional request + server-side classify/update handling. Degrade:
old server → request absent from Valid-requests → client never sends it → today's behaviour. Old
client + new server → request never sent → today's behaviour.

**Estimated LoC.** ~120-180 (classify/update_fileproc handling shared by local and server modes,
request plumbing, option).

**Risk.** Medium: silently cancels removals when enabled; must be scoped to exactly this classify
state (`vn_user[0] == '-'`, no user file, head ≠ removed base — `src/classify.cpp:180-206`), never
the other `T_CONFLICT` causes.

**Testing.** Script the race in a sandbox repo (remove+no-commit locally, commit a change from a
second working copy), then update with/without the flag in local and pserver modes; assert entry
state, file contents, exit codes; matrix against an old server build for the degrade path.

### 3.10 No built-in way to delete unknown files (sweep)

**What goes wrong today.** CVS only *reports* unknown files (`? name`): client-side via the
`Questionable` request (`src/client.cpp:5481-5490`) and server-side echo (`serve_questionable`,
`src/server.cpp:2938-2963`, after `ign_name`); nothing can delete them. Keeping a build tree
byte-identical to the repository therefore requires re-implementing Entries and ignore parsing
outside CVS — re-implementations that inevitably drift from the real semantics.

**Why the client should handle it.** The client already owns correct `Entries`(+`.Log`) replay
(`src/entries.cpp:801-952`) and the real ignore semantics (`src/ignore.cpp:143/:239/:310`);
duplicating those externally is exactly what produces divergence bugs.

**Proposed behaviour.** A new local-only command:
`cvs clean [-n] [-f] [-d] [-x] [-l] [-I pat] [path...]`
- Default (and `-n`): dry-run — list `? would remove <path>` and change nothing.
- `-f`: delete unknown *files*. `-d`: also unknown directories (recursive). `-x`: include ignored
  entries too. `-l`: this directory only. `-I pat`: extra ignore patterns (mirror of update's `-I`,
  `src/update.cpp:210-212`).
- Walk only directories that have a `CVS/` admin dir (stop at the boundary otherwise); build the
  known set from `Entries` with `Entries.Log` replayed (`Entries_Open`); reuse the existing
  unknown-file enumerator `ignore_files` (`src/ignore.cpp:377`), which already handles the
  per-directory `.cvsignore` hold semantics (`ign_add_file`) and skips symlinks unconditionally
  (`src/ignore.cpp:472-477`); never touch `CVS/` or `.git`; refuse to run if the starting
  directory has no `CVS/`. Note the default ignore list covers `*.exe *.dll *.obj *.o` and
  friends (`ign_default`, `src/ignore.cpp:36-41`), so plain `clean -f` leaves typical build
  outputs alone — removing those too requires the explicit `-x`.
- Prints one line per removal (`removed <path>`); summary count at the end. On disk: only
  unknown (and, with `-x`, ignored) entries removed.

**Switch.** New command `clean` in `cmds[]` (`src/main.cpp:130-212`), flags
`CVS_CMD_USES_WORK_DIR | CVS_CMD_NO_CONNECT` (precedent: `switch`, `src/main.cpp:202`). Name
collision: verified absent from the command table (no `clean`, no `sweep`). Its own option letters
are a fresh namespace; chosen letters mirror git-clean muscle memory. `.cvsrc`-settable like any
command (`src/main.cpp:1649`).

**Default.** Dry-run unless `-f` — the command never deletes without an explicit flag.

**Server-side implications.** None. `CVS_CMD_NO_CONNECT`; ignores server entirely; works against
any server version because it never talks to one. (A later enhancement could consult the server's
`cvsignore`; explicitly out of scope here.)

**Estimated LoC.** ~300-400 (new `clean.cpp`, table entry, usage, man page stub).

**Risk.** Highest of the set — mass deletion. Mitigations: dry-run default, `-f`/`-d` gating,
admin-dir requirement, symlink/no-descend rules, and unit tests over the classification walk
before any deletion code lands.

**Testing.** Fixture tree with known/unknown/ignored files, nested `.cvsignore`, `Entries.Log`
pending records, symlinks, read-only files (Windows `FILE_ATTRIBUTE_READONLY` — deletion must
clear it the way `unlink_file` does, `windows-NT/filesubr.cpp:785-812`), unknown dirs with known
children (must be kept without `-d`... and with `-d` only if fully unknown). Dry-run output
compared golden; then `-f`/`-d`/`-x` state assertions.

### 3.11 Multiple passes needed to converge — analysis (no port)

The investigation found no single "ordering bug"; the multi-pass behaviour is the composition of
verified mechanics:
1. **Fatal aborts mid-run** kill the remainder of the update, so later problems surface only on the
   next run: missing `Entries` (`src/recurse.cpp:1144-1157`), in-use rename
   (`windows-NT/filesubr.cpp:722` → fatal `:763-780`), lock exhaustion (`src/lock.cpp:310/:349`).
2. **Non-fatal but unresolved conflicts persist**: in-the-way files are reported and skipped every
   run (`src/client.cpp:1582/:2433`), removed-vs-modified likewise (`src/classify.cpp:200-206`) —
   the run "completes" (exit 1 via `src/client.cpp:4276`) without converging.
3. **Silently skipped stale directories** don't even affect the exit status
   (`src/recurse.cpp:770` — the skip bypasses the error accounting), so a run can look successful
   while work remains.
Items 3.1, 3.3, 3.4, 3.5-3.7 remove causes 1-3 respectively; after Phase 2 (below) a single run
should converge or fail with a true error. Recommendation: no separate code change; add the
convergence test matrix to Phase 2's acceptance criteria and re-investigate only if a
counterexample survives.

### 3.12 Command-line length — already solved (no port)

`cvs` already supports response files, twice over, verified:
- Trailing `@file`: detected at the very top of `main` (`src/main.cpp:690`; error text
  `Can't open response file <%s>`), documented in the usage text: "response-file should be
  preceded with @ and it has to be the last command line argument" (`src/main.cpp:229`, `:237-238`).
  Format: **one argument per line** (`fgets` into an 8192-byte line buffer, CR/LF stripped, empty
  lines skipped, confirmation `parsed response file <%s>...` printed — `src/main.cpp:700-718`);
  runs before any option parsing, so the file may contain global options, the command name, and
  file arguments; not recursive. One line = one argument also means paths with spaces need no
  quoting at all.
- Global `-F <file>` (`src/main.cpp:736` `F:`, handler `:1004-1005`) → `append_args`
  (`src/main.cpp:365-392`) appends the file's tokens after the real argv, applied *after* global
  options and *before* command dispatch (`src/main.cpp:1072-1073`) — so the file may contain the
  command's options and file arguments. Tokenisation is `line2argv` (`src/subr.cpp:298`), which
  supports quoting and backslash escapes, so paths with spaces work.
This fully covers Windows command-line length limits for update/commit file lists. **Needs no
port — only documentation**: add a short section with examples to the user docs, and one test that
drives `cvs -F` with a >32 KB file list. Estimated LoC: 0 code, ~docs only.

## 4. Items recommended NOT to port

- **Item 8 — `there is no version here` auto-wipe-and-recheckout.** The fatal sites
  (`src/recurse.cpp:274-280`, `src/repos.cpp:81-83`) mean "this directory is not a working copy at
  all". Deleting the directory and re-checking out is a provisioning decision, not an update
  recovery — the directory's contents are, from CVS's point of view, 100% unknown data, and
  destroying unknown data on a failed precondition is exactly the class of behaviour this plan
  refuses to default into the client. The clean failure already exists (clear message, exit ≠ 0);
  scripted environments can respond with an explicit `checkout`. Revisit only if a concrete
  `checkout --force-into-nonempty` use case is written up (see open questions).
- **Deleting unknown *directories* mid-update** (a behaviour adjacent to items 1/10): superseded by
  the explicit `clean` command; folding deletion into `update` itself couples two orthogonal
  decisions ("get current" vs "discard unknown data") behind one flag.
- **Item 11** — no code change; analysis and test matrix only (§3.11).
- **Item 12** — no code change; documentation only (§3.12).

## 5. Phased implementation plan

Phases are vertical slices: each crosses option-parsing, core behaviour, (where relevant)
protocol, docs, and tests, and each is independently shippable.

### Phase 1 — Tracer bullet: complete the backup off-switch (item 2)

**Components:** `src/update.cpp` (gates in `merge_file`/`join_file`, `--no-backups` long alias),
`src/client.cpp` (`copy_a_file` gate), usage text, tests.
**Testing strategy:** sandbox repo scripts (local + loopback pserver); golden output; forced-merge
failure for the degraded restore path.
**Verification gate:** local merge, c/s merge, join, and `-C` runs with the switch leave zero
`.#*` files; identical byte-for-byte behaviour without it.
**Acceptance criteria:**
- [ ] `update -n` / `--no-backups` suppresses all three producers (§3.2) in local and c/s modes.
- [ ] Merge results (conflict markers, exit codes) unchanged by the switch.
- [ ] No behaviour change with the switch absent.

### Phase 2 — Client-only recovery switches (items 1, 3, 7; verifies 11)

**Components:** `src/client.cpp` (in-the-way move-aside at both sites), `src/classify.cpp` +
`src/update.cpp` (local-mode hook), `windows-NT/filesubr.cpp` (in-use rename-aside),
`src/main.cpp` (global `--rename-in-use`), `src/recurse.cpp` (`--recreate-entries`),
`src/checkout.cpp` (share `--move-in-the-way`), docs, tests.
**Testing strategy:** per-item tests from §3.1/3.3/3.7 plus the §3.11 convergence matrix: a
fixture tree seeded with an in-the-way file, a loaded DLL, and a gutted `CVS/Entries`, updated
once with all Phase-2 switches on.
**Verification gate:** the convergence matrix reaches a clean tree in **one** run; every recovery
prints its one-line notice; nothing is deleted (only renamed/created).
**Acceptance criteria:**
- [ ] Each switch off ⇒ byte-identical behaviour to today.
- [ ] Each switch on ⇒ documented recovery, correct exit status, artifacts only in the `.#` family.
- [ ] Windows in-use test passes on a real Windows runner.

### Phase 3 — Protocol slice: stale-directory pruning (items 5+6)

**Components:** `src/server.cpp` (emit `Removed-directory`, gated), `src/client.cpp` (response
table entry + handler + prune walk), `src/update.cpp`/`src/checkout.cpp`
(`--prune-removed-dirs`), docs, tests.
**Testing strategy:** version-matrix tests (new/new, new/old, old/new builds), unversioned-file
retention, `-n` dry-run, nested dirs.
**Verification gate:** with the flag, a repository-deleted directory disappears locally in one
run (unversioned files retained, reported); against an old peer, behaviour is exactly today's.
**Acceptance criteria:**
- [ ] Negotiation: response never sent to a client that didn't declare it.
- [ ] Deletion strictly limited to Entries-known content + `CVS/` + empty dirs.
- [ ] Exit status reflects remaining stale dirs when the flag is off? — no: unchanged (recorded).

### Phase 4 — New command: `cvs clean` (item 10)

**Components:** new `src/clean.cpp`, `cmds[]` entry in `src/main.cpp`, usage/man, tests.
**Testing strategy:** classification-walk unit tests first (dry-run listing vs golden), then
deletion-state tests; Windows read-only files; symlinks.
**Verification gate:** dry-run output matches golden on the fixture tree; `-f -d -x` leaves
exactly the Entries-known + non-ignored set.
**Acceptance criteria:**
- [ ] No deletion ever without `-f`.
- [ ] Ignore semantics match `update`'s reporting (`? ` parity on the same fixture).
- [ ] Refuses to run outside a working copy.

### Phase 5 — Server-side policy (items 4, 9)

**Components:** `src/parseinfo.cpp` (`LockWaitMax`), `src/lock.cpp` (bound), `src/server.cpp` +
`src/classify.cpp`/`src/update.cpp` (`Resurrect-removed` request + resolution),
`src/client.cpp` (capability-gated send), docs, tests.
**Testing strategy:** two-session lock contention timing; removed-vs-modified race scripts;
old/new build matrix for the capability gate.
**Verification gate:** lock waits honour the key with unchanged default; resurrect works in local
and c/s modes and degrades silently against an old server.
**Acceptance criteria:**
- [ ] `LockWaitMax` absent ⇒ timing identical to today (±1 iteration).
- [ ] `--resurrect-removed` touches only the exact classify state of §3.9.
- [ ] Old-server degrade prints its notice and changes nothing.

### Phase sequence

```
Phase 1 (tracer, no deps)
    ↓
Phase 2 (client-only recovery; depends on 1 only for shared test scaffolding)
    ↓
Phase 3 (protocol prune)   — independent of 4, 5
Phase 4 (clean command)    — independent of 3, 5
Phase 5 (server policy)    — independent of 3, 4
```
Phases 3-5 can proceed in parallel after Phase 2.

### Scope boundaries

**In scope:** the twelve catalogued situations; update/checkout/clean client behaviour; the two
protocol additions; `CVSROOT/config` lock policy.
**Out of scope:** the maintenance tools' duplicated lock client (`tools/simpleLock.cpp.inc`),
blob-transfer subsystem, any change to default behaviour without its switch, repository-side
storage changes.

## 6. Open questions (each with a recommended answer)

1. **Extend `-n` (item 2) or add a separate flag?** Recommended: extend `-n` + `--no-backups`
   alias — its documented contract already says "Do not backup local files". Alternative (new
   independent flag) preserved in §3.2 if reviewers consider `-n`'s blast radius too subtle.
2. **Backup-name spellings.** Recommended: `.#<name>.notversioned.<ts>` (item 1) and
   `.#<name>.inuse.<pid>.<ts>` (item 3) — keeps everything in the `BAKPREFIX` family one `clean`
   run collects. Alternative: a dedicated `.cvs-aside/` subdirectory (cleaner listing, but
   invents a new artifact class and breaks the "same directory" expectation of `Copy-file`-style
   names).
3. **Should `--move-in-the-way` also cover `checkout`?** Recommended: yes (same client install
   path); Phase 2 includes the checkout option row. Alternative: update-only first.
4. **`Removed-directory` prune semantics with unversioned files present.** Recommended: keep the
   directory, retain unversioned files, report (§3.5). Alternative (delete everything under a
   second `--force` tier) deferred until real-world need is shown.
5. **`LockWaitMax` default.** Recommended: preserve today's ≈40 s to make the change pure opt-in.
   Alternative: raise the default to 600 s (matches long-job reality but changes behaviour for
   every installation — needs an owner's call).
6. **Command name `clean` vs `sweep`.** Recommended: `clean` (matches the dominant mental model);
   no alias initially. `sweep` recorded as the alternative.
7. **Is a `checkout --into-nonempty` wanted** (the constructive replacement for the rejected item
   8 wipe)? Recommended: no for now; re-open with a concrete use case.
8. **Should Phase 3's flag-off runs exit nonzero when stale dirs are skipped** (today exit 0,
   §2.5)? Recommended: yes as a *separate follow-up* — it is a behaviour change visible to every
   existing consumer and deserves its own review.

## 7. Decision log (autonomous-run record)

- Research was performed blind-to-intent per scope (update/client flow, filesystem layer, locking,
  recursion/Entries, options/main, protocol) and compiled here; every anchor cited was read
  directly during this investigation.
- Patterns adopted: capability gating via `supported_request`/`supported_response`
  (`src/client.cpp:5481`, `src/server.cpp:4322`) for anything protocol-visible; long-option-first
  naming (short letters in `update` are nearly exhausted — `src/update.cpp:183`); `CVSROOT/config`
  keys for server policy (`src/parseinfo.cpp:320-334`); `.#` `BAKPREFIX` family for every
  aside-file this plan creates (`src/cvs.h:269`); dry-run-by-default for the destructive command
  (no CVS precedent for `git clean`, so the safest external convention was adopted).
- Pattern deviated from: the codebase's habit of burying policy in hardcoded constants
  (`bWaited==20`, `count==100`) — new bounds are configurable or flag-gated.
- Single-file output (this document) chosen over the usual multi-artifact layout to match the
  deliverable contract for this plan.
