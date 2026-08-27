# G-CVSNT code audit — findings index

Static analysis of the Gaijin-modified CVSNT source
(`cvsnt/cvsnt-2.5.05.3744/`). Each `*.md` file is one finding: location,
severity, a code snippet, why it is a bug, and a suggested fix. Prefixes:
`blob-` = content-addressed blob layer, `core-` = core cvs commands/client/
server, `api-` = cvsapi / protocols / Windows layer, `perf-` = the
update/tag/branch performance analysis.

**62 findings: 1 critical, 15 high, 28 medium, 18 low.** Findings are static —
each was verified by re-reading the surrounding source, and several were
independently confirmed while auditing (e.g. a correct sibling implementation
proving the buggy one wrong). They are not all runtime-reproduced; treat
severity as "worst plausible impact".

## Critical

| # | Area | Summary |
|---|------|---------|
| [api-009](api-009-sspi-unix-server-read-buffer-overflow.md) | protocols | sspi (unix) **server**: client-controlled length drives `read()` into a fixed 1024-byte stack buffer — remote pre-auth stack overflow |

## High

| # | Area | Summary |
|---|------|---------|
| [core-011](core-011-send-modified-size-truncated-4gb.md) | client | `send_modified` announces file size with `%lu` — silently truncates >4 GB files on Windows, desyncing the protocol |
| [core-013](core-013-server-receive-file-size-int-overflow.md) | server | receive path uses `int`/`atoi` for file size — >2 GB uploads overflow |
| [core-015](core-015-server-updated-size-unsigned-long-truncation.md) | server | `server_updated` uses 32-bit `unsigned long` + `%lu` — truncates >4 GB checkouts on Windows |
| [core-014](core-014-server-updated-blob-ref-created-dead-condition.md) | server/blob | `Blob-ref-created` branch is dead code (`vers==NULL && vers->ts_user`), can clobber untracked local files |
| [core-017](core-017-rcs-checkin-static-diffopts-strcat-overflow.md) | server | `RCS_checkin` `strcat`s encoding onto a `static char[64]` every call — accumulates and overflows |
| [core-001](core-001-join-file-frees-global-options.md) | update/merge | `join_file` frees the global `options` instead of the local copy — drops sticky `-kb`, possible double free |
| [blob-001](blob-001-compress-stream-zlib-calls-inflate.md) | blob | streaming ZLIB compressor calls `inflate()` instead of `deflate()` |
| [blob-002](blob-002-split-header-hdrpart-miscalc.md) | blob | `decode_stream_blob_data` miscomputes header-part size when the header is split across chunks — remotely triggerable OOB |
| [blob-007](blob-007-mmap-failure-not-checked-map-failed.md) | blob | POSIX `mmap` `MAP_FAILED` not detected; `(void*)-1` used as a mapping → server crash |
| [api-001](api-001-sserver-unscramble-null-deref.md) | protocols | sserver auth: NULL-pointer `strcpy` on malformed scrambled password (pserver guards it, sserver doesn't) |
| [api-003](api-003-sserver-win32-unscramble-null-deref.md) | protocols | sserver (win32): same Unscramble NULL deref |
| [api-018](api-018-breaknameintoparts-unbounded-copy-preauth-overflow.md) | win32 | `BreakNameIntoParts` unbounded copy → pre-auth server stack overflow via client username |
| [api-022](api-022-sspi-unix-client-challenge-overflow.md) | protocols | sspi (unix) client: server-controlled length drives `tcp_read` into a fixed challenge struct |
| [api-014](api-014-sspi-client-format-string.md) | protocols | sspi client passes a server-controlled string as a printf format |
| [api-015](api-015-sspi-unix-client-format-string.md) | protocols | sspi (unix) client: same format-string bug |

## Medium

| # | Area | Summary |
|---|------|---------|
| [core-002](core-002-checkout-file-resurrect-frees-rcs-node-version.md) | update | `checkout_file` frees RCS-owned version string when resurrecting during a join |
| [core-003](core-003-bound-merge-by-bugid-null-deref.md) | update | `bound_merge_by_bugid` NULL deref when the bug's earliest change is the first revision |
| [core-004](core-004-commit-sends-bare-i-argument.md) | client | commit sends bare `i` instead of `-i` for ignore-keywords, breaking option parsing |
| [core-007](core-007-commit-fileproc-lock-leak-on-error.md) | commit | error paths skip `do_unlock_file`, leaving files write-locked on the lock server |
| [core-008](core-008-pretag-proc-uninitialized-ret.md) | tag | `pretag_proc` returns an uninitialized value when a trigger lib has no pretag handler |
| [core-009](core-009-tag-alias-plus-branch-double-free.md) | tag | `cvs tag/rtag -A -b` double-frees the revision string |
| [core-010](core-010-send-blob-file-direct-unchecked-fopen.md) | blob | `send_blob_file_direct` never checks `fopen` before `fread`/`fclose` |
| [core-012](core-012-serve-entry-extra-null-deref.md) | server | `serve_entry_extra` NULL deref if `EntryExtra` arrives before any `Entry` (remote) |
| [core-018](core-018-client-overwrite-existing-never-reset-C.md) | client | `update -C`: `client_overwrite_existing` latches on and never resets, silently overwriting untracked in-the-way files (found during client-port research) |
| [blob-003](blob-003-oneshot-decompress-unpacked-returns-error.md) | blob | one-shot `decompress()` returns Error for Unpacked data on success; leaks z_stream on error |
| [blob-005](blob-005-finish-leaks-temp-file-on-error.md) | blob | `finish()` leaks the temp blob file on error paths (remote disk-fill when trust off) |
| [blob-006](blob-006-blobe-fileio-pull-ignores-from-offset.md) | blob | `blobe_fileio_pull` ignores the `from` offset — corrupts any non-zero-offset pull |
| [blob-009](blob-009-send-blob-file-data-net-unchecked-fopen.md) | blob | `send_blob_file_data_net` uses `fopen` result without NULL check |
| [blob-012](blob-012-get-session-blob-ref-unchecked-fopen.md) | blob | `get_session_blob_reference_hash` uses `fopen` result without NULL check |
| [blob-014](blob-014-start-push-server-should-stop-not-dereferenced.md) | blob | accept loop tests the `should_stop` pointer, not `*should_stop` |
| [blob-015](blob-015-exchange-session-keys-leaks-decrypt-ctx.md) | blob | `exchange_session_keys` frees the wrong cipher ctx, leaks decrypt ctx per handshake |
| [blob-016](blob-016-available-disk-space-inverted-error-check.md) | blob | `available_disk_space` inverts the error check — disk-full GC never triggers |
| [blob-018](blob-018-handle-pull-oversends-partial-range.md) | blob | `handle_pull` over-sends the whole blob for a bounded range request |
| [api-002](api-002-sserver-win32-uninitialized-certonly.md) | protocols | sserver (win32) uses uninitialized `certonly` when the registry value is absent |
| [api-004](api-004-sspi-tokensource-sprintf-overflow.md) | protocols | SSPI client: unbounded `sprintf` of hostname into a 60-byte SPN buffer |
| [api-006](api-006-ext-disconnect-wrong-fd-reset.md) | protocols | `ext_disconnect` resets `current_in` twice, leaving `current_out` stale/closed |
| [api-007](api-007-fork-disconnect-wrong-fd-reset.md) | protocols | `fork_disconnect`: same double-reset (copy-paste twin of api-006) |
| [api-011](api-011-cvs-wide-utf8-oob-read.md) | cvsapi | `cvs::wide` UTF-8 decoder reads past end on truncated multibyte |
| [api-012](api-012-filaccess-mimetype-byte-vs-wchar-oob.md) | cvsapi | `CFileAccess::mimetype` writes NUL at a byte index into a `wchar_t` buffer |
| [api-013](api-013-unix-socketio-recv-bufpos-corruption.md) | cvsapi | unix `CSocketIO::recv` over-advances `m_bufpos`, dropping buffered bytes |
| [api-017](api-017-setuid-lsa-rights-wrong-index.md) | win32 | `nt_setuid` reads `lsaUserRights[0]` instead of `[n]` when building token privileges |
| [api-020](api-020-pserver-auth-end-double-free-missing-return.md) | protocols | pserver auth: double-free + accepts bad protocol end when `server_error` returns |
| [api-021](api-021-service-unison-setstdhandle-race.md) | service | `DoUnisonThread` races on process-global std handles across connections |

## Low

| # | Area | Summary |
|---|------|---------|
| [core-005](core-005-fixaddfile-leaves-really-quiet-set.md) | add | `fixaddfile` fails to restore `really_quiet` (and leaks) on unparseable RCS |
| [core-006](core-006-rcs-checkout-options-passed-as-nametag.md) | remove/import | keyword-expansion options passed in the `nametag` slot of `RCS_checkout` |
| [core-016](core-016-scratch-entry-duplicate-entrieslog-line.md) | entries | `Scratch_Entry`/`Rename_Entry` write a duplicate implicit-Add line into `Entries.Log` |
| [blob-004](blob-004-compress-stream-wrong-ctx-pointer.md) | blob | one-shot `compress_stream` reads StreamType from the wrong pointer in the Unpacked branch |
| [blob-008](blob-008-set-root-null-contract-crash.md) | blob | `set_root()` dereferences its arg despite a documented nullptr contract |
| [blob-010](blob-010-ftell-long-truncation-large-files.md) | blob | upload size via `ftell` (`long`) truncates for >2 GB blobs |
| [blob-011](blob-011-http-err-code-appended-as-char.md) | blob | HTTP error message appends the status code as a raw char |
| [blob-013](blob-013-pull-at-once-ignores-decode-failure-uninit-tail.md) | blob | `pull_at_once` ignores decode failures, exposes uninitialized tail |
| [blob-017](blob-017-connect-timeout-format-string-arg-mismatch.md) | blob | `connect_with_timeout` log has two `%d` but one argument (varargs UB) |
| [blob-019](blob-019-base-repo-trailing-slash-off-by-one.md) | blob | `base_repo` trailing-slash check indexes the NUL terminator, yields `//` |
| [blob-020](blob-020-atomic-wrapper-assignment-missing-return.md) | blob | `atomic_wrapper` copy-assignment has no return statement (UB) |
| [blob-021](blob-021-sample-pushfile-chunk-size-arithmetic.md) | blob (sample) | sample `cafs_client` PUSHFILE computes the wrong chunk size |
| [blob-022](blob-022-sample-server-wont-compile.md) | blob (sample) | `sample_server.cpp` does not compile |
| [api-005](api-005-ssh-key-parse-null-deref.md) | protocols | `ssh_connect` dereferences `strchr` result before its NULL check |
| [api-008](api-008-ext-expand-command-line-overflow.md) | protocols | ext `expand_command_line`: unbounded `%`-expansion strcpy + off-by-one |
| [api-010](api-010-server-rsh-read-return-truncated.md) | protocols | server (rsh): `tcp_read` int return truncated into `unsigned char` |
| [api-016](api-016-common-proxy-error-missing-format-specifier.md) | protocols | `tcp_connect_http`: proxy error text dropped due to missing `%s` |
| [api-019](api-019-zeroconf-srv-resize-underflow.md) | cvsapi | Zeroconf SRV parse: `size_t` underflow in `resize(i-1)` on crafted mDNS name |

## Themes worth noting

* **Large-file (>2 GB/4 GB) handling in the legacy non-blob transfer path is
  broken in both directions and both processes** (core-011 client send,
  core-013 server receive, core-015 server send). The newer blob path uses
  `uint64_t`/`atoll` correctly, but any fallback to `Modified`/`Updated`
  truncates. This directly undermines the fork's reason to exist.
* **Copy-paste divergence between sibling implementations**: pserver vs
  sserver Unscramble guard (api-001), win32 vs unix `recv` bookkeeping
  (api-013), ext vs fork disconnect (api-006/007), UNICODE vs ANSI bounds
  (api-018). When fixing one, check its twin.
* **Unvalidated on-wire lengths in the network/auth layer** (api-009,
  api-018, api-022) are the most security-sensitive: remotely reachable,
  pre-authentication, memory-corrupting.
* **Blob-reference feature bugs** (core-014, core-010, blob-002, blob-006)
  sit directly in the fork's flagship code.

See [../docs/performance.md](../docs/performance.md) for the separate
update/tag/branch scaling analysis (`perf-analysis.md` in this folder).
