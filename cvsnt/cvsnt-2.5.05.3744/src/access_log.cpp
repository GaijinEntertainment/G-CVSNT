/*
 * Server-side access log: records every file a client reads from or
 * writes to the repository (checkout, update, export, diff, annotate,
 * log, commit, import, tag, edit, admin), with who, from where and what,
 * so repository access can be audited and shipped to a log collector.
 * One record per file, formatted from a configurable template (JSON
 * lines by default).
 *
 * Records are buffered in memory for the whole command and appended to the
 * log file with a single O_APPEND write (FILE_APPEND_DATA on Windows) at
 * command end and from server_cleanup. Each flush ends on a line boundary,
 * so records from concurrent server processes never interleave inside a
 * line and no lock is needed on a local filesystem. See access_log.h for
 * the configuration surface.
 */

#include "cvs.h"
#include "access_log.h"

#include <string>
#include <vector>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/file.h>
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

int access_log_enabled = 0;
static char *access_log_path = NULL;
static char *access_log_format = NULL;
static int access_log_escape_json = 1;

#define ACCESS_LOG_DEFAULT_FILENAME "access-%Y%m%d.jsonl"
#define ACCESS_LOG_DEFAULT_FORMAT \
	"{\"ts\":\"$ts\",\"sid\":\"$sid\",\"user\":\"$user\",\"host\":\"$host\"," \
	"\"cmd\":\"$cmd\",\"root\":\"$root\",\"op\":\"$op\",\"kind\":\"$kind\"," \
	"\"path\":\"$path\",\"rev\":\"$rev\",\"tag\":\"$tag\",\"hash\":\"$hash\",\"size\":$size}"

/* Flush early if the buffer grows past this; the normal flush is at
   command end. A record is a few hundred bytes, so this is many thousand
   files per write. */
static const size_t ACCESS_LOG_FLUSH_BYTES = 4 * 1024 * 1024;

namespace {

enum field_t
{
	F_LITERAL,
	/* Constant for a session: baked into the literals at compile time. */
	F_SID, F_USER, F_HOST, F_CMD, F_ROOT, F_SERVER, F_PID,
	/* Per record. */
	F_TS, F_TS_UNIX, F_OP, F_KIND, F_DIR, F_FILE, F_PATH, F_REV, F_TAG, F_HASH, F_SIZE
};

struct field_name_t
{
	const char *name;
	field_t field;
};

static const field_name_t field_names[] =
{
	{ "sid", F_SID }, { "user", F_USER }, { "host", F_HOST }, { "cmd", F_CMD },
	{ "root", F_ROOT }, { "server", F_SERVER }, { "pid", F_PID },
	{ "ts", F_TS }, { "ts_unix", F_TS_UNIX }, { "op", F_OP }, { "kind", F_KIND },
	{ "dir", F_DIR }, { "file", F_FILE }, { "path", F_PATH },
	{ "rev", F_REV }, { "tag", F_TAG }, { "hash", F_HASH }, { "size", F_SIZE },
};

struct segment_t
{
	field_t field;
	std::string text;	/* F_LITERAL only */
};

struct access_log_state_t
{
	std::vector<segment_t> segments;
	bool compiled;
	const char *compiled_for_cmd;	/* recompile when the command changes */
	std::string buffer;
	std::string ts_cache;
	time_t ts_cache_time;
	bool write_failed;		/* warn once per session, then drop */
	bool dirty_line;		/* a short write left a partial line on disk */

	access_log_state_t() : compiled(false), compiled_for_cmd(NULL), ts_cache_time(0), write_failed(false), dirty_line(false) {}
};

static access_log_state_t *state = NULL;

static access_log_state_t *get_state ()
{
	if (!state)
	{
		state = new access_log_state_t;
		state->buffer.reserve (64 * 1024);
	}
	return state;
}

static char *copy_stripped (const char *value)
{
	size_t len = strlen (value);
	while (len && (isspace ((unsigned char) value[len - 1]) || value[len - 1] == '\r'))
		len--;
	char *copy = (char *) xmalloc (len + 1);
	memcpy (copy, value, len);
	copy[len] = '\0';
	return copy;
}

/* Length of the well-formed UTF-8 sequence at s (1..4), or 0 if the lead byte
   begins no valid sequence. Rejects overlong forms, surrogates and code points
   above U+10FFFF, and is bounds-safe: a continuation byte that is the string's
   NUL terminator fails the 0x80..0xBF test and short-circuits before the next
   byte is read. */
static int utf8_sequence_length (const unsigned char *s)
{
	unsigned char c = s[0];
	if (c < 0x80) return 1;
	if (c < 0xC2) return 0;                 /* 0x80..0xC1: continuation or overlong */
	if (c < 0xE0)                           /* 0xC2..0xDF: two bytes */
		return ((s[1] & 0xC0) == 0x80) ? 2 : 0;
	if (c < 0xF0)                           /* 0xE0..0xEF: three bytes */
	{
		unsigned char b1 = s[1];
		if ((b1 & 0xC0) != 0x80) return 0;
		if (c == 0xE0 && b1 < 0xA0) return 0;   /* overlong */
		if (c == 0xED && b1 > 0x9F) return 0;   /* surrogate half */
		return ((s[2] & 0xC0) == 0x80) ? 3 : 0;
	}
	if (c < 0xF5)                           /* 0xF0..0xF4: four bytes */
	{
		unsigned char b1 = s[1];
		if ((b1 & 0xC0) != 0x80) return 0;
		if (c == 0xF0 && b1 < 0x90) return 0;   /* overlong */
		if (c == 0xF4 && b1 > 0x8F) return 0;   /* above U+10FFFF */
		return ((s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) ? 4 : 0;
	}
	return 0;                               /* 0xF5..0xFF */
}

static void append_escaped (std::string &out, const char *s)
{
	if (!s)
		return;
	if (!access_log_escape_json)
	{
		out += s;
		return;
	}
	while (*s)
	{
		unsigned char c = (unsigned char) *s;
		switch (c)
		{
		case '%':  out += "%25"; ++s; continue;
		case '"':  out += "\\\""; ++s; continue;
		case '\\': out += "\\\\"; ++s; continue;
		case '\n': out += "\\n"; ++s; continue;
		case '\r': out += "\\r"; ++s; continue;
		case '\t': out += "\\t"; ++s; continue;
		}
		if (c < 0x20)
		{
			char buf[8];
			sprintf (buf, "\\u%04x", c);
			out += buf;
			++s;
			continue;
		}
		if (c < 0x80)
		{
			out += (char) c;
			++s;
			continue;
		}
		/* A file name is bytes, not text; a client may send invalid UTF-8.
		   Pass a well-formed multi-byte sequence through unchanged, and replace
		   an invalid lead byte with U+FFFD so the JSON line stays valid UTF-8. */
		int n = utf8_sequence_length ((const unsigned char *) s);
		if (n > 0)
		{
			out.append (s, n);
			s += n;
		}
		else
		{
			/* Not valid UTF-8: percent-encode the byte (%XX). Distinct invalid
			   bytes stay distinct, and because '%' itself is percent-encoded
			   (above) the mapping is reversible and cannot collide with valid
			   UTF-8 that decodes to the same code point. */
			char buf[4];
			sprintf (buf, "%%%02x", c);
			out += buf;
			++s;
		}
	}
}

static void append_literal (std::vector<segment_t> &segments, const std::string &text)
{
	if (text.empty ())
		return;
	if (!segments.empty () && segments.back ().field == F_LITERAL)
		segments.back ().text += text;
	else
	{
		segment_t seg;
		seg.field = F_LITERAL;
		seg.text = text;
		segments.push_back (seg);
	}
}

static const char *session_field (field_t field, std::string &scratch)
{
	switch (field)
	{
	case F_SID:
		return global_session_id;
	case F_USER:
		return getcaller ();
	case F_HOST:
		return remote_host_name ? remote_host_name : "";
	case F_CMD:
		return server_command_name ? server_command_name : (command_name ? command_name : "");
	case F_ROOT:
		return current_parsed_root ? current_parsed_root->directory : "";
	case F_SERVER:
		return hostname;
	case F_PID:
	{
		char buf[32];
		sprintf (buf, "%ld", (long) getpid ());
		scratch = buf;
		return scratch.c_str ();
	}
	default:
		return "";
	}
}

/* Turn the template into a segment list, resolving session-constant
   fields into literal text so the per-record loop only touches the
   fields that actually vary. */
static void compile_format (access_log_state_t *st)
{
	const char *fmt = access_log_format ? access_log_format : ACCESS_LOG_DEFAULT_FORMAT;
	std::vector<segment_t> segments;
	std::string literal;
	std::string scratch;

	for (const char *p = fmt; *p; )
	{
		if (*p != '$')
		{
			literal += *p++;
			continue;
		}
		p++;
		if (*p == '$')
		{
			literal += '$';
			p++;
			continue;
		}
		const char *start = p;
		while (isalnum ((unsigned char) *p) || *p == '_')
			p++;
		size_t name_len = p - start;
		field_t field = F_LITERAL;
		for (size_t i = 0; i < sizeof (field_names) / sizeof (field_names[0]); i++)
		{
			if (strlen (field_names[i].name) == name_len && !strncmp (field_names[i].name, start, name_len))
			{
				field = field_names[i].field;
				break;
			}
		}
		if (field == F_LITERAL)
		{
			/* Unknown name: keep it verbatim so the mistake is visible in the log. */
			if (!st->compiled)
			{
				std::string name (start, name_len);
				error (0, 0, "warning: unknown field $%s in AccessLogFormat", name.c_str ());
			}
			literal += '$';
			literal.append (start, name_len);
		}
		else if (field <= F_PID)
		{
			std::string value;
			append_escaped (value, session_field (field, scratch));
			literal += value;
		}
		else
		{
			append_literal (segments, literal);
			literal.clear ();
			segment_t seg;
			seg.field = field;
			segments.push_back (seg);
		}
	}
	append_literal (segments, literal);

	st->segments.swap (segments);
	st->compiled = true;
	st->compiled_for_cmd = server_command_name;
}

static const char *timestamp (access_log_state_t *st, time_t now)
{
	if (now != st->ts_cache_time || st->ts_cache.empty ())
	{
		char buf[32];
		struct tm *tm = gmtime (&now);
		if (tm && strftime (buf, sizeof (buf), "%Y-%m-%dT%H:%M:%SZ", tm))
			st->ts_cache = buf;
		else
			st->ts_cache.clear ();
		st->ts_cache_time = now;
	}
	return st->ts_cache.c_str ();
}

static void expand_path (std::string &out)
{
	std::string base;
	const char *path = access_log_path;
	if (!path || !*path)
	{
		base = current_parsed_root->directory;
		base += "/";
		base += CVSROOTADM;
		base += "/";
		base += ACCESS_LOG_DEFAULT_FILENAME;
		path = base.c_str ();
	}
	if (!strchr (path, '%'))
	{
		out = path;
		return;
	}
	char buf[4096];
	time_t now = time (NULL);
	struct tm *tm = localtime (&now);
	if (tm && strftime (buf, sizeof (buf), path, tm))
		out = buf;
	else
		out = path;
}

/* One open, one write, one close. See the note on concurrency at the top. */
/* Append the whole buffer in a SINGLE write, so one record never spans two
   O_APPEND writes that a concurrent server could interleave between. Returns the
   number of bytes written (0..len), or -1 if the file could not be opened; sets
   *closed_ok to whether close reported success. A return < len is an incomplete
   flush the caller must not paper over by re-writing the same bytes. */
static long append_to_file (const char *path, const char *data, size_t len, bool *closed_ok)
{
	*closed_ok = true;
#ifdef _WIN32
	HANDLE h = CreateFileA (path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
				OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE)
	{
		errno = EIO;   /* GetLastError carries the Win32 detail; errno stays generic */
		return -1;
	}
	DWORD written = 0;
	if (!WriteFile (h, data, (DWORD) len, &written, NULL))
		errno = EIO;
	if (!CloseHandle (h))
	{
		*closed_ok = false;
		if (written == len)
			errno = EIO;   /* don't leave a stale errno on a close-only failure */
	}
	return (long) written;
#else
	int fd = open (path, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
	if (fd < 0)
		return -1;
	/* Serialize whole-buffer appends across server processes so no other
	   process can write between a short write and any retry -- records from
	   two processes never interleave inside a line. */
	while (flock (fd, LOCK_EX) < 0 && errno == EINTR)
		;
	ssize_t n;
	do
		n = write (fd, data, len);
	while (n < 0 && errno == EINTR);   /* EINTR on a regular file wrote nothing */
	/* n < 0 here is a write error with nothing written; return it as -1 so the
	   caller retains the buffer for retry, distinct from a short write (0..len)
	   whose bytes did reach the file. */
	int werrno = errno;
	flock (fd, LOCK_UN);
	if (close (fd) != 0)
	{
		*closed_ok = false;
		if (n >= 0)
			werrno = errno;   /* a real write error outranks a close error */
	}
	errno = werrno;
	return (long) n;
#endif
}

} /* anonymous namespace */

int access_log_set_enabled (const char *value)
{
	char *v = copy_stripped (value);
	int r = 1;
	value = v;
	if (!strcasecmp (value, "yes") || !strcasecmp (value, "on") || !strcasecmp (value, "true") || !strcmp (value, "1"))
		access_log_enabled = 1;
	else if (!strcasecmp (value, "no") || !strcasecmp (value, "off") || !strcasecmp (value, "false") || !strcmp (value, "0"))
		access_log_enabled = 0;
	else
		r = 0;
	xfree (v);
	return r;
}

void access_log_set_path (const char *value)
{
	xfree (access_log_path);
	access_log_path = copy_stripped (value);
}

void access_log_set_format (const char *value)
{
	xfree (access_log_format);
	access_log_format = copy_stripped (value);
	if (state)
		state->compiled = false;
}

int access_log_set_escape (const char *value)
{
	char *v = copy_stripped (value);
	int r = 1;
	if (!strcasecmp (v, "json"))
		access_log_escape_json = 1;
	else if (!strcasecmp (v, "none"))
		access_log_escape_json = 0;
	else
		r = 0;
	xfree (v);
	return r;
}

void access_log_file (const char *op, const char *kind, const char *update_dir,
		      const char *repository, const char *file, const char *rev,
		      const char *tag, const char *hash, size_t size)
{
	if (!access_log_enabled || !server_active || noexec || !current_parsed_root)
		return;

	access_log_state_t *st = get_state ();
	if (!st->compiled || st->compiled_for_cmd != server_command_name)
		compile_format (st);

	const char *dir = repository ? Short_Repository (repository) : "";
	const time_t now = time (NULL);   /* one timestamp per record for $ts and $ts_unix */
	std::string &out = st->buffer;

	for (std::vector<segment_t>::const_iterator seg = st->segments.begin (); seg != st->segments.end (); ++seg)
	{
		switch (seg->field)
		{
		case F_LITERAL:
			out += seg->text;
			break;
		case F_TS:
			out += timestamp (st, now);
			break;
		case F_TS_UNIX:
		{
			char buf[32];
			sprintf (buf, "%ld", (long) now);
			out += buf;
			break;
		}
		case F_OP:
			append_escaped (out, op);
			break;
		case F_KIND:
			append_escaped (out, kind);
			break;
		case F_DIR:
			append_escaped (out, dir);
			break;
		case F_FILE:
			append_escaped (out, file);
			break;
		case F_PATH:
			if (dir && *dir)
			{
				append_escaped (out, dir);
				out += '/';
			}
			append_escaped (out, file);
			break;
		case F_REV:
			append_escaped (out, rev);
			break;
		case F_TAG:
			append_escaped (out, tag);
			break;
		case F_HASH:
			append_escaped (out, hash);
			break;
		case F_SIZE:
		{
			char buf[32];
			sprintf (buf, "%lu", (unsigned long) size);
			out += buf;
			break;
		}
		default:
			break;
		}
	}
	out += '\n';

	if (out.size () >= ACCESS_LOG_FLUSH_BYTES)
	{
		if (state->write_failed)
			out.clear ();   /* cannot write; bound memory by dropping (already warned) */
		else
			access_log_flush ();
	}
}

void access_log_flush (void)
{
	if (!state || state->buffer.empty ())
		return;
	if (!current_parsed_root && (!access_log_path || !*access_log_path))
	{
		/* Nowhere to derive the default path from any more. */
		state->buffer.clear ();
		return;
	}

	std::string path;
	expand_path (path);

	TRACE (3, "access_log_flush(%s, %lu bytes)", path.c_str (), (unsigned long) state->buffer.size ());

	/* A previous short write left the file ending mid-line; start this write
	   with a newline so that partial record becomes its own isolated line
	   instead of merging with the records that follow. */
	std::string payload;
	if (state->dirty_line)
		payload = "\n";
	payload += state->buffer;

	bool closed_ok = true;
	long n = append_to_file (path.c_str (), payload.data (), payload.size (), &closed_ok);

	if (n == (long) payload.size ())
	{
		/* The whole payload reached the file (it ends on a newline, so no
		   dirty_line). A close failure means the final store may not have
		   committed, but the bytes are written, so retrying would duplicate;
		   clear the buffer and only warn. */
		state->buffer.clear ();
		state->dirty_line = false;
		if (closed_ok)
		{
			state->write_failed = false;   /* fully clean */
			return;
		}
	}

	/* Logging must never fail the command; warn once per failure streak. */
	if (!state->write_failed)
	{
		state->write_failed = true;
		error (0, errno, "warning: incomplete access log write to %s", fn_root (path.c_str ()));
		/* error() reaches only the CVS client in server mode; also record it
		   where a server operator can see it. */
		CServerIo::log (CServerIo::logError, "incomplete access log write to %s: %s",
				path.c_str (), strerror (errno));
	}

	if (n == (long) payload.size ())
		return;             /* full write, close failed: handled above */
	if (n < 0)
		return;             /* open or write error: nothing written, keep the buffer */
	/* 0 <= n < size: a short write left a partial line on disk. */
	state->dirty_line = true;
	state->buffer.clear ();
}
