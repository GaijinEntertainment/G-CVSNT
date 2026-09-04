/*
 * Server-side access log: one record per file a client reads or writes.
 *
 * Records are buffered per server process and appended to the log file with
 * an O_APPEND write at the end of each command (and from server_cleanup on a
 * normal exit), so a checkout costs one open/write/close instead of one per
 * file. A command that buffers more than a few MB flushes early, so a large
 * command may write more than once; each write ends on a line boundary.
 *
 * $op is "read" or "write"; $kind says what happened to the file:
 *   read:  updated, meta, patched, rcsdiff, merged, blobref (file sent to
 *          the client), removed (client told to delete it), diff and rdiff
 *          (one record per revision read), annotate, log
 *   write: add, modify, remove (commit), import, tag, branch, untag,
 *          edit, unedit, admin
 *
 * Configured in CVSROOT/config:
 *   AccessLog=yes                       enable (off by default)
 *   AccessLogPath=/var/log/cvs/access-%Y%m%d.jsonl
 *                                       strftime expanded at each flush;
 *                                       default CVSROOT/access-%Y%m%d.jsonl
 *   AccessLogFormat={"ts":"$ts",...}    $name placeholders, $$ is a literal $;
 *                                       default is a JSON object per line
 *   AccessLogEscape=json|none           escape field values for JSON (default)
 *
 * Fields: $ts (UTC, ISO 8601), $ts_unix, $sid, $user, $host, $cmd, $root,
 * $server, $pid, $op, $kind, $dir, $file, $path, $rev, $tag, $hash, $size.
 * $dir and $path are repository-relative (from the RCS path, not the client
 * working directory); $dir is $path without the file name. Invalid UTF-8 bytes
 * in a name are percent-encoded (%XX, a literal % as %25) so the JSON stays
 * valid UTF-8 and two distinct names never collapse to one identity.
 */

#ifndef ACCESS_LOG_H
#define ACCESS_LOG_H

#include <stddef.h>

extern int access_log_enabled;

/* CVSROOT/config setters (parseinfo.cpp). The two returning int give 0 for
   an unrecognised value. Values are copied with trailing whitespace and
   CR stripped, since the config parser hands over the raw line. */
int access_log_set_enabled (const char *value);
void access_log_set_path (const char *value);
void access_log_set_format (const char *value);
int access_log_set_escape (const char *value);

/* Record that the client accessed FILE in REPOSITORY (client side
   UPDATE_DIR). OP is "read" or "write", KIND the detail (see above). REV,
   TAG and HASH may be NULL; SIZE is the byte count sent, 0 when nothing
   was. Only the server writes records; a no-op in local mode. */
void access_log_file (const char *op, const char *kind, const char *update_dir,
		      const char *repository, const char *file, const char *rev,
		      const char *tag, const char *hash, size_t size);

/* Append buffered records to the log file. Cheap when nothing is buffered. */
void access_log_flush (void);

#endif /* ACCESS_LOG_H */
