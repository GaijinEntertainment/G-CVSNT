/* JT thinks BeOS is worth the trouble. */
/* It wasn't... */

/* CVS client-related stuff.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#ifdef HAVE_CONFIG_H
#define MAIN_CVS
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "../version.h"

#include "cvs.h"
#include "getline.h"
#include "edit.h"
#include "buffer.h"

#ifdef MAC_HFS_STUFF
#	include "mac_hfs_stuff.h"
#endif

int client_overwrite_existing;
int client_max_dotdot;
int is_cvsnt = 1;

// Hack to reformat the output of '-n co' to ls format for old servers.
extern unsigned global_ls_response_hack;

/* The protocol layer that the client is connected to */
const struct protocol_interface *client_protocol;

static void add_prune_candidate (const char *);

/* All the commands.  */
int add (int argc, char **argv);
int admin (int argc, char **argv);
int checkout (int argc, char **argv);
int commit (int argc, char **argv);
int diff (int argc, char **argv);
int history (int argc, char **argv);
int import (int argc, char **argv);
int cvslog (int argc, char **argv);
int patch (int argc, char **argv);
int release (int argc, char **argv);
int cvsremove (int argc, char **argv);
int rtag (int argc, char **argv);
int status (int argc, char **argv);
int tag (int argc, char **argv);
int update (int argc, char **argv);

/* All the response handling functions.  */
static void handle_ok(char *, int);
static void handle_error(char *, int);
static void handle_valid_requests(char *, int);
static void handle_checked_in(char *, int);
static void handle_new_entry(char *, int);
static void handle_checksum(char *, int);
static void handle_copy_file(char *, int);
static void handle_updated(char *, int);
static void handle_merged(char *, int);
static void handle_patched(char *, int);
static void handle_rcs_diff(char *, int);
static void handle_removed(char *, int);
static void handle_renamed(char *, int);
static void handle_remove_entry(char *, int);
static void handle_set_static_directory(char *, int);
static void handle_clear_static_directory(char *, int);
static void handle_set_sticky(char *, int);
static void handle_clear_sticky(char *, int);
static void handle_module_expansion(char *, int);
static void handle_wrapper_rcs_option(char *, int);
static void handle_entries_extra(char *, int);
static void handle_m(char *, int);
static void handle_e(char *, int);
static void handle_f(char *, int);
static void handle_notified(char *, int);

/* We need to keep track of the list of directories we've sent to the
   server.  This list, along with the current CVSROOT, will help us
   decide which command-line arguments to send.  */
List *dirs_sent_to_server = NULL;

static char *extra_entry;

static char *server_codepage;
static const char *client_codepage;
static int server_codepage_translation;

static void send_filename(const char *fn, bool linefeed, bool slash)
{
	char *p,*q;
	char *fncpy = xstrdup(fn);

	for(p=fncpy,q=fncpy; *p; p++)
	{
		// There's an assumption here that directory seperator doesn't occur in a multibyte sequence.  This
		// is true for most character sets.
		if(ISDIRSEP(*p) && slash)
			*p='/';
		// Same assumption as above
		if(*p=='\n')
		{
			if(linefeed)
			{
				if(p>q) send_to_server(q, p-q);
				send_to_server("\nArgumentx ",0);
				q=p+1;
			}
			else
				*p=' ';
		}
	}
	if(p>q) send_to_server(q, p-q);
	xfree(fncpy);
}

static int is_arg_a_parent_or_listed_dir (Node *n, void *d)
{
    char *directory = n->key;	/* name of the dir sent to server */
    char *this_argv_elem = (char *) d;	/* this argv element */

    /* Say we should send this argument if the argument matches the
       beginning of a directory name sent to the server.  This way,
       the server will know to start at the top of that directory
       hierarchy and descend. */

    if (strncmp (directory, this_argv_elem, strlen (this_argv_elem)) == 0)
	return 1;

    return 0;
}

/* Return nonzero if this argument should not be sent to the
   server. */

static int arg_should_not_be_sent_to_server (char *arg)
{
    /* Decide if we should send this directory name to the server.  We
       should always send argv[i] if:

       1) the list of directories sent to the server is empty (as it
       will be for checkout, etc.).

       2) the argument is "."

       3) the argument is a file in the cwd and the cwd is checked out
       from the current root

       4) the argument lies within one of the paths in
       dirs_sent_to_server.

       */

    if (list_isempty (dirs_sent_to_server))
	return 0;		/* always send it */

    if (strcmp (arg, ".") == 0)
	return 0;		/* always send it */

    /* We should send arg if it is one of the directories sent to the
       server or the parent of one; this tells the server to descend
       the hierarchy starting at this level. */
    if (isdir (arg))
    {
	if (walklist (dirs_sent_to_server, is_arg_a_parent_or_listed_dir, arg))
	    return 0;

	/* If arg wasn't a parent, we don't know anything about it (we
	   would have seen something related to it during the
	   send_files phase).  Don't send it.  */
	return 1;
    }

    /* Try to decide whether we should send arg to the server by
       checking the contents of the corresponding CVSADM directory. */
    {
	char *t, *this_root;

	/* Calculate "dirname arg" */
	for (t = arg + strlen (arg) - 1; t >= arg; t--)
	{
	    if (ISDIRSEP(*t))
		break;
	}

	/* Now we're either poiting to the beginning of the
	   string, or we found a path separator. */
	if (t >= arg)
	{
	    /* Found a path separator.  */
	    char c = *t;
	    *t = '\0';
	    
	    /* First, check to see if we sent this directory to the
               server, because it takes less time than actually
               opening the stuff in the CVSADM directory.  */
	    if (walklist (dirs_sent_to_server, is_arg_a_parent_or_listed_dir,
			  arg))
	    {
		*t = c;		/* make sure to un-truncate the arg */
		return 0;
	    }

	    /* Since we didn't find it in the list, check the CVSADM
               files on disk.  */
	    this_root = Name_Root (arg, (char *) NULL);
	    *t = c;
	}
	else
	{
	    /* We're at the beginning of the string.  Look at the
               CVSADM files in cwd.  */
	    this_root = Name_Root ((char *) NULL, (char *) NULL);
	}

	/* Now check the value for root. */
	if (this_root && current_parsed_root
	    && (strcmp (this_root, current_parsed_root->original) != 0))
	{
	    /* Don't send this, since the CVSROOTs don't match. */
	    xfree (this_root);
	    return 1;
	}
	xfree (this_root);
    }
    
    /* OK, let's send it. */
    return 0;
}

/* Shared with server.  */

/*
 * Return a malloc'd, '\0'-terminated string
 * corresponding to the mode in SB.
 */
char *
mode_to_string (mode_t mode)
{
    char buf[18], u[4], g[4], o[4];
    int i;

    i = 0;
    if (mode & S_IRUSR) u[i++] = 'r';
    if (mode & S_IWUSR) u[i++] = 'w';
    if (mode & S_IXUSR) u[i++] = 'x';
    u[i] = '\0';

    i = 0;
    if (mode & S_IRGRP) g[i++] = 'r';
    if (mode & S_IWGRP) g[i++] = 'w';
    if (mode & S_IXGRP) g[i++] = 'x';
    g[i] = '\0';

    i = 0;
    if (mode & S_IROTH) o[i++] = 'r';
    if (mode & S_IWOTH) o[i++] = 'w';
    if (mode & S_IXOTH) o[i++] = 'x';
    o[i] = '\0';

    sprintf(buf, "u=%s,g=%s,o=%s", u, g, o);
    return xstrdup(buf);
}

/*
 * Change mode of FILENAME to MODE_STRING.
 * Returns 0 for success or errno code.
 * If RESPECT_UMASK is set, then honor the umask.
 */
int change_mode (char *filename, char *mode_string, int respect_umask)
{
#ifdef CHMOD_BROKEN
    char *p;
    int writeable = 0;

    TRACE(3,"change_mode (%s,%s,%d)",PATCH_NULL(filename),PATCH_NULL(mode_string),respect_umask);

    /* We can only distinguish between
         1) readable
         2) writeable
         3) Picasso's "Blue Period"
       We handle the first two. */
    p = mode_string;
    while (*p != '\0')
    {
	if ((p[0] == 'u' || p[0] == 'g' || p[0] == 'o') && p[1] == '=')
	{
	    char *q = p + 2;
	    while (*q != ',' && *q != '\0')
	    {
		if (*q == 'w')
		    writeable = 1;
		++q;
	    }
	}
	/* Skip to the next field.  */
	while (*p != ',' && *p != '\0')
	    ++p;
	if (*p == ',')
	    ++p;
    }

    /* xchmod honors the umask for us.  In the !respect_umask case, we
       don't try to cope with it (probably to handle that well, the server
       needs to deal with modes in data structures, rather than via the
       modes in temporary files).  */
    xchmod (filename, writeable);
	return 0;

#else /* ! CHMOD_BROKEN */

    char *p;
    mode_t mode = 0;
    mode_t oumask;

    TRACE(3,"change_mode (%s,%s,%d)",PATCH_NULL(filename),PATCH_NULL(mode_string),respect_umask);

    p = mode_string;
    while (*p != '\0')
    {
	if ((p[0] == 'u' || p[0] == 'g' || p[0] == 'o') && p[1] == '=')
	{
	    int can_read = 0, can_write = 0, can_execute = 0;
	    char *q = p + 2;
	    while (*q != ',' && *q != '\0')
	    {
		if (*q == 'r')
		    can_read = 1;
		else if (*q == 'w')
		    can_write = 1;
		else if (*q == 'x')
		    can_execute = 1;
		++q;
	    }
	    if (p[0] == 'u')
	    {
		if (can_read)
		    mode |= S_IRUSR;
		if (can_write)
		    mode |= S_IWUSR;
		if (can_execute)
		    mode |= S_IXUSR;
	    }
	    else if (p[0] == 'g')
	    {
		if (can_read)
		    mode |= S_IRGRP;
		if (can_write)
		    mode |= S_IWGRP;
		if (can_execute)
		    mode |= S_IXGRP;
	    }
	    else if (p[0] == 'o')
	    {
		if (can_read)
		    mode |= S_IROTH;
		if (can_write)
		    mode |= S_IWOTH;
		if (can_execute)
		    mode |= S_IXOTH;
	    }
	}
	/* Skip to the next field.  */
	while (*p != ',' && *p != '\0')
	    ++p;
	if (*p == ',')
	    ++p;
    }

    if (respect_umask)
    {
	oumask = umask (0);
	(void) umask (oumask);
	mode &= ~oumask;
    }

    if (chmod (filename, mode & 0777) < 0)
	return errno;
    return 0;
#endif /* ! CHMOD_BROKEN */
}

int client_prune_dirs;

static List *ignlist = (List *) NULL;

/* Buffer to write to the server.  */
static struct buffer *to_server = NULL;

/* Buffer used to read from the server.  */
static struct buffer *from_server = NULL;

/* We want to be able to log data sent between us and the server.  We
   do it using log buffers.  Each log buffer has another buffer which
   handles the actual I/O, and a file to log information to.

   This structure is the closure field of a log buffer.  */

struct log_buffer
{
    /* The underlying buffer.  */
    struct buffer *buf;
    /* The file to log information to.  */
    FILE *log;
};

struct buffer *log_buffer_initialize(struct buffer *, FILE *, int, void (*) (struct buffer *));
static int log_buffer_input (void *, char *, int, int, int *);
static int log_buffer_output (void *, const char *, int, int *);
static int log_buffer_flush (void *);
static int log_buffer_block (void *, int);
static int log_buffer_shutdown (void *);

/* Create a log buffer.  */

struct buffer *log_buffer_initialize (struct buffer *buf, FILE *fp, int input, void (*memory) (struct buffer *))
{
    struct log_buffer *n;

    n = (struct log_buffer *) xmalloc (sizeof *n);
    n->buf = buf;
    n->log = fp;
    return buf_initialize (input ? log_buffer_input : NULL,
			   input ? NULL : log_buffer_output,
			   input ? NULL : log_buffer_flush,
			   log_buffer_block,
			   log_buffer_shutdown,
			   memory,
			   n);
}

/* The input function for a log buffer.  */

static int log_buffer_input (void *closure, char *data, int need, int size, int *got)
{
    struct log_buffer *lb = (struct log_buffer *) closure;
    int status;
    size_t n_to_write;

    if (lb->buf->input == NULL)
	abort ();

    status = (*lb->buf->input) (lb->buf->closure, data, need, size, got);
    if (status != 0)
	return status;

    if (*got > 0)
    {
	n_to_write = *got;
	if (fwrite (data, 1, n_to_write, lb->log) != n_to_write)
	    error (0, errno, "writing to log file");
    }

    return 0;
}

/* The output function for a log buffer.  */

static int log_buffer_output (void *closure, const char *data, int have, int *wrote)
{
    struct log_buffer *lb = (struct log_buffer *) closure;
    int status;
    size_t n_to_write;

    if (lb->buf->output == NULL)
	abort ();

    status = (*lb->buf->output) (lb->buf->closure, data, have, wrote);
    if (status != 0)
	return status;

    if (*wrote > 0)
    {
	n_to_write = *wrote;
	if (fwrite (data, 1, n_to_write, lb->log) != n_to_write)
	    error (0, errno, "writing to log file");
    }

    return 0;
}

/* The flush function for a log buffer.  */

static int log_buffer_flush (void *closure)
{
    struct log_buffer *lb = (struct log_buffer *) closure;

    if (lb->buf->flush == NULL)
	abort ();

    /* We don't really have to flush the log file here, but doing it
       will let tail -f on the log file show what is sent to the
       network as it is sent.  */
    if (fflush (lb->log) != 0)
        error (0, errno, "flushing log file");

    return (*lb->buf->flush) (lb->buf->closure);
}

/* The block function for a log buffer.  */

static int log_buffer_block (void *closure, int block)
{
    struct log_buffer *lb = (struct log_buffer *) closure;

    if (block)
	return set_block (lb->buf);
    else
	return set_nonblock (lb->buf);
}

/* The shutdown function for a log buffer.  */

static int log_buffer_shutdown (void *closure)
{
    struct log_buffer *lb = (struct log_buffer *) closure;
    int retval;

    retval = buf_shutdown (lb->buf);
    if (fclose (lb->log) < 0)
	error (0, errno, "closing log file");
    return retval;
}

struct client_buffer
{
	const struct protocol_interface *protocol;
};

static struct buffer *client_buffer_initialize(const struct protocol_interface *, int, void (*) (struct buffer *));
static int client_buffer_input (void *, char *, int, int, int *);
static int client_buffer_output (void *, const char *, int, int *);
static int client_buffer_flush (void *);

/* Create a buffer based on a socket.  */

static struct buffer *client_buffer_initialize (const struct protocol_interface *protocol, int input, void (*memory)(struct buffer *))
{
    struct client_buffer *n;

    n = (struct client_buffer *) xmalloc (sizeof *n);
    n->protocol = protocol;
    return buf_initialize (input ? client_buffer_input : NULL,
			   input ? NULL : client_buffer_output,
			   input ? NULL : client_buffer_flush,
			   (int (*) (void *, int)) NULL,
			   (int (*) (void *)) NULL,
			   memory,
			   n);
}

/* The buffer input function */

static int client_buffer_input (void *closure, char *data, int need, int size, int *got)
{
    struct client_buffer *sb = (struct client_buffer *) closure;
	int nbytes;

	*got = 0;

	do
	{
		nbytes = sb->protocol->read_data(sb->protocol, data, size);
		if (nbytes < 0)
			error (1, 0, "reading from server: error %d", nbytes);
		if (nbytes == 0)
		{
			/* End of file (for example, the server has closed
			   the connection).  If we've already read something, we
			   just tell the caller about the data, not about the end of
			   file.  If we've read nothing, we return end of file.  */
			if (*got == 0)
				return -1;
			else
				return 0;
		}
		need -= nbytes;
		size -= nbytes;
		data += nbytes;
		*got += nbytes;
	}
	while (need > 0);

    return 0;
}

/* The buffer output function for a buffer built on a socket.  */

static int client_buffer_output (void *closure, const char *data, int have, int *wrote)
{
    struct client_buffer *sb = (struct client_buffer *) closure;

    *wrote = have;

    while (have > 0)
    {
		int nbytes;

		nbytes = sb->protocol->write_data(sb->protocol, data, have);
		if (nbytes < 0)
			error (1, errno, "writing to server socket: error %d", nbytes);

		have -= nbytes;
		data += nbytes;
    }

    return 0;
}

/* The buffer flush function for a buffer built on a socket.  */

/*ARGSUSED*/
static int client_buffer_flush (void *closure)
{
    struct client_buffer *sb = (struct client_buffer *) closure;
	return sb->protocol->flush_data(sb->protocol);
}

/*
 * Read a line from the server.  Result does not include the terminating \n.
 *
 * Space for the result is malloc'd and should be freed by the caller.
 *
 * Returns number of bytes read.
 */
int read_line (char **resultp)
{
    int status;
    char *result;
    int len;
    size_t slen;

    status = buf_flush (to_server, 1);
    if (status != 0)
		error (1, status, "writing to server");

    status = buf_read_line (from_server, &result, &len);
    if (status != 0)
    {
	if (status == -1)
	    error (1, 0, "end of file from server (consult above messages if any)");
	else if (status == -2)
	    error (1, 0, "out of memory in client.c");
	else
	    error (1, status, "reading from server");
    }

    if (resultp != NULL)
	{
		if(server_codepage_translation>0)
		{
			void *rslt;
			int ret = CCodepage::TranscodeBuffer(server_codepage,client_codepage,result,0,rslt,slen);
			*resultp=(char*)rslt;
			if(ret>0)
			{
				TRACE(3,"Translation from server codepage '%s' to client codepage '%s' lost characters",server_codepage, client_codepage);
			}
			if(ret<0)
			{
				TRACE(3,"Translation not posible - disabling");
				server_codepage_translation = 0;
			}
			xfree(result);
			len = (int)slen;
		}
		else
			*resultp = result;

	}
    else
		xfree (result);

    return len;
}

/*
 * Zero if compression isn't supported or requested; non-zero to indicate
 * a compression level to request from gzip.
 */
int gzip_level = -1;

/*
 * The Repository for the top level of this command (not necessarily
 * the CVSROOT, just the current directory at the time we do it).
 */
static char *toplevel_repos = NULL;

/* Working directory when we first started.  Note: we could speed things
   up on some systems by using savecwd.h here instead of just always
   storing a name.  */
char *toplevel_wd;

static void handle_ok (char *args, int len)
{
    return;
}

static void handle_error (char *args, int len)
{   
    /*
     * First there is a symbolic error code followed by a space, which
     * we ignore.
     */
    char *p = strchr (args, ' ');
    if (p == NULL)
    {
	error (0, 0, "invalid data from cvs server");
	return;
    }
    ++p;

    /* Next we print the text of the message from the server.  We
       probably should be prefixing it with "server error" or some
       such, because if it is something like "Out of memory", the
       current behavior doesn't say which machine is out of
       memory.  */

    len -= p - args;
	if(len>0)
	{
	    cvs_outerr(p, len);
		cvs_outerr("\n", 1);
	}
}

static void handle_valid_requests (char *args, int len)
{
    char *p = args;
    char *q;
    struct request *rq;
    do
    {
	q = strchr (p, ' ');
	if (q != NULL)
	    *q++ = '\0';
	for (rq = requests; rq->name != NULL; ++rq)
	{
	    if (strcmp (rq->name, p) == 0)
		break;
	}
	if (rq->name == NULL)
	    /*
	     * It is a request we have never heard of (and thus never
	     * will want to use).  So don't worry about it.
	     */
	    ;
	else
	{
	    if (rq->flags & RQ_ENABLEME)
	    {
		/*
		 * Server wants to know if we have this, to enable the
		 * feature.
		 */
		send_to_server (rq->name, 0);
                send_to_server ("\n", 0);
	    }
	    else
		rq->flags |= RQ_SUPPORTED;
	}
	p = q;
    } while (q != NULL);
    for (rq = requests; rq->name != NULL; ++rq)
    {
	if ((rq->flags & RQ_SUPPORTED)
	    || (rq->flags & RQ_ENABLEME))
	    continue;
	if (rq->flags & RQ_ESSENTIAL)
	    error (1, 0, "request `%s' not supported by server", rq->name);
    }
}

/* This variable holds the result of Entries_Open, so that we can
   close Entries_Close on it when we move on to a new directory, or
   when we finish.  */
static List *last_entries;

/*
 * Do all the processing for PATHNAME, where pathname consists of the
 * repository and the filename.  The parameters we pass to FUNC are:
 * DATA is just the DATA parameter which was passed to
 * call_in_directory; ENT_LIST is a pointer to an entries list (which
 * we manage the storage for); SHORT_PATHNAME is the pathname of the
 * file relative to the (overall) directory in which the command is
 * taking place; and FILENAME is the filename portion only of
 * SHORT_PATHNAME.  When we call FUNC, the curent directory points to
 * the directory portion of SHORT_PATHNAME.  */

static char *last_dir_name;

static void call_in_directory (char *pathname, void (*func)(char *data, List *ent_list, char *short_pathname, char *filename), char *data)
{
    char *dir_name;
    char *filename;
    /* This is what we get when we hook up the directory (working directory
       name) from PATHNAME with the filename from REPOSNAME.  For example:
       pathname: ccvs/src/
       reposname: /u/src/master/ccvs/foo/ChangeLog
       short_pathname: ccvs/src/ChangeLog
       */
    char *short_pathname;
    char *p;

    /*
     * Do the whole descent in parallel for the repositories, so we
     * know what to put in CVS/Repository files.  I'm not sure the
     * full hair is necessary since the server does a similar
     * computation; I suspect that we only end up creating one
     * directory at a time anyway.
     *
     * Also note that we must *only* worry about this stuff when we
     * are creating directories; `cvs co foo/bar; cd foo/bar; cvs co
     * CVSROOT; cvs update' is legitimate, but in this case
     * foo/bar/CVSROOT/CVS/Repository is not a subdirectory of
     * foo/bar/CVS/Repository.
     */
    char *reposname;
    char *short_repos;
    char *reposdirname;
    char *rdirp;
    int reposdirname_absolute;

    reposname = NULL;
    read_line (&reposname);
    assert (reposname != NULL);

	TRACE(3,"call_in_directory %s,%s",PATCH_NULL(reposname),PATCH_NULL(pathname));

    reposdirname_absolute = 0;
    if (strncmp (reposname, toplevel_repos, strlen (toplevel_repos)) != 0)
    {
	reposdirname_absolute = 1;
	short_repos = reposname;
    }
    else
    {
	short_repos = reposname + strlen (toplevel_repos) + 1;
	if (short_repos[-1] != '/')
	{
	    reposdirname_absolute = 1;
	    short_repos = reposname;
	}
    }
    reposdirname = xstrdup (short_repos);
    p = strrchr (reposdirname, '/');
    if (p == NULL)
    {
	reposdirname = (char*)xrealloc (reposdirname, 2);
	reposdirname[0] = '.'; reposdirname[1] = '\0';
    }
    else
	*p = '\0';

	if(isabsolute(pathname) || pathname_levels(pathname)>client_max_dotdot)
    {
		error (0, 0,
               "Server attempted to update a file via an invalid pathname:");
        error (1, 0, "'%s'.", pathname);
    }

	if(pathname_levels(pathname))
	{
		TRACE(3,"Upward relative path '%s' allowed by client_max_dotdot of %d",PATCH_NULL(pathname),client_max_dotdot);
	}

    dir_name = xstrdup (pathname);
    p = strrchr (dir_name, '/');
    if (p == NULL)
    {
	dir_name = (char*)xrealloc (dir_name, 2);
	dir_name[0] = '.'; dir_name[1] = '\0';
    }
    else
	*p = '\0';
    if (client_prune_dirs)
		add_prune_candidate (dir_name);

    filename = strrchr (short_repos, '/');
    if (filename == NULL)
	filename = short_repos;
    else
	++filename;

    short_pathname = (char*)xmalloc (strlen (pathname) + strlen (filename) + 5);
    strcpy (short_pathname, pathname);
    strcat (short_pathname, filename);

    if (last_dir_name == NULL
	|| strcmp (last_dir_name, dir_name) != 0)
    {
	int newdir;

	if (strcmp (command_name, "export") != 0)
	    if (last_entries)
		Entries_Close (last_entries);

	if (last_dir_name)
	    xfree (last_dir_name);
	last_dir_name = dir_name;

	if (toplevel_wd == NULL)
	{
	    toplevel_wd = xgetwd_mapped ();
	    if (toplevel_wd == NULL)
		error (1, errno, "could not get working directory");
	}

	if (CVS_CHDIR (toplevel_wd) < 0)
	    error (1, errno, "could not chdir to %s", toplevel_wd);
	newdir = 0;

	/* Create the CVS directory at the top level if needed.  The
	   isdir seems like an unneeded system call, but it *does*
	   need to be called both if the CVS_CHDIR below succeeds
	   (e.g.  "cvs co .") or if it fails (e.g. basicb-1a in
	   testsuite).  We only need to do this for the "." case,
	   since the server takes care of forcing this directory to be
	   created in all other cases.  If we don't create CVSADM
	   here, the call to Entries_Open below will fail.  FIXME:
	   perhaps this means that we should change our algorithm
	   below that calls Create_Admin instead of having this code
	   here? */
	if (/* I think the reposdirname_absolute case has to do with
	       things like "cvs update /foo/bar".  In any event, the
	       code below which tries to put toplevel_repos into
	       CVS/Repository is almost surely unsuited to
	       the reposdirname_absolute case.  */
	    !reposdirname_absolute
	    && (strcmp (dir_name, ".") == 0)
	    && ! isdir (CVSADM))
	{
	    char *repo;
	    char *r;

	    newdir = 1;

	    repo = (char*)xmalloc (strlen (toplevel_repos)
			    + 10);
	    strcpy (repo, toplevel_repos);
	    r = repo + strlen (repo);
	    if (r[-1] != '.' || r[-2] != '/')
	        strcpy (r, "/.");

	    Create_Admin (".", ".", repo, (char *) NULL,
			  (char *) NULL, 0, 0);

	    xfree (repo);
	}

	if ( CVS_CHDIR (dir_name) < 0)
	{
	    char *dir;
	    char *dirp;
	    
	    if (! existence_error (errno))
		error (1, errno, "could not chdir to %s", dir_name);
	    
	    /* Directory does not exist, we need to create it.  */
	    newdir = 1;

	    /* Provided we are willing to assume that directories get
	       created one at a time, we could simplify this a lot.
	       Do note that one aspect still would need to walk the
	       dir_name path: the checking for "fncmp (dir, CVSADM)".  */

	    dir = (char*)xmalloc (strlen (dir_name) + 1);
	    dirp = dir_name;
	    rdirp = reposdirname;

	    /* This algorithm makes nested directories one at a time
               and create CVS administration files in them.  For
               example, we're checking out foo/bar/baz from the
               repository:

	       1) create foo, point CVS/Repository to <root>/foo
	       2)     .. foo/bar                   .. <root>/foo/bar
	       3)     .. foo/bar/baz               .. <root>/foo/bar/baz
	       
	       As you can see, we're just stepping along DIR_NAME (with
	       DIRP) and REPOSDIRNAME (with RDIRP) respectively.

	       We need to be careful when we are checking out a
	       module, however, since DIR_NAME and REPOSDIRNAME are not
	       going to be the same.  Since modules will not have any
	       slashes in their names, we should watch the output of
	       STRCHR to decide whether or not we should use STRCHR on
	       the RDIRP.  That is, if we're down to a module name,
	       don't keep picking apart the repository directory name.  */

	    do
	    {
		dirp = strchr (dirp, '/');
		if (dirp)
		{
		    strncpy (dir, dir_name, dirp - dir_name);
		    dir[dirp - dir_name] = '\0';
		    /* Skip the slash.  */
		    ++dirp;
		    if (rdirp == NULL)
			/* This just means that the repository string has
			   fewer components than the dir_name string.  But
			   that is OK (e.g. see modules3-8 in testsuite).  */
			;
		    else
			rdirp = strchr (rdirp, '/');
		}
		else
		{
		    /* If there are no more slashes in the dir name,
                       we're down to the most nested directory -OR- to
                       the name of a module.  In the first case, we
                       should be down to a DIRP that has no slashes,
                       so it won't help/hurt to do another STRCHR call
                       on DIRP.  It will definitely hurt, however, if
                       we're down to a module name, since a module
                       name can point to a nested directory (that is,
                       DIRP will still have slashes in it.  Therefore,
                       we should set it to NULL so the routine below
                       copies the contents of REMOTEDIRNAME onto the
                       root repository directory (does this if rdirp
                       is set to NULL, because we used to do an extra
                       STRCHR call here). */

		    rdirp = NULL;
		    strcpy (dir, dir_name);
		}

		if (fncmp (dir, CVSADM) == 0 
			|| fncmp(dir, CVSDUMMY) == 0)
		{
		    error (0, 0, "cannot create a directory named %s", dir);
		    error (0, 0, "because CVS uses \"%s\" for its own uses", dir);
		    error (1, 0, "rename the directory and try again");
		}

		if (mkdir_if_needed (dir))
		{
		    /* It already existed, fine.  Just keep going.  */
		}
		else if (strcmp (command_name, "export") == 0)
		    /* Don't create CVSADM directories if this is export.  */
		    ;
		else
		{
		    /*
		     * Put repository in CVS/Repository.  For historical
		     * (pre-CVS/Root) reasons, this is an absolute pathname,
		     * but what really matters is the part of it which is
		     * relative to cvsroot.
		     */
		    char *repo;
		    char *r, *b;

		    repo = (char*)xmalloc (strlen (reposdirname)
				    + strlen (toplevel_repos)
				    + 80);
		    if (reposdirname_absolute)
			r = repo;
		    else
		    {
			strcpy (repo, toplevel_repos);
			strcat (repo, "/");
			r = repo + strlen (repo);
		    }

		    if (rdirp)
		    {
			/* See comment near start of function; the only
			   way that the server can put the right thing
			   in each CVS/Repository file is to create the
			   directories one at a time.  I think that the
			   CVS server has been doing this all along.  */
			error (0, 0, "\
warning: server is not creating directories one at a time");
			strncpy (r, reposdirname, rdirp - reposdirname);
			r[rdirp - reposdirname] = '\0';
		    }
		    else
			strcpy (r, reposdirname);

		    Create_Admin (dir, dir, repo,
				  (char *)NULL, (char *)NULL, 0, 0);
		    xfree (repo);

		    b = strrchr (dir, '/');
		    if (b == NULL)
			Subdir_Register ((List *) NULL, (char *) NULL, dir);
		    else
		    {
			*b = '\0';
			Subdir_Register ((List *) NULL, dir, b + 1);
			*b = '/';
		    }
		}

		if (rdirp != NULL)
		{
		    /* Skip the slash.  */
		    ++rdirp;
		}

	    } while (dirp != NULL);
	    xfree (dir);
	    /* Now it better work.  */
	    if ( CVS_CHDIR (dir_name) < 0)
		error (1, errno, "could not chdir to %s", dir_name);
	}
	else if (!isdir (CVSADM) && strcmp (command_name, "export"))
	{
	    /*
	     * Put repository in CVS/Repository.  For historical
	     * (pre-CVS/Root) reasons, this is an absolute pathname,
	     * but what really matters is the part of it which is
	     * relative to cvsroot.
	     */
	    char *repo;

	    if (reposdirname_absolute)
		repo = reposdirname;
	    else
	    {
		repo = (char*)xmalloc (strlen (reposdirname)
				+ strlen (toplevel_repos)
				+ 10);
		strcpy (repo, toplevel_repos);
		strcat (repo, "/");
		strcat (repo, reposdirname);
	    }

	    Create_Admin (".", ".", repo, (char *)NULL, (char *)NULL, 0, 1);
	    if (repo != reposdirname)
		xfree (repo);
	}

	if (strcmp (command_name, "export") != 0)
	{
	    last_entries = Entries_Open (0, dir_name);

	    /* If this is a newly created directory, we will record
	       all subdirectory information, so call Subdirs_Known in
	       case there are no subdirectories.  If this is not a
	       newly created directory, it may be an old working
	       directory from before we recorded subdirectory
	       information in the Entries file.  We force a search for
	       all subdirectories now, to make sure our subdirectory
	       information is up to date.  If the Entries file does
	       record subdirectory information, then this call only
	       does list manipulation.  */
	    if (newdir)
		Subdirs_Known (last_entries);
	    else
	    {
		List *dirlist;

		dirlist = Find_Directories ((char *) NULL, W_LOCAL,
					    last_entries, NULL);
		dellist (&dirlist);
	    }
	}
    }
    else
	xfree (dir_name);
    xfree (reposdirname);
    (*func) (data, last_entries, short_pathname, filename);
    xfree (short_pathname);
    xfree (reposname);
}

static void copy_a_file (char *data, List *ent_list, char *short_pathname, char *filename)
{
    char *newname;

    read_line (&newname);

    /* cvsclient.texi has said for a long time that newname must be in the
       same directory.  Wouldn't want a malicious or buggy server overwriting
       ~/.profile, /etc/passwd, or anything like that.  */
    if (last_component (newname) != newname)
	error (1, 0, "protocol error: Copy-file tried to specify directory");

    copy_file (filename, newname, 1, 1);
    xfree (newname);
}

static void handle_copy_file (char *args, int len)
{
    call_in_directory (args, copy_a_file, (char *)NULL);
}

/* Read from the server the count for the length of a file, then read
   the contents of that file and write them to FILENAME.  FULLNAME is
   the name of the file for use in error messages.  FIXME-someday:
   extend this to deal with compressed files and make update_entries
   use it.  On error, gives a fatal error.  */
static void read_counted_file (char *filename, char *fullname)
{
    char *size_string;
    size_t size;
    char *buf;

    /* Pointers in buf to the place to put data which will be read,
       and the data which needs to be written, respectively.  */
    char *pread;
    char *pwrite;
    /* Number of bytes left to read and number of bytes in buf waiting to
       be written, respectively.  */
    size_t nread;
    size_t nwrite;

    FILE *fp;

    read_line (&size_string);
    if (size_string[0] == 'z')
	error (1, 0, "\
protocol error: compressed files not supported for that operation");
    /* FIXME: should be doing more error checking, probably.  Like using
       strtoul and making sure we used up the whole line.  */
    size = atoi (size_string);
    xfree (size_string);

    /* A more sophisticated implementation would use only a limited amount
       of buffer space (8K perhaps), and read that much at a time.  We allocate
       a buffer for the whole file only to make it easy to keep track what
       needs to be read and written.  */
    buf = (char*)xmalloc (size);

    /* FIXME-someday: caller should pass in a flag saying whether it
       is binary or not.  I haven't carefully looked into whether
       CVS/Template files should use local text file conventions or
       not.  */
    fp = CVS_FOPEN (filename, "wb");
    if (fp == NULL)
	error (1, errno, "cannot open %s for writing", fullname);
    nread = size;
    nwrite = 0;
    pread = buf;
    pwrite = buf;
    while (nread > 0 || nwrite > 0)
    {
	size_t n;

	if (nread > 0)
	{
	    n = try_read_from_server (pread, nread);
	    nread -= n;
	    pread += n;
	    nwrite += n;
	}

	if (nwrite > 0)
	{
	    n = fwrite (pwrite, 1, nwrite, fp);
	    if (ferror (fp))
		error (1, errno, "cannot write %s", fullname);
	    nwrite -= n;
	    pwrite += n;
	}
    }
    xfree (buf);
    if (fclose (fp) < 0)
	error (1, errno, "cannot close %s", fullname);
}

/* OK, we want to swallow the "U foo.c" response and then output it only
   if we can update the file.  In the future we probably want some more
   systematic approach to parsing tagged text, but for now we keep it
   ad hoc.  "Why," I hear you cry, "do we not just look at the
   Update-existing and Created responses?"  That is an excellent question,
   and the answer is roughly conservatism/laziness--I haven't read through
   update.c enough to figure out the exact correspondence or lack thereof
   between those responses and a "U foo.c" line (note that Merged, from
   join_file, can be either "C foo" or "U foo" depending on the context).  */
/* Nonzero if we have seen +updated and not -updated.  */
static int updated_seen;
/* Filename from an "fname" tagged response within +updated/-updated.  */
static char *updated_fname;

/* This struct is used to hold data when reading the +importmergecmd
   and -importmergecmd tags.  We put the variables in a struct only
   for namespace issues.  FIXME: As noted above, we need to develop a
   more systematic approach.  */
static struct
{
    /* Nonzero if we have seen +importmergecmd and not -importmergecmd.  */
    int seen;
    /* Number of conflicts, from a "conflicts" tagged response.  */
    int conflicts;
    /* First merge tag, from a "mergetag1" tagged response.  */
    char *mergetag1;
    /* Second merge tag, from a "mergetag2" tagged response.  */
    char *mergetag2;
    /* Repository, from a "repository" tagged response.  */
    char *repository;
} importmergecmd;

/* Nonzero if we should arrange to return with a failure exit status.  */
static int failure_exit;


/*
 * The time stamp of the last file we registered.
 */
static time_t last_register_time;

/*
 * The Checksum response gives the checksum for the file transferred
 * over by the next Updated, Merged or Patch response.  We just store
 * it here, and then check it in update_entries.
 */

static int stored_checksum_valid;
static char stored_checksum[33];

static void handle_checksum (char *args, int len)
{
    if (stored_checksum_valid)
        error (1, 0, "Checksum received before last one was used");

	if(strlen(args)!=32)
        error (1, 0, "Invalid Checksum response: `%s'", args);

	strcpy(stored_checksum, args);

    stored_checksum_valid = 1;
}

/* Mode that we got in a "Mode" response (malloc'd), or NULL if none.  */
static char *stored_mode;

static void handle_mode (char *args, int len)
{
    if (stored_mode != NULL)
	error (1, 0, "protocol error: duplicate Mode");
    stored_mode = xstrdup (args);
}

/* Nonzero if time was specified in Mod-time.  */
static int stored_modtime_valid;
/* Time specified in Mod-time.  */
static time_t stored_modtime;

static void handle_mod_time (char *args, int len)
{
    if (stored_modtime_valid)
		error (0, 0, "protocol error: duplicate Mod-time");
    stored_modtime = get_date (args, NULL);
    if (stored_modtime == (time_t) -1)
		error (0, 0, "protocol error: cannot parse date %s", args);
    else
		stored_modtime_valid = 1;
}

/*
 * If we receive a patch, but the patch program fails to apply it, we
 * want to request the original file.  We keep a list of files whose
 * patches have failed.
 */

char **failed_patches;
int failed_patches_count;

enum contents_t
{
    /*
    * We are just getting an Entries line; the local file is
    * correct.
    */
    UPDATE_ENTRIES_CHECKIN,
    /* We are getting the file contents as well.  */
    UPDATE_ENTRIES_UPDATE,
    /*
    * We are getting a patch against the existing local file, not
    * an entire new file.
    */
    UPDATE_ENTRIES_PATCH,
    /*
    * We are getting an RCS change text (diff -n output) against
    * the existing local file, not an entire new file.
    */
    UPDATE_ENTRIES_RCS_DIFF
};

enum existp_t
{
	/* We are replacing an existing file.  */
	UPDATE_ENTRIES_EXISTING,
	/* We are creating a new file.  */
	UPDATE_ENTRIES_NEW,
	/* We don't know whether it is existing or new.  */
	UPDATE_ENTRIES_EXISTING_OR_NEW
};

struct update_entries_data
{
    enum contents_t contents;
    enum existp_t existp;

    /*
     * String to put in the timestamp field or NULL to use the timestamp
     * of the file.
     */
    char *timestamp;
};

/* Update the Entries line for this file.  */
static void update_entries (char *data_arg, List *ent_list, char *short_pathname, char *filename)
{
    char *entries_line;
    struct update_entries_data *data = (struct update_entries_data *)data_arg;

    char *cp;
    char *user;
    char *vn;
    /* Timestamp field.  Always empty according to the protocol.  */
    char *ts;
    char *options = NULL;
	kflag options_flags;
	LineType crlf;
    char *tag = NULL;
    char *date = NULL;
    char *tag_or_date;
    char *scratch_entries = NULL;
	bool open_binary,encode;
	CCodepage::Encoding encoding;
	char *merge_tag1, *merge_tag2;
	time_t rcs_timestamp;
	char *edit_revision = "*";
	char *edit_tag = NULL;
	char *edit_bugid = NULL;
	char *md5 = NULL;

#ifdef UTIME_EXPECTS_WRITABLE
    int change_it_back = 0;
#endif

    read_line (&entries_line);

    /*
     * Parse the entries line.
     */
    scratch_entries = xstrdup (entries_line);

    if (scratch_entries[0] != '/')
        error (1, 0, "bad entries line `%s' from server", entries_line);
    user = scratch_entries + 1;
    if ((cp = strchr (user, '/')) == NULL)
        error (1, 0, "bad entries line `%s' from server", entries_line);
    *cp++ = '\0';
    vn = cp;
    if ((cp = strchr (vn, '/')) == NULL)
        error (1, 0, "bad entries line `%s' from server", entries_line);
    *cp++ = '\0';
    
    ts = cp;
    if ((cp = strchr (ts, '/')) == NULL)
        error (1, 0, "bad entries line `%s' from server", entries_line);
    *cp++ = '\0';
    options = cp;
	if(options[0]=='-' && options[1]=='k')
		options+=2;
    if ((cp = strchr (options, '/')) == NULL)
        error (1, 0, "bad entries line `%s' from server", entries_line);
    *cp++ = '\0';
    tag_or_date = cp;
    
    /* If a slash ends the tag_or_date, ignore everything after it.  */
    cp = strchr (tag_or_date, '/');
    if (cp != NULL)
        *cp = '\0';
    if (*tag_or_date == 'T')
        tag = tag_or_date + 1;
    else if (*tag_or_date == 'D')
        date = tag_or_date + 1;

    /* Done parsing the entries line. */

    if (data->contents == UPDATE_ENTRIES_UPDATE
	|| data->contents == UPDATE_ENTRIES_PATCH
	|| data->contents == UPDATE_ENTRIES_RCS_DIFF)
    {
	char *size_string;
	char *mode_string;
	int size;
	char *buf;
	char *temp_filename;
	int patch_failed;
	char *realfilename;

	read_line (&mode_string);
	
	read_line (&size_string);
    size = atoi (size_string);
	xfree (size_string);

	/* Note that checking this separately from writing the file is
	   a race condition: if the existence or lack thereof of the
	   file changes between now and the actual calls which
	   operate on it, we lose.  However (a) there are so many
	   cases, I'm reluctant to try to fix them all, (b) in some
	   cases the system might not even have a system call which
	   does the right thing, and (c) it isn't clear this needs to
	   work.  */
	if (data->existp == UPDATE_ENTRIES_EXISTING
	    && !isfile (filename))
	    /* Emit a warning and update the file anyway.  */
	    error (0, 0, "warning: %s unexpectedly disappeared",
		   short_pathname);

	if (filenames_case_insensitive && client_overwrite_existing && isfile(filename) && !case_isfile(filename,&realfilename))
	{
		xfree(realfilename);
	}
	else
	if (data->existp == UPDATE_ENTRIES_NEW && !client_overwrite_existing && isfile (filename))
	{
	    /* Emit a warning and refuse to update the file; we don't want
	       to clobber a user's file.  */
	    size_t nread;
	    size_t toread;

	    /* size should be unsigned, but until we get around to fixing
	       that, work around it.  */
	    size_t usize;

	    char buf[8192];

	if (filenames_case_insensitive && !case_isfile(filename,&realfilename))
	{
		if(!quiet)
			error(0,0,"Case ambiguity between %s and %s.  Using %s.",filename,realfilename,realfilename);
		xfree(realfilename);
		goto discard_file_and_return;
	}
		/* This error might be confusing; it isn't really clear to
		the user what to do about it.  Keep in mind that it has
		several causes: (1) something/someone creates the file
		during the time that CVS is running, (2) the repository
		has two files whose names clash for the client because
		of case-insensitivity or similar causes, (3) a special
		case of this is that a file gets renamed for example
		from a.c to A.C.  A "cvs update" on a case-insensitive
		client will get this error.  Repeating the update takes
		care of the problem, but is it clear to the user what
		is going on and what to do about it?, (4) the client
		has a file which the server doesn't know about (e.g. "?
		foo" file), and that name clashes with a file the
		server does know about, (5) classify.c will print the same
		message for other reasons.

		I hope the above paragraph makes it clear that making this
		clearer is not a one-line fix.  */
		error (0, 0, "move away %s; it is in the way", short_pathname);
		if (updated_fname != NULL)
		{
		cvs_output ("C ", 0);
		cvs_output (updated_fname, 0);
		cvs_output ("\n", 1);
		}
		failure_exit = 1;

	discard_file_and_return:
	    /* Now read and discard the file contents.  */
	    usize = size;
	    nread = 0;
	    while (nread < usize)
	    {
		toread = usize - nread;
		if (toread > sizeof buf)
		    toread = sizeof buf;

		nread += try_read_from_server (buf, toread);
		if (nread == usize)
		    break;
	    }

	    xfree (mode_string);
	    xfree (scratch_entries);
	    xfree (entries_line);

	    /* The Mode, Mod-time, and Checksum responses should not carry
	       over to a subsequent Created (or whatever) response, even
	       in the error case.  */
	    if (stored_mode != NULL)
	    {
		xfree (stored_mode);
		stored_mode = NULL;
	    }
	    stored_modtime_valid = 0;
	    stored_checksum_valid = 0;

	    if (updated_fname != NULL)
	    {
		xfree (updated_fname);
		updated_fname = NULL;
	    }
	    return;
	}

	temp_filename = (char*)xmalloc (strlen (filename) + 80);
	sprintf(temp_filename,"_new_%s",filename);

	buf = (char*)xmalloc (size);

        /* Some systems, like OS/2 and Windows NT, end lines with CRLF
           instead of just LF.  Format translation is done in the C
           library I/O funtions.  Here we tell them whether or not to
           convert -- if this file is marked "binary" with the RCS -kb
           flag, then we don't want to convert, else we do (because
           CVS assumes text files by default). */

	crlf = crlf_mode;
	if (options)
	{
		RCS_get_kflags(options, false, options_flags);
		if(options_flags.flags&KFLAG_MAC)
			crlf=ltCr;
		else if(options_flags.flags&KFLAG_DOS)
			crlf=ltCrLf;
		else if(options_flags.flags&(KFLAG_BINARY|KFLAG_UNIX))
			crlf=ltLf;
		open_binary = (options_flags.flags&(KFLAG_BINARY|KFLAG_ENCODED)) || (crlf!=CRLF_DEFAULT);
		encode = !(options_flags.flags&KFLAG_BINARY) && ((options_flags.flags&KFLAG_ENCODED) || (crlf!=CRLF_DEFAULT));
		encoding = options_flags.encoding;
	}
	else
	{
		encode = (crlf!=CRLF_DEFAULT);
		open_binary = (crlf!=CRLF_DEFAULT);
		encoding = CCodepage::NullEncoding;
	}

	patch_failed = 0;

	/* Talking to a Unix CVS server.  In theory options can't be '-ku'
	   but it's good to check, just in case. */
	if(!supported_request("Utf8"))
	{
		if(encode && encoding.encoding)
		{
			error(0,0,"Transformation not supported by server.  Results may not be correct");
			encode = false;
		}
	}

	if (data->contents == UPDATE_ENTRIES_RCS_DIFF)
	{
	    /* This is an RCS change text.  We just hold the change
	       text in memory.  */

	    read_from_server (buf, size);
	}
	else
	{
	    int fd;

	    fd = CVS_OPEN (temp_filename,
			   (O_WRONLY | O_CREAT | O_TRUNC
			    | (open_binary ? OPEN_BINARY : 0)),
			   0777);

	    if (fd < 0)
	    {
		/* I can see a case for making this a fatal error; for
		   a condition like disk full or network unreachable
		   (for a file server), carrying on and giving an
		   error on each file seems unnecessary.  But if it is
		   a permission problem, or some such, then it is
		   entirely possible that future files will not have
		   the same problem.  */
#ifdef _WIN32
		if(errno==EBADF) 
		{
			// This case from validate_filename() and the filename has already been reported as bad
			goto discard_file_and_return;
		}
		else
#endif
		{
			error (0, errno, "cannot open temp file %s for writing", temp_filename);
			goto discard_file_and_return;
		}
	    }

	    if (size > 0)
	    {
			CMD5Calc md5;

			read_from_server (buf, size);

			if(stored_checksum_valid)
				md5.Update(buf,size);

			if(encode)
			{
				CCodepage *cdp3;
				cdp3 = new CCodepage;
				cdp3->BeginEncoding(CCodepage::NullEncoding, encoding);
				if(cdp3->OutputAsEncoded(fd,buf,size,crlf))
					error(1, errno, "Cannot write %s", short_pathname);
				cdp3->EndEncoding();
				delete cdp3;
				cdp3=NULL;
			}
			else
			{
				if (write (fd, buf, size) != size)
					error (1, errno, "writing %s", short_pathname);
			}

			/* Note that the checksum must be calculated on the *original* buffer,
			   not the new one.  This takes into account unicode and text files */
			if(stored_checksum_valid)
			{
				if (strcmp (md5.Final(), stored_checksum) != 0)
				{
					error (0, 0, "checksum failure after patch to %s; will refetch", short_pathname);
					patch_failed = 1;
				}
				stored_checksum_valid = 0;
			}
		}

	    if (close (fd) < 0)
			error (1, errno, "writing %s", short_pathname);
	}

	/* This is after we have read the file from the net (a change
	   from previous versions, where the server would send us
	   "M U foo.c" before Update-existing or whatever), but before
	   we finish writing the file (arguably a bug).  The timing
	   affects a user who wants status info about how far we have
	   gotten, and also affects whether "U foo.c" appears in addition
	   to various error messages.  */
	if (updated_fname != NULL)
	{
	    cvs_output ("U ", 0);
	    cvs_output (updated_fname, 0);
	    cvs_output ("\n", 1);
	    xfree (updated_fname);
	    updated_fname = 0;
	}

	if (data->contents == UPDATE_ENTRIES_UPDATE)
	{
#ifdef MAC_HFS_STUFF
		/* decode into data & resource fork if needed, adjust character encoding if needed and adjust HFS file type */
		mac_decode_file(temp_filename, filename, open_binary);
		
	    if (CVS_UNLINK (temp_filename) < 0)
			error (0, errno, "warning: can't remove temp file %s", temp_filename);
#else
	    rename_file (temp_filename, filename);
#endif
	}
	else if (data->contents == UPDATE_ENTRIES_PATCH)
	{
	    /* You might think we could just leave Patched out of
	       Valid-responses and not get this response.  However, if
	       memory serves, the CVS 1.9 server bases this on -u
	       (update-patches), and there is no way for us to send -u
	       or not based on whether the server supports "Rcs-diff".  

	       Fall back to transmitting entire files.  */
	    patch_failed = 1;
	}
	else /* Handle UPDATE_ENTRIES_RCS_DIFF.  */
	{
	    char *filebuf;
	    size_t filebufsize;
	    size_t nread;
	    char *patchedbuf;
	    size_t patchedlen;

	    if (!isfile (filename))
			error (1, 0, "patch original file %s does not exist", short_pathname);
	    filebuf = NULL;
	    filebufsize = 0;
	    nread = 0;

#ifdef MAC_HFS_STUFF
		/* encode data & resource fork into flat file if needed and adjust character encoding if needed */
		mac_encode_file(filename, temp_filename, open_binary);
		
		get_file (temp_filename, short_pathname, (open_binary&&!encode) ? FOPEN_BINARY_READ : "r",
				  &filebuf, &filebufsize, &nread, options_flags);
		
	    if (CVS_UNLINK (temp_filename) < 0)
			error (0, errno, "warning: can't remove temp file %s", temp_filename);
#else
		get_file (filename, short_pathname, (open_binary&&!encode) ? "rb" : "r",
		      &filebuf, &filebufsize, &nread, options_flags);
#endif
	    /* At this point the contents of the existing file are in
               FILEBUF, and the length of the contents is in NREAD.
               The contents of the patch from the network are in BUF,
               and the length of the patch is in SIZE.  */

	    if (! rcs_change_text (short_pathname, filebuf, nread, buf, size,
				   &patchedbuf, &patchedlen))
			patch_failed = 1;
	    else
	    {
			if (stored_checksum_valid)
			{
				CMD5Calc md5;

				/* We have a checksum.  Check it before writing
				the file out, so that we don't have to read it
				back in again.  */
				md5.Update(patchedbuf, patchedlen);
				if (strcmp (md5.Final(), stored_checksum) != 0)
				{
					error (0, 0, "checksum failure after patch to %s; will refetch", short_pathname);

					patch_failed = 1;
				}

				stored_checksum_valid = 0;
			}

			if (! patch_failed)
			{
				FILE *e;

				e = open_file (temp_filename,
						open_binary ? "wb" : "w");
				if(encode)
				{
					CCodepage *cdp4;
					cdp4 = new CCodepage;
					cdp4->BeginEncoding(CCodepage::NullEncoding,encoding);
					if(cdp4->OutputAsEncoded(fileno(e),patchedbuf,patchedlen,crlf))
						error(1, errno, "Cannot write %s", temp_filename);
					cdp4->EndEncoding();
					delete cdp4;
					cdp4=NULL;
				}
				else
				{
					if (fwrite (patchedbuf, 1, patchedlen, e) != patchedlen)
						error (1, errno, "cannot write %s", temp_filename);
				}
				if (fclose (e) == EOF)
					error (1, errno, "cannot close %s", temp_filename);
#ifdef MAC_HFS_STUFF
				/* decode into data & resource fork if needed, adjust character encoding if needed and adjust HFS file type */
				mac_decode_file(temp_filename, filename, open_binary);
				
	    		if (CVS_UNLINK (temp_filename) < 0)
					error (0, errno, "warning: can't remove temp file %s", temp_filename);
#else
				rename_file (temp_filename, filename);
#endif
			}

			xfree (patchedbuf);
	    }

	    xfree (filebuf);
	}

	xfree (temp_filename);

	if (patch_failed) 
	{
	    /* Save this file to retrieve later.  */
	    failed_patches = (char **) xrealloc ((char *) failed_patches,
						 ((failed_patches_count + 1)
						  * sizeof (char *)));
	    failed_patches[failed_patches_count] = xstrdup (short_pathname);
	    ++failed_patches_count;

	    stored_checksum_valid = 0;

	    xfree (mode_string);
	    xfree (buf);
	    xfree (scratch_entries);
	    xfree (entries_line);

	    return;
	}

    {
	    int status = change_mode (filename, mode_string, 1);
	    if (status != 0)
			error (0, status, "cannot change mode of %s", short_pathname);
	}

	xfree (mode_string);
	xfree (buf);
    }

    if (stored_mode != NULL)
    {
	change_mode (filename, stored_mode, 1);
	xfree (stored_mode);
	stored_mode = NULL;
    }
   
    if (stored_modtime_valid)
    {
	struct utimbuf t;

	memset (&t, 0, sizeof (t));
	/* There is probably little point in trying to preserved the
	   actime (or is there? What about Checked-in?).  */
	t.modtime = t.actime = stored_modtime;

#ifdef UTIME_EXPECTS_WRITABLE
	if (!iswritable (filename))
	{
	    xchmod (filename, 1);
	    change_it_back = 1;
	}
#endif  /* UTIME_EXPECTS_WRITABLE  */

	if (utime (filename, &t) < 0)
	    error (0, errno, "cannot set time on %s", filename);

#ifdef UTIME_EXPECTS_WRITABLE
	if (change_it_back == 1)
	{
	    xchmod (filename, 0);
	    change_it_back = 0;
	}
#endif  /*  UTIME_EXPECTS_WRITABLE  */

	stored_modtime_valid = 0;
    }

    /*
     * Process the entries line.  Do this after we've written the file,
     * since we need the timestamp.
     */
    if (strcmp (command_name, "export") != 0)
    {
	char *local_timestamp;
	char *localtime_timestamp;
	char *file_timestamp;

	(void) time (&last_register_time);

	local_timestamp = data->timestamp;
	if (local_timestamp == NULL || ts[0] == '+')
	{
	    file_timestamp = time_stamp (filename,0);
	    localtime_timestamp = time_stamp (filename,1);
	}
	else
	    file_timestamp = NULL;

	/*
	 * These special version numbers signify that it is not up to
	 * date.  Create a dummy timestamp which will never compare
	 * equal to the timestamp of the file.
	 */
	// TMA 2001/12/18: Accept version numbers starting with "0."
	if (vn[0] == '\0' || (vn[0] == '0' && vn[1] == '\0') || vn[0] == '-')
	    local_timestamp = "dummy timestamp";
	else if (local_timestamp == NULL)
	{
	    local_timestamp = file_timestamp;

	    /* Checking for command_name of "commit" doesn't seem like
	       the cleanest way to handle this, but it seem to roughly
	       parallel what the :local: code which calls
	       mark_up_to_date ends up amounting to.  Some day, should
	       think more about what the Checked-in response means
	       vis-a-vis both Entries and Base and clarify
	       cvsclient.texi accordingly.  */

	    if (!strcmp (command_name, "commit"))
			mark_up_to_date (filename);
	}

	/* Get the data for Entries.Extra */
	merge_tag1 = merge_tag2 = NULL;
	rcs_timestamp = (time_t)-1;
	while(extra_entry && extra_entry[0]=='/')
	{
		char *fn, *tag1, *tag2, *rcs_timestamp_string;
		
		fn = extra_entry  + 1;
		if ((cp = strchr (fn, '/')) == NULL)
			break;
		*cp++ = '\0';
		if(fncmp(fn, filename))
			break;
		tag1 = cp;
		if ((cp = strchr (tag1, '/')) == NULL)
			break;
		*cp++ = '\0';
		tag2=cp;
		if ((cp = strchr (tag2, '/')) == NULL)
			break;
		*cp++ = '\0';
		merge_tag1 = tag1;
		merge_tag2 = tag2;
		rcs_timestamp_string=cp;
		if ((cp = strchr (rcs_timestamp_string, '/')) == NULL)
			break;
		*cp++ = '\0';

		if(sscanf(rcs_timestamp_string,"%"TIME_T_SPRINTF"d",&rcs_timestamp)!=1)
			rcs_timestamp=(time_t)-1;

		edit_revision = cp;
		if ((cp = strchr (edit_revision, '/')) == NULL)
		{
			edit_revision="*"; /* Revision was unseen by client */
			break;
		}
		*cp++ = '\0';

		edit_tag = cp;
		if ((cp = strchr (edit_tag, '/')) == NULL)
			break;
		*cp++ = '\0';

		edit_bugid = cp;
		if ((cp = strchr (edit_bugid, '/')) == NULL)
			break;
		*cp++ = '\0';

		md5 = cp;
		if ((cp = strchr (md5, '/')) == NULL)
			break;
		*cp++ = '\0';

		break;
	}

	Register (ent_list, filename, vn, local_timestamp,
		  options, tag, date, ts[0] == '+' ? file_timestamp : NULL, merge_tag1, merge_tag2, rcs_timestamp, edit_revision, edit_tag, edit_bugid, md5);

	if (file_timestamp)
	    xfree (file_timestamp);

    }
    xfree (scratch_entries);
    xfree (entries_line);
}

static void handle_checked_in (char *args, int len)
{
    struct update_entries_data dat;
    dat.contents = UPDATE_ENTRIES_CHECKIN;
    dat.existp = UPDATE_ENTRIES_EXISTING_OR_NEW;
    dat.timestamp = NULL;
    call_in_directory (args, update_entries, (char *)&dat);
}

static void handle_new_entry (char *args, int len)
{
    struct update_entries_data dat;
    dat.contents = UPDATE_ENTRIES_CHECKIN;
    dat.existp = UPDATE_ENTRIES_EXISTING_OR_NEW;
    dat.timestamp = "dummy timestamp from new-entry";
    call_in_directory (args, update_entries, (char *)&dat);
}

static void handle_updated (char *args, int len)
{
    struct update_entries_data dat;
    dat.contents = UPDATE_ENTRIES_UPDATE;
    dat.existp = UPDATE_ENTRIES_EXISTING_OR_NEW;
    dat.timestamp = NULL;
    call_in_directory (args, update_entries, (char *)&dat);
}

static void handle_created (char *args, int len)
{
    struct update_entries_data dat;
    dat.contents = UPDATE_ENTRIES_UPDATE;
    dat.existp = UPDATE_ENTRIES_NEW;
    dat.timestamp = NULL;
    call_in_directory (args, update_entries, (char *)&dat);
}

static void handle_update_existing (char *args, int len)
{
    struct update_entries_data dat;
    dat.contents = UPDATE_ENTRIES_UPDATE;
    dat.existp = UPDATE_ENTRIES_EXISTING;
    dat.timestamp = NULL;
    call_in_directory (args, update_entries, (char *)&dat);
}

static void handle_merged (char *args, int len)
{
    struct update_entries_data dat;
    dat.contents = UPDATE_ENTRIES_UPDATE;
    /* Think this could be UPDATE_ENTRIES_EXISTING, but just in case...  */
    dat.existp = UPDATE_ENTRIES_EXISTING_OR_NEW;
    dat.timestamp = "Result of merge";
    call_in_directory (args, update_entries, (char *)&dat);
}

static void handle_patched (char *args, int len)
{
    struct update_entries_data dat;
    dat.contents = UPDATE_ENTRIES_PATCH;
    /* Think this could be UPDATE_ENTRIES_EXISTING, but just in case...  */
    dat.existp = UPDATE_ENTRIES_EXISTING_OR_NEW;
    dat.timestamp = NULL;
    call_in_directory (args, update_entries, (char *)&dat);
}

static void handle_rcs_diff (char *args, int len)
{
    struct update_entries_data dat;
    dat.contents = UPDATE_ENTRIES_RCS_DIFF;
    /* Think this could be UPDATE_ENTRIES_EXISTING, but just in case...  */
    dat.existp = UPDATE_ENTRIES_EXISTING_OR_NEW;
    dat.timestamp = NULL;
    call_in_directory (args, update_entries, (char *)&dat);
}

static void update_baserev(char *data, List *ent_list, char *short_pathname, char *filename)
{
	char *type;
	char *basepath = (char*)xmalloc(sizeof(CVSADM_BASE)+strlen(filename)+10);
	char *basepathgz = (char*)xmalloc(sizeof(CVSADM_BASE)+strlen(filename)+10);

	sprintf(basepath,"%s/%s",CVSADM_BASE,filename);
	sprintf(basepathgz,"%s/%s.gz",CVSADM_BASE,filename);

	read_line(&type);

	TRACE(3,"Updating base revision %s [%c]",basepath,type[0]);
	switch(type[0])
	{
	case 'C':
		if(isfile(basepathgz))
			copy_and_zip_file(filename,basepath,1,1);
		else
			copy_file(filename,basepath,1,1);
		break;
	case 'R':
		CVS_UNLINK(basepath);
		CVS_UNLINK(basepathgz);
		break;
	case 'U':
		read_counted_file(basepath,basepath);
		if(isfile(basepathgz))
		{
			copy_and_zip_file(basepath,basepathgz,1,1);
			CVS_UNLINK(basepath);
		}
		break;
	default:
		error(0,0,"Unsupported base update option '%c'",type[0]);
		break;
	}

	xfree(basepathgz);
	xfree(basepath);
}

static void handle_update_baserev(char *args, int len)
{
	call_in_directory(args, update_baserev, NULL);
}

static void remove_entry (char *data, List *ent_list, char *short_pathname, char *filename)
{
    Scratch_Entry (ent_list, filename);
}

static void handle_remove_entry (char *args, int len)
{
    call_in_directory (args, remove_entry, (char *)NULL);
}

static void remove_entry_and_file (char *data, List *ent_list, char *short_pathname, char *filename)
{
    Scratch_Entry (ent_list, filename);
    /* Note that we don't ignore existence_error's here.  The server
       should be sending Remove-entry rather than Removed in cases
       where the file does not exist.  And if the user removes the
       file halfway through a cvs command, we should be printing an
       error.  */
    if (unlink_file (filename) < 0)
		error (0, errno, "unable to remove %s", short_pathname);
}

static void rename_entry_and_file (char *data, List *ent_list, char *short_pathname, char *filename)
{
	char *renamed_to;

    read_line (&renamed_to);

    Rename_Entry (ent_list, filename, renamed_to);

    /* Note that we don't ignore existence_error's here.  The server
       should be sending Remove-entry rather than Removed in cases
       where the file does not exist.  And if the user removes the
       file halfway through a cvs command, we should be printing an
       error.  */
    if (CVS_RENAME(filename,renamed_to) < 0)
		error (0, errno, "unable to rename %s", short_pathname);

	xfree(renamed_to);
}

static void handle_removed (char *args, int len)
{
    call_in_directory (args, remove_entry_and_file, (char *)NULL);
}

static void handle_renamed (char *args, int len)
{
    call_in_directory (args, rename_entry_and_file, (char *)NULL);
}

/* Is this the top level (directory containing CVSROOT)?  */
static int is_cvsroot_level (char *pathname)
{
    if (strcmp (toplevel_repos, current_parsed_root->directory) != 0)
	return 0;

    return strchr (pathname, '/') == NULL;
}

std::vector<cvs::filename> client_module_expansion;

static void handle_module_expansion(char *args, int len)
{
	client_module_expansion.push_back(args);
}

static void set_static (char *data, List *ent_list, char *short_pathname, char *filename)
{
    FILE *fp;
    fp = open_file (CVSADM_ENTSTAT, "w+");
    if (fclose (fp) == EOF)
        error (1, errno, "cannot close %s", CVSADM_ENTSTAT);
}

static void handle_set_static_directory (char *args, int len)
{
    if (strcmp (command_name, "export") == 0)
    {
	/* Swallow the repository.  */
	read_line (NULL);
	return;
    }
    call_in_directory (args, set_static, (char *)NULL);
}

static void clear_static (char *data, List *ent_list, char *short_pathname, char *filename)
{
    if (unlink_file (CVSADM_ENTSTAT) < 0 && ! existence_error (errno))
        error (1, errno, "cannot remove file %s", CVSADM_ENTSTAT);
}

static void clear_rename (char *data, List *ent_list, char *short_pathname, char *filename)
{
	if (unlink_file (CVSADM_RENAME) < 0 && ! existence_error(errno))
        error (1, errno, "cannot remove file %s", CVSADM_RENAME);
}

static void handle_clear_static_directory (char *pathname, int len)
{
    if (strcmp (command_name, "export") == 0)
    {
	/* Swallow the repository.  */
	read_line (NULL);
	return;
    }

    if (is_cvsroot_level (pathname))
    {
        /*
	 * Top level (directory containing CVSROOT).  This seems to normally
	 * lack a CVS directory, so don't try to create files in it.
	 */
	return;
    }
    call_in_directory (pathname, clear_static, (char *)NULL);
}

static void handle_clear_rename(char *pathname, int len)
{
    if (strcmp (command_name, "export") == 0)
    {
	/* Swallow the repository.  */
	read_line (NULL);
	return;
    }

    if (is_cvsroot_level (pathname))
    {
        /*
	 * Top level (directory containing CVSROOT).  This seems to normally
	 * lack a CVS directory, so don't try to create files in it.
	 */
	return;
    }
    call_in_directory (pathname, clear_rename, (char *)NULL);
}

static void set_sticky (char *data, List *ent_list, char *short_pathname, char *filename)
{
    char *tagspec;
    FILE *f;

    read_line (&tagspec);

    /* FIXME-update-dir: error messages should include the directory.  */
    f = CVS_FOPEN (CVSADM_TAG, "w+");
    if (f == NULL)
    {
	/* Making this non-fatal is a bit of a kludge (see dirs2
	   in testsuite).  A better solution would be to avoid having
	   the server tell us about a directory we shouldn't be doing
	   anything with anyway (e.g. by handling directory
	   addition/removal better).  */
	error (0, errno, "cannot open %s", CVSADM_TAG);
	xfree (tagspec);
	return;
    }
    if (fprintf (f, "%s\n", tagspec) < 0)
	error (1, errno, "writing %s", CVSADM_TAG);
    if (fclose (f) == EOF)
	error (1, errno, "closing %s", CVSADM_TAG);
    xfree (tagspec);
}

static void handle_set_sticky (char *pathname, int len)
{
    if (strcmp (command_name, "export") == 0)
    {
	/* Swallow the repository.  */
	read_line (NULL);
        /* Swallow the tag line.  */
	read_line (NULL);
	return;
    }
    if (is_cvsroot_level (pathname))
    {
        /*
	 * Top level (directory containing CVSROOT).  This seems to normally
	 * lack a CVS directory, so don't try to create files in it.
	 */

	/* Swallow the repository.  */
	read_line (NULL);
        /* Swallow the tag line.  */
	read_line (NULL);
	return;
    }

    call_in_directory (pathname, set_sticky, (char *)NULL);
}

static void clear_sticky (char *data, List *ent_list, char *short_pathname, char *filename)
{
    if (unlink_file (CVSADM_TAG) < 0 && ! existence_error (errno))
		error (1, errno, "cannot remove %s", CVSADM_TAG);
}

static void handle_clear_sticky (char *pathname, int len)
{
    if (strcmp (command_name, "export") == 0)
    {
	/* Swallow the repository.  */
	read_line (NULL);
	return;
    }

    if (is_cvsroot_level (pathname))
    {
        /*
	 * Top level (directory containing CVSROOT).  This seems to normally
	 * lack a CVS directory, so don't try to create files in it.
	 */
	return;
    }

    call_in_directory (pathname, clear_sticky, (char *)NULL);
}


static void templat (char *data, List *ent_list, char *short_pathname, char *filename)
{
    /* FIXME: should be computing second argument from CVSADM_TEMPLATE
       and short_pathname.  */
    read_counted_file (CVSADM_TEMPLATE, "<CVS/Template file>");
}

static void handle_template (char *pathname, int len)
{
    call_in_directory (pathname, templat, NULL);
}

struct save_dir {
    char *dir;
    struct save_dir *next;
};

struct save_dir *prune_candidates;

static void add_prune_candidate (const char *dir)
{
    struct save_dir *p;

    if ((dir[0] == '.' && dir[1] == '\0')
	|| (prune_candidates != NULL
	    && strcmp (dir, prune_candidates->dir) == 0))
	return;
    p = (struct save_dir *) xmalloc (sizeof (struct save_dir));
    p->dir = xstrdup (dir);
    p->next = prune_candidates;
    prune_candidates = p;
}

static void process_prune_candidates ()
{
    struct save_dir *p;
    struct save_dir *q;
	char *saved_cwd;

    if (toplevel_wd != NULL)
    {
	if (CVS_CHDIR (toplevel_wd) < 0)
	    error (1, errno, "could not chdir to %s", toplevel_wd);
    }
	saved_cwd = xgetwd();
    for (p = prune_candidates; p != NULL; )
    {
		if (isemptydir (p->dir, 1))
		{
			List *ents = NULL;
			char *dir = xstrdup(p->dir),*qq,*dirnm;

			for(qq=dir+strlen(dir)-1; qq>dir; qq--)
				if(ISDIRSEP(*qq))
					break;
			if(qq>dir)
			{
				*qq='\0';
				dirnm=qq+1;
				CVS_CHDIR(dir);
				ents = Entries_Open(0,dir);
			}
			else
			{
				dirnm=dir;
				ents = Entries_Open(0,".");
			}

			if (unlink_file_dir (dirnm) < 0)
				error (0, errno, "cannot remove %s", p->dir);
			Subdir_Deregister (ents, ".", dirnm);
			if(ents)
				Entries_Close(ents);
			xfree(dir);
		}
		xfree (p->dir);
		q = p->next;
		xfree (p);
		p = q;
		CVS_CHDIR(saved_cwd);
    }
	xfree(saved_cwd);
    prune_candidates = NULL;
}

void send_renames(const char *dir)
{
	char from[MAX_PATH],to[MAX_PATH];
	char *ren = (char*)xmalloc(strlen(dir)+sizeof(CVSADM_RENAME)+sizeof(CVSADM_VIRTREPOS)+20);
	FILE *fp;

	if(!dir[0]) dir="./";

	sprintf(ren,"%s/%s",dir,CVSADM_RENAME);
	if(isfile(ren) && supported_request("Rename"))
	{
		fp = CVS_FOPEN(ren,"r");
		if(!fp)
			error(1,errno,"Couldn't open %s",ren);
		while(fgets(from,sizeof(from),fp) && fgets(to,sizeof(to),fp))
		{
			send_to_server("Rename ", 0);
			send_to_server(from, 0);
			send_to_server(to, 0);
		}
		fclose(fp);
	}

	sprintf(ren,"%s/%s",dir,CVSADM_VIRTREPOS);
	if(isfile(ren) && supported_request("VirtualRepository"))
	{
		fp = CVS_FOPEN(ren,"r");
		if(!fp)
			error(1,errno,"Couldn't open %s",ren);
		while(fgets(from,sizeof(from),fp))
		{
			send_to_server("VirtualRepository ", 0);
			send_to_server(from, 0);
		}
		fclose(fp);
	}
	xfree(ren);
}

/* Send a Repository line.  */

static char *last_repos;
static char *last_update_dir;

static void send_repository(const char *dir, const char *repos, const char *update_dir)
{
    char *adm_name;
	FILE *f;
    char line[MAX_PATH];

	TRACE(3,"send_repository(%s,%s,%s)",(dir)?dir:"NULL",(repos)?repos:"NULL",(update_dir)?update_dir:"NULL");
    /* FIXME: this is probably not the best place to check; I wish I
     * knew where in here's callers to really trap this bug.  To
     * reproduce the bug, just do this:
     * 
     *       mkdir junk
     *       cd junk
     *       cvs -d some_repos update foo
     *
     * Poof, CVS seg faults and dies!  It's because it's trying to
     * send a NULL string to the server but dies in send_to_server.
     * That string was supposed to be the repository, but it doesn't
     * get set because there's no CVSADM dir, and somehow it's not
     * getting set from the -d argument either... ?
     */
    if (repos == NULL)
    {
        /* Lame error.  I want a real fix but can't stay up to track
           this down right now. */
        error (1, 0, "no repository");
    }

    if (update_dir == NULL || update_dir[0] == '\0')
	update_dir = ".";

    if (last_repos != NULL
	&& strcmp (repos, last_repos) == 0
	&& last_update_dir != NULL
	&& strcmp (update_dir, last_update_dir) == 0)
	{
	/* We've already sent it.  */
	return;
	}

    if (client_prune_dirs)
	{
		add_prune_candidate (update_dir);
	}

    /* Add a directory name to the list of those sent to the
       server. */
    if (update_dir && (*update_dir != '\0')
	&& (strcmp (update_dir, ".") != 0)
	&& (findnode_fn (dirs_sent_to_server, update_dir) == NULL))
    {
	Node *n;
	n = getnode ();
	n->type = NT_UNKNOWN;
	n->key = xstrdup (update_dir);
	n->data = NULL;

	if (dirs_sent_to_server==NULL)
	{
		TRACE(3,"create a list dirs_sent_to_server");
	    dirs_sent_to_server = getlist ();
	}

	if (addnode (dirs_sent_to_server, n))
	    error (1, 0, "cannot add directory %s to list", n->key);
    }

    /* 80 is large enough for any of CVSADM_*.  */
	TRACE(3,"allocate adm_name * 80 is large enough for any of CVSADM_*.  ");
    adm_name = (char*)xmalloc (strlen (dir) + 80);

    send_to_server ("Directory ", 0);
    {
		/* Send the directory name.  I know that this
		sort of duplicates code elsewhere, but each
		case seems slightly different...  */
		TRACE(3,"* Send the directory name.  ");
		send_filename (update_dir, false, true);
	}
    send_to_server ("\n", 1);
	adm_name[0] = '\0';
	if (dir[0] != '\0')
	{
	    strcat (adm_name, dir);
	    strcat (adm_name, "/");
	}
	strcat (adm_name, CVSADM_VIRTREPOS);
	if(isfile(adm_name))
	{
	  adm_name[0] = '\0';
	  if (dir[0] != '\0')
	  {
	    strcat (adm_name, dir);
	    strcat (adm_name, "/");
	  }
	  strcat (adm_name, CVSADM_REP);
	  f = CVS_FOPEN (adm_name, "r");
	  if (f == NULL)
		error (1, errno, "reading %s", adm_name);
	  fgets (line, sizeof (line), f);
	  line[strlen(line)-1]='\0';
	  if(!isabsolute(line))
	  {
		send_to_server(current_parsed_root->directory,0);
		send_to_server("/",1);
	  }
	  send_to_server(line,0);
          if (fclose (f) == EOF)
		error (0, errno, "closing %s", adm_name);
	}
	else
		send_to_server(repos,0);
	send_to_server("\n",1);

    if (supported_request ("Static-directory"))
    {
	adm_name[0] = '\0';
	if (dir[0] != '\0')
	{
	    strcat (adm_name, dir);
	    strcat (adm_name, "/");
	}
	strcat (adm_name, CVSADM_ENTSTAT);
	if (isreadable (adm_name))
	{
	    send_to_server ("Static-directory\n", 0);
	}
    }
    if (supported_request ("Sticky"))
    {
	if (dir[0] == '\0')
	    strcpy (adm_name, CVSADM_TAG);
	else
	    sprintf (adm_name, "%s/%s", dir, CVSADM_TAG);

	f = CVS_FOPEN (adm_name, "r");
	if (f == NULL)
	{
	    if (! existence_error (errno))
		error (1, errno, "reading %s", adm_name);
	}
	else
	{
	    char *nl = NULL;
	    send_to_server ("Sticky ", 0);
	    while (fgets (line, sizeof (line), f) != NULL)
	    {
		// why don't we do this? line[strlen(line)-1]='\0';
		send_to_server (line, 0);
		nl = strchr (line, '\n');
		if (nl != NULL)
		    break;
	    }
	    if (nl == NULL)
                send_to_server ("\n", 1);
	    if (fclose (f) == EOF)
		error (0, errno, "closing %s", adm_name);
	}
    }
	send_renames(dir);
    xfree (adm_name);
	adm_name=NULL;
    if (last_repos != NULL)
	{
	xfree (last_repos);
	last_repos=NULL;
	}
    if (last_update_dir != NULL)
	{
	xfree (last_update_dir);
	last_update_dir=NULL;
	}
    last_repos = xstrdup (repos);
    last_update_dir = xstrdup (update_dir);
}

/* Send a Repository line and set toplevel_repos.  */

void send_a_repository (const char *dir, const char *repository, const char *update_dir)
{
	TRACE(3,"send_a_repository(%s,%s,%s)",(dir)?dir:"NULL",(repository)?repository:"NULL",(update_dir)?update_dir:"NULL");
    if (toplevel_repos == NULL && repository != NULL)
    {
	if (update_dir[0] == '\0'
	    || (update_dir[0] == '.' && update_dir[1] == '\0'))
	    toplevel_repos = xstrdup (repository);
	else
	{
	    /*
	     * Get the repository from a CVS/Repository file if update_dir
	     * is absolute.  This is not correct in general, because
	     * the CVS/Repository file might not be the top-level one.
	     * This is for cases like "cvs update /foo/bar" (I'm not
	     * sure it matters what toplevel_repos we get, but it does
	     * matter that we don't hit the "internal error" code below).
	     */
	    if (update_dir[0] == '/')
		{
		toplevel_repos = Name_Repository (update_dir, update_dir);
		}
	    else
	    {
		/*
		 * Guess the repository of that directory by looking at a
		 * subdirectory and removing as many pathname components
		 * as are in update_dir.  I think that will always (or at
		 * least almost always) be 1.
		 *
		 * So this deals with directories which have been
		 * renamed, though it doesn't necessarily deal with
		 * directories which have been put inside other
		 * directories (and cvs invoked on the containing
		 * directory).  I'm not sure the latter case needs to
		 * work.
		 *
		 * 21 Aug 1998: Well, Mr. Above-Comment-Writer, it
		 * does need to work after all.  When we are using the
		 * client in a multi-cvsroot environment, it will be
		 * fairly common that we have the above case (e.g.,
		 * cwd checked out from one repository but
		 * subdirectory checked out from another).  We can't
		 * assume that by walking up a directory in our wd we
		 * necessarily walk up a directory in the repository.
		 */
		/*
		 * This gets toplevel_repos wrong for "cvs update ../foo"
		 * but I'm not sure toplevel_repos matters in that case.
		 */
		/* Well actually it does...  It causes the server to try
		   to traverse outside the repository root, which is a major deal.
		   We fix this in the server as there are lot of clients that get
		   this wrong. */

		int repository_len, update_dir_len;

		repository_len = (repository)? strlen (repository) : 0;
		update_dir_len = (update_dir)? strlen (update_dir) : 0;
		while (update_dir_len >= 0 && ISDIRSEP(update_dir[update_dir_len-1]))
			update_dir_len--;

		/* Try to remove the path components in UPDATE_DIR
                   from REPOSITORY.  If the path elements don't exist
                   in REPOSITORY, or the removal of those path
                   elements mean that we "step above"
                   current_parsed_root->directory, set toplevel_repos to
                   current_parsed_root->directory. */
		if ((repository_len > update_dir_len)
		    && (fncmp (repository + repository_len - update_dir_len,
				update_dir) == 0)
		    /* TOPLEVEL_REPOS shouldn't be above current_parsed_root->directory */
		    && ((repository_len - update_dir_len)
			> strlen (current_parsed_root->directory)))
		{
		    /* The repository name contains UPDATE_DIR.  Set
                       toplevel_repos to the repository name without
                       UPDATE_DIR. */

		    toplevel_repos = (char*)xmalloc (repository_len - update_dir_len + 10);
		    /* Note that we don't copy the trailing '/'.  */
		    strncpy (toplevel_repos, repository,
			     repository_len - update_dir_len - 1);
		    toplevel_repos[repository_len - update_dir_len - 1] = '\0';
		}
		else
		{
				toplevel_repos = xstrdup (current_parsed_root->directory);
		}
	    }
	}
    }

    send_repository (dir, repository, update_dir);
}

/* Receive a cvswrappers line from the server; it must be a line
   containing an RCS option (e.g., "*.exe   -k 'b'").

   Note that this doesn't try to handle -t/-f options (which are a
   whole separate issue which noone has thought much about, as far
   as I know).

   We need to know the keyword expansion mode so we know whether to
   read the file in text or binary mode.  */

static void handle_wrapper_rcs_option (char *args, int len)
{
    char *p;

    /* Enforce the notes in cvsclient.texi about how the response is not
       as free-form as it looks.  */
    p = strchr (args, ' ');
    if (p == NULL)
		goto handle_error;

    /* Add server-side cvswrappers line to our wrapper list. */
    wrap_add (args, false, true, true, false);
    return;
 handle_error:
    error (0, errno, "protocol error: ignoring invalid wrappers %s", args);
}

static void handle_entries_extra(char *args, int len)
{
	xfree(extra_entry);
	extra_entry = xstrdup(args);
}

static void handle_rename(char *args, int len)
{
	FILE *fp;
	char *from, *to;

	fp = fopen(CVSADM_RENAME,"a");
	if(!fp)
		error(1,errno,"Couldn't open %s",CVSADM_RENAME);

	from = args;
    read_line (&to);
	
	if (isabsolute(from) || strstr(from,".."))
		error(1,0,"Protocol error: Bad rename %s",from);

	if (isabsolute(to) || strstr(to,".."))
		error(1,0,"Protocol error: Bad rename %s",to);

	fprintf(fp,"%s\n%s\n",from,to);

	xfree(to);
	fclose(fp);
}


static void handle_m (char *args, int len)
{
    /* In the case where stdout and stderr point to the same place,
       fflushing stderr will make output happen in the correct order.
       Often stderr will be line-buffered and this won't be needed,
       but not always (is that true?  I think the comment is probably
       based on being confused between default buffering between
       stdout and stderr.  But I'm not sure).  */
    fflush (stderr);
    cvs_output(args, len);
    cvs_output("\n", 1);
}

static void handle_mbinary (char *args, int len)
{
    char *size_string;
    size_t size;
    size_t totalread;
    size_t nread;
    size_t toread;
    char buf[8192];

    /* See comment at handle_m about (non)flush of stderr.  */

    /* Get the size.  */
    read_line (&size_string);
    size = atoi (size_string);
    xfree (size_string);

    /* OK, now get all the data.  The algorithm here is that we read
       as much as the network wants to give us in
       try_read_from_server, and then we output it all, and then
       repeat, until we get all the data.  */
    totalread = 0;
    while (totalread < size)
    {
	toread = size - totalread;
	if (toread > sizeof buf)
	    toread = sizeof buf;

	nread = try_read_from_server (buf, toread);
	cvs_output_binary (buf, nread);
	totalread += nread;
    }
}

static void handle_e (char *args, int len)
{
	if(global_ls_response_hack && strstr(args,"checkout: "))
	{
		char *p=strchr(args,':')+2;
		if(!strncmp(p,"Updating",8))
		{
			if(!(global_ls_response_hack&4))
			{
				if(!strcmp(p+9,"."))
					printf("Listing modules on server\n\n");
				else
					printf("Listing module: %s\n\n",p+9);
			}
			return;

		}
		else if(!strncmp(p,"New directory",13))
		{
			const char *fn=p+15,*q;
			p=(char*)strchr(fn,'\'');
			if(!p) p=(char*)strchr(fn,'`');
			if(p)
			{
				q=strrchr(fn,'/');
				if(!q) q=fn;
				else q++;
				if(global_ls_response_hack&2)
					printf("D/%-*.*s////\n",p-q,p-q,q);
				else
					printf("%-*.*s\n",p-q,p-q,q);
				return;
			}
		}
		else
		{

			// Fixup the error so it looks like an ls message
			char *q=p-6;
			strcpy(q-3,"ls:");
			strcpy(q,p);
		}
	}
    /* In the case where stdout and stderr point to the same place,
       fflushing stdout will make output happen in the correct order.  */
    fflush (stdout);
    cvs_outerr(args, len);
    cvs_outerr("\n", 1);
}

/*ARGSUSED*/
static void handle_f (char *args, int len)
{
    fflush (stderr);
}

static void handle_mt (char *args, int len)
{
    char *p;
    char *tag = args;
    char *text;

    /* See comment at handle_m for more details.  */
    fflush (stderr);

    p = strchr (args, ' ');
    if (p == NULL)
	text = NULL;
    else
    {
	*p++ = '\0';
	text = p;
    }

    switch (tag[0])
    {
	case '+':
	    if (strcmp (tag, "+updated") == 0)
		updated_seen = 1;
	    else if (strcmp (tag, "+importmergecmd") == 0)
		importmergecmd.seen = 1;
	    break;
	case '-':
	    if (strcmp (tag, "-updated") == 0)
		updated_seen = 0;
	    else if (strcmp (tag, "-importmergecmd") == 0)
	    {
		char buf[80];

		/* Now that we have gathered the information, we can
                   output the suggested merge command.  */

		if (importmergecmd.conflicts == 0
		    || importmergecmd.mergetag1 == NULL
		    || importmergecmd.mergetag2 == NULL
		    || importmergecmd.repository == NULL)
		{
		    error (0, 0,
			   "invalid server: incomplete importmergecmd tags");
		    break;
		}

		sprintf (buf, "\n%d conflicts created by this import.\n",
			 importmergecmd.conflicts);
		cvs_output (buf, 0);
		cvs_output ("Use the following command to help the merge:\n\n",
			    0);
		cvs_output ("\t", 1);
		cvs_output (program_name, 0);
		if (CVSroot_cmdline != NULL)
		{
		    cvs_output (" -d ", 0);
		    cvs_output (CVSroot_cmdline, 0);
		}
		cvs_output (" checkout -j", 0);
		cvs_output (importmergecmd.mergetag1, 0);
		cvs_output (" -j", 0);
		cvs_output (importmergecmd.mergetag2, 0);
		cvs_output (" ", 1);
		cvs_output (importmergecmd.repository, 0);
		cvs_output ("\n\n", 0);

		/* Clear the static variables so that everything is
                   ready for any subsequent importmergecmd tag.  */
		importmergecmd.conflicts = 0;
		xfree (importmergecmd.mergetag1);
		importmergecmd.mergetag1 = NULL;
		xfree (importmergecmd.mergetag2);
		importmergecmd.mergetag2 = NULL;
		xfree (importmergecmd.repository);
		importmergecmd.repository = NULL;

		importmergecmd.seen = 0;
	    }
	    break;
	default:
	    if (updated_seen)
	    {
		if (strcmp (tag, "fname") == 0)
		{
		    if (updated_fname != NULL)
		    {
				if(global_ls_response_hack)
				{
					const char *p=strrchr(updated_fname,'/');
					if(p)
					{
						if(global_ls_response_hack&2)
							printf("/%s////\n",p+1);
						else
							printf("%s\n",p+1);
					}
				}
				else
				{
					/* Output the previous message now.  This can happen
					if there was no Update-existing or other such
					response, due to the -n global option.  */
					cvs_output ("U ", 0);
					cvs_output (updated_fname, 0);
					cvs_output ("\n", 1);
				}
				xfree (updated_fname);
		    }
		    updated_fname = xstrdup (text);
		}
		/* Swallow all other tags.  Either they are extraneous
		   or they reflect future extensions that we can
		   safely ignore.  */
	    }
	    else if (importmergecmd.seen)
	    {
		if (strcmp (tag, "conflicts") == 0)
		    importmergecmd.conflicts = atoi (text);
		else if (strcmp (tag, "mergetag1") == 0)
		    importmergecmd.mergetag1 = xstrdup (text);
		else if (strcmp (tag, "mergetag2") == 0)
		    importmergecmd.mergetag2 = xstrdup (text);
		else if (strcmp (tag, "repository") == 0)
		    importmergecmd.repository = xstrdup (text);
		/* Swallow all other tags.  Either they are text for
                   which we are going to print our own version when we
                   see -importmergecmd, or they are future extensions
                   we can safely ignore.  */
	    }
	    else if (strcmp (tag, "newline") == 0)
		cvs_output("\n", 1);
	    else if (text != NULL)
			cvs_output(text, 0);
    }
}

/*ARGSUSED*/
static void handle_notranslatebegin(char *args, int len)
{
	if(server_codepage_translation)
		server_codepage_translation=-1;
}

/*ARGSUSED*/
static void handle_notranslateend(char *args, int len)
{
	if(server_codepage_translation)
		server_codepage_translation=1;
}

static void proxy_line1(char *cmd)
{
#ifdef SERVER_SUPPORT
	cvs_output_raw(cmd,0,false);
	cvs_output_raw("\n",1,false);
	cmd=NULL;
	read_line(&cmd);
	cvs_output_raw(cmd,0,false);
	cvs_output_raw("\n",1,true);
	xfree(cmd);
#endif
}

static void proxy_line2(char *cmd)
{
#ifdef SERVER_SUPPORT
	cvs_output_raw(cmd,0,false);
	cvs_output_raw("\n",1,false);
	cmd=NULL;
	read_line(&cmd);
	cvs_output_raw(cmd,0,false);
	cvs_output_raw("\n",1,false);
	xfree(cmd);
	read_line(&cmd);
	cvs_output_raw(cmd,0,false);
	cvs_output_raw("\n",1,true);
	xfree(cmd);
#endif
}

static void proxy_updated(char *cmd)
{
#ifdef SERVER_SUPPORT
	int size,n;
	char *buf;

	proxy_line2(cmd);
	cmd=NULL;
	read_line(&cmd);
	cvs_output_raw(cmd,0,false);
	cvs_output_raw("\n",1,true);
	xfree(cmd);
	read_line(&cmd);
	size = atoi(cmd);
	cvs_output_raw(cmd,0,false);
	cvs_output_raw("\n",1,true);
	xfree(cmd);

	if(size)
	{
		buf = (char*)xmalloc(size);
		while (size > 0)
		{
			n = try_read_from_server (buf, size);
			size -= n;
		        cvs_output_raw(buf,n,size?false:true);
		}
		xfree(buf);
	}
#endif
}

static void proxy_file(char *cmd)
{
#ifdef SERVER_SUPPORT
	int size,n;
	char *buf;

	cvs_output_raw(cmd,0,false);
	cvs_output_raw("\n",1,false);
	cmd=NULL;
	read_line(&cmd);
	size = atoi(cmd);
	cvs_output_raw(cmd,0,false);
	cvs_output_raw("\n",1,true);
	xfree(cmd);

	if(size)
	{
		buf = (char*)xmalloc(size);
		while (size > 0)
		{
			n = try_read_from_server (buf, size);
			size -= n;
			cvs_output_raw(buf,n,size?false:true);
		}
		xfree(buf);
	}
#endif
}

/* This table must be writeable if the server code is included.  */
struct response responses[] =
{
#define RSP_LINE(n, f, p, t, s) {n, f, p, t, s}

    RSP_LINE("ok", handle_ok, NULL, response_type_ok, rs_essential),
    RSP_LINE("error", handle_error, NULL, response_type_error, rs_essential),
    RSP_LINE("Valid-requests", handle_valid_requests, NULL, response_type_normal,
       rs_essential),
    RSP_LINE("Checked-in", handle_checked_in, proxy_line2, response_type_normal,
       rs_essential),
    RSP_LINE("New-entry", handle_new_entry, proxy_line2, response_type_normal, rs_optional),
    RSP_LINE("Checksum", handle_checksum, NULL, response_type_normal, rs_optional),
    RSP_LINE("Copy-file", handle_copy_file, proxy_line1, response_type_normal, rs_optional),
    RSP_LINE("Updated", handle_updated, proxy_updated, response_type_normal, rs_essential),
    RSP_LINE("Created", handle_created, proxy_updated, response_type_normal, rs_optional),
    RSP_LINE("Update-existing", handle_update_existing, proxy_updated, response_type_normal,
       rs_optional),
    RSP_LINE("Merged", handle_merged, proxy_updated, response_type_normal, rs_essential),
    RSP_LINE("Patched", handle_patched, proxy_updated, response_type_normal, rs_optional),
    RSP_LINE("Rcs-diff", handle_rcs_diff, proxy_updated, response_type_normal, rs_optional),
    RSP_LINE("Update-baserev", handle_update_baserev, proxy_updated, response_type_normal, rs_optional),
    RSP_LINE("Mode", handle_mode, NULL, response_type_normal, rs_optional),
    RSP_LINE("Mod-time", handle_mod_time, NULL, response_type_normal, rs_optional),
    RSP_LINE("Removed", handle_removed, NULL, response_type_normal, rs_essential),
	RSP_LINE("Renamed", handle_renamed, NULL, response_type_normal, rs_optional),
    RSP_LINE("Remove-entry", handle_remove_entry, NULL, response_type_normal,
       rs_optional),
    RSP_LINE("Set-static-directory", handle_set_static_directory, proxy_line1,
       response_type_normal,
       rs_optional),
    RSP_LINE("Clear-static-directory", handle_clear_static_directory, proxy_line1,
       response_type_normal,
       rs_optional),
    RSP_LINE("Set-sticky", handle_set_sticky, proxy_line2, response_type_normal,
       rs_optional),
    RSP_LINE("Clear-sticky", handle_clear_sticky, proxy_line1, response_type_normal, 
       rs_optional),
    RSP_LINE("Template", handle_template, proxy_file, response_type_normal,
       rs_optional),
    RSP_LINE("Notified", handle_notified, NULL, response_type_normal, rs_optional),
    RSP_LINE("Module-expansion", handle_module_expansion, NULL, response_type_normal, 
       rs_optional),
    RSP_LINE("Wrapper-rcsOption", handle_wrapper_rcs_option, NULL,
       response_type_normal,
       rs_optional),
    RSP_LINE("Clear-rename", handle_clear_rename, NULL, response_type_normal, rs_optional),
	RSP_LINE("Rename", handle_rename, NULL, response_type_normal, rs_optional), 
	RSP_LINE("EntriesExtra", handle_entries_extra, NULL, response_type_normal, rs_optional), 
    RSP_LINE("M", handle_m, NULL, response_type_normal, rs_essential), 
    RSP_LINE("Mbinary", handle_mbinary, proxy_file, response_type_normal, rs_optional),
    RSP_LINE("E", handle_e, NULL, response_type_normal, rs_essential),
    RSP_LINE("F", handle_f, NULL, response_type_normal, rs_optional),
    RSP_LINE("MT", handle_mt, NULL, response_type_normal, rs_optional),
	RSP_LINE("NoTranslateBegin", handle_notranslatebegin, NULL, response_type_normal, rs_optional),
	RSP_LINE("NoTranslateEnd", handle_notranslateend, NULL, response_type_normal, rs_optional),
    /* Possibly should be response_type_error.  */
    RSP_LINE(NULL, NULL, NULL, response_type_normal, rs_essential)

#undef RSP_LINE
};

void send_to_server (const char *str, size_t len)
{
  if(server_codepage_translation>0)
  {
	void *buf = NULL;
	int ret = CCodepage::TranscodeBuffer(client_codepage,server_codepage,str,len,buf,len);
	if(ret>0)
	{
		TRACE(3,"Translation from client codepage '%s' to server codepage '%s' lost characters",client_codepage, server_codepage);
	}
	if(ret<0)
	{
		TRACE(3,"Translation not posible - disabling");
		server_codepage_translation = 0;
		if(force_locale)
		{
			error(0,0,"Couldn't translate to codepage %s - disabling translation",force_locale);
		}
	}
	send_to_server_untranslated((const char *)buf,len);
	xfree(buf);
  }
  else
    send_to_server_untranslated(str,len);
}

/* 
 * If LEN is 0, then send_to_server() computes string's length itself.
 *
 * Therefore, pass the real length when transmitting data that might
 * contain 0's.
 */
void send_to_server_untranslated(const char *str, size_t len)
{
    static int nbytes;

    if (len == 0)
	len = strlen (str);

    buf_output (to_server, str, len);
      
    /* There is no reason not to send data to the server, so do it
       whenever we've accumulated enough information in the buffer to
       make it worth sending.  */
    nbytes += len;
    if (nbytes >= 2 * BUFFER_DATA_SIZE)
    {
	int status;

        status = buf_send_output (to_server);
	if (status != 0)
	    error (1, status, "error writing to server");
	nbytes = 0;
    }
}

/* Read up to LEN bytes from the server.  Returns actual number of
   bytes read, which will always be at least one; blocks if there is
   no data available at all.  Gives a fatal error on EOF or error.  */
size_t try_read_from_server (char *buf, size_t len)
{
    int status, nread;
    char *data;

    status = buf_read_data (from_server, len, &data, &nread);
    if (status != 0)
    {
	if (status == -1)
	    error (1, 0,
		   "end of file from server (consult above messages if any)");
	else if (status == -2)
	    error (1, 0, "out of memory in client.c");
	else
	    error (1, status, "reading from server");
    }

    memcpy (buf, data, nread);

    return nread;
}

/*
 * Read LEN bytes from the server or die trying.
 */
void read_from_server (char *buf, size_t len)
{
    size_t red = 0;
    while (red < len)
    {
	red += try_read_from_server (buf + red, len - red);
	if (red == len)
	    break;
    }
}

/*
 * Get some server responses and process them.  Returns nonzero for
 * error, 0 for success.  */
int get_server_responses ()
{
    struct response *rs;
    do
    {
	char *cmd;
	int len;
	
	len = read_line (&cmd);
	for (rs = responses; rs->name != NULL; ++rs)
	    if (strncmp (cmd, rs->name, strlen (rs->name)) == 0)
	    {
		int cmdlen = strlen (rs->name);
		if (cmd[cmdlen] == '\0')
		    ;
		else if (cmd[cmdlen] == ' ')
		    ++cmdlen;
		else
		    /*
		     * The first len characters match, but it's a different
		     * response.  e.g. the response is "oklahoma" but we
		     * matched "ok".
		     */
		    continue;
#ifdef SERVER_SUPPORT
		if(proxy_active)
		{
			// We are proxying for a client.  Don't execute this commmand, proxy it
			if(rs->proxy)
				(*rs->proxy) (cmd);
			else
			{
				cvs_output_raw(cmd,0,false);
				cvs_output_raw("\n",1,true);
			}
		}
		else
#endif
		{
			// Local command
			(*rs->func) (cmd + cmdlen, len - cmdlen);
		}
		break;
	    }
	if (rs->name == NULL)
	    /* It's OK to print just to the first '\0'.  */
	    /* We might want to handle control characters and the like
	       in some other way other than just sending them to stdout.
	       One common reason for this error is if people use :ext:
	       with a version of rsh which is doing CRLF translation or
	       something, and so the client gets "ok^M" instead of "ok".
	       Right now that will tend to print part of this error
	       message over the other part of it.  It seems like we could
	       do better (either in general, by quoting or omitting all
	       control characters, and/or specifically, by detecting the CRLF
	       case and printing a specific error message).  */
	    error (0, 0,
		   "warning: unrecognized response `%s' from cvs server",
		   cmd);
	xfree (cmd);
    } while (rs->type == response_type_normal);

    if (updated_fname != NULL)
    {
		if(global_ls_response_hack)
		{
			const char *p=strrchr(updated_fname,'/');
			if(p)
			{
				if(global_ls_response_hack&2)
					printf("/%s////\n",p+1);
				else
					printf("%s\n",p+1);
			}
		}
		else
		{
			/* Output the previous message now.  This can happen
			if there was no Update-existing or other such
			response, due to the -n global option.  */
			cvs_output ("U ", 0);
			cvs_output (updated_fname, 0);
			cvs_output ("\n", 1);
		}
		xfree (updated_fname);
		updated_fname = NULL;
    }

    if (rs->type == response_type_error)
	return 1;
    if (failure_exit)
	return 1;
    return 0;
}

int get_server_responses_noproxy()
{
#ifdef SERVER_SUPPORT
	int _prox=proxy_active;
	proxy_active=0;
#endif
	int ret = get_server_responses();
#ifdef SERVER_SUPPORT
	proxy_active=_prox;
#endif
	return ret;
}

/*
 * Flag var; we'll set it in start_server() and not one of its
 * callees, such as start_rsh_server().  This means that there might
 * be a small window between the starting of the server and the
 * setting of this var, but all the code in that window shouldn't care
 * because it's busy checking return values to see if the server got
 * started successfully anyway.
 */
int server_started = 0;

int cleanup_and_close_server()
{
	int status;

    if (last_entries != NULL)
    {
		Entries_Close (last_entries);
		last_entries = NULL;
    }

    if (client_prune_dirs)
		process_prune_candidates ();

	/* The calls to buf_shutdown are currently only meaningful when we
       are using compression.  First we shut down TO_SERVER.  That
       tells the server that its input is finished.  It then shuts
       down the buffer it is sending to us, at which point our shut
       down of FROM_SERVER will complete.  */

    status = buf_shutdown (to_server);
    if (status != 0)
        error (0, status, "shutting down buffer to server");
    status = buf_shutdown (from_server);
    if (status != 0)
	error (0, status, "shutting down buffer from server");

	if(client_protocol->shutdown && client_protocol->shutdown(client_protocol))
	    error (1, 0, "shutting down server socket");

	if(client_protocol->disconnect && client_protocol->disconnect(client_protocol))
	    error (1, 0, "disconnecting server socket");
         
    buf_free (to_server);
    buf_free (from_server);
    server_started = 0;

    /* see if we need to sleep before returning to avoid time-stamp races */
    if (last_register_time)
    {
		sleep_past (last_register_time);
    }

	return status;
}

int get_responses_and_close ()
{
	int errs = get_server_responses();
	cleanup_and_close_server();
	return errs;
}
	
int supported_request (const char *name)
{
    struct request *rq;

    for (rq = requests; rq->name; rq++)
	if (!strcmp (rq->name, name))
	    return (rq->flags & RQ_SUPPORTED) != 0;
    error (1, 0, "internal error: testing support for unknown option %s?",name);
    /* NOTREACHED */
    return 0;
}



/* Read a line from socket SOCK.  Result does not include the
   terminating linefeed.  This is only used by the authentication
   protocol, which we call before we set up all the buffering stuff.
   It is possible it should use the buffers too, which would be faster
   (unlike the server, there isn't really a security issue in terms of
   separating authentication from the rest of the code).

   Space for the result is malloc'd and should be freed by the caller.

   Returns number of bytes read.  */
static int recv_line (char **resultp)
{
    char *result;
    size_t input_index = 0;
    size_t result_size = 80;

    result = (char *) xmalloc (result_size);

    while (1)
    {
	char ch;
	int n;
	n = client_protocol->read_data(client_protocol,&ch, 1);
	if (n <= 0)
	    error (1, errno, "Error reading from server %s: %d", current_parsed_root->hostname, n );

	if (ch == '\n')
	    break;

	result[input_index++] = ch;
	while (input_index + 1 >= result_size)
	{
	    result_size *= 2;
	    result = (char *) xrealloc (result, result_size);
	}
    }

    if (resultp)
	*resultp = result;

    /* Terminate it just for kicks, but we *can* deal with embedded NULs.  */
    result[input_index] = '\0';

    if (resultp == NULL)
	xfree (result);
    return input_index;
}

static void send_variable_proc (variable_list_t::const_reference item)
{
    send_to_server ("Set ", 0);
    send_to_server (item.first.c_str(), 0);
    send_to_server ("=", 1);
    send_to_server (item.second.c_str(), 0);
    send_to_server ("\n", 1);
}

#ifdef _WIN32
bool SendStatistics(char *url)
{
	CHttpSocket sock;

	if(!sock.create("http://march-hare.com"))
	{
		TRACE(3,"Failed to send statistics.  Server not responding");
		return false;
	}

	if(!sock.request("GET",url))
	{
		// Not sure this can actually fail.. it would normally succeed with an error code
		TRACE(3,"Failed to send statistics.  Server not responding to GET requests");
		return false;
	}

	if(sock.responseCode()!=200)
	{
		TRACE(3,"Failed to send statistics.  Server returned error %d\n",sock.responseCode());
		return false;
	}

	return true;
}
#endif

/* Contact the server.  */
int start_server (int verify_only)
{
    const char *log = CProtocolLibrary::GetEnvironment ("CVS_CLIENT_LOG");
	char *read_buf;
	int connect_state;
	int rootless_encryption;
	int status;

	to_server = NULL;
	from_server = NULL;

	TRACE(3,"start_server(%d)",verify_only);


    /* Clear our static variables for this invocation. */
    if (toplevel_repos != NULL)
	xfree (toplevel_repos);
    toplevel_repos = NULL;

	TRACE(2,"client start - client_protocol->connect");
	connect_state = client_protocol->connect(client_protocol,verify_only);
	if(connect_state==CVSPROTO_AUTHFAIL)
	{
		if(current_parsed_root->username)
		{
			if(client_protocol->logout)
				client_protocol->logout(client_protocol);
			error (1, 0,
				"authorization failed: server %s rejected access to %s for user %s",
				current_parsed_root->hostname, current_parsed_root->directory, current_parsed_root->username);
		}
		else
		{
			error (1, 0,
				"authorization failed: server %s rejected access to %s",
				current_parsed_root->hostname, current_parsed_root->directory);
		}
	}
	else if(connect_state!=CVSPROTO_SUCCESS && connect_state!=CVSPROTO_SUCCESS_NOPROTOCOL)
		error(1,0,"Connection to server failed");
	else if(connect_state==CVSPROTO_SUCCESS)
	{
		/* Loop, getting responses from the server.  */
		TRACE(2,"client start - Loop, getting responses from the server.");
		while (1)
		{
			recv_line (&read_buf);
			TRACE(2,"client start - got \"%s\"",read_buf);

			if (strcmp (read_buf, "I HATE YOU") == 0)
			{
			/* Authorization not granted.
			 *
			 */
				if(current_parsed_root->username)
				{
					if(client_protocol->logout)
					   client_protocol->logout(client_protocol);
					error (1, 0,
						"authorization failed: server %s rejected access to %s for user %s",
						current_parsed_root->hostname, current_parsed_root->directory, current_parsed_root->username);
				}
				else
				{
					error (1, 0,
						"authorization failed: server %s rejected access to %s",
						current_parsed_root->hostname, current_parsed_root->directory);
				}
			}
			else if (strncmp (read_buf, "E ", 2) == 0)
			{
				fprintf (stderr, "%s\n", read_buf + 2);

			/* Continue with the authentication protocol.  */
			}
			else if (strncmp (read_buf, "error ", 6) == 0)
			{
				char *p;

				/* First skip the code.  */
				p = read_buf + 6;
				while (*p != ' ' && *p != '\0')
					++p;

				/* Skip the space that follows the code.  */
				if (*p == ' ')
					++p;

				/* Now output the text.  */
				error(1, 0, "%s", p);
			}
			else if (strcmp (read_buf, "I LOVE YOU") == 0)
			{
				xfree (read_buf);
				break;
			}
			else if (!strstr (read_buf," aborted]: "))
			{
				const char* p = strchr(read_buf,']');
				if (p)
					strcpy(read_buf, p+3);
				/* Overflow here??  possible, but we're only on the client and about to die anyway */
				if(!strcmp(read_buf,"bad auth protocol start"))
					sprintf(read_buf,":%s: protocol not supported by server",client_protocol->name);
				error(1, 0, read_buf);
			}
			else
			{
				/* Unrecognized response from server. */
				if (client_protocol->shutdown (client_protocol) < 0)
				{
					error (0, 0,
					   "unrecognized auth response from %s: %s", 
					   current_parsed_root->hostname, read_buf);
					error (1, 0,
					   "shutdown() failed, server %s",
					   current_parsed_root->hostname);
				}
				error (1, 0, 
				   "unrecognized auth response from %s: %s", 
				   current_parsed_root->hostname, read_buf);
				}
				xfree (read_buf);
		}
	}

	if(verify_only == 1)
		return 0;

	TRACE(2,"client start - continue login.");

	/* If there was a password in the root string, and the login has succeeded,
	   do an implicit login from the supplied data so that future operations work */
	if(current_parsed_root->password_used && client_protocol->login)
		client_protocol->login(client_protocol,(char*)current_parsed_root->password);

	TRACE(2,"client start - server started.");

    /* "Hi, I'm Darlene and I'll be your server tonight..." */
    server_started = 1;

    /* If to_server hasn't already been initialized by the connect
       method. */
    if (to_server == NULL)
	{
		to_server = client_buffer_initialize(client_protocol, 0, (BUFMEMERRPROC) NULL);
		from_server = client_buffer_initialize(client_protocol, 1, (BUFMEMERRPROC) NULL);
	}

    /* Set up logfiles, if any. */
    if (log)
    {
	int len = strlen (log);
	char *buf = (char*)xmalloc (len + 5);
	char *p;
	FILE *fp;

	TRACE(2,"client start - set up logfiles.");
	strcpy (buf, log);
	p = buf + len;

	/* Open logfiles in binary mode so that they reflect
	   exactly what was transmitted and received (that is
	   more important than that they be maximally
	   convenient to view).  */
	/* Note that if we create several connections in a single CVS client
	   (currently used by update.c), then the last set of logfiles will
	   overwrite the others.  There is currently no way around this.  */
	strcpy (p, ".in");
	fp = open_file (buf, "wb");
        if (fp == NULL)
	    error (0, errno, "opening to-server logfile %s", buf);
	else
	    to_server = log_buffer_initialize (to_server, fp, 0,
					       (BUFMEMERRPROC) NULL);

	strcpy (p, ".out");
	fp = open_file (buf, "wb");
        if (fp == NULL)
	    error (0, errno, "opening from-server logfile %s", buf);
	else
	    from_server = log_buffer_initialize (from_server, fp, 1,
						 (BUFMEMERRPROC) NULL);

	xfree (buf);
    }

    /* Clear static variables.  */
    if (toplevel_repos != NULL)
	xfree (toplevel_repos);
    toplevel_repos = NULL;
    if (last_dir_name != NULL)
	xfree (last_dir_name);
    last_dir_name = NULL;
    if (last_repos != NULL)
	xfree (last_repos);
    last_repos = NULL;
    if (last_update_dir != NULL)
	xfree (last_update_dir);
    last_update_dir = NULL;
    stored_checksum_valid = 0;
    if (stored_mode != NULL)
    {
	xfree (stored_mode);
	stored_mode = NULL;
    }

    {
	struct response *rs;

	TRACE(2,"client start - send Valid-responses to server.");
	send_to_server ("Valid-responses", 0);

	for (rs = responses; rs->name != NULL; ++rs)
	{
		// If we are proxying then we remove things our client doesn't want
		if(verify_only==2 && rs->status == rs_not_supported)
			continue;
	    send_to_server (" ", 0);
	    send_to_server (rs->name, 0);
	}
	send_to_server ("\n", 1);
    }
	TRACE(2,"client start - send valid-requests to server.");
    send_to_server ("valid-requests\n", 0);

    if (get_server_responses ())
		error_exit ();

    /* Only CVSNT sends this */
    is_cvsnt = supported_request("Utf8");
    
	if(supported_request("Rootless-stream-modification"))
		rootless_encryption = 1;
	else
		rootless_encryption = 0;

	if(gzip_level<0 && (supported_request("Compression-Requested") || supported_request("Compression-Required")))
		gzip_level = 3; /* gzip_level doesn't actually make much difference */
	if(supported_request("Encryption-Requested") || supported_request("Encryption-Required"))
		cvsencrypt = -1;
	else if(supported_request("Authentication-Requested") || supported_request("Authentication-Required"))
		cvsauthenticate = -1;

    if (!rootless_encryption)
    {
		TRACE(2,"client start - !rootless_encryption.");
		send_to_server ("Root ", 0);
		if(server_active)
			send_to_server (current_parsed_root->remote_repository, 0);
		else
			send_to_server (current_parsed_root->directory, 0);
		send_to_server ("\n", 1);
    }

    status = buf_flush (to_server, 1);
    if (status != 0)
		error (1, status, "writing to server");

	if (cvsencrypt && rootless_encryption)
	{
		/* Turn on encryption before turning on compression.  We do
		not want to try to compress the encrypted stream.  Instead,
		we want to encrypt the compressed stream.  If we can't turn
		on encryption, bomb out; don't let the user think the data
		is being encrypted when it is not.  */
		TRACE(2,"client start - cvsencrypt && rootless_encryption.");
	if (client_protocol->wrap)
	{
		if(supported_request("Protocol-encrypt")) // CVSNT specific protocol encryption
		{
			send_to_server ("Protocol-encrypt\n", 0);
		}
		else if(!strcasecmp(current_parsed_root->method,"kserver") && supported_request("Kerberos-encrypt"))
		{
			send_to_server ("Kerberos-encrypt\n", 0);
		}
		else if(!strcasecmp(current_parsed_root->method,"gserver") && supported_request("Gssapi-encrypt"))
		{
			send_to_server ("Gssapi-encrypt\n", 0);
		}
		else
			error (1, 0, "This server does not support encryption");

		to_server = cvs_encrypt_wrap_buffer_initialize (client_protocol, to_server, 0,
													1,
													((BUFMEMERRPROC)
														NULL));
		from_server = cvs_encrypt_wrap_buffer_initialize (client_protocol, from_server, 1,
														1,
														((BUFMEMERRPROC)
														NULL));

		TRACE(1,"Encryption enabled");
	}
	else
	{
		if(cvsencrypt>0 && !(client_protocol->valid_elements&flagAlwaysEncrypted))
			error (1, 0, "This protocol does not support encryption");
	}
	}

	if (gzip_level>0 && rootless_encryption)
	{
	TRACE(2,"client start - gzip_level>0 && rootless_encryption.");
	if (supported_request ("Gzip-stream"))
	{
		char gzip_level_buf[5];
		send_to_server ("Gzip-stream ", 0);
		sprintf (gzip_level_buf, "%d", gzip_level);
		send_to_server (gzip_level_buf, 0);
		send_to_server ("\n", 1);

		/* All further communication with the server will be
			compressed.  */

		to_server = compress_buffer_initialize (to_server, 0, gzip_level,
							(BUFMEMERRPROC) NULL);
		from_server = compress_buffer_initialize (from_server, 1,
							gzip_level,
							(BUFMEMERRPROC) NULL);
		TRACE(1,"Compression enabled");
	}
	else
	{
		fprintf (stderr, "server doesn't support gzip-stream\n");
		/* Setting gzip_level to 0 prevents us from giving the
			error twice if update has to contact the server again
			to fetch unpatchable files.  */
		gzip_level = 0;
	}
	}

	if (cvsauthenticate && !cvsencrypt && rootless_encryption)
	{
		/* Turn on authentication after turning on compression, so
		that we can compress the authentication information.  We
		assume that encrypted data is always authenticated--the
		ability to decrypt the data stream is itself a form of
		authentication.  */
	TRACE(2,"client start - cvsauthenticate && !cvsencrypt && rootless_encryption.");
	if (client_protocol->wrap)
	{
		if(supported_request("Protocol-authenticate")) // CVSNT specific protocol authentication
		{
			send_to_server ("Protocol-encrypt\n", 0);
		}
		else if(!strcasecmp(current_parsed_root->method,"gserver") && supported_request("Gssapi-authenticate"))
		{
			send_to_server ("Gssapi-authenticate\n", 0);
		}
		else
			error (1, 0, "This server does not support authentication");

		to_server = cvs_encrypt_wrap_buffer_initialize (client_protocol, to_server, 0,
													0,
													((BUFMEMERRPROC)
														NULL));
		from_server = cvs_encrypt_wrap_buffer_initialize (client_protocol, from_server, 1,
														0,
														((BUFMEMERRPROC)
														NULL));
		TRACE(1,"Authentication enabled");
	}
	else
	{
		if(cvsauthenticate>0 && !(client_protocol->valid_elements&flagAlwaysEncrypted))
			error (1, 0, "This protocol does not support authentication");
	}
	}

    if (rootless_encryption)
    {
		TRACE(2,"client start - rootless_encryption.");
		send_to_server ("Root ", 0);
		if(server_active)
			send_to_server (current_parsed_root->remote_repository, 0);
		else
			send_to_server (current_parsed_root->directory, 0);
		send_to_server ("\n", 1);
    }

	/*
	 * Return the ANSI codepage in use (may also be MBCS eg. utf8)
	 */
	server_codepage = NULL;
	server_codepage_translation = 0;
	if(force_locale || supported_request("server-codepage"))
	{
		if(!force_locale)
		{
		   send_to_server("server-codepage\n", 0);
		   read_line(&server_codepage);
		   if(!strncmp(server_codepage,"E ",2))
			error(1,0,"%s",server_codepage+2);
		}
		else
		   server_codepage = xstrdup(force_locale);

		for(char *p=server_codepage; p && *p; p++)
			*p=toupper((unsigned char)*p);

#ifdef _WIN32
		if(win32_global_codepage==CP_UTF8)
			client_codepage="UTF-8";
		else
#endif
			client_codepage=CCodepage::GetDefaultCharset();

		for(char *p=server_codepage; p && *p; p++)
			*p=toupper((unsigned char)*p);

		TRACE(1,"Server codepage is %s%s",server_codepage,force_locale?" (forced)":"");
		TRACE(1,"Client codepage is %s",client_codepage);
		if(server_codepage[0] && client_codepage[0] && strcmp(server_codepage,client_codepage))
		{
			if(locale_active)
			{
				TRACE(1,"Server->Client codepage translation is active");
				server_codepage_translation = 1;
			}
			else
			{
				TRACE(1,"Server->Client codepage translation is disabled.  Use -l to enable");
			}
		}
	}

	/*
	 * Pass the client version string to the server, for logging
	 */
	if (supported_request("client-version"))
	{
		char *server_version = NULL;

		send_to_server ("client-version ", 0);
		send_to_server ("CVSNT "CVSNT_PRODUCTVERSION_STRING, 0);
		if(CCvsgui::Active())
			send_to_server(" (cvsgui active)", 0);
		send_to_server ("\n", 0);
		read_line(&server_version);
		TRACE(1,"Server version is %s",server_version);
		TRACE(1,"Client version is %s%s","CVSNT "CVSNT_PRODUCTVERSION_STRING,CCvsgui::Active()?" (cvsgui active)":"");
		xfree(server_version);
	}

	/*
     * Now handle global options.
     *
     * -H, -f, -d, -e should be handled OK locally.
     *
     * -b we ignore (treating it as a server installation issue).
     * FIXME: should be an error message.
     *
     * -v we print local version info; FIXME: Add a protocol request to get
     * the version from the server so we can print that too.
     *
     * -l -t -r -w -q -n and -Q need to go to the server.
     */

    {
	int have_global = supported_request ("Global_option");

	if (noexec)
	{
	    if (have_global)
	    {
		send_to_server ("Global_option -n\n", 0);
	    }
	    else
		error (1, 0,
		       "This server does not support the global -n option.");
	}
	if (quiet)
	{
	    if (have_global)
	    {
		send_to_server ("Global_option -q\n", 0);
	    }
	    else
		error (1, 0,
		       "This server does not support the global -q option.");
	}
	if (really_quiet)
	{
	    if (have_global)
	    {
		send_to_server ("Global_option -Q\n", 0);
	    }
	    else
		error (1, 0,
		       "This server does not support the global -Q option.");
	}
	if (!cvswrite)
	{
	    if (have_global)
	    {
		send_to_server ("Global_option -r\n", 0);
	    }
	    else
		error (1, 0,
		       "This server does not support the global -r option.");
	}
	if (trace)
	{
	    if (have_global)
	    {
			int count;

			for(count=0; count<trace; count++)
				send_to_server ("Global_option -t\n", 0);
	    }
	    else
			error (1, 0,
		       "This server does not support the global -t option.");
	}
    }

    /* Find out about server-side cvswrappers.  An extra network
       turnaround for cvs import seems to be unavoidable, unless we
       want to add some kind of client-side place to configure which
       filenames imply binary.  For cvs add, we could avoid the
       problem by keeping a copy of the wrappers in CVSADM (the main
       reason to bother would be so we could make add work without
       contacting the server, I suspect).  */

    if (!supported_request("read-cvswrappers") &&
        (!strcmp (command_name, "import") 
         || !strcmp (command_name, "add")))
    {
	if (supported_request ("wrapper-sendme-rcsOptions"))
	{
	    int err;
	    send_to_server ("wrapper-sendme-rcsOptions\n", 0);
	    err = get_server_responses ();
	    if (err != 0)
		error (err, 0, "error reading from server");
	}
    }

    if (filenames_case_insensitive && supported_request ("Case"))
		send_to_server ("Case\n", 0);
	if(supported_request("Valid-RcsOptions"))
	{
		const kflag_t *p;

		char buf[2]={0};
		send_to_server("Valid-RcsOptions ",0);
		for(p=kflag_encoding;p->flag;p++)
		{
			buf[0]=p->flag;
			if(buf[0]>0)
				send_to_server(buf,1);
		}
		for(p=kflag_flags;p->flag;p++)
		{
			buf[0]=p->flag;
			if(buf[0]>0)
				send_to_server(buf,1);
		}
		send_to_server("\n",1);
	}

    /* If "Set" is not supported, just silently fail to send the variables.
       Users with an old server should get a useful error message when it
       fails to recognize the ${=foo} syntax.  This way if someone uses
       several servers, some of which are new and some old, they can still
       set user variables in their .cvsrc without trouble.  */
    if (supported_request ("Set"))
		std::for_each(variable_list.begin(), variable_list.end(), send_variable_proc);
	return 0;
}

/* Send an argument STRING.  */
void send_arg (const char *string)
{
    send_to_server ("Argument ", 0);
	send_filename(string,true,false);
    send_to_server ("\n", 1);
}

/* VERS->OPTIONS specifies whether the file is binary or not.  NOTE: BEFORE
   using any other fields of the struct vers, we would need to fix
   client_process_import_file to set them up.  */

static void send_modified (const char *file, const char *short_pathname, const Vers_TS *vers)
{
    /* File was modified, send it.  */
    struct stat sb;
    int fd;
    char *buf;
    char *mode_string;
    size_t bufsize;
	bool open_binary, encode;
	CCodepage::Encoding encoding,targetencoding;
 	size_t newsize;
	kflag options_flags;
	LineType crlf;

	TRACE(1,"Sending file '%s' to server", PATCH_NULL(file));

    /* Don't think we can assume fstat exists.  */
    if ( CVS_STAT (file, &sb) < 0)
	error (1, errno, "reading %s", short_pathname);

    mode_string = mode_to_string (sb.st_mode);

    /* Is the file marked as containing binary data by the "-kb" flag?
       If so, make sure to open it in binary mode: */

	crlf = crlf_mode;
    if (vers && vers->options)
	{
		RCS_get_kflags(vers->options, false, options_flags);
		if(options_flags.flags&KFLAG_MAC)
			crlf=ltCr;
		else if(options_flags.flags&KFLAG_DOS)
			crlf=ltCrLf;
		else if(options_flags.flags&(KFLAG_BINARY|KFLAG_UNIX))
			crlf=ltLf;
		open_binary = (options_flags.flags&(KFLAG_BINARY|KFLAG_ENCODED)) || (crlf!=CRLF_DEFAULT);
		encode = !(options_flags.flags&KFLAG_BINARY) && ((options_flags.flags&KFLAG_ENCODED) || (crlf!=CRLF_DEFAULT));
		encoding = options_flags.encoding;
		targetencoding = (options_flags.flags&KFLAG_ENCODED)?CCodepage::Utf8Encoding:CCodepage::NullEncoding;
	}
    else
	{
		open_binary = false;
		encode = false;
		encoding = targetencoding = CCodepage::NullEncoding;
	}

    /* Beware: on systems using CRLF line termination conventions,
       the read and write functions will convert CRLF to LF, so the
       number of characters read is not the same as sb.st_size.  Text
       files should always be transmitted using the LF convention, so
       we don't want to disable this conversion.  */
    bufsize = sb.st_size;
    buf = (char*)xmalloc (bufsize);

#ifdef MAC_HFS_STUFF
	{
#	ifndef _POSIX_NO_TRUNC
		char tfile[1024]; strcpy(tfile, file); strcat(tfile, ".CVSBFCTMP");
#	else
		char tfile[1024]; sprintf (tfile, "%.9s%s", file, ".CVSBFCTMP");
#	endif
		/* encode data & resource fork into flat file if needed and adjust character encoding if needed */
		mac_encode_file(file, tfile, open_binary);
		xfree (buf);
		if ( CVS_STAT (tfile, &sb) < 0)
			error (1, errno, "reading %s", tfile);
		bufsize = sb.st_size;
		buf = (char*)xmalloc (bufsize);
		fd = CVS_OPEN (tfile, O_RDONLY | OPEN_BINARY,0);
		if (fd < 0)
			error (1, errno, "reading %s", short_pathname);
	}
#else
	fd = CVS_OPEN (file, O_RDONLY | (open_binary ? OPEN_BINARY : 0),0);
#endif
	
    if (fd < 0)
	error (1, errno, "reading %s", short_pathname);


	{
	    char *bufp = buf;
		CCodepage *cdp1;
	    int len;

	    /* FIXME: This is gross.  It assumes that we might read
	       less than st_size bytes (true on NT), but not more.
	       Instead of this we should just be reading a block of
	       data (e.g. 8192 bytes), writing it to the network, and
	       so on until EOF.  */
	    while ((len = read (fd, bufp, (buf + sb.st_size) - bufp)) > 0)
	        bufp += len;

	    if (len < 0)
	        error (1, errno, "reading %s", short_pathname);

	    newsize = bufp - buf;

		/* If the file is unicode (and the remote server supports it), translate it to utf8 before transmitting */
		TRACE(3,"If the file is unicode (and the remote server supports it), translate it to utf8 before transmitting.");
		if((!open_binary)||(encode))
		{
			TRACE(3,"instantiate cdp1.");
			cdp1 = new CCodepage;
			TRACE(3,"!open_binary - will begin.");
			cdp1->BeginEncoding(encoding,targetencoding);
		}
		if(encode)
		{
			TRACE(3,"encode is set.");
		    if(supported_request("Utf8"))
			{
				void *newbuf = NULL;
				int res;
				CCodepage *cdp2;
				TRACE(3,"instantiate cdp2.");
				cdp2 = new CCodepage;
				TRACE(3,"Utf8 is a supported request.");
				cdp2->BeginEncoding(encoding,targetencoding);
				if((res=cdp2->ConvertEncoding(buf,newsize,newbuf,newsize))>0)
				{
					xfree(buf);
					buf = (char*)newbuf;
				}
				else if(res<0)
				{
					error(0,0,"Unable to file from %s to UTF-8.  Checkin may be invalid.",encoding.encoding);
				}
				cdp2->EndEncoding();
				delete cdp2;
				cdp2=NULL;
			}
			else 
				error(0,0,"Remote server does not support codepage transformation.  Checkin may be invalid.");
		}
		else
			TRACE(3,"encode is NOT set.");
		/* Normalise the file, removing CRLF, CR only, etc. */
		if((encode)||(!open_binary))
		{
			cdp1->StripCrLf(buf,newsize);
			cdp1->EndEncoding();
			delete cdp1;
			cdp1=NULL;
		}
	}
	if (close (fd) < 0)
	    error (0, errno, "warning: can't close %s", short_pathname);
   
	if (supported_request ("Checkin-time") && strcmp(command_name,"import"))
	{
	    struct stat sb;
	    char *rcsdate;
	    char netdate[MAXDATELEN];

	    if (CVS_STAT (file, &sb) < 0)
			error (1, errno, "cannot stat %s", file);
	    rcsdate = date_from_time_t (sb.st_mtime);
	    date_to_internet (netdate, rcsdate);
	    xfree (rcsdate);

	    send_to_server ("Checkin-time ", 0);
	    send_to_server (netdate, 0);
	    send_to_server ("\n", 1);
	}

#if 0 // Not implemented yet
	if(newsize>1024*1024 && supported_request("Binary-transfer")) // We don't do this on files <1MB..
	{
		char tmp[80];
		const char *p = buf;
		size_t s = newsize,l;
		CMD5Calc md5;

		send_to_server("Binary-transfer",0);
		send_to_server (file, 0);
		send_to_server ("\n", 1);
		send_to_server (mode_string, 0);
		send_to_server ("\n", 1);
		snprintf (tmp, 80, "%lu\n", (unsigned long) newsize);
		send_to_server (tmp, 0);

		md5.Update(buf,newsize);
		send_to_server(md5.Final(),0);
		send_to_server("\n",1);

		while(s)
		{
			md5.Init();
			l=s>65536?65536:s;
			md5.Update(p,l);
			p+=l;
			s-=l;
			send_to_server_untranslated(md5.Final(),0);
			send_to_server_untranslated("\n",1);
		}
	}
	else
#endif
	{
		char tmp[80];

		send_to_server ("Modified ", 0);
		send_to_server (file, 0);
		send_to_server ("\n", 1);
		send_to_server (mode_string, 0);
		send_to_server ("\n", 1);
		sprintf (tmp, "%lu\n", (unsigned long) newsize);
		send_to_server (tmp, 0);

		if (newsize > 0)
			send_to_server_untranslated(buf, newsize);
	}

#ifdef MAC_HFS_STUFF
	{
#	ifndef _POSIX_NO_TRUNC
		char tfile[1024]; strcpy(tfile, file); strcat(tfile, ".CVSBFCTMP");
#	else
		char tfile[1024]; sprintf (tfile, "%.9s%s", file, ".CVSBFCTMP");
#	endif
		if (CVS_UNLINK (tfile) < 0)
			error (0, errno, "warning: can't remove temp file %s", tfile);
	}
#endif
			/*
	 * Note that this only ends with a newline if the file ended with
	 * one.
	 */

    xfree (buf);
    xfree (mode_string);
}

/* The address of an instance of this structure is passed to
   send_fileproc, send_filesdoneproc, and send_direntproc, as the
   callerdat parameter.  */

struct send_data
{
    /* Each of the following flags are zero for clear or nonzero for set.  */
    int build_dirs;
    int force;
    int no_contents;
    int backup_modified;
	int modified;
	int case_sensitive;
};


/* Deal with one file.  */
static int send_fileproc (void *callerdat, struct file_info *finfo)
{
    struct send_data *args = (struct send_data *) callerdat;
    Vers_TS *vers;
    struct file_info xfinfo;
    /* File name to actually use.  Might differ in case from
       finfo->file.  */
    const char *filename;

    TRACE(3,"send_fileproc (1)");
    send_a_repository ("", finfo->repository, finfo->update_dir);

    xfinfo = *finfo;
    xfinfo.repository = NULL;
    xfinfo.rcs = NULL;
    vers = Version_TS (&xfinfo, NULL, NULL, NULL, 0, 0, args->case_sensitive);

    if (vers->entdata != NULL)
	filename = vers->entdata->user;
    else
	filename = finfo->file;

	if (vers->vn_user != NULL)
    {
	/* The Entries request.  */
	send_to_server ("Entry /", 0);
	send_to_server (filename, 0);
	send_to_server ("/", 0);
	send_to_server (vers->vn_user, 0);
	send_to_server ("/", 0);
	if (vers->ts_conflict != NULL)
	{
	    if (vers->ts_user != NULL &&
		strcmp (vers->ts_conflict, vers->ts_user) == 0)
		send_to_server ("+=", 0);
	    else
		send_to_server ("+modified", 0);
	}
	else if(is_cvsnt && vers->ts_rcs)
		send_to_server(vers->ts_rcs,0);
	send_to_server ("/", 0);
	const char *ko = vers->entdata?vers->entdata->options:vers->options;
	if(ko && *ko)
	{
		send_to_server("-k",0);
		send_to_server (ko,0);
	}
	send_to_server ("/", 0);
	if (vers->entdata != NULL && vers->entdata->tag)
	{
	    send_to_server ("T", 0);
	    send_to_server (vers->entdata->tag, 0);
	}
	else if (vers->entdata != NULL && vers->entdata->date)
          {
	    send_to_server ("D", 0);
	    send_to_server (vers->entdata->date, 0);
          }
	send_to_server ("\n", 1);

	/* Must send the EntryExtra line after the Entry line, otherwise things barf */
	if(vers->entdata && supported_request("EntryExtra"))
	{
		send_to_server("EntryExtra /",0);
		send_to_server (filename, 0);
		send_to_server ("/", 0);
		send_to_server(vers->entdata->merge_from_tag_1,0);
		send_to_server ("/", 0);
		send_to_server(vers->entdata->merge_from_tag_2,0);
		send_to_server ("/", 0);
		/* No point in sending RCS checkin time - we know it already */
		send_to_server ("/", 0);
		send_to_server (vers->entdata->edit_revision,0);
		send_to_server ("/", 0);
		send_to_server (vers->entdata->edit_tag,0);
		send_to_server ("/", 0);
		send_to_server (vers->entdata->edit_bugid,0);
		send_to_server ("/", 0);
		send_to_server (vers->entdata->md5,0);
		send_to_server ("/\n", 0);
	}
    }
    else
    {
	/* It seems a little silly to re-read this on each file, but
	   send_dirent_proc doesn't get called if filenames are specified
	   explicitly on the command line.  */
	wrap_add_file (CVSDOTWRAPPER, true);
    }

    if (vers->ts_user == NULL)
    {
	/*
	 * Do we want to print "file was lost" like normal CVS?
	 * Would it always be appropriate?
	 */
	/* File no longer exists.  Don't do anything, missing files
	   just happen.  */
    }
    else if (vers->ts_rcs == NULL
	     || args->force || strcmp (vers->ts_user, vers->ts_rcs) != 0)
    {
		bool modified = true;
		if(!args->force && vers->entdata && vers->entdata->md5)
		{
			char *buf = NULL;
			size_t bufsize=0,len=0;
			kflag kf;

			// This is the one case where for md5 we have to deal with unicode
			// The md5 stored is the server side protocol file which is always ANSI or UTF8 and
			// contains only LF.
			RCS_get_kflags(vers->options, false, kf);
			get_file(finfo->file,filename,"r",&buf,&bufsize,&len,kf);

			if(len)
			{
				CMD5Calc md5;
				md5.Update(buf,len);
				if(!strcmp(md5.Final(),vers->entdata->md5))
					modified = false;
			}

			xfree(buf);
		}

		if(modified)
		{
			if (args->no_contents && supported_request ("Is-modified"))
			{
				send_to_server ("Is-modified ", 0);
				send_to_server (filename, 0);
				send_to_server ("\n", 1);
			}
			else
				send_modified (filename, finfo->fullname, vers);

			if (args->backup_modified)
			{
				char *bakname;
				bakname = backup_file (filename, vers->vn_user);
				/* This behavior is sufficiently unexpected to
				justify overinformativeness, I think. */
				if (! really_quiet)
					printf ("(Locally modified %s moved to %s)\n",
							filename, bakname);
				xfree (bakname);
				client_overwrite_existing = 1;
			}
		}
		else
		{
			send_to_server ("Unchanged ", 0);
			send_to_server (filename, 0);
			send_to_server ("\n", 1);
		}
    }
    else
    {
		send_to_server ("Unchanged ", 0);
		send_to_server (filename, 0);
		send_to_server ("\n", 1);
    }

    /* if this directory has an ignore list, add this file to it */
    if (ignlist)
    {
	Node *p;

	p = getnode ();
	p->type = FILES;
	p->key = xstrdup (finfo->file);
	(void) addnode (ignlist, p);
    }

    freevers_ts (&vers);
    return 0;
}

static void send_ignproc (char *file, char *dir)
{
    if (!supported_request ("Questionable"))
    {
	if (dir[0] != '\0')
	    (void) printf ("? %s/%s\n", dir, file);
	else
	    (void) printf ("? %s\n", file);
    }
    else
    {
	send_to_server ("Questionable ", 0);
	send_to_server (file, 0);
	send_to_server ("\n", 1);
    }
}

static int send_filesdoneproc (void *callerdat, int err, char *repository, char *update_dir, List *entries)
{
    /* if this directory has an ignore list, process it then free it */
    if (ignlist)
    {
	ignore_files (ignlist, entries, update_dir, send_ignproc);
	dellist (&ignlist);
    }

    return (err);
}

/*
 * send_dirent_proc () is called back by the recursion processor before a
 * sub-directory is processed for update.
 * A return code of 0 indicates the directory should be
 * processed by the recursion code.  A return of non-zero indicates the
 * recursion code should skip this directory.
 *
 */
static Dtype send_dirent_proc (void *callerdat, char *dir, char *repository, char *update_dir, List *entries, const char *virtual_repository, Dtype hint)
{
    struct send_data *args = (struct send_data *) callerdat;
    int dir_exists=0;
    char *cvsadm_name=NULL;

	TRACE(3,"send_dirent_proc called by recursion processor?");
    if (ignore_directory (update_dir))
    {
	/* print the warm fuzzy message */
	if (!quiet)
	    error (0, 0, "Ignoring %s", update_dir);
        return (R_SKIP_ALL);
    }

    /*
     * If the directory does not exist yet (e.g. "cvs update -d foo"),
     * no need to send any files from it.  If the directory does not
     * have a CVS directory, then we pretend that it does not exist.
     * Otherwise, we will fail when trying to open the Entries file.
     * This case will happen when checking out a module defined as
     * ``-a .''.
     */
    cvsadm_name = (char*)xmalloc (strlen (dir) + sizeof (CVSADM) + 10);
    sprintf (cvsadm_name, "%s/%s", dir, CVSADM);
    dir_exists = isdir (cvsadm_name);
    xfree (cvsadm_name);
	cvsadm_name=NULL;

    /*
     * If there is an empty directory (e.g. we are doing `cvs add' on a
     * newly-created directory), the server still needs to know about it.
     */

    if (dir_exists)
    {
	/*
	 * Get the repository from a CVS/Repository file whenever possible.
	 * The repository variable is wrong if the names in the local
	 * directory don't match the names in the repository.
	 */
	char *repos = Name_Repository (dir, update_dir);
	send_a_repository (dir, repos, update_dir);
	xfree (repos);
	repos=NULL;

	/* initialize the ignore list for this directory */
	ignlist = getlist ();
    }
    else
    {
	/* It doesn't make sense to send a non-existent directory,
	   because there is no way to get the correct value for
	   the repository (I suppose maybe via the expand-modules
	   request).  In the case where the "obvious" choice for
	   repository is correct, the server can figure out whether
	   to recreate the directory; in the case where it is wrong
	   (that is, does not match what modules give us), we might as
	   well just fail to recreate it.

	   Checking for noexec is a kludge for "cvs -n add dir".  */
	/* Don't send a non-existent directory unless we are building
           new directories (build_dirs is true).  Otherwise, CVS may
           see a D line in an Entries file, and recreate a directory
           which the user removed by hand.  */
	if (args->build_dirs && noexec)
	    send_a_repository (dir, repository, update_dir);
    }

    return (dir_exists ? R_PROCESS : R_SKIP_ALL);
}

/*
 * send_dirleave_proc () is called back by the recursion code upon leaving
 * a directory.  All it does is delete the ignore list if it hasn't already
 * been done (by send_filesdone_proc).
 */
/* ARGSUSED */
static int send_dirleave_proc (void *callerdat, char *dir, int err, char *update_dir, List *entries)
{

    /* Delete the ignore list if it hasn't already been done.  */
    if (ignlist)
	dellist (&ignlist);
    return err;
}

/*
 * Send each option in a string to the server, one by one.
 * This assumes that the options are separated by spaces, for example
 * STRING might be "--foo -C5 -y".
 */

void send_option_string (char *string)
{
    char *copy;
    char *p;

    copy = xstrdup (string);
    p = copy;
    while (1)
    {
        char *s;
	char l;

	for (s = p; *s != ' ' && *s != '\0'; s++)
	    ;
	l = *s;
	*s = '\0';
	if (s != p)
	    send_arg (p);
	if (l == '\0')
	    break;
	p = s + 1;
    }
    xfree (copy);
}


/* Send the names of all the argument files to the server.  */

void send_file_names (int argc, char **argv, unsigned int flags)
{
    int i;
    int level;
    int max_level;
    
    /* The fact that we do this here as well as start_recursion is a bit 
       of a performance hit.  Perhaps worth cleaning up someday.  */
    if (flags & SEND_EXPAND_WILD)
		expand_wild (argc, argv, &argc, &argv);

	TRACE(3,"send_files (1)");
	if(!client_max_dotdot)
	{
		/* Send Max-dotdot if needed.  */
		max_level = 0;
		for (i = 0; i < argc; ++i)
		{
			if(isabsolute(argv[i]))
				error(1,0,"Absolute pathname `%s' not allowed",argv[i]);
			level = pathname_levels (argv[i]);
			if (level > max_level)
				max_level = level;
		}
		if (max_level > 0)
		{
		if (supported_request ("Max-dotdot"))
		{
				char buf[10];
				sprintf (buf, "%d", max_level);

			send_to_server ("Max-dotdot ", 0);
			send_to_server (buf, 0);
			send_to_server ("\n", 1);
			client_max_dotdot = max_level;
			TRACE(3,"client_max_dotdot = %d",client_max_dotdot);
		}
		else
			/*
			* "leading .." is not strictly correct, as this also includes
			* cases like "foo/../..".  But trying to explain that in the
			* error message would probably just confuse users.
			*/
			error (1, 0,
			"leading .. not supported by old (pre-Max-dotdot) servers");
		}
	}

    for (i = 0; i < argc; ++i)
    {
		char *file = argv[i];
		char *dir = NULL;

		if (arg_should_not_be_sent_to_server (file))
		    continue;

		if(flags&SEND_DIRECTORIES_ONLY && !isdir(file))
		{
			error(1,0,"Can only specify directories for this command");
			continue;
		}

		if(filenames_case_insensitive && !(flags&SEND_CASE_SENSITIVE))
		{
			const char *p;

			if ((p=last_component (file))!=NULL && isdir (CVSADM))
			{
				List *entries = NULL;
				Node *node;

				/* Note that if we are adding a directory,
				the following will read the entry
				that we just wrote there, that is, we
				will get the case specified on the
				command line, not the case of the
				directory in the filesystem.  This
				is correct behavior.  */

				/* If the file doesn't exist then the send the argument as passed anyway.. let the
				   server sort out whether that matters or not */
				if(p>file)
				{
					dir = xstrdup(file);
					dir[p-file]='\0';
					if(isdir(dir))
					   	entries = Entries_Open_Dir(0, dir, dir);
					else
						xfree(dir);
				}
				else
					entries = Entries_Open(0, NULL);
				if(entries)
				{
					node = findnode_fn (entries, p);
					if (node != NULL)
					{
				  		if(!dir)
							dir = xstrdup(node->key);
				  		else
				  		{
							dir = (char *)xrealloc(dir, strlen(dir)+strlen(node->key)+2);
							strcat(dir,node->key);
				  		}
					}
					else
				  		xfree(dir);
					Entries_Close (entries);
				}
			}
		}

		send_to_server ("Argument ", 0);
		send_filename(dir?dir:file, true, true);
		send_to_server ("\n", 1);
		xfree(dir);
	}

    if (flags & SEND_EXPAND_WILD)
    {
		int i;
		for (i = 0; i < argc; ++i)
			xfree (argv[i]);
		xfree (argv);
    }
}


/* Send Repository, Modified and Entry.  argc and argv contain only
  the files to operate on (or empty for everything), not options.
  local is nonzero if we should not recurse (-l option).  flags &
  SEND_BUILD_DIRS is nonzero if nonexistent directories should be
  sent.  flags & SEND_FORCE is nonzero if we should send unmodified
  files to the server as though they were modified.  flags &
  SEND_NO_CONTENTS means that this command only needs to know
  _whether_ a file is modified, not the contents.  Also sends Argument
  lines for argc and argv, so should be called after options are sent.  */
void send_files (int argc, char **argv, int local, int aflag, unsigned int flags)
{
    struct send_data args;
    int err,i,max_level,level;

	if(!client_max_dotdot)
	{
		/* Send Max-dotdot if needed.  */
		max_level = 0;
		for (i = 0; i < argc; ++i)
		{
			if(isabsolute(argv[i]))
				error(1,0,"Absolute pathname `%s' not allowed",argv[i]);
			level = pathname_levels (argv[i]);
			if (level > max_level)
				max_level = level;
		}
		if (max_level > 0)
		{
		if (supported_request ("Max-dotdot"))
		{
				char buf[10];
				sprintf (buf, "%d", max_level);

			send_to_server ("Max-dotdot ", 0);
			send_to_server (buf, 0);
			send_to_server ("\n", 1);
			client_max_dotdot = max_level;
			TRACE(3,"client_max_dotdot = %d",client_max_dotdot);
		}
		else
			/*
			* "leading .." is not strictly correct, as this also includes
			* cases like "foo/../..".  But trying to explain that in the
			* error message would probably just confuse users.
			*/
			error (1, 0,
			"leading .. not supported by old (pre-Max-dotdot) servers");
		}
	}

	/*
     * aflag controls whether the tag/date is copied into the vers_ts.
     * But we don't actually use it, so I don't think it matters what we pass
     * for aflag here.
     */
    args.build_dirs = flags & SEND_BUILD_DIRS;
    args.force = flags & SEND_FORCE;
	args.case_sensitive = flags & SEND_CASE_SENSITIVE;
    args.no_contents = flags & SEND_NO_CONTENTS;
    args.backup_modified = flags & BACKUP_MODIFIED_FILES;
    err = start_recursion
	(send_fileproc, send_filesdoneproc, (PREDIRENTPROC) NULL,
	 send_dirent_proc, send_dirleave_proc, (void *) &args,
	 argc, argv, local, W_LOCAL, aflag, 0, (char *)NULL, NULL, 0,
	 (PERMPROC) NULL, NULL);
    if (err)
		error_exit ();
    if (toplevel_repos == NULL)
	{
		/*
		* This happens if we are not processing any files,
		* or for checkouts in directories without any existing stuff
		* checked out.  The following assignment is correct for the
		* latter case; I don't think toplevel_repos matters for the
		* former.
		*/
		toplevel_repos = xstrdup (current_parsed_root->directory);
	}
    send_repository ("", toplevel_repos, ".");
}

void client_import_setup (char *repository)
{
    if (toplevel_repos == NULL)		/* should always be true */
        send_a_repository ("", repository, "");
}

/*
 * Process the argument import file.
 */
int client_process_import_file(const char *message, const char *vfile, const char *vtag, 
							   int targc, const char **targv, const char *repository, 
							   const char *options, int modtime)
{
    const char *update_dir;
    char *fullname;
    Vers_TS vers;

    assert (toplevel_repos != NULL);

    if (strncmp (repository, toplevel_repos, strlen (toplevel_repos)) != 0)
		error (1, 0, "internal error: pathname `%s' doesn't specify file in `%s'", repository, toplevel_repos);

    if (strcmp (repository, toplevel_repos) == 0)
    {
		update_dir = "";
		fullname = xstrdup (vfile);
    }
    else
    {
		update_dir = repository + strlen (toplevel_repos) + 1;

		fullname = (char*)xmalloc (strlen (vfile) + strlen (update_dir) + 10);
		strcpy (fullname, update_dir);
		strcat (fullname, "/");
		strcat (fullname, vfile);
    }

    send_a_repository ("", repository, update_dir);
	vers.options = wrap_rcsoption(vfile);
	assign_options(&vers.options,options);
    if (vers.options != NULL)
    {
		if (supported_request ("Kopt"))
		{
			send_to_server ("Kopt ", 0);
			send_to_server ("-k", 0);
			send_to_server (vers.options, 0);
			send_to_server ("\n", 1);
		}
		else
			error (0, 0,
			"warning: ignoring -k options due to server limitations");
	}
	if (modtime)
	{
		if (supported_request ("Checkin-time"))
		{
			struct stat sb;
			char *rcsdate;
			char netdate[MAXDATELEN];

			if (CVS_STAT (vfile, &sb) < 0)
			error (1, errno, "cannot stat %s", fullname);
			rcsdate = date_from_time_t (sb.st_mtime);
			date_to_internet (netdate, rcsdate);
			xfree (rcsdate);

			send_to_server ("Checkin-time ", 0);
			send_to_server (netdate, 0);
			send_to_server ("\n", 1);
		}
		else
			error (0, 0,
			"warning: ignoring -d option due to server limitations");
	}
	send_modified (vfile, fullname, &vers);
	xfree (vers.options);
	xfree (fullname);
	return 0;
}

void
client_import_done ()
{
    if (toplevel_repos == NULL)
	/*
	 * This happens if we are not processing any files,
	 * or for checkouts in directories without any existing stuff
	 * checked out.  The following assignment is correct for the
	 * latter case; I don't think toplevel_repos matters for the
	 * former.
	 */
        /* FIXME: "can't happen" now that we call client_import_setup
	   at the beginning.  */
	toplevel_repos = xstrdup (current_parsed_root->directory);
    send_repository ("", toplevel_repos, ".");
}

static void notified_a_file (char *data, List *ent_list, char *short_pathname, char *filename)
{
    FILE *fp;
    FILE *newf;
    size_t line_len = 8192;
    char *line = (char*)xmalloc (line_len);
    char *cp;
    int nread;
    int nwritten;
    char *p;

    fp = open_file (CVSADM_NOTIFY, "r");
	if(!fp)
	{
		if(!existence_error(errno))
			error(0,errno,"cannot read %s", CVSADM_NOTIFY);
		goto error_exit;
	}
	
    if (getline (&line, &line_len, fp) < 0)
    {
		if (feof (fp))
			error (0, 0, "cannot read %s: end of file", CVSADM_NOTIFY);
		else
			error (0, errno, "cannot read %s", CVSADM_NOTIFY);
		goto error_exit;
    }
    cp = strchr (line, '\t');
    if (cp == NULL)
    {
		error (0, 0, "malformed %s file", CVSADM_NOTIFY);
		goto error_exit;
    }
    *cp = '\0';
    if (strcmp (filename, line + 1) != 0)
    {
	error (0, 0, "protocol error: notified %s, expected %s", filename,
	       line + 1);
    }

    if (getline (&line, &line_len, fp) < 0)
    {
	if (feof (fp))
	{
	    xfree (line);
	    if (fclose (fp) < 0)
		error (0, errno, "cannot close %s", CVSADM_NOTIFY);
	    if ( CVS_UNLINK (CVSADM_NOTIFY) < 0)
		error (0, errno, "cannot remove %s", CVSADM_NOTIFY);
	    return;
	}
	else
	{
	    error (0, errno, "cannot read %s", CVSADM_NOTIFY);
	    goto error_exit;
	}
    }
    newf = open_file (CVSADM_NOTIFYTMP, "w");
    if (fputs (line, newf) < 0)
    {
	error (0, errno, "cannot write %s", CVSADM_NOTIFYTMP);
	goto error2;
    }
    while ((nread = fread (line, 1, line_len, fp)) > 0)
    {
	p = line;
	while ((nwritten = fwrite (p, 1, nread, newf)) > 0)
	{
	    nread -= nwritten;
	    p += nwritten;
	}
	if (ferror (newf))
	{
	    error (0, errno, "cannot write %s", CVSADM_NOTIFYTMP);
	    goto error2;
	}
    }
    if (ferror (fp))
    {
	error (0, errno, "cannot read %s", CVSADM_NOTIFY);
	goto error2;
    }
    if (fclose (newf) < 0)
    {
	error (0, errno, "cannot close %s", CVSADM_NOTIFYTMP);
	goto error_exit;
    }
    xfree (line);
    if (fclose (fp) < 0)
    {
	error (0, errno, "cannot close %s", CVSADM_NOTIFY);
	return;
    }

    {
        /* In this case, we want rename_file() to ignore noexec. */
        int saved_noexec = noexec;
        noexec = 0;
        rename_file (CVSADM_NOTIFYTMP, CVSADM_NOTIFY);
        noexec = saved_noexec;
    }

    return;
  error2:
    if(newf) fclose (newf);
  error_exit:
    xfree (line);
    if(fp) fclose (fp);
}

static void handle_notified (char *args, int len)
{
    call_in_directory (args, notified_a_file, NULL);
}

void
client_notify (char *repository, char *update_dir, char *filename,
    int notif_type, char *val, char *user)
{
    char buf[2];

    send_a_repository ("", repository, update_dir);
	if(user && supported_request("NotifyUser"))
	{
		send_to_server("NotifyUser ", 0);
		send_to_server(user, 0);
		send_to_server("\n", 1);
	}
    send_to_server ("Notify ", 0);
    send_to_server (filename, 0);
    send_to_server ("\n", 1);
    buf[0] = notif_type;
    buf[1] = '\0';
    send_to_server (buf, 1);
    send_to_server ("\t", 1);
    send_to_server (val, 0);
}

/*
 * Send an option with an argument, dealing correctly with newlines in
 * the argument.  If ARG is NULL, forget the whole thing.
 */
void option_with_arg (const char *option, const char *arg)
{
    if (arg == NULL)
		return;

    send_to_server ("Argument ", 0);
    send_to_server (option, 0);
    send_to_server ("\n", 1);

    send_arg (arg);
}

/* Send a date to the server.  The input DATE is in RCS format.
   The time will be GMT.

   We then convert that to the format required in the protocol
   (including the "-D" option) and send it.  According to
   cvsclient.texi, RFC 822/1123 format is preferred.  */

void client_senddate (const char *date)
{
    char buf[MAXDATELEN];

    date_to_internet (buf, (char *)date);
    option_with_arg ("-D", buf);
}

void
to_server_buffer_flush (void)
{
    buf_flush (to_server, 1);
}

void from_server_buffer_read (char **line, int *lenp)
{
    buf_read_line (from_server, line, lenp);
}
