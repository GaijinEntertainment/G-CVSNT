---
id: PLAN-02
area: update / path exclusion
status: proposal
---

# PLAN-02 — `cvs update --exclude`: a path-exclusion switch for the update routine

All file references are relative to `cvsnt/cvsnt-2.5.05.3744/` unless prefixed otherwise. Every
`file:line` below was read in the tree at the time of writing.

**Process note.** This document was produced with the DRAFT pipeline stages R (research) →
A (alignment) → F (frame), run autonomously. Where the pipeline normally pauses for a human
decision, the decision is recorded inline as **D-n** with the considered alternatives and the
reasoning, and the pipeline continued. Agent teams were unavailable in this environment, so the
research stage was executed as a direct single-agent investigation instead of parallel blind
teammates; the neutrality discipline (facts first, opinions quarantined into §2) was kept.

---

## 0. The gap, verified

- `update`'s argument list is an **inclusion** list: named args are split into `dirlist` /
  `filelist` in `start_recursion` (src/recurse.cpp:320-443) and only those subtrees are walked.
- `update` has no exclusion option of any kind: the full option set is the getopt string at
  src/update.cpp:183 plus the single long option `--blob_zero` (src/update.cpp:175-179); the usage
  text (src/update.cpp:129-160) confirms. The only `--exclude` hits in the tree are GNU-diff
  passthrough options (src/diff.cpp:92-93) and unrelated ChangeLog entries.
- `-I` is not exclusion: `-I` feeds `ign_add` (src/update.cpp:210-212) and the ignore list is
  consulted only for **unknown** files — see §1.2.
- The modules file's `!path` exclusion (src/modules.cpp:156-160) works only through `do_module`,
  i.e. `checkout`/`export` of a module; `update` of a working copy never routes its recursion
  through `do_module` (update calls it only afterwards to run `mtUPDATE` trigger programs,
  src/update.cpp:589-591, callback `NULL`). Passing `!dir` as an update argument is treated as a
  filename and fails in `start_recursion` ("no such directory", src/recurse.cpp:436).

So today there is no way to say "update everything except these paths". Confirmed.

---

## 1. Research (R) — how the machinery actually works

### 1.1 The recursion engine and its callback contract

`start_recursion` (src/recurse.cpp:144) fills a `recursion_frame` with six callbacks —
`fileproc`, `filesdoneproc`, `predirentproc`, `direntproc`, `dirleaveproc`, `permproc`
(src/recurse.cpp:32-46) — expands wildcards (src/recurse.cpp:204), and:

- With no args: `dirlist = ["."]` (src/recurse.cpp:301) and calls `do_recursion`
  (src/recurse.cpp:457).
- With args: directories go to `dirlist` (src/recurse.cpp:325-329); file args are grouped per
  directory in `files_by_dir` (src/recurse.cpp:389,426,432) and unrolled by `unroll_files_proc`
  (src/recurse.cpp:1434), which chdirs into the holding directory and runs `do_recursion` with a
  pre-set `filelist` — **no `direntproc` is invoked for the holding directory itself** in that
  path (src/recurse.cpp:1447-1508).

`do_recursion` (src/recurse.cpp:601), per directory:

1. Enumerates files into `filelist` via `Find_Names` (src/recurse.cpp:766) and subdirectories into
   `dirlist` via `Find_Directories` (src/recurse.cpp:784) — unless lists were pre-set from args
   (src/recurse.cpp:743).
2. Runs `fileproc` for each file through `do_file_proc` (src/recurse.cpp:826, dispatch at 954/957).
3. Runs `filesdoneproc` (src/recurse.cpp:840).
4. Recurses into each subdir through `do_dir_proc` (src/recurse.cpp:862).

`do_dir_proc` (src/recurse.cpp:975), per subdirectory:

- Builds the child `update_dir` by appending the dir name (src/recurse.cpp:1028-1043) — this is
  the slash-separated path **relative to the invocation directory** and the string every
  path-based decision keys on.
- Calls `predirentproc` (src/recurse.cpp:1256-1260), then `direntproc` (src/recurse.cpp:1285).
  A `direntproc` return of `R_SKIP_ALL` (the `Dtype` enum, src/cvs.h:393-404) makes the engine
  skip the subtree entirely (src/recurse.cpp:1298); otherwise it chdirs and recurses
  (src/recurse.cpp:1350) and finally calls `dirleaveproc` (src/recurse.cpp:1358).
- `-l` (local) is implemented as `frame.flags = R_SKIP_DIRS` (src/recurse.cpp:196,1332-1333).

**Cut-point inventory** (where a dir/file skip is even possible): (a) inside the generic engine
(`do_dir_proc`/`do_file_proc`); (b) inside the enumeration layer (`Find_Names`/`Find_Directories`);
(c) inside each command's `predirentproc`/`direntproc`/`fileproc`; (d) on the client, in the send
walk's procs; (e) on the client, at response-application time. §3.3 selects among these.

### 1.2 The ignore machinery — and why "ignored" is not "excluded"

- The list: `ign_list`, built by `ign_add` (src/ignore.cpp:239-307) from the hard-coded defaults
  (src/ignore.cpp:36-41), `CVSROOT/cvsignore` (server/local: src/ignore.cpp:74-83; client: the
  `read-cvsignore` request, src/ignore.cpp:84-115), `~/.cvsignore` (src/ignore.cpp:124-130),
  `$CVSIGNORE` (src/ignore.cpp:132-133), `-I` (src/update.cpp:210-212), and per-directory
  `.cvsignore` files loaded with hold semantics (src/ignore.cpp:412). A single `!` token resets
  the list (src/ignore.cpp:256-294).
- The match: `ign_name` runs `CVS_FNMATCH(pattern, name, CVS_CASEFOLD)` against the **base name
  only** (src/ignore.cpp:310-323).
- The consumer: `ignore_files` (src/ignore.cpp:377-493) walks `readdir(".")` and **skips every
  file that is in the entries/processed list** (`findnode_fn(ilist, file) != NULL → continue`,
  src/ignore.cpp:423-424) and every checked-out subdirectory (src/ignore.cpp:425-447). Only what
  survives — i.e. *unknown* items — is passed to the per-command proc, which prints `? name`
  (update: `update_ignproc`, src/update.cpp:998-1013, invoked from `update_filesdone_proc`,
  src/update.cpp:1032-1037; client send walk: `send_ignproc`/`Questionable`,
  src/client.cpp:5479-5494 via src/client.cpp:5496-5506).

**Conclusion (confirms the hypothesis in the task):** the ignore machinery only suppresses
reporting of files CVS does *not* manage. It never prevents a *versioned* file or directory from
being enumerated, sent, updated or created. Exclusion must act on the versioned walk itself —
a different mechanism, though it can share list-handling idioms (`!` reset, repeatable option,
`Argument` transmission via `ign_send`, src/ignore.cpp:497-516).

### 1.3 Prior art hiding in the tree: `dir_ign_list` and the modules `!` exclusion

There is a second, separate list in ignore.cpp that does *exactly* directory exclusion — but it is
reachable only through the modules machinery:

- `ign_dir_add` appends to `dir_ign_list` (src/ignore.cpp:335-347); `ignore_directory(name)`
  answers whether `name` starts with any stored prefix, using `fnncmp` (src/ignore.cpp:352-367).
- It is populated in exactly one place: `do_module` treats a module argument `!path` as
  "exclude path" (src/modules.cpp:156-160).
- It is consulted at three walk sites, all with identical behavior — print `Ignoring %s` unless
  quiet and return `R_SKIP_ALL`:
  - `update_predirent_proc` (src/update.cpp:1079-1080),
  - `update_dirent_proc` (src/update.cpp:1189-1195),
  - the client send walk's `send_dirent_proc` (src/client.cpp:5523-5529),
  - plus `tag_dirproc` for tag/rtag (src/tag.cpp:1227,1233-1239).

Because checkout/export pass module args verbatim to the server (`send_file_names`,
src/checkout.cpp:388; server `co` → `do_module`), `cvs co bigmod !bigmod/assets` already works in
client/server mode — the list is built **on the side that runs the walk**. The feature requested
here is, at heart, a command-line front end to this mechanism for `update`, plus transmission,
plus file-level patterns, plus a safer matcher.

Two quirks of the existing matcher worth knowing:

- `ignore_directory` is a **prefix** match with no component-boundary check
  (src/ignore.cpp:362): excluding `lib` also excludes `libfoo`.
- `fnncmp` is `strncasecmp` on Windows/macOS and `strncmp` on POSIX
  (lib/system.h:487, lib/system.h:497-498) — platform-dependent case behavior.

### 1.4 Client vs server: who decides what is walked

Client side, `update` (src/update.cpp:315-446):

- Sends option args (src/update.cpp:330-374), the ignore list (`ign_send`, src/update.cpp:369),
  wrappers, then `--` (src/update.cpp:406), then walks the working copy with
  `send_files` → `start_recursion(send_fileproc, send_filesdoneproc, NULL, send_dirent_proc,
  send_dirleave_proc, …, W_LOCAL, …)` (src/client.cpp:5961-5965), then `send_file_names`
  (src/update.cpp:411, implementation src/client.cpp:5637), then `update\n`
  (src/update.cpp:444).
- The send walk emits one `Directory` request per directory (`send_a_repository`,
  src/client.cpp:5557-5558) and per file an `Entry` plus `Modified`/`Is-modified`/`Unchanged`
  (`send_fileproc`, src/client.cpp:5297, emission at src/client.cpp:5423-5462).

Server side:

- Each `Directory` request physically materializes the directory inside the server's temp
  working area (`serve_directory`, src/server.cpp:1563 → `dirswitch`: `mkdir_p`,
  src/server.cpp:1386, `create_adm_p`, src/server.cpp:1400).
- `Argument` requests accumulate an argv (src/server.cpp:2852-2876; continuation lines via
  `Argumentx`, src/server.cpp:2878-2897).
- `update` runs **the same `update()` function** over that temp tree
  (`serve_update` → `do_cvs_command("update", update)`, src/server.cpp:3786-3789), with
  `which = W_LOCAL | W_REPOS` (src/update.cpp:506).

Consequently the server's walk covers (i) every directory the client sent, and (ii) repository
subdirectories of those directories discovered by `Find_Directories(W_REPOS)`
(src/find_names.cpp:234-251) that the client did *not* send. For case (ii)
`update_dirent_proc` sees `!isdir(dir)` in the temp tree and, **without `-d`, skips it**
(src/update.cpp:1220-1224); **with `-d` it creates and serves the whole subtree**
(src/update.cpp:1081-1121 in the predirent, boilerplate duplicate at 1259-1298).

**Implication, quantified.** A purely client-side filter (never telling the server):

- *Without `-d`*: complete and free. Not sending the subtree's `Directory`/`Entry` lines means the
  server never enumerates it. For an already-checked-out excluded subtree of N files the client
  also stops uploading ≈N `Entry` + `Unchanged` lines (tens of bytes each — for a 200k-file asset
  tree that is roughly 10–20 MB of upstream traffic and 200k server-side stat/classify rounds,
  per update) — the filter *saves* work.
- *With `-d`* (the primary use case — "pick up every new file/dir except these trees"): broken.
  The server rediscovers each excluded directory in the repository, creates it in the temp area
  and streams **the entire subtree content** as `Created` responses (full file bodies). The
  client would have to receive-and-discard: the whole excluded payload (potentially many GB for
  the asset directories that motivate this feature) crosses the wire for nothing. That is why the
  exclusion list must also reach the server, and why a client-side response guard is still needed
  for correctness against old servers (§3.4).

Client response side: every file/directory-touching response funnels through
`call_in_directory` (src/client.cpp:812; 23 call sites in client.cpp), which chdirs to the
response's directory and **creates missing directories** (src/client.cpp:971-999) and feeds
prune candidates (src/client.cpp:896-897). This is the single choke point for a degraded-mode
guard.

Capability negotiation precedent: the request table (`requests[]`, src/server.cpp:4908) contains
`update-patches` → `serve_ignore`, a no-op whose only purpose is to advertise that `update`
accepts `-u` (src/server.cpp:4875-4882, table entry src/server.cpp:4968). The server advertises
via `Valid-requests` (src/server.cpp:5026-5047); the client tests with `supported_request`
(src/client.cpp:4356), e.g. `supported_request("update-patches")` gating `-u`
(src/update.cpp:392-393). Unknown *requests* are ignorable; unknown *options* are not — an old
server's `update()` would hit `default → usage()` (src/update.cpp:300-303) and abort, so the
option must be capability-gated.

### 1.5 `update -d` creation points

Directory creation for repo-new directories happens in `update_predirent_proc`: the
`ignore_directory` check (src/update.cpp:1079-1080) runs **before** the `!isdir(dir)` branch that
creates the directory (`make_directory`, src/update.cpp:1108; `Create_Admin`,
src/update.cpp:1109; `Subdir_Register`, src/update.cpp:1118; `WriteTag`, src/update.cpp:1120).
The same ordering holds in `update_dirent_proc` (check at 1189, creation at 1220-1298, described
in-source as boilerplate that "probably won't ever be called any more", src/update.cpp:1259).
Therefore a filter placed at the existing check sites suppresses `-d` creation with no extra
work. `-d` also clears `Entries.Static` at the top (src/update.cpp:488-503) and per directory
(src/update.cpp:1330-1343) — irrelevant for skipped dirs since the code never runs for them.

### 1.6 The enumeration layer, `Entries.Static`, and the modules2 regex

- `Find_Names` (src/find_names.cpp:55): merges the entries list (src/find_names.cpp:68-83) with a
  repository scan (`find_rcs`, src/find_names.cpp:88-105) — but the repository scan is skipped
  entirely when `CVS/Entries.Static` exists (src/find_names.cpp:85). `Entries.Static` is thus the
  existing *per-directory, file-level* "don't add new files" persistent flag (created server-side
  by the `Static-directory` request, src/server.cpp:1593-1608). There is no directory-level or
  recursive equivalent.
- `Find_Directories` (src/find_names.cpp:168): entries subdir info (src/find_names.cpp:181-232)
  plus repository scan (`find_dirs`, src/find_names.cpp:237).
- Both honor a **server-side regex filter** from the modules2 definition of the current module:
  `lookup_regex` (src/mapping.cpp:691-696) → PCRE match (`regex_filename_match`,
  src/mapping.cpp:698-705; `matches_regexp`, cvsapi/cvs_string.cpp:52-63) applied to files
  (src/find_names.cpp:292) and to directories with a trailing `/` appended
  (src/find_names.cpp:398-405), in non-remote mode only (src/find_names.cpp:61,174). This is
  admin-configured, repo-side filtering — proof the enumeration cut works, but contractually a
  module *property*, not a per-invocation user option.

### 1.7 Option-parsing landscape — the `-exc` collision analysis

- `update` optstring: `"+AB:pCcPflRQqdnuk:r:D:j:bmI:W:3Stxe::i"` (src/update.cpp:183).
  - `-e[bugid]` is taken: "automatically edit modified/merged files", optional attached arg
    (`e::`; src/update.cpp:141,296-299).
  - `-c` is taken: "update base revision copies" (src/update.cpp:138,207-209).
  - `-x` sits in the optstring with **no case label** — it currently falls to
    `default → usage()` (src/update.cpp:300-303), i.e. reserved/dead.
- The getopt implementation (lib/getopt_long.c) gives an optional-arg option any **attached**
  text as its argument (lib/getopt_long.c:608-621). So `cvs update -exc` parses as `-e` with
  bugid `"xc"` — **no error, silently wrong behavior** (auto-edit mode engaged with a bogus bug
  id). A required-arg option consumes the next word (lib/getopt_long.c:622-651), so at the global
  level (`"+QqrwtnlvT:e:d:HfF:z:s:axyNRo::OL:C:cj:"`, src/main.cpp:736) `cvs -exc …` sets the
  *editor* to `"xc"`. Globally `-x` is encrypt (`--encrypt` alias, src/main.cpp:741) and `-a` is
  authenticate (src/main.cpp:742).
- Single-dash long options are not recognized: only `--`-prefixed words take the long-option
  path (lib/getopt_long.c:565-582 falls back to short-option scanning), so `-exclude` parses as
  `-e xclude`. Any multi-letter single-dash spelling is a trap.
- Long options per command already exist and work through `getopt_long` with numeric `val`s:
  `long_update_options` / `--blob_zero` (src/update.cpp:175-179,187-190); the global table
  (src/main.cpp:737-763) and diff (src/diff.cpp:224) do the same.
- Short letters: unused by `update` today are E,F,G,H,J,K,L,M,N,O,T,U,V,X,Y,Z,a,g,h,s,v,w,y,z,
  digits other than 3. `-X` would be the natural mnemonic — but `status` already uses **both**
  `-x` and `-X` as output-format switches (src/status.cpp:32-33,49,65-70), so `-X` cannot ever be
  uniform across the command family.

### 1.8 Persistence machinery already available

- `read_cvsrc` prepends per-command default options from `~/.cvsrc`
  (src/cvsrc.cpp:186-216) and — CVSNT-specific — from the server's `CVSROOT/cvsrc` via the
  `read-cvsrc2` request (src/cvsrc.cpp:72-94; table entries src/server.cpp:5001-5002). Any new
  update option becomes persistable per-user and per-repository for free. This is the CVS-idiomatic
  "sticky config" surface; note that because CVS has no sandbox-root concept (every directory is
  self-similar), there is no existing per-sandbox config file to piggyback on.
- Per-directory sticky state lives in `CVS/Tag` (written by `WriteTag`; update rewrites it at
  src/update.cpp:1388-1390 and `-A` resets it by passing a NULL tag, src/update.cpp:1358-1368) and
  `CVS/Entries.Static` (§1.6). Both are created/cleared only for directories the walk actually
  visits.

### 1.9 Which commands share the recursion

- `checkout`/`export` run `do_update` themselves (src/checkout.cpp:1223,1280, with
  `which |= W_LOCAL | W_REPOS`, src/checkout.cpp:1190), so they inherit any filter placed in
  update's dirent procs; module-level `!` exclusion already covers them today
  (src/modules.cpp:156-160).
- `status` is `W_LOCAL`-only (src/status.cpp:116-119) — it never enumerates the repository, so it
  cannot "pull in" excluded content; its client phase sends the local tree
  (src/status.cpp:105-107).
- `commit` runs three `W_LOCAL` recursions (src/commit.cpp:515-518,705-717,733-735).
- `tag` honors `ignore_directory` already (src/tag.cpp:1233).

### 1.10 Commit-safety trace (failure mode #9)

`commit` walks only the local working copy (`W_LOCAL`, src/commit.cpp:518): a directory that is
absent locally (never checked out, or excluded at checkout time) is simply not enumerated —
`Find_Names`/`Find_Directories` with `W_LOCAL` read `Entries` and the filesystem only
(src/find_names.cpp:68-83,181-232). Deletion of server content requires an explicit removed entry
(a `-`-prefixed version in `Entries`, cf. `isremoved`, src/update.cpp:1451-1459) produced by
`cvs remove`; nothing in the commit path converts "absent local directory" into repository
removal. The client send walk shares `send_dirent_proc` (src/client.cpp:5516) across commands,
so an exclusion list populated **only during `update`/`checkout` option parsing** is empty during
commit and cannot suppress a commit send. A directory that is still present locally but was
excluded from updates commits normally — no silent data loss.

---

## 2. Alignment (A) — patterns adopted, decisions and their alternatives

Patterns adopted from the codebase (each with prevalence):

- **Skip-at-direntproc with `R_SKIP_ALL` + "Ignoring" message** — the established exclusion
  behavior at 4/4 existing sites (src/update.cpp:1079,1189; src/client.cpp:5523;
  src/tag.cpp:1233). Followed.
- **Repeatable list option with `!` reset** — `-I`/`ign_add` (src/ignore.cpp:256-294) and `-W`.
  Followed for `--exclude`.
- **Marker request advertising an option** — `update-patches`/`serve_ignore`
  (src/server.cpp:4875-4882,4968; src/update.cpp:392-393). Followed (`update-exclude`).
- **Long options via `getopt_long` numeric vals** — `--blob_zero` (src/update.cpp:175-190),
  global table (src/main.cpp:737-763). Followed.
- **`CVS_FNMATCH` + `CVS_CASEFOLD` for name matching** — dominant matcher (src/ignore.cpp:319;
  src/find_names.cpp:280,369; cvsapi/cvs_string.h:113-115). Followed, with `FNM_PATHNAME` and
  `FNM_LEADING_DIR` (both implemented in lib/fnmatch.c:74-123,68,96) for path patterns.
- **Not followed:** raw prefix matching via `fnncmp` as in `ignore_directory`
  (src/ignore.cpp:362) — no component-boundary check (`lib` excludes `libfoo`); replaced by a
  boundary-safe matcher in the new code while leaving the modules-`!` path untouched.

Decision records (autonomous; each lists the alternatives that a reviewer may reopen):

- **D-1 Spelling: `--exclude=PATTERN` (long option only, repeatable), reset `--exclude=!`.**
  Alternatives: (a) `-exc` — rejected: silently misparses as `-e xc` both at update level
  (src/update.cpp:183 `e::` + lib/getopt_long.c:608-621) and global level (src/main.cpp:736
  `e:`); (b) short `-X` — viable for update alone but permanently colliding with
  `status -X` (src/status.cpp:49) so the family could never be uniform; kept as an optional
  later alias, not the primary; (c) `-x`/`-E`/other letters — either taken, reserved, or
  non-mnemonic. Runner-up if a short option is mandated: `-X` on update only.
- **D-2 One pattern per option occurrence.** Alternatives: space-splitting one argument (as
  `ign_send` does for `-I`, src/ignore.cpp:501-515) — rejected, asset paths contain spaces;
  separator characters (`;`/`:`) — rejected, collide with Windows paths and shell habits;
  `--exclude-from=FILE` — deferred, `.cvsrc`/`CVSROOT/cvsrc` (§1.8) already provide reusable
  lists.
- **D-3 New list + new matcher (`excl_*` in ignore.cpp) instead of reusing `dir_ign_list`.**
  Alternative: feed `ign_dir_add` directly — rejected because (i) the modules-`!` prefix
  semantics (no boundary check, no globs, src/ignore.cpp:362) would silently change, and
  (ii) `--exclude=!` reset must not clear modules-supplied exclusions. The new checks are added
  beside the existing `ignore_directory` calls at the same sites.
- **D-4 Transport: plain `Argument --exclude` / `Argument <pattern>` pairs + `update-exclude`
  marker request.** Alternatives: a new protocol request carrying the list — rejected, more
  surface, no gain (the server's `update()` parses argv anyway, src/server.cpp:3786-3789,
  2852-2876); overloading `-I` — rejected, different semantics (§1.2).
- **D-5 Old-server degradation: withhold the option, filter locally, stay correct.** The client
  applies (i) the send-walk filter and (ii) a response-time guard in `call_in_directory`
  (src/client.cpp:812), and prints a one-time warning when `-d` is in effect that excluded
  subtrees may be transferred and discarded. Alternatives: hard-error `--exclude` + `-d` against
  old servers — rejected (blocks mixed fleets); silently proceeding without any filter —
  rejected (violates the option's contract and materializes excluded trees).
- **D-6 Exclusion wins over explicit arguments, enforced by an early error.** `cvs update
  --exclude=P A…` where an argument lies inside an excluded subtree is a contradiction; update
  errors out before any traffic (cheap pre-scan of argv against the pattern list in `update()`),
  matching CVS's existing conflicting-option style (e.g. src/update.cpp:263-264). Alternatives:
  explicit-arg-wins — rejected: mechanically it holds for bare file args (the unrolled-files path
  never runs a direntproc, src/recurse.cpp:1447-1508) but not for directory args, giving an
  inconsistent rule that would also require threading "origin" data through the generic engine
  (src/recurse.cpp:320-443) to implement honestly; silent skip with warning — workable but hides
  user error.
- **D-7 No new sticky state in `CVS/`; persistence via `~/.cvsrc` and `CVSROOT/cvsrc`.**
  Reasoning: CVS has no sandbox root; per-directory persistence would need a new admin file
  written into every parent of an excluded path, a clearing rule, `-A` semantics, and a
  protocol story for mixed old/new clients — while the natural state of an excluded directory
  ("absent locally") is *already* self-persisting: a plain `update` never recreates absent dirs
  (src/update.cpp:1220-1224); only `-d` does, and `-d` users keep `--exclude` in `.cvsrc`
  alongside it (where `update -d` traditionally lives). Alternatives recorded for a future
  iteration: (a) `CVS/Exclude` file in each parent directory, read at walk time, cleared by
  `update -A` (which must then also be transmitted; substantial); (b) reusing `Entries.Static`
  semantics recursively — rejected, changes the meaning of an existing flag. If (a) is ever
  built, `update -A` must clear it and `update --exclude=!` must override it for one run.
- **D-8 Matching semantics: gitignore/rsync-style dual rule** (see §3.2): slash-containing
  patterns are anchored paths with subtree containment (`FNM_LEADING_DIR`), slash-free patterns
  match base names at any depth. Alternatives: literal prefixes only (modules-`!` semantics) —
  rejected, users expect `*.dds`-class patterns and the fnmatch machinery is already in-tree
  (lib/fnmatch.c); full PCRE (modules2-style, cvsapi/cvs_string.cpp:52-63) — rejected for a
  user-facing option (footguns, escaping across two shells and the protocol).
- **D-9 Case sensitivity follows the platform of the side doing the match** (`CVS_CASEFOLD`:
  Win32 folds unless `FsCaseSensitive`, POSIX does not — cvsapi/lib/api_system.h:31,121;
  lib/system.h:454-467). This matches every existing matcher (src/ignore.cpp:319 etc.). The
  asymmetry (Windows client folds, Linux server doesn't) is documented; users should write
  patterns in repository case. Alternative: force-fold everywhere — rejected, breaks
  case-sensitive repositories; open question §6-Q2.
- **D-10 Scope: `update` first; `checkout`/`export` in a follow-up phase; `status` and all
  write commands never.** `status` is `W_LOCAL` and harmless (§1.9) and its `-X` letter is taken;
  exclusion during `commit`/`add`/`remove` would manufacture silent data-integrity surprises
  (§1.10 shows commit is safe today precisely because it ignores this feature).

---

## 3. The design

### 3.1 Recommended option spelling

**Primary: `--exclude=PATTERN`** (equivalently `--exclude PATTERN`), repeatable; each occurrence
appends one pattern. **`--exclude=!`** (a lone `!`) clears the list accumulated so far — exactly
the `-I !` idiom — which lets a user override patterns injected by `~/.cvsrc`/`CVSROOT/cvsrc`.
Implementation slot: `long_update_options` (src/update.cpp:175-179) with the next free numeric
`val` (2), alongside `--blob_zero`.

**Runner-up:** `-X PATTERN` as an update-only short alias (free in update's optstring,
src/update.cpp:183). Deliberately *not* recommended for v1 because `status` owns `-X` for output
format (src/status.cpp:49,68-70), so the letter can never be uniform if the option family grows.

**Rejected: `-exc`.** getopt has no multi-letter short options: update parses `-exc` as `-e`
with attached optional argument `"xc"` (src/update.cpp:183 `e::`, lib/getopt_long.c:608-621) —
i.e. it silently enables automatic `cvs edit` of merged files under bug id "xc"
(src/update.cpp:296-299). At the global level `cvs -exc` sets the editor to `xc`
(src/main.cpp:736 `e:`). Also `-exclude` (one dash) parses as `-e xclude`
(lib/getopt_long.c:565-582): only `--` engages long-option parsing. Documentation must always
show the double dash.

### 3.2 Semantics, as if for the manual

> `--exclude=PATTERN` — Limit `update` by exclusion. May be given any number of times; each adds
> one pattern to the exclusion list for this invocation. A lone `!` clears the list built so far
> (useful to override patterns supplied by `.cvsrc`).
>
> Every directory and file that the update would otherwise visit is tested against the list; if
> any pattern matches, the path is skipped completely: it is not examined, not reported, not
> created, not modified, not deleted, and its sticky information is left untouched. For a skipped
> directory the entire subtree is skipped. `update` prints `Excluding dir` once per skipped
> directory unless `-q`/`-Q` is given.
>
> Matching is against the path **relative to the directory where `cvs update` was invoked**, with
> `/` separators — the same string `update` prints in its `cvs update: Updating <path>` messages.
> Two kinds of pattern exist:
>
> - A pattern containing `/` is **anchored**: it must match a leading portion of the relative
>   path, whole components at a time. `--exclude=develop/assets` skips `develop/assets` and
>   everything below it; it does not skip `other/develop/assets`, nor `develop/assets2`.
>   Shell wildcards (`*`, `?`, `[…]`) are allowed and never match across `/`
>   (e.g. `--exclude=develop/*/textures`).
> - A pattern without `/` is a **name pattern**: it is tested against the base name of every
>   directory and file at every level. `--exclude=assets` skips any directory (or file) named
>   `assets` anywhere; `--exclude=*.dds` skips all `.dds` files everywhere.
>
> Patterns are matched case-insensitively where the local system compares file names
> case-insensitively (e.g. Windows), case-sensitively otherwise; when client and server differ,
> write patterns in the repository's case. A trailing `/` on a pattern is ignored; `\` is
> accepted as `/` on Windows. Naming a command-line argument that lies inside an excluded path is
> an error.
>
> Exclusion is not sticky: it applies only to the invocation that specifies it. To exclude
> the same paths on every update, put a line such as
> `update -d -P --exclude=develop/assets --exclude=*.dds` in `~/.cvsrc` (or the repository-wide
> `CVSROOT/cvsrc`). Directories that stay absent locally are never re-created by a plain
> `update` anyway; only `update -d` (or a fresh `checkout`) would bring them back.
>
> Interaction summary: `-d` does not create excluded directories; `-P` never prunes into skipped
> subtrees; `-A`, `-r`, `-D`, `-j` simply do not act on skipped paths (their sticky state is left
> exactly as it was); `-l`/`-R` compose (both restrict the walk); `-I`/.cvsignore are unrelated
> (they only affect the `?` reporting of unversioned files). Other commands — in particular
> `commit` and `status` — ignore the exclusion list entirely.

Mechanically, the match target for a directory is the child `update_dir` handed to the dirent
procs (src/recurse.cpp:1028-1043) and for a file `finfo->fullname` (`update_dir + '/' + file`,
src/recurse.cpp:901-911). The matcher (new `excl_path_match()` in ignore.cpp beside the existing
list code) is, for slash-containing patterns,
`CVS_FNMATCH(pattern, path, FNM_PATHNAME|FNM_LEADING_DIR|CVS_CASEFOLD) == 0`
(`FNM_LEADING_DIR` gives subtree containment, lib/fnmatch.c:68,96; `FNM_PATHNAME` keeps `*`
within one component, lib/fnmatch.c:74-123; `CVS_CASEFOLD` per cvsapi/lib/api_system.h:31,121)
and, for slash-free patterns, `CVS_FNMATCH(pattern, lastcomponent(path), CVS_CASEFOLD) == 0`
(mirroring `ign_name`, src/ignore.cpp:319). Normalization at parse time: map `\`→`/`, strip
trailing `/`, reject absolute paths and `..` (same checks args get: src/recurse.cpp:220-229).

### 3.3 Where the filter is applied — cut points and why

| # | Side | Function | Anchor | What it achieves |
|---|------|----------|--------|------------------|
| 1 | client, send walk | `send_dirent_proc` | src/client.cpp:5516, beside the `ignore_directory` check at :5523-5529 | Directory subtree neither walked nor sent (`Directory`/`Entry`/`Unchanged` suppressed). Kills upstream traffic and, absent `-d`, all server work for the subtree (§1.4). |
| 2 | server & local | `update_predirent_proc` | src/update.cpp:1059, beside :1079-1080 | Cuts before any admin writes and before `-d` creation (`make_directory` at :1108). |
| 3 | server & local | `update_dirent_proc` | src/update.cpp:1184, beside :1189-1195 | Belt-and-braces with #2 (this proc contains the second `-d` creation path, :1220-1298) and produces the `Excluding` message. |
| 4 | server & local | `update_fileproc` entry | src/update.cpp:711 (first statement, before `Classify_File` at :719) | File-level patterns: a matched versioned file is neither classified nor checked out/merged/patched; server sends nothing for it. |
| 5 | client, responses | `call_in_directory` | src/client.cpp:812, after `dir_name` is computed at :887-895 and before the prune hook at :896 and dir creation at :971-999 | Old-server safety net (D-5): drop any response addressed into an excluded path, so an old server's `-d` push cannot materialize excluded content. Covers all 23 response call sites in one place. |

Rejected cut points, with reasons:

- **Generic engine** (`do_dir_proc`/`do_file_proc`, src/recurse.cpp:975/884): one check would
  cover every command — including `commit`, `remove`, `tag` — turning an update filter into a
  data-integrity hazard (a user's stale `.cvsrc` exclusion silently shrinking a commit). The
  per-command procs are the contract-appropriate layer, exactly as `ignore_directory` is wired
  today.
- **Enumeration layer** (`Find_Names`/`Find_Directories`, src/find_names.cpp:292,398-405): the
  modules2 regex proves it works, but it (i) runs only in non-remote mode
  (src/find_names.cpp:61,174) so the client send walk still needs its own filter, (ii) affects
  every enumerating command, and (iii) cannot print per-directory messages or interact with the
  `-d` creation logic (which lives in the dirent procs, §1.5).
- **Per-response-handler filtering** instead of #5: `Created`, `Updated`, `Merged`, `Patched`,
  `Removed`, `Remove-entry`, `Set-sticky`, `Clear-static-directory`, … would each need the same
  guard; `call_in_directory` is their common funnel (src/client.cpp:812) — one guard, complete
  coverage.
- **`send_fileproc`** (src/client.cpp:5297) for file patterns: deliberately *not* filtered. If
  the client withheld `Entry` lines for excluded files while the server still walks the
  directory, a new server without the file pattern (or an old server) would classify the file as
  missing and stream it back (`T_CHECKOUT`); keeping the send phase honest and cutting at the
  server fileproc (#4) plus the response guard (#5) is both cheaper and correct in every pairing.

### 3.4 Client/server protocol design

- **Transport: no new data request.** The client sends, before the `--` separator
  (src/update.cpp:406), one `Argument --exclude` + `Argument <pattern>` pair per pattern
  (via `option_with_arg`, src/client.cpp:6222), in command-line order so `!` reset order is
  preserved. The server accumulates argv (src/server.cpp:2852-2876) and its `update()` parses
  `--exclude` with the same `getopt_long` table (src/server.cpp:3786-3789; src/update.cpp:183).
  Patterns travel verbatim; the `\`→`/` normalization happens at client parse time.
- **Capability marker: one new table entry** `REQ_LINE("update-exclude", serve_ignore, 0)` next
  to `update-patches` (src/server.cpp:4968), automatically advertised by `Valid-requests`
  (src/server.cpp:5026-5047). Being a client/server-shared table (src/server.cpp:4888-4892), the
  same entry lets the client call `supported_request("update-exclude")`
  (src/client.cpp:4356).
- **New client → old server:** `supported_request` is false → the client sends **no** `--exclude`
  arguments (an old server would abort in `usage()`, src/update.cpp:300-303). It still applies
  cut #1 (send walk) and cut #5 (response guard), so results are *correct*: excluded content is
  neither uploaded nor materialized. If `-d` is also in effect it prints one warning that the
  server may transfer excluded subtrees that will be discarded locally (the bandwidth cost of
  §1.4). Behavior table:

  | Scenario | Old server result |
  |---|---|
  | excluded dir present locally | not sent, not touched — identical to new server |
  | excluded dir absent, no `-d` | server never learns of it — identical to new server |
  | excluded dir absent/new, with `-d` | server streams subtree; client guard discards; warning printed; wasted bandwidth only |
  | excluded file pattern | server streams matched files it would update; client guard discards |

- **Old client → new server:** the old client never sends `--exclude`; nothing changes. The
  marker request is inert (`serve_ignore`, src/server.cpp:4875-4882).
- **Proxy note:** G-CVSNT proxies replay the same request stream; `Argument` passthrough needs no
  proxy changes (unlike a new structured request — a further point for D-4).

### 3.5 Interaction matrix

| Combined with | Behavior (anchor) |
|---|---|
| `-d` | Excluded dirs are not created: checks precede creation in both procs (src/update.cpp:1079→1108, 1189→1220-1298). Non-excluded new dirs/files everywhere else are still picked up — the core use case. |
| `-P` | Skipped dirs are never visited, so `update_dirleave_proc`'s prune (src/update.cpp:1437-1445) can't run there; client-side prune candidates arise only from server responses (src/client.cpp:896-897), which excluded paths don't produce. An excluded-but-present dir is also non-empty, failing `isemptydir` (src/update.cpp:1464). Net: `-P` never removes excluded content. |
| `-A` | Acts only on visited dirs (`WriteTag` NULL-reset path, src/update.cpp:1358-1368,1388-1390): excluded dirs keep their old sticky tag/date. There is no sticky exclusion state for `-A` to clear (D-7). |
| `-r`/`-D` | Sticky tag/date applied only to visited dirs; an excluded subtree stays on its previous tag — the same mixed-tag state a partial `cvs update -r T subdir` produces today. Documented. |
| `-j` | Merges simply don't happen inside skipped paths; the per-dir merge-permission checks (src/update.cpp:1197-1217,1370-1386) never run there. Users merging a whole branch should not exclude (warned in docs). |
| `-l`/`-R` | Compose: `-l` is `R_SKIP_DIRS` (src/recurse.cpp:196), exclusion is `R_SKIP_ALL` per matched dir. Both subtractive; no conflict. |
| explicit file/dir args | An argument inside an excluded path is a hard error before any traffic (D-6). Args elsewhere behave as today; exclusion still filters *enumeration under* an argument dir (e.g. `--exclude='*.dds' bigdir`). |
| `-I`/`.cvsignore` | Orthogonal: ignore affects unknown-file reporting only (§1.2). Excluded paths additionally suppress their `?` lines because their dirs are never walked; P3 adds the same test to `update_ignproc`/`send_ignproc` for stray unknown files whose *names* match file patterns. |
| `-p` (pipeout) | Filter applies before `checkout_file`-to-stdout; matched paths produce no output. |
| `-C`, `-n`, `-t`, `-c`, `-e` | No interaction; all act per visited file. |
| `commit`, `status`, `diff`, … | Unaffected by design (D-10): the list is populated only inside `update()` (and later `checkout()`), and all cut points are update/send-specific. Shared `send_dirent_proc` sees an empty list for other commands (§1.10). |

### 3.6 Edge cases

- **Excluding the top level** (`--exclude=.` or a pattern matching the invocation dir): the root
  is entered as dir `"."` (src/recurse.cpp:301,1088-1103); a `.` pattern would skip everything and
  update nothing. Parse-time rule: reject `.` and patterns normalizing to empty with an error
  ("--exclude pattern would exclude everything").
- **Pattern matches nothing:** silent no-op, matching `-I` behavior; the server cannot
  distinguish "nothing yet" from a typo, and a pattern may legitimately match only future
  content. Documented.
- **Pattern matches a file present in `CVS/Entries`:** the file's fileproc is skipped (cut #4);
  its entry, timestamp, sticky options and local content stay untouched; it grows stale by
  design. `status` (not filtered) will show it needs update.
- **Excluded dir contains locally modified files:** subtree never visited ⇒ no `M`/`C` lines from
  update, files untouched; `commit` still sees and commits them (§1.10). No data loss.
- **Excluding on one run, not the next:** nothing persistent was written. Next plain `update`:
  still-absent dirs stay absent (src/update.cpp:1220-1224); present dirs are processed normally
  again (catch-up transfer happens then). Next `update -d`: previously excluded absent subtrees
  are created and fetched. Entries files are consistent throughout: skipped new dirs were never
  `Subdir_Register`ed (registration sits inside the skipped branch, src/update.cpp:1118), and
  skipped existing dirs keep their `D/...` lines because the server never sent removals for them.
- **`Renamed` responses** (`handle_renamed`, table src/client.cpp:4051): a server-side rename
  into/out of an excluded path is applied by the rename handler, not `call_in_directory`; P2
  adds the same guard there. Until then this is only reachable with the rename feature and an
  old server (new servers won't emit renames for subtrees they skip).
- **Case-mismatch across platforms:** Windows client folds, Linux server doesn't (D-9): a
  wrong-case pattern still filters client-side (send walk + response guard) but not server-side —
  with `-d` this degenerates to the old-server bandwidth case, never to wrong content.
- **Patterns from `.cvsrc` when invoked from a subdirectory:** anchored patterns are relative to
  the invocation dir, so a root-anchored pattern in `.cvsrc` matches nothing when run from
  `develop/` (harmless no-op); name patterns (`*.dds`) keep working. Documented; a per-sandbox
  anchor is part of the deferred stickiness design (D-7).

---

## 4. Frame (F) — phased implementation plan

Each phase is a vertical slice crossing option-parsing → matcher → walk → protocol → observable
behavior, independently testable, cheapest first. LoC estimates are hand-written non-test source
lines (tests listed separately).

### Phase 1 — Tracer bullet: directory exclusion end-to-end (~170 LoC)

**Components**

- `src/ignore.cpp` (+`src/cvs.h` decls): new `excl_add(pattern)`, `excl_reset()`,
  `excl_path_match(path)` (dual rule of §3.2), `excl_active()`, `excl_send()` (emits
  `--exclude` argument pairs); pattern normalization/validation (~80).
- `src/update.cpp`: `long_update_options` entry `{"exclude",1,NULL,2}` + case (~10); usage text
  (~3); client block sends patterns via `excl_send()` when
  `supported_request("update-exclude")`, else remembers degraded mode (~15); argv-vs-exclusion
  conflict error (D-6) (~15); checks in `update_predirent_proc` (beside :1079) and
  `update_dirent_proc` (beside :1189) with `Excluding %s` message (~10).
- `src/client.cpp`: check in `send_dirent_proc` beside :5523 (~5).
- `src/server.cpp`: `REQ_LINE("update-exclude", serve_ignore, 0)` (~1).

**Testing strategy** — sanity-style scripted scenarios (§5 T1–T4) against a local `:local:` root
(exercises the server-side procs directly) and a loopback `:pserver:`/`:sspi:` client-server
pair; verify with `-t` traces that no `Directory` request is emitted for excluded subtrees.

**Verification gate** — T1–T4 green: excluded subtree neither sent, updated, nor created under
`-d`, both local and client/server; `Excluding` message printed; conflict error fires.

**Acceptance criteria**

- [ ] `cvs update --exclude=big` on a tree with `big/` checked out: no lines for `big/`, contents
  byte-identical afterwards.
- [ ] `cvs update -d --exclude=big` with `big/` repo-new: not created; sibling new dir created.
- [ ] Old-style spelling `-exc` still errors/behaves per current getopt (no regression), and
  `--exclude` round-trips through `Argument` to a same-build server.
- [ ] `cvs update --exclude=big big/file.c` exits with the conflict error before contacting the
  server.

### Phase 2 — Old-server correctness: response guard + warning (~45 LoC)

**Components** — `src/client.cpp`: guard in `call_in_directory` after :887-895 (drop responses
whose `dir_name`/`short_pathname` matches; count drops) (~25) and in `handle_renamed` (~6);
`src/update.cpp`: one-time warning in the client block when degraded and `update_build_dirs`
(~10).

**Testing strategy** — simulate an old server by suppressing the marker (build flag or a test
hook that forces `supported_request` false): run T5; assert excluded dirs absent afterwards and
the warning printed once; assert drop counter matches transferred-but-discarded entities.

**Verification gate** — with the option withheld and `-d` on, the working copy after update is
identical to the new-server run of T3 (bandwidth aside).

**Acceptance criteria**

- [ ] Degraded `update -d --exclude=big`: `big/` not present afterwards; warning printed once.
- [ ] Degraded run without `-d`: no warning, no behavioral difference from Phase 1.

### Phase 3 — File-level patterns + message hygiene (~40 LoC)

**Components** — `src/update.cpp`: `excl_path_match(finfo->fullname)` at the top of
`update_fileproc` (:711) (~6); extend the D-6 conflict check to file args (~6); same test in
`update_ignproc` (:998) and `src/client.cpp` `send_ignproc` (:5479) so `?` lines honor name
patterns (~8); doc text. Server side needs nothing new (same function). Client responses already
guarded by Phase 2.

**Testing strategy** — T6/T7: name pattern `*.dds` and anchored file pattern `dir/file.c`;
verify skipped files keep timestamp/content/entry; verify old-server degraded run discards
streamed matches.

**Verification gate** — matched versioned files untouched across update while unmatched siblings
update; `status` still reports them out of date.

**Acceptance criteria**

- [ ] `cvs update --exclude='*.dds'` leaves all `.dds` at old revisions, updates the rest.
- [ ] `Entry`/`Unchanged` lines for matched files are still sent (protocol trace), and no
  response for them is applied.

### Phase 4 — `checkout`/`export` parity + documentation (~70 LoC)

**Components** — `src/checkout.cpp`: accept `--exclude` (getopt_long conversion of the existing
`getopt` loop at :156 with a small long-options table, or pre-scan) (~30); forward patterns in
the client block before `co`/`export` is sent (:408) gated on the same `update-exclude` marker
(~10); populate the same `excl_*` list server-side (parsed by `checkout()` since `serve_co` runs
it) (already shared); usage strings (~6); manual section (§3.2 text) in the docs tree.

**Testing strategy** — T8: fresh `checkout -d wc mod --exclude=mod/big`; compare with the
modules-`!` result (`cvs co mod !mod/big`) — identical trees expected.

**Verification gate** — a fresh partial checkout plus subsequent `update -d --exclude=…` keeps a
stable partial sandbox with no spurious diffs.

**Acceptance criteria**

- [ ] `checkout --exclude` produces the same tree as the modules `!` spelling.
- [ ] `export --exclude` works (shares the checkout path, src/checkout.cpp:408).

### Phase sequence

```
Phase 1 (tracer bullet)
   ├─→ Phase 2 (old-server guard)      [parallel with Phase 3]
   ├─→ Phase 3 (file patterns)         [parallel with Phase 2]
   └─→ Phase 4 (checkout/export)       [independent of 2 and 3]
```

### Scope boundaries

**In scope:** `update` (all modes), later `checkout`/`export`; directory and file patterns;
protocol capability negotiation; degraded-mode correctness.
**Out of scope:** sticky exclusion state in `CVS/` (D-7, deferred design recorded);
`--exclude-from` files (D-2); `status`/`commit`/write commands (D-10); server-enforced
(admin-mandated) exclusions — modules2 regex already serves that need (§1.6).

---

## 5. Test plan

Common setup: repository with `top/a/`, `top/big/` (many files), `top/big/sub/`, `top/c.txt`,
`top/big/huge.dds`; working copy `wc` = full checkout of `top` unless stated. "Server" tests run
once against `:local:` and once against a loopback client/server pair.

| # | Scenario | Setup | Command | Expected |
|---|---|---|---|---|
| T1 | Present dir excluded | full `wc`; commit changes to `a/` and `big/` elsewhere | `cvs update --exclude=big` | `Updating a` seen; `Excluding big` seen; `big/` bytes unchanged; exit 0 |
| T2 | No upstream traffic for excluded subtree | as T1, client/server, `cvs -t` | same | trace shows no `Directory top/big` request; server log shows no classify of `big/*` |
| T3 | `-d` with repo-new dirs | delete `wc/big`; add repo-new `top/newdir/` and `top/big/newsub/` | `cvs update -d --exclude=big` | `newdir/` created; `big/` **not** created; `U` lines only outside `big` |
| T4 | Conflict error | full `wc` | `cvs update --exclude=big big/f.txt` | error mentioning both the arg and pattern; server not contacted; exit ≠ 0 |
| T5 | Old-server degradation | force `supported_request("update-exclude")==0`; as T3 | `cvs update -d --exclude=big` | warning printed once; `big/` absent afterwards; exit 0 |
| T6 | Name pattern on files | commit new revs of `big/huge.dds` and `c.txt`; full `wc` | `cvs update --exclude='*.dds'` | `U c.txt`; `huge.dds` untouched (old rev in `Entries`, old mtime) |
| T7 | Reset from cvsrc | `~/.cvsrc`: `update --exclude=big` | `cvs update --exclude=! ` | `big/` updates normally (reset overrides cvsrc) |
| T8 | Prune interaction | `wc` with `big/` present; `-P` habitual | `cvs update -P --exclude=big` | `big/` not pruned, not visited; empty *non-excluded* dirs pruned as usual |
| T9 | Sticky preservation | `cvs update -r BR big` earlier (mixed sandbox) | `cvs update -A --exclude=big` | `big/CVS/Tag` still `TBR`; rest of tree reset to trunk |
| T10 | Local mods inside excluded dir | edit `big/f.txt` | `cvs update --exclude=big` then `cvs commit -m x big` | update prints nothing for `big`; commit succeeds and commits the edit |
| T11 | Case behavior | Windows client, Linux server; pattern `BIG` | `cvs update --exclude=BIG` | client-side skip works (folded); doc-warning scenario: with `-d`, server-side skip requires repo case — verify no wrong content either way |
| T12 | Excluding everything | any | `cvs update --exclude=.` | immediate error "would exclude everything"; nothing sent |
| T13 | Pattern matches nothing | any | `cvs update --exclude=nosuch` | behaves as plain update; exit 0; no warning |
| T14 | Boundary safety | repo has `lib/` and `libfoo/` | `cvs update -d --exclude=lib` | `libfoo/` updated/created; `lib/` skipped (regression against the `fnncmp` prefix quirk, src/ignore.cpp:362) |

---

## 6. Open questions (each with the recommended answer)

1. **Should a short alias exist, and which letter?** Recommended: none in v1; add `-X` to
   `update` only if field feedback demands it (it can never be family-uniform — `status -X`,
   src/status.cpp:49). Decision needed before the option appears in release notes.
2. **Case folding policy across mixed platforms.** Recommended: platform semantics of the side
   doing the match (`CVS_CASEFOLD`, D-9), documented "write patterns in repository case".
   Alternative on the table: always-fold, only if Gaijin repositories are guaranteed
   case-unique — needs an owner's call.
3. **Explicit argument vs exclusion: error (recommended, D-6) or warn-and-skip?** Error is
   safer and simpler; warn-and-skip is friendlier to scripted callers that assemble both lists.
4. **Should `checkout`/`export` land in the same release as update (Phase 4) or later?**
   Recommended: same release — a partial sandbox is most naturally *born* partial, and the
   modules-`!` parity makes it cheap; but Phase 4 is severable if schedule demands.
5. **Sticky exclusion in `CVS/` (deferred design D-7).** Recommended: do not build; revisit only
   if `.cvsrc` proves insufficient in practice (signal: users repeatedly bitten by forgetting
   `--exclude` with `-d` from subdirectories). If built: per-parent `CVS/Exclude` file, cleared
   by `update -A`, overridden per-run by `--exclude=!`, and the client must send it exactly as
   command-line patterns.
6. **`--exclude-from=FILE`** for very long lists. Recommended: defer (cvsrc lines cover the
   known workflows; response-file support already exists at the argv level for the whole
   command line, src/main.cpp:700-717).
