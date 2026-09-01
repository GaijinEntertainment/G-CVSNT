# Known issues

87 defects were found by a review pass over the tree. 15 are fixed on this branch; **70 remain
open** and are listed here. Two more were fixed and then reverted, for reasons recorded below —
those are the most interesting entries in this document, because in both cases the obvious one-line
fix is wrong.

Every entry links to a self-contained report under [`_reports/`](_reports/) holding the offending
code, why it is wrong, a concrete failure scenario, a suggested fix, and a record of what was
checked that could have made it a false positive.

## How this list was triaged

Only fixes meeting all of these went in:

* the defect is unambiguous — a wrong token, not a design judgement;
* the fix is small, and does not change behaviour that anything could reasonably depend on;
* it compiles, and the regression and unit suites still pass.

Everything else is here. That is not a quality judgement on the finding — several of the most
serious defects in the tree are in this list precisely *because* fixing them properly is more than a
one-line change.

## The two that were fixed and reverted

These deserve reading before anyone reaches for the "obvious" fix again.

### `BUG-update-05` — `special_file_mismatch()` can never report a mismatch

`src/update.cpp:3370` sets `result = 0` in the branch that has just detected an execute-bit
difference, so the function always returns "no mismatch". Changing it to `1` is a one-token fix and
it is **wrong**.

The comparison it would make live is `(rev1_mode & 0111) != (rev2_mode & 0111)` — exact equality of
all three execute bits. But the working file's mode is not the recorded mode: the umask is
subtracted on every checkout (`src/client.cpp:390` with `respect_umask = 1`,
`src/filesubr.cpp:704` locally), while the repository records the *unreduced* mode
(`src/server.cpp:2063` applies the client's mode string with `respect_umask = 0`).

So on a client with umask `027` — the CIS/RHEL hardening default — a file committed `0755` is
checked out `0750`, and `0750 & 0111 != 0755 & 0111`. Every merge of every executable file becomes a
conflict: the working file is overwritten with the clean repository revision and the user's edits go
to `.#file.rev`. The same applies to any Windows client against a POSIX repository, since the
execute bit is unrepresentable there. `cvs update -j` has the same exposure through `join_file`
(`src/update.cpp:3153`).

The blast radius is the whole repository, not the unusual-mode subset: `src/rcs_checkin.cpp:613`
writes a `permissions` delta on **every** commit with no condition, so `check_modes` is 1 for
essentially every file.

There is no escape hatch. `PreservePermissions` is parsed and discarded —
`src/parseinfo.cpp:291` is an empty statement, and no `preserve_perms` variable exists anywhere.
Upstream GNU CVS compiles this entire block out under `PRESERVE_PERMISSIONS_SUPPORT`; CVSNT kept the
block but pinned `result`, which is a plausible reading of the original as deliberate.

**Two ways forward, both larger than a token:**

1. Compare *executability* rather than the exact bit triple —
   `!!(rev1_mode & 0111) != !!(rev2_mode & 0111)` — which is umask-stable, since a umask that
   clears one execute bit clears the others too. Still fires on mixed Windows/POSIX fleets.
2. Wire `PreservePermissions` up in `parseinfo.cpp` and gate the block on it, so sites that cannot
   guarantee mode fidelity across clients keep the current warn-only behaviour.

### `BUG-blob-14` — `available_disk_space()` returns the sentinel on success

`keyValueServer/proxy/free_disk_space.cpp:17` tests its `std::error_code` inverted, returning
`UINT64_MAX` when the query *succeeded*. Flipping the test is one token, and it makes the proxy
worse.

`available_disk_space` returns `uint64_t`, but every caller stores it in **`int64_t`**
(`gc_proc_monitor.cpp:26, 30, 40, 47, 69`), so `UINT64_MAX` arrives as `-1`. The condition at
`gc_proc_monitor.cpp:35` is

```c
avail == 0 || (lastAvail + lastOccupied > int64_t(file_cache_size) + avail)
```

and with `lastAvail == avail == -1` the two cancel, leaving a working soft-limit check on
`lastOccupied`. `free_space()` is then called unconditionally on every outer iteration
(`gc_proc_monitor.cpp:46`) and trims to `file_cache_size` by mtime. **The cache does not grow
without bound.** Only the `avail == 0` disk-full trip-wire is dead.

Switch the real value on and line 35 becomes `(lastAvail - avail) + lastOccupied > file_cache_size`,
which attributes *all* consumption of the shared filesystem to the cache. On a proxy sharing a
partition with anything else that fires spuriously; each spurious fire is a full
`recursive_directory_iterator` walk with `is_regular_file` + `file_size` + `last_write_time` per
file, returns 0 without refreshing `lastAvail`, and repeats every 60 seconds for up to ten
iterations. No over-eviction — `free_space` never trims below `file_cache_size` — but a recurring
full-tree stat storm that did not exist before.

**The right change:** give `available_disk_space` an `int64_t` return so the sentinel is honest, or
have `init_gc` test for the sentinel explicitly instead of relying on `-1` arithmetic.

## Corrections to commit messages on this branch

A review pass over the fix commits found four factual slips in commit-message *rationale*. The code
in every case is correct; only the explanation is off, and since commit messages are the permanent
record the corrections are kept here.

* **`9fcd6de`** (10.0.0.0/8 mask) — the illustrative list of affected first octets wrongly includes
  `10`, which is genuinely private, and omits `234`. The count, "fifteen public /8 networks", is
  right; `(ip & 0xF) == 10` matches sixteen values, fifteen of them public.
* **`0aa8fed`** (`fnncmp` parenthesis) — the message describes only the case where
  `file[vrlen] == '/'`. When it is not `'/'`, the mis-parenthesised third argument evaluates to
  **0**, and `fnncmp(a, b, 0)` returns 0 — so the condition was true for *every* path, not merely
  those sharing a first character. The message also credits the out-of-bounds read to the `sprintf`
  tail alone; `file[strlen(virtual_repos)]` was itself an unconditional overread whenever `file` was
  shorter than the prefix.
* **`980d2f9`** (write-lock prefix) — two claims are wrong. The defective line is in the `#else`
  half of `#ifdef _WIN32`, so it was never compiled on Windows and cannot have affected
  `set_lockers_name` there. And a second writer never reaches `readers_exist`, because `write_lock`
  holds the master `#cvs.lck` directory across the success path, so writer B is stopped earlier by
  `set_lock` returning `L_LOCKED`. The real consequence is narrower and worse: a mis-named file left
  by a *crashed* writer is invisible to every scan, since `src/cvs.h` defines `CVSRFLPAT` but no
  `CVSWFLPAT`. Also unmentioned: `config.h:282` and `windows-NT/config.h:39` both define
  `HAVE_LONG_FILE_NAMES`, so the branch is dead in every configuration in the tree.
* **`e140973`** (zlib `deflate`) — "both `compress_lambda()` call sites" undercounts; there are
  three. `ca_blobs_fs/push_whole_blob.h:40` is live code, included by `src/rcs_cvt_kB.cpp:2`. It
  passes `pack ? ZSTD : Unpacked`, so the conclusion that nothing selects ZLIB for compression
  still holds — but the enumeration was presented as exhaustive and was not.
* **`e56e5c0`** (missing vararg) — "every other `error()` call in the function passes
  `fn_root(finfo->file)`" is false: the others pass `fn_root(finfo->fullname)`
  (`src/checkin.cpp:101`, `:155`, `:164`) and one passes no argument. The added argument is
  `finfo->file`, which is what `CVS_FOPEN` was handed and is defensible, but it is less specific
  than its neighbours during a recursive commit.

## Investigated and rejected

Recorded so they are not raised again.

### `_open_osfhandle((long)h, ...)` truncating a 64-bit HANDLE

`windows-NT/win32.cpp:1241` and several siblings cast a `HANDLE` to `long` — 32 bits under LLP64 —
before passing it to a function taking `intptr_t`. This produces C4311 and looks like a Win64 bug.

It is not. Microsoft documents Windows handle values as 32-bit significant and explicitly sanctions
truncating and sign-extending them when passing between 32- and 64-bit code. `(long)h` followed by
the implicit sign-extension back to `intptr_t` is exactly that pattern. Leave it alone; "fixing" it
to `(intptr_t)h` is harmless but pointless, and changing it to an unsigned cast would be wrong.

## Cross-cutting observations

Two patterns account for a large share of the list, and both suggest a mechanical sweep would find
more than a file-by-file read does.

**The wrong variable of a pair.** `buffer`/`buffer2`, `fd2`/`fd3`, `m_inputData`/`m_outputData`,
`m_inFd`/`m_errFd`, `name`/`group`, `len`/`len-oldlen`, `CVSRFL`/`CVSWFL`. Nine findings in the
support libraries alone are this shape, and several are security-relevant rather than cosmetic.

**A correct sibling a few lines from a broken one.** The 172.16/12 network test was repaired in
`01c6c85` while the 10/8 test on the line above kept its 4-bit mask. `cvsapi/win32/SocketIO.cpp` has
the buffer arithmetic its unix twin gets wrong. `Reader_Lock` uses the read-lock prefix in both
branches while `write_lock` used it in one. Wherever a fix has been applied once in this tree, it is
worth checking the paired site.

**The unix platform layer is measurably weaker than win32.** `cvsapi/unix/SocketIO.cpp`,
`cvsapi/unix/RunFile.cpp`, `cvsapi/unix/DnsApi.cpp` and `cvstools/unix/GlobalSettings.cpp` each
carry a defect whose win32 counterpart is correct. A mechanical unix-vs-win32 diff of the paired
files is the highest-yield follow-up available.

## Dead code

Not defects, but a source of wasted effort: `src/Modules1.cpp`, `src/Modules2.cpp` and
`src/RecurseRepository.cpp` (with their headers) appear in no `Makefile.am` and no `.vcxproj`, and
are included by nothing but each other. `RecurseRepository.cpp:102` additionally has a
format-string/argument mismatch (`BUG-lib-14`), which is why it appears in the list below despite
never being compiled.

## Parser divergence

`cvstools/RootSplitter.cpp` and `src/root.cpp:569` parse the same `CVSROOT` keyword grammar with two
separate implementations. After the fixes on this branch they agree on quoting, but `RootSplitter`
still lacks the backslash-escape handling that `root.cpp:581` has.

The divergence needs a backslash **outside** a quote — for example
`:pserver;a=\"b:c:host:/repo`, where `root.cpp`'s `escape` swallows the `"` and the root is
accepted, while `RootSplitter` opens a quote and rejects it. A backslash *inside* a quote does not
diverge: `root.cpp:581` only sets `escape` when `!in_quote`, so both parsers reject
`:pserver;a="x\"y":host:/repo` alike.

One implementation should be deleted in favour of the other.

## Open findings

Ordered by severity. "Fix size" is the estimated lines of change; "changes behaviour" flags whether
applying the fix alters observable behaviour, which is what kept several of the small ones out of
this branch.

### High (22)

| ID | Severity | Area | Issue | Fix size | Changes behaviour |
| --- | --- | --- | --- | ---: | --- |
| [`BUG-blob-03`](_reports/BUG-blob-03-encrypted-send-short-write.md) | high | `keyValueServer/blob_sockets/blob_sockets.cpp:178` | Encrypted `send()` silently discards ciphertext on a short write and still reports full success | 8 | no |
| [`BUG-blob-05`](_reports/BUG-blob-05-fileio-pull-ignores-from.md) | high | `ca_blobs_fs/src/fileio.cpp:313` | `blobe_fileio_pull()` ignores the `from` offset — ranged `PULL` returns the wrong bytes and over-sends past the announced length | 1 | yes |
| [`BUG-blob-06`](_reports/BUG-blob-06-downloadblobinfo-uninit-cctx-free.md) | high | `ca_blobs_fs/streaming_blobs.h:82` | `~DownloadBlobInfo` frees a decompressor that was never created when the blob magic is invalid | 3 | no |
| [`BUG-blob-07`](_reports/BUG-blob-07-cafs-server-allow-trust-off-ignored.md) | high | `keyValueServer/server/cafs_server.cpp:45` | `cafs_server ... off` never disables hash trust — `set_allow_trust()` is only ever called with `false`, and only when the operator asked for `on` | 2 | yes |
| [`BUG-lib-01`](_reports/BUG-lib-01-mapping-directory-stack-realloc-off-by-one.md) | high | `src/mapping.cpp:1043` | `open_directory()` restores `current_directory` one slot too far after `xrealloc`, causing a permanent +1 index drift and a one-element heap overflow | 1 | no |
| [`BUG-lib-05`](_reports/BUG-lib-05-cvs-wide-utf8-overread.md) | high | `cvsapi/cvs_string.h:228` | `cvs::wide::utf82ucs2()` walks past the terminating NUL on any truncated/invalid UTF-8 sequence, and still dereferences a NULL `src` | 10 | no |
| [`BUG-lib-12`](_reports/BUG-lib-12-root-parse-keyword-method-strcpy.md) | high | `src/root.cpp:385` | `strcpy(newroot->method, xstrdup(value))` overflows the existing `method` allocation (or dereferences NULL), and leaks the duplicate | 2 | no |
| [`BUG-lib-13`](_reports/BUG-lib-13-normalize-cvsroot-port-stack-overflow.md) | high | `src/root.cpp:1059` | `normalize_cvsroot()` `strcpy`s the CVSROOT port into a 64-byte stack buffer, and `sprintf`s a non-literal format string into it | 3 | no |
| [`BUG-lib-15`](_reports/BUG-lib-15-transcodebuffer-olen-unset-strcpy-binary.md) | high | `cvsapi/Codepage.cpp:461` | `CCodepage::TranscodeBuffer()` `strcpy`s a length-counted binary buffer on its failure path and returns without ever setting the `olen` out-parameter | 8 | no |
| [`BUG-lib-18`](_reports/BUG-lib-18-runfile-setoutput-wrong-member.md) | high | `cvsapi/unix/RunFile.cpp:100` | unix `CRunFile::setOutput()` stores the user data in `m_inputData`, so the output callback is invoked with an uninitialised `void*` — used directly as a `this` pointer | 2 | no |
| [`BUG-lib-22`](_reports/BUG-lib-22-perms-reserved-group-check-wrong-variable.md) | high | `src/perms.cpp:102` | The guard against redefining the reserved groups `admin` and `owner` tests the group *member* name instead of the *group* name, so `CVSROOT/group` can silently redefine both | 10 | yes |
| [`BUG-server-01`](_reports/BUG-server-01-rcsbuf-getkey-stale-pat.md) | high | `src/rcs.cpp:1308` | `rcsbuf_getkey` fails to relocate `pat` and `keystart` after a buffer-growing `rcsbuf_fill`, producing dangling pointers into a freed heap block | 4 | no |
| [`BUG-server-03`](_reports/BUG-server-03-compressed-delta-uninit-heap.md) | high | `src/rcs_checkin.cpp:1165` | `-kz` compressed delta length is set to `deflateBound()` instead of `stream.total_out`, writing uninitialized heap into the RCS file | 1 | yes |
| [`BUG-server-09`](_reports/BUG-server-09-entries-line-null-deref.md) | high | `src/server.cpp:2232` | Client-supplied `Entry` line with a missing `/` makes the server dereference address `0x1` (`strchr(...) + 1` with no NULL check) | 18 | yes |
| [`BUG-server-10`](_reports/BUG-server-10-tag-alias-branch-double-free.md) | high | `src/tag.cpp:767` | `cvs tag -A -b` / `cvs rtag -A -b`: `rev` aliases `version`, then both are freed — double free | 6 | no |
| [`BUG-server-12`](_reports/BUG-server-12-blob-pull-failure-ignored.md) | high | `src/rcs_cvt_kB.cpp:73` | A failed or truncated blob pull is reported as success: `-kB` checkout silently yields an empty file, a NULL deref, or a tail of uninitialized heap | 15 | yes |
| [`BUG-update-01`](_reports/BUG-update-01-global-tag-clobbered-head.md) | high | `src/update.cpp:1372` | `update_dirent_proc` overwrites the file-static `tag` with the string literal `"HEAD"`, which is later passed to `xfree()` | 6 | yes |
| [`BUG-update-04`](_reports/BUG-update-04-join-file-frees-static-options.md) | high | `src/update.cpp:3207` | `join_file()` frees the file-static `options` instead of its local `t_options` (double free / use-after-free via `checkout()`) | 1 | yes |
| [`BUG-update-08`](_reports/BUG-update-08-entries-log-duplicate-line.md) | high | `src/entries.cpp:257` | **Fixed on this branch** (Register in the Tier 1 slice; Scratch_Entry and Rename_Entry completed during its review). Was: every `Entries.Log` record written twice; the unprefixed second copy replays as an implicit `A`, resurrecting scratched/renamed entries | 4 | yes |
| [`BUG-update-10`](_reports/BUG-update-10-client-file-mode-never-applied.md) | high | `src/client.cpp:1938` | `update_entries()` applies the wire `mode_string` only when a `Mode` response was *also* received, so checked-out file permissions are never set | 2 | yes |
| [`BUG-update-12`](_reports/BUG-update-12-renamed-response-path-traversal.md) | high | `src/client.cpp:3015` | `rename_entry_and_file()` accepts an unvalidated destination path from the server — the `Renamed` response can write outside the working copy | 4 | yes |
| [`BUG-update-18`](_reports/BUG-update-18-line2argv-runs-off-buffer.md) | high | `src/subr.cpp:309` | `line2argv()` skips separators with `strchr(sepchars, *p)`, which is true for the NUL terminator — it walks off the end of the buffer | 2 | no |

### Medium (32)

| ID | Severity | Area | Issue | Fix size | Changes behaviour |
| --- | --- | --- | --- | ---: | --- |
| [`BUG-blob-04`](_reports/BUG-blob-04-evp-decrypt-ctx-leak.md) | medium | `keyValueServer/blob_sockets/blob_sockets.cpp:421` | Handshake frees `enc` twice and never frees `dec` — one `EVP_CIPHER_CTX` leaked per authenticated connection | 1 | no |
| [`BUG-blob-08`](_reports/BUG-blob-08-stream-to-server-leak-on-fatal.md) | medium | `src/blob_kv_processor.cpp:70` | Short-circuit in `send_blob_file_data_net()` leaks the 64 KiB `StreamToServerData` on every fatal upload error | 3 | no |
| [`BUG-blob-09`](_reports/BUG-blob-09-compress-lambda-leaks-stream-on-error.md) | medium | `ca_blobs_fs/streaming_compressors.h:46` | `compress_lambda()` returns without killing the compression stream when the producer or consumer reports an error | 4 | no |
| [`BUG-blob-11`](_reports/BUG-blob-11-backgroundprocessor-member-order-terminate.md) | medium | `src/download_blob_to.cpp:119` | `BackgroundProcessor` declares `queue` before `threads`, so teardown destroys the thread vector first — `std::terminate()` on any early exit, dangling `waiting_threads` otherwise | 4 | no |
| [`BUG-blob-12`](_reports/BUG-blob-12-download-retry-exhaustion-falls-through.md) | medium | `src/download_blob_to.cpp:451` | `download_blob_ref_file()` falls out of its 16-attempt retry loop into the success path and fatally renames a temp file it just deleted | 5 | yes |
| [`BUG-blob-15`](_reports/BUG-blob-15-roundrobin-private-mirrors-skipped.md) | medium | `src/download_blob_to.cpp:207` | Mirror round-robin compares `attempt < privateCount` instead of `attempt < publicCount + privateCount`, so most private mirrors are never tried | 1 | yes |
| [`BUG-blob-16`](_reports/BUG-blob-16-finishdownloads-checks-errors-before-waiting.md) | medium | `src/download_blob_to.cpp:67` | `finishDownloads()` samples `hasErrors` *before* waiting for the workers, so failures during the final drain never affect the exit status | 6 | yes |
| [`BUG-build-02`](_reports/BUG-build-02-x64-release-exceptions-disabled.md) | medium | `cvsnt.vcxproj:188` | `Release\|x64` compiles `cvs.exe` with C++ exceptions disabled, but `setuid.cpp` contains a live `try`/`catch` — the 64-bit release binary has no unwind semantics where the 32-bit one does | 1 | yes |
| [`BUG-lib-03`](_reports/BUG-lib-03-mapping-strlen-minus-one-underflow.md) | medium | `src/mapping.cpp:1185` | `buf[strlen(buf)-1] = '\0'` writes one byte *before* the buffer when a CVS admin line starts with a NUL byte | 12 | no |
| [`BUG-lib-07`](_reports/BUG-lib-07-globalsettings-file-handle-leak.md) | medium | `cvstools/unix/GlobalSettings.cpp:154` | `CGlobalSettings::_GetUserValue()` and `GetGlobalValue()` leak the config `FILE*` on every successful lookup | 2 | no |
| [`BUG-lib-08`](_reports/BUG-lib-08-globalsettings-strncpy-no-nul.md) | medium | `cvstools/unix/GlobalSettings.cpp:151` | `strncpy(buffer, …, buffer_len)` leaves the caller's buffer unterminated when a config value is at least `buffer_len` bytes | 8 | no |
| [`BUG-lib-09`](_reports/BUG-lib-09-enumvalues-null-deref.md) | medium | `cvstools/unix/GlobalSettings.cpp:271` | `EnumUserValues()`/`EnumGlobalValues()` dereference a NULL `p` on any config line that has no `=` | 2 | no |
| [`BUG-lib-10`](_reports/BUG-lib-10-getuserconfigfile-getpwuid-null.md) | medium | `cvstools/unix/GlobalSettings.cpp:80` | `GetUserConfigFile()` dereferences the result of `getpwuid()` without checking it for NULL | 3 | no |
| [`BUG-lib-16`](_reports/BUG-lib-16-dnsapi-delete-mismatch-and-debug-printf.md) | medium | `cvsapi/unix/DnsApi.cpp:225` | `CDnsApi::Close()` frees a `new[]` array with scalar `delete`; the same file leaks debug `printf`s onto stdout and passes the wrong base pointer to `dn_expand()` | 12 | no |
| [`BUG-lib-19`](_reports/BUG-lib-19-runfile-child-stderr-wrong-pipe.md) | medium | `cvsapi/unix/RunFile.cpp:190` | In the forked child, the stderr branch closes and dups `fd2` (the stdout pipe) instead of `fd3`; and the non-blocking `fcntl` for the input pipe is applied to `m_errFd` | 3 | yes |
| [`BUG-lib-20`](_reports/BUG-lib-20-runfile-wait-result-uninitialised.md) | medium | `cvsapi/unix/RunFile.cpp:229` | `CRunFile::wait()` returns `-1` from a `bool` function (i.e. `true`) and leaves the `result` out-parameter unwritten, so `run_exec()` returns an uninitialised exit status | 8 | yes |
| [`BUG-lib-21`](_reports/BUG-lib-21-main-response-file-unique-ptr-strdup.md) | medium | `src/main.cpp:683` | Response-file handling stores `strdup()` results in `std::unique_ptr<char[]>`, so every argument is released with `delete[]` on `malloc`'d memory; the vector is then reinterpreted as `char**` | 6 | no |
| [`BUG-server-02`](_reports/BUG-server-02-linevector-free-postdecrement.md) | medium | `src/rcs.cpp:5491` | `linevector_free` uses post-decrement on the binary refcount, so binary file buffers are never freed | 1 | no |
| [`BUG-server-04`](_reports/BUG-server-04-inflate-state-leak.md) | medium | `src/rcs.cpp:649` | Two `inflateInit()` calls with no matching `inflateEnd()` leak a full zlib inflate state per compressed revision | 2 | no |
| [`BUG-server-05`](_reports/BUG-server-05-lock-recv-off-by-one.md) | medium | `src/lock.cpp:246` | Off-by-one stack write when a lock-server reply exactly fills the receive buffer | 4 | no |
| [`BUG-server-11`](_reports/BUG-server-11-rcs-getbranch-leak.md) | medium | `src/rcs.cpp:2545` | `RCS_getbranch` frees its `branch` buffer only inside the `atomic_checkouts` branch, leaking on every call in the default configuration | 4 | no |
| [`BUG-server-13`](_reports/BUG-server-13-admin-p-validation-not-enforced.md) | medium | `src/admin.cpp:823` | `cvs admin -p`: property-name validation prints an error but does not stop the write, letting `;` and `@` into the RCS `properties` newphrase | 6 | yes |
| [`BUG-server-16`](_reports/BUG-server-16-add-rcs-file-fclose-null.md) | medium | `src/import.cpp:1622` | `add_rcs_file` write-error path calls `fclose(fpuser)` and `fn_root(userfile)` without the NULL guard the success path has | 5 | no |
| [`BUG-server-18`](_reports/BUG-server-18-expand-keywords-per-keyword-leak.md) | medium | `src/rcs.cpp:3937` | `expand_keywords` leaks two heap strings on every `$KEYWORD$` candidate it examines | 4 | no |
| [`BUG-update-03`](_reports/BUG-update-03-patch-file-write-oob-read.md) | medium | `src/update.cpp:2246` | `patch_file_write()` reads `buffer[len - 1]` without checking `len != 0` — out-of-bounds read on empty revisions | 2 | no |
| [`BUG-update-06`](_reports/BUG-update-06-bound-merge-bugid-null-deref.md) | medium | `src/update.cpp:2687` | `bound_merge_by_bugid()` dereferences the result of `previous_version()` without a NULL check | 6 | yes |
| [`BUG-update-07`](_reports/BUG-update-07-nonrecursive-module-noop.md) | medium | `src/recurse.cpp:779` | `nonrecursive_module()` check in `do_recursion()` is a no-op — non-recursive modules are still recursed into | 1 | yes |
| [`BUG-update-09`](_reports/BUG-update-09-scratch-rename-entry-unchecked-fopen.md) | medium | `src/entries.cpp:252` | `Scratch_Entry()` and `Rename_Entry()` use the result of `CVS_FOPEN()` without a NULL check — `fprintf(NULL, ...)` crash on a read-only working directory | 16 | yes |
| [`BUG-update-15`](_reports/BUG-update-15-find-rcs-node-leak.md) | medium | `src/find_names.cpp:297` | `find_rcs()` leaks a `Node` plus its key for every repository file that is already in the list — i.e. for nearly every file of every update | 3 | no |
| [`BUG-update-16`](_reports/BUG-update-16-ign-add-else-misplaced.md) | medium | `src/ignore.cpp:276` | `ign_add()`: the temporary-reset `else if` is attached to the wrong `if`, so a lone `!` in `.cvsignore` does nothing and any `!xxx` token wipes the ignore list | 3 | yes |
| [`BUG-update-17`](_reports/BUG-update-17-xcmp-symlink-inverted.md) | medium | `src/filesubr.cpp:967` | `xcmp()` returns inverted results when both operands are symlinks | 1 | yes |
| [`BUG-update-19`](_reports/BUG-update-19-send-repository-unchecked-fgets.md) | medium | `src/client.cpp:3444` | `send_repository()` ignores the `fgets()` return value and then indexes `line[strlen(line)-1]` on a possibly-uninitialised stack buffer | 6 | yes |

### Low (16)

| ID | Severity | Area | Issue | Fix size | Changes behaviour |
| --- | --- | --- | --- | ---: | --- |
| [`BUG-blob-17`](_reports/BUG-blob-17-gc-condvar-lost-wakeup.md) | low | `keyValueServer/proxy/gc_thread_monitor.cpp:51` | Proxy GC thread waits on a condition variable with no predicate, and the notifier does not hold the mutex — wakeups are lost and the cache overruns its limit | 6 | no |
| [`BUG-blob-18`](_reports/BUG-blob-18-unchecked-fopen-null-deref.md) | low | `src/blob_operations.cpp:71` | Two blob helpers use the result of `fopen()` without a null check | 6 | no |
| [`BUG-blob-19`](_reports/BUG-blob-19-push-to-server-zero-progress-hang.md) | low | `keyValueServer/clientLib/blob_push_client_cmd.cpp:35` | `blob_push_to_server()` spins forever when the producer callback reports zero progress — `cafs_client push` hangs on every file | 6 | yes |
| [`BUG-build-01`](_reports/BUG-build-01-vcxproj-external-libs-x64-typo.md) | low | `cvsnt.vcxproj:204` | `cvsnt.vcxproj` x64 configurations point at `..\external_libsx64` — a missing backslash makes the first library search path nonexistent | 2 | no |
| [`BUG-lib-14`](_reports/BUG-lib-14-recurserepository-sprintf-arg-mismatch.md) | low | `src/RecurseRepository.cpp:102` | `cvs::sprintf` called with a 2-conversion format and 3 arguments: the filename is silently dropped and every child gets the name `<parent>//` | 2 | yes |
| [`BUG-lib-23`](_reports/BUG-lib-23-misspelled-user-facing-strings.md) | low | `src/buffer.cpp:1567` | Misspelled user-facing strings: "recieved", "Depreciated", "Eraseing", "FindPrototocol" | 10 | no |
| [`BUG-server-06`](_reports/BUG-server-06-do-lock-server-shadowed-ob.md) | low | `src/lock.cpp:263` | Shadowed local `ob` in `do_lock_server` defeats all three `xfree(ob)` calls, leaking a path buffer per lock | 1 | no |
| [`BUG-server-07`](_reports/BUG-server-07-win32-lockers-name-leak.md) | low | `src/lock.cpp:1053` | Windows variant of `set_lockers_name` never frees the previous `lockers_name` | 2 | no |
| [`BUG-server-14`](_reports/BUG-server-14-magicrev-duplicated-for.md) | low | `src/rcs.cpp:2257` | Duplicated `for` header in `RCS_magicrev` makes the whole `findnextmagicrev` optimisation dead code | 2 | no |
| [`BUG-server-19`](_reports/BUG-server-19-commitpt-cleared-before-ternary.md) | low | `src/rcs_checkin.cpp:967` | `commitpt` is set to NULL four lines before `commitpt?'A':'M'` is evaluated, so the lock server is never told "Added" | 3 | yes |
| [`BUG-server-20`](_reports/BUG-server-20-cmp-file-bitwise-and.md) | low | `src/rcs_checkin.cpp:1450` | `RCS_cmp_file` uses bitwise `&` instead of `&&`, silently testing only bit 0 of `ignore_keywords` | 1 | no |
| [`BUG-server-21`](_reports/BUG-server-21-history-cp-gt-workdir-string-compare.md) | low | `src/history.cpp:801` | `cp > workdir` in `history_write` is a lexicographic string comparison, not the intended pointer bounds check | 1 | yes |
| [`BUG-update-02`](_reports/BUG-update-02-existence-error-guard-broken.md) | low | `src/update.cpp:1899` | Inserted `TRACE()` statement stole the body of `if (!existence_error (errno))`, making the error unconditional | 3 | yes |
| [`BUG-update-11`](_reports/BUG-update-11-localtime-timestamp-leak.md) | low | `src/client.cpp:2006` | `localtime_timestamp` is allocated once per updated file in three functions and never used or freed | 6 | no |
| [`BUG-update-13`](_reports/BUG-update-13-version-ts-per-file-leaks.md) | low | `src/vers_ts.cpp:312` | `Version_TS()` leaks the results of `RCS_getexpand()` and `wrap_rcsoption()` — once per file, per command | 4 | no |
| [`BUG-update-14`](_reports/BUG-update-14-create-admin-null-dir-path.md) | low | `src/create_adm.cpp:166` | `Create_Admin()`'s documented `dir == NULL` path is doubly broken: `strlen(NULL)` at entry and `CVSADM_ENT` written where `CVSADM_ENTEXT` is meant | 2 | no |

## Fixed on this branch

For reference, the 15 defects that were fixed:

| ID | Issue |
| --- | --- |
| [`BUG-blob-01`](_reports/BUG-blob-01-classify-ip-nibble-mask.md) | `blob_classify_ip()` masked 4 bits instead of 8, classifying fifteen public /8 networks as private and gating encryption drop, no-auth acceptance and fail2ban logging on that verdict |
| [`BUG-blob-02`](_reports/BUG-blob-02-decode-stream-header-split-underflow.md) | Blob header remainder computed from the chunk size instead of the header size: out-of-bounds write plus a length wrapping to ~2^64 |
| [`BUG-lib-11`](_reports/BUG-lib-11-socketio-recv-bufpos-desync.md) | unix `CSocketIO::recv()` advanced the buffer position by the bytes *wanted*, not the bytes taken, letting a `memcpy` length underflow |
| [`BUG-lib-02`](_reports/BUG-lib-02-mapping-fnncmp-misplaced-paren.md) | Misplaced parenthesis made `fnncmp()` compare 0 or 1 characters and read past the end of a stack buffer |
| [`BUG-lib-04`](_reports/BUG-lib-04-rootsplitter-quote-loop-inverted.md) | Quoted `CVSROOT` keyword values were rejected outright; also completed with the missing quote-open guard |
| [`BUG-lib-06`](_reports/BUG-lib-06-main-repository-name-wrong-buffer.md) | Trailing-separator strip tested `buffer2` but truncated `buffer`, so `/cvs/` became `/cv` |
| [`BUG-lib-17`](_reports/BUG-lib-17-tokenline-append-overload-escapes.md) | `arg.append('\n',1)` selected the `(count, char)` overload and appended ten `0x01` bytes |
| [`BUG-lib-24`](_reports/BUG-lib-24-serverio-syslog-priority-clobbered.md) | `syslog(l \| LOG_NOTICE, ...)` downgraded every logged error from `LOG_ERR` to `LOG_DEBUG` |
| [`BUG-lib-25`](_reports/BUG-lib-25-getoptions-strchr-nul-overread.md) | A bare `-` made `strchr(fmt, '\0')` succeed, reading past the format string |
| [`BUG-blob-10`](_reports/BUG-blob-10-zlib-compress-calls-inflate.md) | `compress_stream_zlib()` called `inflate()` |
| [`BUG-blob-13`](_reports/BUG-blob-13-start-push-server-should-stop-pointer.md) | The accept loop tested the `should_stop` *pointer* rather than the flag |
| [`BUG-blob-20`](_reports/BUG-blob-20-encode-hash-infinite-recursion.md) | `encode_hash_str_to_blob_hash()` called itself — unconditional infinite recursion |
| [`BUG-server-08`](_reports/BUG-server-08-writelock-uses-CVSRFL.md) | `write_lock` built the write-lock filename from the read-lock prefix |
| [`BUG-server-15`](_reports/BUG-server-15-checkin-format-missing-arg.md) | `%s` with no argument on the reopen-failure path |
| [`BUG-server-17`](_reports/BUG-server-17-pnew-file-comment-typo.md) | `"pnew file"` written into the RCS `comment` field of every imported file |
