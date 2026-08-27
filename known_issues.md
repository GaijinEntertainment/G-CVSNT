# Known issues

Bugs found by static audit of the Gaijin-modified CVSNT source that are **not
yet fixed**. 17 simple, behaviour-preserving fixes landed in this iteration
(see the git history / the fixes are each a separate commit); the 45 issues
below were deferred because they change behaviour on a normal path, need
64-bit protocol plumbing, are security-sensitive enough to want careful
review, or live in sample code. Full analysis for each id (code, failure
scenario, suggested fix) is in `_reports/<id>.md`.

Legend for "why deferred": **size** = needs coordinated 64-bit size plumbing
across client+server+buffer; **behaviour** = fix activates/alters a normal
code path and wants a test first; **security** = network-facing/pre-auth, wants
careful review + a regression test; **cross** = has a sibling site that must be
fixed together; **sample** = non-shipping sample/test code.

## Critical

| id | file | issue | why deferred |
|----|------|-------|--------------|
| api-009 | protocols/sspi_unix.cpp | sspi (unix) **server**: client-controlled length drives `read()` into a fixed 1024-byte stack buffer — remote pre-auth stack overflow | security |

## High

| id | file | issue | why deferred |
|----|------|-------|--------------|
| core-011 | src/client.cpp | `send_modified` announces file size with `%lu` — truncates >4 GB on Windows, desyncs the protocol | size |
| core-013 | src/server.cpp | receive path uses `int`/`atoi` for file size — >2 GB uploads overflow | size |
| core-015 | src/server.cpp | `server_updated` uses 32-bit `unsigned long` + `%lu` — truncates >4 GB checkouts | size |
| core-014 | src/server.cpp | `Blob-ref-created` branch is dead code (`vers==NULL && vers->ts_user==NULL`); can clobber untracked files | behaviour |
| core-017 | src/rcs_checkin.cpp | `RCS_checkin` `strcat`s encoding onto a `static char[64]` every call — accumulates and overflows | behaviour |
| core-001 | src/update.cpp | `join_file` frees the global `options` instead of the local copy — drops sticky `-kb`, possible double free | behaviour |
| blob-001 | ca_blobs_fs/src/streaming_compressors.cpp | streaming ZLIB compressor calls `inflate()` instead of `deflate()` | behaviour (path currently unused) |
| blob-002 | ca_blobs_fs/src/streaming_compressors.cpp | `decode_stream_blob_data` miscomputes header-part size when the header splits across chunks — remotely triggerable OOB | security |
| api-001 | protocols/sserver.cpp | sserver auth: NULL `strcpy` on malformed scrambled password (pserver guards it, sserver doesn't) | security, cross (api-003) |
| api-003 | protocols/sserver.cpp (win32) | same Unscramble NULL deref on win32 | security, cross (api-001) |
| api-018 | windows-NT/win32.cpp | `BreakNameIntoParts` unbounded copy → pre-auth server stack overflow via client username | security |
| api-022 | protocols/sspi_unix.cpp | sspi (unix) client: server-controlled length drives `tcp_read` into a fixed challenge struct | security |
| api-014 | protocols/sspi.cpp | sspi client passes a server-controlled string as a printf format | security, cross (api-015) |
| api-015 | protocols/sspi_unix.cpp | sspi (unix) client: same format-string bug | security, cross (api-014) |

## Medium

| id | file | issue | why deferred |
|----|------|-------|--------------|
| core-018 | src/client.cpp | `update -C`: `client_overwrite_existing` latches on and never resets, silently overwriting untracked in-the-way files | behaviour (fixed as part of the client-update-features work) |
| core-002 | src/update.cpp | `checkout_file` frees RCS-owned version string when resurrecting during a join | behaviour |
| core-003 | src/update.cpp | `bound_merge_by_bugid` NULL deref when the bug's earliest change is the first revision | behaviour |
| core-007 | src/commit.cpp | `commit_fileproc` error paths skip `do_unlock_file`, leaving files write-locked on the lock server | behaviour |
| core-009 | src/tag.cpp | `cvs tag/rtag -A -b` double-frees the revision string | behaviour |
| core-012 | src/server.cpp | `serve_entry_extra` NULL deref if `EntryExtra` arrives before any `Entry` (remote) | security |
| blob-003 | ca_blobs_fs/src/streaming_compressors.cpp | one-shot `decompress()` returns Error for Unpacked data on success; leaks z_stream on error | behaviour |
| blob-006 | ca_blobs_fs/src/fileio.cpp | `blobe_fileio_pull` ignores the `from` offset — corrupts any non-zero-offset pull | behaviour |
| blob-012 | src/client.cpp | `get_session_blob_reference_hash` unchecked `fopen` (same class as the two fixed this round) | cross |
| blob-014 | keyValueServer/serverLib | accept loop tests the `should_stop` pointer, not `*should_stop` | behaviour |
| blob-018 | keyValueServer/serverLib | `handle_pull` over-sends the whole blob for a bounded range request | behaviour |
| api-002 | protocols/sserver.cpp (win32) | uses uninitialized `certonly` when the registry value is absent | behaviour |
| api-006 | protocols/ext.cpp | `ext_disconnect` resets `current_in` twice, leaving `current_out` stale/closed | cross (api-007) |
| api-007 | protocols/fork.cpp | `fork_disconnect`: same double-reset | cross (api-006) |
| api-011 | cvsapi/cvs_string.cpp | `cvs::wide` UTF-8 decoder reads past end on truncated multibyte | behaviour |
| api-012 | cvsapi/win32/FileAccess.cpp | `CFileAccess::mimetype` writes NUL at a byte index into a `wchar_t` buffer | behaviour |
| api-013 | cvsapi/unix/SocketIO.cpp | unix `CSocketIO::recv` over-advances `m_bufpos`, dropping buffered bytes | behaviour |
| api-017 | windows-NT/setuid.cpp | `nt_setuid` reads `lsaUserRights[0]` instead of `[n]` when building token privileges | behaviour |
| api-020 | protocols/pserver.cpp | pserver auth: double-free + accepts bad protocol end when `server_error` returns | security |
| api-021 | cvsservice/Service.cpp | `DoUnisonThread` races on process-global std handles across connections | behaviour |

## Low

| id | file | issue | why deferred |
|----|------|-------|--------------|
| core-006 | src/import.cpp / rcs | keyword-expansion options passed in the `nametag` slot of `RCS_checkout` | behaviour |
| core-016 | src/entries.cpp | `Scratch_Entry`/`Rename_Entry` write a duplicate implicit-Add line into `Entries.Log` | behaviour |
| blob-004 | ca_blobs_fs/src/streaming_compressors.cpp | one-shot `compress_stream` reads StreamType from the wrong pointer in the Unpacked branch | behaviour (path unused) |
| blob-010 | src/blob_kv_processor.cpp | upload size via `ftell` (`long`) truncates for >2 GB blobs | size |
| blob-013 | ca_blobs_fs/src/fileio.cpp | `pull_at_once` ignores decode failures, exposes uninitialized tail | behaviour |
| api-008 | protocols/ext.cpp | `expand_command_line` unbounded `%`-expansion strcpy + off-by-one | security |
| api-010 | protocols/server.cpp | server (rsh): `tcp_read` int return truncated into `unsigned char` | behaviour |
| api-019 | cvsapi/Zeroconf.cpp | SRV parse: `size_t` underflow in `resize(i-1)` on crafted mDNS name | behaviour |
| blob-021 | keyValueServer/sample/cafs_client.cpp | sample PUSHFILE computes the wrong chunk size | sample |
| blob-022 | keyValueServer/sample/sample_server.cpp | `sample_server.cpp` does not compile | sample |

## Recommended next iteration, in priority order

1. **The >2 GB/4 GB size-truncation cluster (core-011, core-013, core-015,
   blob-010).** This directly undermines the fork's purpose (large binaries).
   It is one coordinated change: widen the size type to 64-bit on both sides
   and fix the `%lu`/`atoi` formatting, plus `buf_chain_length`/`buf_length`
   which return `int`. Needs a >2 GB round-trip test.
2. **Network/pre-auth memory safety (api-009, api-018, api-022, blob-002,
   api-001/003, api-014/015).** Remotely reachable; fix the copy-paste twins
   together and add a malformed-input regression test each.
3. **Blob-reference correctness (core-014, blob-006, blob-012).** In the
   flagship binary path.
4. **The remaining behaviour fixes**, each with a test written first.

Sample-code issues (blob-021, blob-022) can be fixed opportunistically or the
samples dropped from the build.
