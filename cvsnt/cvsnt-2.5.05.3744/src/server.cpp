/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#include "cvs.h"
#include "watch.h"
#include "edit.h"
#include "fileattr.h"
#include "getline.h"
#include "buffer.h"
#include "savecwd.h"

#include "../version.h"

/* For debug logs (in client.c) */
struct buffer *log_buffer_initialize(struct buffer *, FILE *, int, void (*) (struct buffer *));

/* The cvs username sent by the client, which might or might not be
   the same as the system username the server eventually switches to
   run as.  CVS_Username gets set iff password authentication is
   successful. */
const char *CVS_Username = NULL;

/* Should we check for system usernames/passwords?  Can be changed by
   CVSROOT/config.  */
int system_auth = 1;

/* We need to check this */
extern int root_allow_count;

/* Name of server command */
const char *server_command_name;

/* Do we chroot? */
char *chroot_base = NULL;
int chroot_done = 0;

/* Force run as a user */
char *runas_user = NULL;

/* Client version sent from system */
const char *serv_client_version = NULL;

#ifdef SERVER_SUPPORT

extern int server_io_socket;

static bool have_local_root = true;

// Size above which checksums are used rather than sending the file if a timestamp difference is detected
int server_checksum_threshold = 1048576*5; // 5MB

#ifdef _WIN32
#include <fcntl.h>
#endif

/* EWOULDBLOCK is not defined by POSIX, but some BSD systems will
   return it, rather than EAGAIN, for nonblocking writes.  */
#ifdef EWOULDBLOCK
#define blocking_error(err) ((err) == EWOULDBLOCK || (err) == EAGAIN)
#else
#define blocking_error(err) ((err) == EAGAIN)
#endif

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

/* For initgroups().  */
#if HAVE_INITGROUPS
#include <grp.h>
#endif /* HAVE_INITGROUPS */

# ifdef SERVER_SUPPORT

#   ifdef HAVE_GETSPNAM
#ifdef __hpux
#define command_name __hpux_command_name
#endif
#     include <shadow.h>
#ifdef __hpux
#undef command_name
#endif
#   endif


/* Redhat doesn't handle this correctly */
extern "C" {

#ifdef HAVE_PAM
#ifdef HAVE_SECURITY_PAM_MISC_H
#include <security/pam_misc.h>
#endif
#ifdef HAVE_PAM_PAM_MISC_H
#include <pam/pam_misc.h>
#endif
#ifdef HAVE_SECURITY_PAM_APPL_H
#include <security/pam_appl.h>
#endif
#ifdef HAVE_PAM_PAM_APPL_H
#include <pam/pam_appl.h>
#endif
#endif

} /* End of redhat extern */

# endif /* SERVER_SUPPORT */

#ifdef HAVE_SYSLOG_H
  #ifndef LOG_AUTHPRIV
  #define LOG_AUTHPRIV LOG_DAEMON
  #endif
#endif

static int checksum_valid;
static char stored_checksum[33];

// If true, the client has sent server-codepage, and is therefore codepage aware and we need do nothing at this end.
// If false, and there's a default codepage set, we translate in/out in text items from the client to our codepage.
static bool server_codepage_sent; 
static const char *default_client_codepage;

/* While processing requests, this buffer accumulates data to be sent to
   the client, and then once we are in do_cvs_command, we use it
   for all the data to be sent.  */
static struct buffer *buf_to_net;

/* This buffer is used to read input from the client.  */
static struct buffer *buf_from_net;

/* Buffer where output begins with 'T ' - linked to buf_to_net */
static struct buffer *stdout_buf;

/* Buffer where output begins with 'E ' - linked to buf_to_net */
static struct buffer *stderr_buf;

/*
 * This is where we stash stuff we are going to use.  Format string
 * which expects a single directory within it, starting with a slash.
 */
static char *server_temp_dir;

/*
 * Temporary protocol used during authentication 
 */
static const protocol_interface *temp_protocol;

/*
 *
 * protocol structure used in server mode.  See also client_protocol
 *
 */
const struct protocol_interface *server_protocol;

/* This is the original value of server_temp_dir, before any possible
   changes inserted by serve_max_dotdot.  */
static char *orig_server_temp_dir;

/* If Valid-RcsOptions is sent, this contains the list */
static char *valid_rcsoptions;

/* Nonzero if we should keep the temp directory around after we exit.  */
static int dont_delete_temp;

#define PROTOCOL_ENCRYPTION 2
#define PROTOCOL_AUTHENTICATION 1

/* This is set when encryption or authentication is enabled,
   It will be set to 0, PROTOCOL_ENCRYPTION, or PROTOCOL_AUTHENTICATION
   depending on what protocol-specific encryption/authentication requests
   are received.
 */
static int protocol_encryption_enabled = 0;

// Set to 1 if the data stream is compressed
static int protocol_compression_enabled = 0;

/* This is set to request/force encryption/compression */
/* 0=Any, 1=Request auth., 2=Request Encr., 3=Require Auth., 4=Require Encr. */
int encryption_level = 0; 
/* 0=Any, 1=Request compr., 2=Require compr. */
int compression_level = 0;

/* Regexp of clients allowed to connect */
const char *allowed_clients = NULL;

static void server_write_entries();
static void server_write_renames();

/* All server communication goes through buffer structures.  Most of
   the buffers are built on top of a file descriptor.  This structure
   is used as the closure field in a buffer.  */

struct fd_buffer
{
    /* The file descriptor.  */
    int fd;
    /* Nonzero if the file descriptor is in blocking mode.  */
    int blocking;
};

static int check_command_legal_p (const char *cmd_name);

int server_main(const char *cmd_name, int (*command)(int argc, char **argv));
int proxy_main(const char *cmd_name, int (*command)(int argc, char **argv));

static void do_chroot();

static struct buffer *fd_buffer_initialize(int, int, void (*) (struct buffer *));
static int fd_buffer_input(void *, char *, int, int, int *);
static int fd_buffer_output(void *, const char *, int, int *);
static int fd_buffer_flush(void *);
static int fd_buffer_block(void *, int);
static int fd_buffer_shutdown(void *);

static struct buffer *client_protocol_buffer_initialize(const struct protocol_interface *, int, void (*) (struct buffer *));
static int client_protocol_buffer_input(void *, char *, int, int, int *);
static int client_protocol_buffer_output(void *, const char *, int, int *);
static int client_protocol_buffer_flush(void *);
static int client_protocol_buffer_block(void *, int);
static int client_protocol_buffer_shutdown(void *);

static struct buffer *cvs_protocol_wrap_buffer_initialize (struct buffer *buf, char prefix);
static void cvs_protocol_wrap_set_buffer(struct buffer *buf, struct buffer *wrap);

static char *check_password (const char *username, const char *password, const char *repository, void **user_token);

#ifdef SERVER_SUPPORT
// buf_read_line with a possible line translation.  Avoid using it unless the line has a filename in it
// that require translation, due to the overhead.
static int server_read_line(struct buffer *buf, char **line, int *lenp);
static void server_buf_output0(buffer *buf, const char *string);
static void server_buf_output(buffer *buf, const char *data, int len);
#endif

static int io_getc(int fd)
{
	char c;
	if(read(fd,&c,1)<1)
		return EOF;
	return c;
}

int io_getline(int fd, char** buffer, int buffer_max)
{
	char *p;
	int l,c;

	*buffer=(char*)malloc(buffer_max);
	if(!*buffer)
		return -1;

	l=0;
	p=*buffer;
	*p='\0';
	while(l<buffer_max-1 && (c=io_getc(fd))!=EOF)
	{
		if(c=='\n')
			break;
		*(p++)=(char)c;
		l++;
	}
	if(l==0 && c==EOF)
		return -1; /* EOF */
	*p='\0';
	return l;
	
}

/* Initialize a buffer built on a file descriptor.  FD is the file
   descriptor.  INPUT is nonzero if this is for input, zero if this is
   for output.  MEMORY is the function to call when a memory error
   occurs.  */

static struct buffer *fd_buffer_initialize (int fd, int input, void (*memory)(struct buffer *))
{
    struct fd_buffer *n;

    n = (struct fd_buffer *) xmalloc (sizeof *n);
    n->fd = fd;
    n->blocking = 1;
    return buf_initialize (input ? fd_buffer_input : NULL,
			   input ? NULL : fd_buffer_output,
			   input ? NULL : fd_buffer_flush,
			   fd_buffer_block,
			   fd_buffer_shutdown,
			   memory,
			   n);
}

/* The buffer input function for a buffer built on a file descriptor.  */

static int fd_buffer_input (void *closure, char *data, int need, int size, int *got)
{
    struct fd_buffer *fd = (struct fd_buffer *) closure;
    int nbytes;

    if (! fd->blocking)
	nbytes = read (fd->fd, data, need);
    else
    {
	/* This case is not efficient.  Fortunately, I don't think it
           ever actually happens.  */
		error(1,0,"nonblocking fd - inneficient server");
    }

    if (nbytes > 0)
    {
	*got = nbytes;
	return 0;
    }

    *got = 0;

    if (nbytes == 0)
    {
	/* End of file.  This assumes that we are using POSIX or BSD
           style nonblocking I/O.  On System V we will get a zero
           return if there is no data, even when not at EOF.  */
	return -1;
    }

    /* Some error occurred.  */
#ifdef WIN32
	// Win32 always returns einval for a blocking read...
	// .. and for an error condition..  arrgh!
	if(errno == EINVAL)
		errno=EAGAIN;
#endif

    if (blocking_error (errno))
    {
	/* Everything's fine, we just didn't get any data.  */
	return 0;
    }

    /* Some other error, but no errno set */
    if (!errno)
	errno = EIO;
	
    return errno;
}

/* The buffer output function for a buffer built on a file descriptor.  */

static int fd_buffer_output (void *closure, const char *data, int have, int *wrote)
{
    struct fd_buffer *fd = (struct fd_buffer *) closure;

    *wrote = 0;

    while (have > 0)
    {
	int nbytes;

	nbytes = write (fd->fd, data, have);

	if (nbytes <= 0)
	{
	    if (! fd->blocking
		&& (nbytes == 0 || blocking_error (errno))
		)
	    {
		/* A nonblocking write failed to write any data.  Just
                   return.  */
		return 0;
	    }

	    /* Some sort of error occurred.  */

	    if (nbytes == 0)
	        return EIO;

	    return errno;
	}

	*wrote += nbytes;
	data += nbytes;
	have -= nbytes;
    }

    return 0;
}

/* The buffer flush function for a buffer built on a file descriptor.  */

/*ARGSUSED*/
static int fd_buffer_flush (void *closure)
{
    /* Nothing to do.  File descriptors are always flushed.  */
	/* .. except on Win32, which is wierd */
#ifdef _WIN32
    struct fd_buffer *fd = (struct fd_buffer *) closure;
	win32flush(fd->fd);
#endif
    return 0;
}

/* The buffer block function for a buffer built on a file descriptor.  */

static int fd_buffer_block (void *closure, int block)
{
#ifdef _WIN32
	/* This should only be called for pipes... */
    struct fd_buffer *fd = (struct fd_buffer *) closure;
	fd->blocking = block;
	win32setblock(fd->fd,block);
	return 0; 
#else
    struct fd_buffer *fd = (struct fd_buffer *) closure;
    int flags;

#ifdef F_GETFL
    flags = fcntl (fd->fd, F_GETFL, 0);
    if (flags < 0)
	return errno;

    if (block)
	flags &= ~O_NONBLOCK;
    else
	flags |= O_NONBLOCK;

    if (fcntl (fd->fd, F_SETFL, flags) < 0)
        return errno;
#else
    if (!block)
	return 1;
#endif

    fd->blocking = block;

    return 0;
#endif
}

/* The buffer shutdown function for a buffer built on a file descriptor.  */

static int fd_buffer_shutdown (void *closure)
{
    xfree (closure);
    return 0;
}

/* Initialize a buffer built on a custom client protocol.  protocol is the 
   descriptor.  INPUT is nonzero if this is for input, zero if this is
   for output.  MEMORY is the function to call when a memory error
   occurs.  */

struct client_protocol_data
{
	const struct protocol_interface *protocol;
	int server_io_socket;
};

static struct buffer *client_protocol_buffer_initialize (const struct protocol_interface *protocol, int input, void (*memory)(struct buffer *))
{
	client_protocol_data *data = new client_protocol_data;
	data->protocol = protocol;
	data->server_io_socket = server_io_socket;
    return buf_initialize (input ? client_protocol_buffer_input : NULL,
			   input ? NULL : client_protocol_buffer_output,
			   input ? NULL : client_protocol_buffer_flush,
			   client_protocol_buffer_block,
			   input ? NULL : client_protocol_buffer_shutdown, /* We only shutdown when buf_to_net is closed */
			   memory,
			   (void*)data);
}

/* The buffer input function for a client protocol buffer.  */

static int client_protocol_buffer_input (void *closure, char *data, int need, int size, int *got)
{
	client_protocol_data *dat = (client_protocol_data*)closure;
	int nbytes;

	if(dat->protocol && dat->protocol->server_read_data)
		nbytes = dat->protocol->server_read_data(dat->protocol, data, size);
	else
		nbytes = read(dat->server_io_socket, data, size);

    if (nbytes > 0)
    {
	*got = nbytes;
	return 0;
    }

    *got = 0;

    if (nbytes == 0)
    {
	/* End of file.  This assumes that we are using POSIX or BSD
           style nonblocking I/O.  On System V we will get a zero
           return if there is no data, even when not at EOF.  */
	return -1;
    }

    /* Some error occurred.  */

    if (blocking_error (errno))
    {
	/* Everything's fine, we just didn't get any data.  */
	return 0;
    }

    /* Some other error, but no errno set */
    if (!errno)
	errno = EIO;
	
    return errno;
}

/* The buffer output function for a client prototocol buffer.  */

static int client_protocol_buffer_output (void *closure, const char *data, int have, int *wrote)
{
	client_protocol_data *dat = (client_protocol_data*)closure;

    *wrote = 0;

    while (have > 0)
    {
	int nbytes;

	if(dat->protocol && dat->protocol->server_write_data)
		nbytes = dat->protocol->server_write_data (dat->protocol, data, have);
	else
		nbytes = write (dat->server_io_socket?dat->server_io_socket:STDOUT_FILENO, data, have);

	if (nbytes <= 0)
	{
	    /* Some sort of error occurred.  */

	    if (nbytes == 0)
	        return EIO;

	    return errno;
	}

	*wrote += nbytes;
	data += nbytes;
	have -= nbytes;
    }

    return 0;
}

/* The buffer flush function for a buffer built on a file descriptor.  */

/*ARGSUSED*/
static int client_protocol_buffer_flush (void *closure)
{
	client_protocol_data *data = (client_protocol_data*)closure;
	if (data) if(data->protocol && data->protocol->server_flush_data)
		data->protocol->server_flush_data(data->protocol);

    return 0;
}

/* The buffer block function for a client protocol buffer.  */

static int client_protocol_buffer_block (void *closure, int block)
{
	/* not sure what to do here... */
    if (!block)
	return 1;
    return 0;
}

/* The buffer shutdown function for a client protocol buffer.  */

static int client_protocol_buffer_shutdown (void *closure)
{
	client_protocol_data *data = (client_protocol_data*)closure;
	if(data->protocol && data->protocol->server_shutdown)
		data->protocol->server_shutdown(data->protocol);
	else if(data->server_io_socket)
	{
#ifdef _WIN32
		shutdown(_get_osfhandle(data->server_io_socket),2);
		closesocket(_get_osfhandle(data->server_io_socket));
#else
		shutdown(data->server_io_socket,2);
		close(data->server_io_socket);
#endif
	}
    return 0;
}

/* Populate all of the directories between BASE_DIR and its relative
   subdirectory DIR with CVSADM directories.  Return 0 for success or
   errno value.  */
static int create_adm_p (char *base_dir, char *dir)
{
    char *dir_where_cvsadm_lives, *dir_to_register, *p, *tmp;
    int retval, done;
    FILE *f;

    if (strcmp (dir, ".") == 0)
	return 0;			/* nothing to do */

    /* Allocate some space for our directory-munging string. */
    p = (char*)xmalloc (strlen (dir) + 1);
    if (p == NULL)
	return ENOMEM;

    dir_where_cvsadm_lives = (char*)xmalloc (strlen (base_dir) + strlen (dir) + 100);
    if (dir_where_cvsadm_lives == NULL)
	return ENOMEM;

    /* Allocate some space for the temporary string in which we will
       construct filenames. */
    tmp = (char*)xmalloc (strlen (base_dir) + strlen (dir) + 100);
    if (tmp == NULL)
	return ENOMEM;

    
    /* We make several passes through this loop.  On the first pass,
       we simply create the CVSADM directory in the deepest directory.
       For each subsequent pass, we try to remove the last path
       element from DIR, create the CVSADM directory in the remaining
       pathname, and register the subdirectory in the newly created
       CVSADM directory. */

    retval = done = 0;

    strcpy (p, dir);
    strcpy (dir_where_cvsadm_lives, base_dir);
    strcat (dir_where_cvsadm_lives, "/");
    strcat (dir_where_cvsadm_lives, p);
    dir_to_register = NULL;

    while (1)
    {
	/* Create CVSADM. */
	(void) sprintf (tmp, "%s/%s", dir_where_cvsadm_lives, CVSADM);
	if ((CVS_MKDIR (tmp, 0777) < 0) && (errno != EEXIST))
	{
	    retval = errno;
	    goto finish;
	}

	/* Create CVSADM_REP. */
	(void) sprintf (tmp, "%s/%s", dir_where_cvsadm_lives, CVSADM_REP);
	if (! isfile (tmp))
	{
	    /* Use Emptydir as the placeholder until the client sends
	       us the real value.  This code is similar to checkout.c
	       (emptydir_name), but the code below returns errors
	       differently.  */

	    char *empty;
	    empty = (char*)xmalloc (strlen (current_parsed_root->directory)
			    + sizeof (CVSROOTADM)
			    + sizeof (CVSNULLREPOS)
			    + 3);
	    if (! empty)
	    {
		retval = ENOMEM;
		goto finish;
	    }

	    /* Create the directory name. */
	    (void) sprintf (empty, "%s/%s/%s", current_parsed_root->directory,
			    CVSROOTADM, CVSNULLREPOS);

	    /* Create the directory if it doesn't exist. */
	    if (! isfile (empty))
	    {
		mode_t omask;
		omask = umask (cvsumask);
		if (CVS_MKDIR (empty, 0777) < 0)
		{
		    retval = errno;
		    xfree (empty);
		    goto finish;
		}
		(void) umask (omask);
	    }
	    
	    
	    f = CVS_FOPEN (tmp, "w");
	    if (f == NULL)
	    {
		retval = errno;
		xfree (empty);
		goto finish;
	    }
	    /* Write the directory name to CVSADM_REP. */
	    if (fprintf (f, "%s\n", empty) < 0)
	    {
		retval = errno;
		fclose (f);
		xfree (empty);
		goto finish;
	    }
	    if (fclose (f) == EOF)
	    {
		retval = errno;
		xfree (empty);
		goto finish;
	    }

	    /* Clean up after ourselves. */
	    xfree (empty);
	}

	/* Create CVSADM_ENT.  We open in append mode because we
	   don't want to clobber an existing Entries file.  */
	(void) sprintf (tmp, "%s/%s", dir_where_cvsadm_lives, CVSADM_ENT);
	f = CVS_FOPEN (tmp, "a");
	if (f == NULL)
	{
	    retval = errno;
	    goto finish;
	}
	if (fclose (f) == EOF)
	{
	    retval = errno;
	    goto finish;
	}

	/* Create CVSADM_ENTEXT.  We open in append mode because we
	   don't want to clobber an existing Entries.Extra file.  */
	(void) sprintf (tmp, "%s/%s", dir_where_cvsadm_lives, CVSADM_ENTEXT);
	f = CVS_FOPEN (tmp, "a");
	if (f == NULL)
	{
	    retval = errno;
	    goto finish;
	}
	if (fclose (f) == EOF)
	{
	    retval = errno;
	    goto finish;
	}

	if (dir_to_register != NULL)
	{
	    /* FIXME: Yes, this results in duplicate entries in the
	       Entries.Log file, but it doesn't currently matter.  We
	       might need to change this later on to make sure that we
	       only write one entry.  */

	    Subdir_Register ((List *) NULL, dir_where_cvsadm_lives,
			     dir_to_register);
	}

	if (done)
	    break;

	dir_to_register = strrchr (p, '/');
	if (dir_to_register == NULL)
	{
	    dir_to_register = p;
	    strcpy (dir_where_cvsadm_lives, base_dir);
	    done = 1;
	}
	else
	{
	    *dir_to_register = '\0';
	    dir_to_register++;
	    strcpy (dir_where_cvsadm_lives, base_dir);
	    strcat (dir_where_cvsadm_lives, "/");
	    strcat (dir_where_cvsadm_lives, p);
	}
    }

  finish:
    xfree (tmp);
    xfree (dir_where_cvsadm_lives);
    xfree (p);
    return retval;
}

/*
 * Make directory DIR, including all intermediate directories if necessary.
 * Returns 0 for success or errno code.
 */
static int mkdir_p (char *dir)
{
    char *p;
    char *q = (char*)xmalloc (strlen (dir) + 1);
    int retval;

    if (q == NULL)
	return ENOMEM;

    retval = 0;

    /*
     * Skip over leading slash if present.  We won't bother to try to
     * make '/'.
     */
    p = dir + 1;
    while (1)
    {
	while (*p != '/' && *p != '\0')
	    ++p;
	if (*p == '/')
	{
	    strncpy (q, dir, p - dir);
	    q[p - dir] = '\0';
	    if (q[p - dir - 1] != '/'  &&  CVS_MKDIR (q, 0777) < 0)
	    {
		int saved_errno = errno;

		if (saved_errno != EEXIST
		    && ((saved_errno != EACCES && saved_errno != EROFS)
			|| !isdir (q)))
		{
		    retval = saved_errno;
		    goto done;
		}
	    }
	    ++p;
	}
	else
	{
	    if (CVS_MKDIR (dir, 0777) < 0)
		retval = errno;
	    goto done;
	}
    }
  done:
    xfree (q);
    return retval;
}

/*
 * Print the error response for error code STATUS.  The caller is
 * reponsible for making sure we get back to the command loop without
 * any further output occuring.
 * Must be called only in contexts where it is OK to send output.
 */
static void print_error (int status)
{
    char *msg;
    char tmpstr[80];

    buf_output0 (buf_to_net, "error  ");
    msg = strerror (status);
    if (msg == NULL)
    {
       sprintf (tmpstr, "unknown error %d", status);
       msg = tmpstr;
    }
    server_buf_output0 (buf_to_net, msg);
    buf_append_char (buf_to_net, '\n');

    buf_flush (buf_to_net, 0);
}


static int supported_response (char *name)
{
    struct response *rs;

    for (rs = responses; rs->name != NULL; ++rs)
	if (strcmp (rs->name, name) == 0)
	    return rs->status == rs_supported;
    error (1, 0, "internal error: testing support for unknown response %s?",name);
    /* NOTREACHED */
    return 0;
}

static void serve_valid_responses (char *arg)
{
    char *p = arg;
    char *q;
    struct response *rs;
    do
    {
	q = strchr (p, ' ');
	if (q != NULL)
	    *q++ = '\0';
	for (rs = responses; rs->name != NULL; ++rs)
	{
	    if (strcmp (rs->name, p) == 0)
		break;
	}
	if (rs->name == NULL)
	    /*
	     * It is a response we have never heard of (and thus never
	     * will want to use).  So don't worry about it.
	     */
	    ;
	else
	    rs->status = rs_supported;
	p = q;
    } while (q != NULL);
    for (rs = responses; rs->name != NULL; ++rs)
    {
	if (rs->status == rs_essential)
	{
	    buf_output0 (buf_to_net, "E response `");
	    buf_output0 (buf_to_net, rs->name);
	    buf_output0 (buf_to_net, "' not supported by client\nerror  \n");

	    /* FIXME: This call to buf_flush could conceivably
	       cause deadlock, as noted in server_cleanup.  */
	    buf_flush (buf_to_net, 1);

	    /* I'm doing this manually rather than via error_exit ()
	       because I'm not sure whether we want to call server_cleanup.
	       Needs more investigation....  */

#ifdef SYSTEM_CLEANUP
	    /* Hook for OS-specific behavior, for example socket subsystems on
	       NT and OS2 or dealing with windows and arguments on Mac.  */
	    SYSTEM_CLEANUP ();
#endif
		CCvsgui::Close(EXIT_FAILURE);

	    exit (EXIT_FAILURE);
	}
	else if (rs->status == rs_optional)
	    rs->status = rs_not_supported;
    }

	if(supported_response("EntriesExtra"))
		compat_level = 1; /* CVSNT client */
	else
		compat_level = 0; /* Legacy client */
	TRACE(3,"Client compatibility level is %d",compat_level);
}

static void serve_root (char *arg)
{
    char *path;
	const char *real_repository,*repository_name;
	
    if(protocol_encryption_enabled != PROTOCOL_ENCRYPTION && client_protocol && client_protocol->valid_elements&flagAlwaysEncrypted)
    {
		buf_flush(stderr_buf,1);
		buf_flush(stdout_buf,1);
		protocol_encryption_enabled = PROTOCOL_ENCRYPTION;
    }


    if (!isabsolute (arg))
    {
		error(1,0,"Root %s must be an absolute pathname", arg);
		return;
    }

	/* Sending "Root" twice is illegal.

       The other way to handle a duplicate Root requests would be as a
       request to clear out all state and start over as if it was a
       new connection.  Doing this would cause interoperability
       headaches, so it should be a different request, if there is
       any reason why such a feature is needed.  */
    if (current_parsed_root != NULL)
    {
		error(1,0,"Protocol error: Duplicate Root request, for %s", arg);
		return;
    }
 
#ifdef SERVER_SUPPORT
    if (server_protocol && server_protocol->auth_repository != NULL)
    {
	if (fncmp (server_protocol->auth_repository, arg) != 0)
	{
		/* The explicitness is to aid people who are writing clients.
		   I don't see how this information could help an
		   attacker.  */
		   error(1,0,"Protocol error: Root says \"%s\" but protocol says \"%s\"",
			 arg, server_protocol->auth_repository);
	}
    }
#endif

    if (current_parsed_root != NULL)
		free_cvsroot_t (current_parsed_root);

	/* For gserver, which doesn't pass a root as part of its protocol, check for a valid root here */
	/* For other protocols (pserver, ntserver) this is harmless duplication */
	/* We can be called with 'cvs server' one of two ways.  Either directly, from
		a script or inetd, or from a wrapper which sets the allow-root directives. */
	real_repository = arg;
	repository_name = arg;
	const root_allow_struct *found_root = NULL;

	if(server_protocol && server_protocol->auth_proxyname)
		repository_name = server_protocol->auth_proxyname;

	if(!root_allow_ok(arg,found_root,real_repository,false))
	{
		error(1,0,"%s: no such repository", real_repository);
	}

	if(!found_root->online)
	{
		error(1,0,"%s: repository is offline", real_repository);
	}
	
	current_parsed_root = local_cvsroot(repository_name,real_repository,found_root->readwrite,found_root->repotype,found_root->remote_server.c_str(),found_root->remote_repository.c_str(),found_root->proxy_repository.c_str(),found_root->remote_passphrase.c_str());
   
	umask(0); 
	do_chroot();

	if (!server_protocol || !server_protocol->auth_repository )
	{
	    parse_config( current_parsed_root->directory );
	}

    /* For pserver, this will already have happened, and the call will do
       nothing.  But for other protocols, we need to do it now.  */
	if(server_protocol && !server_protocol->auth_repository)
	{
		char *host_user;

		/* If we haven't verified the user before, then do it here.  The password will be NULL but we pass
		   it anyway just in case this changes in the future. */
		host_user = check_password (server_protocol->auth_username, server_protocol->auth_password,
									current_parsed_root->directory, /*user_token*/NULL);

		if(!host_user)
		{
			CServerIo::log(CServerIo::logAuth,"login failure for %s on %s", server_protocol->auth_username, server_protocol->auth_repository);
			error (1, 0,
				"authorization failed: server %s rejected access to %s for user %s",
				hostname, current_parsed_root->unparsed_directory, server_protocol->auth_username);
		}
		xfree(host_user);
	}

	if(current_parsed_root->type!=RootTypeProxyAll && current_parsed_root->directory && *current_parsed_root->directory)
	{
		path = (char*)xmalloc (strlen (current_parsed_root->directory)
			+ sizeof (CVSROOTADM)
			+ 2);
		if (path == NULL)
		{
			error(1,ENOMEM,"Alloc failed");
			return;
		}
		(void) sprintf (path, "%s/%s", current_parsed_root->directory, CVSROOTADM);
		if (!isaccessible (path, R_OK | X_OK))
		{
			error(1,errno,"Cannot access %s", path);
		}
		xfree (path);

		lock_register_client(CVS_Username,current_parsed_root->directory);
		have_local_root = true;
	}
	else
		have_local_root = false;

#ifdef HAVE_PUTENV
	cvs_putenv(CVSROOT_ENV, current_parsed_root->directory);
#endif

	/* Setup internal cvsignore/cvswrappers that will have been skipped earlier in main() */
	ign_setup();
	wrap_setup();
}

static void serve_valid_rcsoptions(char *arg)
{
	if(valid_rcsoptions)
		error(1,0,"Protocol error - Valid-RcsOptions sent more than once");
	valid_rcsoptions = xstrdup(arg);
}

static int max_dotdot_limit = 0;

/* Is this pathname OK to recurse into when we are running as the server?
   If not, call error() with a fatal error.  */
void server_pathname_check (const char *path)
{
    /* An absolute pathname is almost surely a path on the *client* machine,
       and is unlikely to do us any good here.  It also is probably capable
       of being a security hole in the anonymous readonly case.  */
    if (isabsolute (path))
	/* Giving an error is actually kind of a cop-out, in the sense
	   that it would be nice for "cvs co -d /foo/bar/baz" to work.
	   A quick fix in the server would be requiring Max-dotdot of
	   at least one if pathnames are absolute, and then putting
	   /abs/foo/bar/baz in the temp dir beside the /d/d/d stuff.
	   A cleaner fix in the server might be to decouple the
	   pathnames we pass back to the client from pathnames in our
	   temp directory (this would also probably remove the need
	   for Max-dotdot).  A fix in the client would have the client
	   turn it into "cd /foo/bar; cvs co -d baz" (more or less).
	   This probably has some problems with pathnames which appear
	   in messages.  */
	error (1, 0, "absolute pathname `%s' illegal for server", path);
    if (pathname_levels (path) > max_dotdot_limit)
    {
	/* Similar to the isabsolute case in security implications.  */
	error (0, 0, "protocol error: `%s' contains more leading ..", path);
	error (1, 0, "than the %d which Max-dotdot specified",
	       max_dotdot_limit);
    }
}

/* Is file or directory REPOS an absolute pathname within the
   current_parsed_root->directory?  If yes, return 0.  If not, abort. */
static int outside_root (const char *repos)
{
    size_t repos_len;
    size_t root_len = strlen (current_parsed_root->unparsed_directory);
    const char *cp;

	repos_len = strlen(repos);

    /* I think isabsolute (repos) should always be true, and that
       any RELATIVE_REPOS stuff should only be in CVS/Repository
       files, not the protocol (for compatibility), but I'm putting
       in the isabsolute check just in case.  */
    if (!isabsolute (repos))
    {
		error(1,0,"protocol error: %s is not absolute", repos);
		return 1;
    }

    if (repos_len < root_len || fnncmp (repos, current_parsed_root->unparsed_directory, root_len) != 0)
    {
    not_within:

		error(1,0,"protocol error: directory '%s' not within root '%s'",
		     repos, current_parsed_root->unparsed_directory);
		return 1;
    }
	cp=repos+root_len;
    if (*cp)
    {
	if ((*cp) != '/')
	    goto not_within;
	if (pathname_levels (cp) > 0)
	    goto not_within;
    }

    return 0;
}

/* Is file or directory FILE outside the current directory (that is, does
   it contain '/')?  If no, return 0.  If yes, abort. */
static int outside_dir (char *file)
{
    if (strchr (file, '/') != NULL)
    {
		error(1,0,"protocol error: directory '%s' not within current directory",
			file);
		return 1;
    }
    return 0;
}
	
/*
 * Add as many directories to the temp directory as the client tells us it
 * will use "..", so we never try to access something outside the temp
 * directory via "..".
 */
static void serve_max_dotdot (char *arg)
{
    int lim = atoi (arg);
    int i;
    char *p;

    if (lim < 0 || lim > 20)
	{
		error(1,0,"Invalid value for max_dotdot");
		return;
	}
    p = (char*)xmalloc (strlen (server_temp_dir) + (sizeof(CVSDUMMY)+5) * lim + 10);
    if (p == NULL)
    {
		error(1,ENOMEM,"Alloc failed");
		return;
    }
    strcpy (p, server_temp_dir);
    for (i = 0; i < lim; ++i)
		strcat (p, "/"CVSDUMMY);
    if (server_temp_dir != orig_server_temp_dir)
		xfree (server_temp_dir);
    server_temp_dir = p;
    max_dotdot_limit = lim;
}

static char *dir_name;

static void dirswitch (char *dir, char *repos)
{
    int status;
    FILE *f;
    size_t dir_len;

    server_write_entries();
	server_write_renames();

	/* Check for bad directory name.
       FIXME: could/should unify these checks with server_pathname_check
       except they need to report errors differently.  */
    if (isabsolute (dir))
	{
		error(1,0,"absolute pathname `%s' illegal for server", dir);
		return;
	}

    if (pathname_levels (dir) > max_dotdot_limit)
    {
		error(1,0,"protocol error: `%s' has too many ..", dir);
		return;
    }

	if(!strcmp(dir,".") && !strcmp(repos,current_parsed_root->unparsed_directory) && max_dotdot_limit > 0)
	{
		/* The client is in a subdirectory and has set us to the root.  To stop the server parsing
		   outside the repository root and potentially leaking information to a hacker, we add
		   CVSDUMMY onto the end of the repository */
		char *r, *p;
		int n;
	    struct saved_cwd cwd;

		save_cwd(&cwd);
		if(CVS_CHDIR(server_temp_dir))
			error(1,errno,"Couldn't change directory to %s",server_temp_dir);
		for(n=0; n<max_dotdot_limit; n++)
			CVS_CHDIR("..");
		repos = Name_Repository(NULL,NULL);
		restore_cwd(&cwd,NULL);
		r = (char*)xmalloc(strlen(repos)+strlen(current_parsed_root->unparsed_directory)+(sizeof(CVSDUMMY)+2)*max_dotdot_limit+100);
		sprintf(r,"%s/%s",current_parsed_root->unparsed_directory,relative_repos(repos));
		xfree(repos);
		p=r+strlen(r);
		for(n=0; n<max_dotdot_limit; n++)
		{
			*(p++)='/';
			strcpy(p,CVSDUMMY);
			p+=sizeof(CVSDUMMY)-1;
		}
		*p='\0';
		repos = r; /* We leak this, but it should only ever be once per session */
	}

    dir_len = strlen (dir);

    /* Check for a trailing '/'.  This is not ISDIRSEP because \ in the
       protocol is an ordinary character, not a directory separator (of
       course, it is perhaps unwise to use it in directory names, but that
       is another issue).  */
    if (dir_len > 0 && dir[dir_len - 1] == '/')
    {
		error(1,0,"protocol error: invalid directory syntax in %s", dir);
		return;
    }

	xfree (dir_name);

    dir_name = (char*)xmalloc (strlen (server_temp_dir) + dir_len + 40);
    if (dir_name == NULL)
    {
		error(1,ENOMEM,"Alloc failed");
		return;
    }
    
    strcpy (dir_name, server_temp_dir);
	if(*dir)
	{
		strcat (dir_name, "/");
		strcat (dir_name, dir);
	}

    status = mkdir_p (dir_name);
    if (status != 0
	&& status != EEXIST)
    {
		error(1,errno,"cannot mkdir %s", dir_name);
		return;
    }

    /* We need to create adm directories in all path elements because
       we want the server to descend them, even if the client hasn't
       sent the appropriate "Argument xxx" command to match the
       already-sent "Directory xxx" command.  See recurse.c
       (start_recursion) for a big discussion of this.  */

    status = create_adm_p (server_temp_dir, dir);
    if (status != 0)
    {
		error(1,errno,"cannot create_adm_p %s", dir_name);
		return;
    }

    if ( CVS_CHDIR (dir_name) < 0)
    {
		error(1,errno,"cannot change to %s", fn_root(dir_name));
		return;
    }
    /*
     * This is pretty much like calling Create_Admin, but Create_Admin doesn't
     * report errors in the right way for us.
     */
    if ((CVS_MKDIR (CVSADM, 0777) < 0) && (errno != EEXIST))
    {
		error(1,errno,"cannot mkdir %s/%s",fn_root(dir_name),CVSADM);
		return;
    }

    /* The following will overwrite the contents of CVSADM_REP.  This
       is the correct behavior -- mkdir_p may have written a
       placeholder value to this file and we need to insert the
       correct value. */

    f = CVS_FOPEN (CVSADM_REP, "w");
    if (f == NULL)
    {
		error(1,errno,"cannot open %s/%s", fn_root(dir_name), CVSADM_REP);
		return;
    }

	if (fprintf (f, "%s", current_parsed_root->directory) < 0)
	{
		fclose (f);
		error(1,errno,"error writing %s/%s", fn_root(dir_name), CVSADM_REP);
		return;
	}

    if (fprintf (f, "%s", repos + strlen(current_parsed_root->unparsed_directory)) < 0)
    {
		fclose (f);
		error(1,errno,"error writing %s/%s", fn_root(dir_name), CVSADM_REP);
		return;
    }

    /* Non-remote CVS handles a module representing the entire tree
       (e.g., an entry like ``world -a .'') by putting /. at the end
       of the Repository file, so we do the same.  */
    if (strcmp (dir, ".") == 0
	&& current_parsed_root != NULL
	&& current_parsed_root->unparsed_directory != NULL
	&& strcmp (current_parsed_root->unparsed_directory, repos) == 0)
    {
        if (fprintf (f, "/.") < 0)
	{
	    fclose (f);
		error(1,errno,"error writing %s/%s",fn_root(dir_name), CVSADM_REP);
	    return;
	}
    }
    if (fprintf (f, "\n") < 0)
    {
		fclose (f);
		error(1,errno,"error writing %s/%s",fn_root(dir_name), CVSADM_REP);
		return;
    }
    if (fclose (f) == EOF)
    {
		error(1,errno,"error closing %s/%s",fn_root(dir_name), CVSADM_REP);
		return;
    }
    /* We open in append mode because we don't want to clobber an
       existing Entries file.  */
    f = CVS_FOPEN (CVSADM_ENT, "a");
    if (f == NULL)
    {
	    error(1,errno, "cannot open %s", CVSADM_ENT);
		return;
    }
    if (fclose (f) == EOF)
    {
		error(1,errno,"cannot close %s",CVSADM_ENT);
		return;
    }
    /* We open in append mode because we don't want to clobber an
       existing Entries file.  */
    f = CVS_FOPEN (CVSADM_ENTEXT, "a");
    if (f == NULL)
    {
		error(1,errno,"cannot open %s",CVSADM_ENTEXT);
		return;
    }
    if (fclose (f) == EOF)
    {
		error(1,errno,"cannot close %s",CVSADM_ENTEXT);
		return;
    }
}

static void
serve_protocol_encrypt(char *arg)
{
	if(protocol_encryption_enabled)
		return;

	if(server_protocol && !server_protocol->wrap && !(server_protocol->valid_elements&flagAlwaysEncrypted))
	{
		error(1,0,"Requested protocol does not support encryption");
		return;
	}

	buf_flush(stderr_buf, 1);
	buf_flush(stdout_buf, 1);

	if(server_protocol->wrap)
	{
		buf_to_net = cvs_encrypt_wrap_buffer_initialize (server_protocol, buf_to_net, 0,
														1,
														buf_to_net->memory_error);
		buf_from_net = cvs_encrypt_wrap_buffer_initialize (server_protocol, buf_from_net, 1,
														1,
														buf_from_net->memory_error);
		cvs_protocol_wrap_set_buffer(stdout_buf, buf_to_net);
		cvs_protocol_wrap_set_buffer(stderr_buf, buf_to_net);
	}

	protocol_encryption_enabled = PROTOCOL_ENCRYPTION;
}

static void
serve_protocol_authenticate(char *arg)
{
	if(protocol_encryption_enabled)
		return;

	if(!server_protocol->wrap && !(server_protocol->valid_elements&flagAlwaysEncrypted))
	{
		error(1,0,"Requested protocol does not support authentication");
		return;
	}

	buf_flush(stderr_buf, 1);
	buf_flush(stdout_buf, 1);

	if(server_protocol->wrap)
	{
		buf_to_net = cvs_encrypt_wrap_buffer_initialize (server_protocol, buf_to_net, 0,
														0,
														buf_to_net->memory_error);
		buf_from_net = cvs_encrypt_wrap_buffer_initialize (server_protocol, buf_from_net, 1,
														0,
														buf_from_net->memory_error);

		cvs_protocol_wrap_set_buffer(stdout_buf, buf_to_net);
		cvs_protocol_wrap_set_buffer(stderr_buf, buf_to_net);
	}

	protocol_encryption_enabled = PROTOCOL_AUTHENTICATION;
}

static void serve_directory (char *arg)
{
    int status;
    char *repos;

    status = server_read_line (buf_from_net, &repos, (int *) NULL);
    if (status == 0)
    {
		if (!outside_root (repos))
		    dirswitch (arg, repos);
		xfree (repos);
    }
    else if (status == -2)
    {
		error(1,ENOMEM,"Alloc failed");
		return;
    }
    else
    {
		if (status == -1)
		{
			error(1,0,"end of file reading mode for %s", arg);
		}
		else
		{
			error(1,0,"error reading mode for %s", arg);
		}
    }
}

static void serve_static_directory (char *arg)
{
    FILE *f;

    f = CVS_FOPEN (CVSADM_ENTSTAT, "w+");
    if (f == NULL)
    {
		error(1,errno,"cannot open %s", CVSADM_ENTSTAT);
		return;
    }
    if (fclose (f) == EOF)
    {
		error(1,errno,"cannot close %s", CVSADM_ENTSTAT);
		return;
    }
}

static void serve_sticky (char *arg)
{
    FILE *f;

	/* Just in case of hand-editing... probably harmless but best to ignore it */
	if(!arg)
		return;
	if((arg[0]=='T' || arg[0]=='N') && !RCS_check_tag(arg+1,true,true,false))
	{
	   error(0,0,"Invalid directory sticky tag '%s' ignored",arg+1);
	   return;
	}
	else if(arg[0]!='D' && arg[0]!='T' && arg[0]!='N')
	{
	   error(0,0,"Invalid directory sticky tag '%s' ignored",arg);
	   return;
	}


    f = CVS_FOPEN (CVSADM_TAG, "w+");
    if (f == NULL)
    {
		error(1,errno,"cannot open %s", CVSADM_TAG);
		return;
    }
    if (fprintf (f, "%s\n", arg) < 0)
    {
		error(1,errno,"cannot write to %s", CVSADM_TAG);
		return;
    }
    if (fclose (f) == EOF)
    {
		error(1,errno,"cannot close %s", CVSADM_TAG);
		return;
    }
}

/*
 * Read SIZE bytes from buf_from_net, write them to FILE.
 *
 * Currently this isn't really used for receiving parts of a file --
 * the file is still sent over in one chunk.  But if/when we get
 * spiffy in-process gzip support working, perhaps the compressed
 * pieces could be sent over as they're ready, if the network is fast
 * enough.  Or something.
 */
static void receive_partial_file (int size, int file, bool check_textfile, CMD5Calc* md5, bool& modified)
{
    while (size > 0)
    {
		int status;
		int nread;
		char *data;

		status = buf_read_data (buf_from_net, size, &data, &nread);
		if (status != 0)
		{
			if (status == -2)
				error(1,ENOMEM,"Alloc failed");
			else
			{
				if (status == -1)
				{
					error(1,0,"premature end of file from client");
				}
				else
				{
					error(1,0,"error reading from client");
				}
			}
			return;
		}

		size -= nread;

		if(check_textfile && nread)
		{
			/* Don't allow people to corrupt their repositories, even accidentally */
			if(memchr(data,'\r',nread)!=NULL)
			{
				CCodepage cdp;
				size_t nr = (size_t)nread;
				cdp.StripCrLf(data,nr);
				nread = (int)nr;
				modified = true;
			}
		}

		if(md5)
			md5->Update(data,nread);			

		while (nread > 0)
		{
			int nwrote;

			nwrote = write (file, data, nread);
			if (nwrote < 0)
			{
				error(1,errno,"unable to write");

				/* Read and discard the file data.  */
				while (size > 0)
				{
					int status, nread;
					char *data;

					status = buf_read_data (buf_from_net, size, &data, &nread);
					if (status != 0)
					return;
					size -= nread;
				}

				return;
			}
			nread -= nwrote;
			data += nwrote;
		}
    }
}

/* Receive SIZE bytes, write to filename FILE.  */
static void receive_file (int size, char *file, bool check_textfile)
{
    int fd;
    char *arg = file;
	bool modified = false;
	CMD5Calc *md5 = NULL;

	if(checksum_valid)
		md5 = new CMD5Calc;

    /* Write the file.  */
    fd = CVS_OPEN (arg, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
    {
		delete md5;
		error(1,errno,"cannot open %s",arg);
		return;
    }

#ifdef _WIN32
	// Statistics tracking
	if(!check_textfile)
	{
		int binarycount=0,binaryavg=0;
		CGlobalSettings::GetGlobalValue("cvsnt","PServer","BinaryCount",binarycount);
		CGlobalSettings::GetGlobalValue("cvsnt","PServer","BinaryAverage",binaryavg);
		binarycount++;
		binaryavg=(binaryavg+size)/2;
		CGlobalSettings::SetGlobalValue("cvsnt","PServer","BinaryCount",binarycount);
		CGlobalSettings::SetGlobalValue("cvsnt","PServer","BinaryAverage",binaryavg);
	}
	else
	{
		int textcount=0,textavg=0;
		CGlobalSettings::GetGlobalValue("cvsnt","PServer","TextCount",textcount);
		CGlobalSettings::GetGlobalValue("cvsnt","PServer","TextAverage",textavg);
		textcount++;
		textavg=(textavg+size)/2;
		CGlobalSettings::SetGlobalValue("cvsnt","PServer","TextCount",textcount);
		CGlobalSettings::SetGlobalValue("cvsnt","PServer","TextAverage",textavg);
	}
#endif

	receive_partial_file (size, fd, check_textfile, md5, modified);

    if (close (fd) < 0)
    {
		delete md5;
		error(1,errno,"cannot close %s", arg);
		return;
    }

	if(md5)
	{
		const char *chk = md5->Final();
		/* If the file has been modified (client sent corrupt data) then we
		can't check for corruption in the data stream.  This case can't happen
		with cvsnt clients, which currently are the only ones that send the checksum */
		if(!modified && checksum_valid && strcmp(stored_checksum,chk))
		{
			error(1,errno,"Corrupt transmission of file '%s'.  Check network.",fn_root(file));
		}
		delete md5;
	}
	checksum_valid = 0;
}

/* Kopt for the next file sent in Modified or Is-modified.  */
static char *kopt;

/* Timestamp (Checkin-time) for next file sent in Modified or
   Is-modified.  */
static int checkin_time_valid;
static time_t checkin_time;

static void serve_is_modified(char *arg);
static kflag serve_file_kopt (char *arg);

static void serve_modified (char *arg)
{
    int size, status;
    char *size_text;
    char *mode_text;

    /*
     * This used to return immediately if error_pending () was true.
     * However, that fails, because it causes each line of the file to
     * be echoed back to the client as an unrecognized command.  The
     * client isn't reading from the socket, so eventually both
     * processes block trying to write to the other.  Now, we try to
     * read the file if we can.
     */

    status = buf_read_line (buf_from_net, &mode_text, (int *) NULL);
    if (status != 0)
    {
        if (status == -2)
			error(1,ENOMEM,"Alloc failed");
		else
		{
			if (status == -1)
				error(1,0,"end of file reading mode for %s", arg);
			else
				error(1,0,"error reading mode for %s", arg);
		}
		return;
    }

    status = buf_read_line (buf_from_net, &size_text, (int *) NULL);
    if (status != 0)
    {
		if (status == -2)
			error(1,ENOMEM,"Alloc failed");
		else
		{
			if (status == -1)
				error(1,0,"end of file reading size for %s", arg);
			else
				error(1,0,"error reading size for %s", arg);
		}
		xfree (mode_text);
		return;
    }
	size = atoi (size_text);
    xfree (size_text);

    if (outside_dir (arg))
    {
	xfree (mode_text);
	return;
    }

    if (size >= 0)
		receive_file (size, arg, !(serve_file_kopt(arg).flags&(KFLAG_BINARY|KFLAG_ENCODED)));

    if (checkin_time_valid)
    {
	struct utimbuf t;

	memset (&t, 0, sizeof (t));
	t.modtime = t.actime = checkin_time;
	if (utime (arg, &t) < 0)
	{
	    xfree (mode_text);
		error(1,errno,"cannot utime %s", arg);
	    return;
	}
	checkin_time_valid = 0;
    }

    {
	int status = change_mode (arg, mode_text, 0);
	xfree (mode_text);
	if (status)
	{
		error(1,0,"cannot change mode for %s", fn_root(arg));
	    return;
	}
    }

    /* Make sure that the Entries indicate the right kopt.  We probably
       could do this even in the non-kopt case and, I think, save a stat()
       call in time_stamp_server.  But for conservatism I'm leaving the
       non-kopt case alone.  */
    if (kopt != NULL)
		serve_is_modified (arg);
}


typedef struct _cs_entry_t { _cs_entry_t(const char *o) { strncpy(entry,o,32); entry[32]='\0'; } char entry[33]; } cs_entry_t;

static void serve_binary_transfer(char *arg)
{
    int size, status;
    char *size_text;
    char *mode_text;

    status = buf_read_line (buf_from_net, &mode_text, (int *) NULL);
    if (status != 0)
    {
        if (status == -2)
			error(1,ENOMEM,"Alloc failed");
		else
		{
			if (status == -1)
				error(1,0,"end of file reading mode for %s", arg);
			else
				error(1,0,"error reading mode for %s", arg);
		}
		return;
    }

    status = buf_read_line (buf_from_net, &size_text, (int *) NULL);
    if (status != 0)
    {
		if (status == -2)
			error(1,ENOMEM,"Alloc failed");
		else
		{
			if (status == -1)
				error(1,0,"end of file reading size for %s", arg);
			else
				error(1,0,"error reading size for %s", arg);
		}
		xfree (mode_text);
		return;
    }
	size = atoi (size_text);
    xfree (size_text);

#ifdef _WIN32
	// Statistics tracking
	int binarycount=0,binaryavg=0;
	CGlobalSettings::GetGlobalValue("cvsnt","PServer","BinaryCount",binarycount);
	CGlobalSettings::GetGlobalValue("cvsnt","PServer","BinaryAverage",binaryavg);
	binarycount++;
	binaryavg=(binaryavg+size)/2;
	CGlobalSettings::SetGlobalValue("cvsnt","PServer","BinaryCount",binarycount);
	CGlobalSettings::SetGlobalValue("cvsnt","PServer","BinaryAverage",binaryavg);
#endif

    if (outside_dir (arg))
    {
		xfree (mode_text);
		return;
    }

	std::vector<cs_entry_t> cs_list;
	cs_list.reserve((size/65536)+2);
	size_t s = size,l,j=0;
	int nread;
	while(s)
	{
		char *buf;

		l=s>65536?65536:s;
		
		if((status = buf_read_data(buf_from_net,33,&buf,&nread))!=0 || nread!=33 || buf[32]!='\n')
		{
			error(0,0,"Invalid binary checksum sent: %s", buf);
			return;
		}
		buf[32]='\0';
		cs_list.push_back(buf);
		s-=l;
	}

	if(cs_list.size()>1)
	{
		// cs_list[0] is the file checksum
		// cs_list[1..n] are the 64k block checksums
		//
		// prb: we should know the original file but we don't yet.. need to store these checksums in such
		// a manner that we can request the data from the client when we can
	}

    if (checkin_time_valid)
    {
		struct utimbuf t;

		memset (&t, 0, sizeof (t));
		t.modtime = t.actime = checkin_time;
		if (utime (arg, &t) < 0)
		{
			xfree (mode_text);
			error(1,errno,"cannot utime %s", arg);
			return;
		}
		checkin_time_valid = 0;
    }

	status = change_mode (arg, mode_text, 0);
	xfree (mode_text);
	if (status)
	{
		error(1,0,"cannot change mode for %s", fn_root(arg));
	    return;
	}

    /* Make sure that the Entries indicate the right kopt.  We probably
       could do this even in the non-kopt case and, I think, save a stat()
       call in time_stamp_server.  But for conservatism I'm leaving the
       non-kopt case alone.  */
    if (kopt != NULL)
		serve_is_modified (arg);
}

static void serve_enable_unchanged (char *arg)
{
}

struct an_entry {
    struct an_entry *next;
    char *entry;
	char *entry_extra;
};

static struct an_entry *entries;
static rename_struct *server_renames;
static const char *virtual_repository;

static void serve_unchanged (char *arg)
{
    struct an_entry *p;
    char *name;
    char *cp;
    char *timefield;

    if (outside_dir (arg))
	return;

    /* Rewrite entries file to have `=' in timestamp field.  */
    for (p = entries; p != NULL; p = p->next)
    {
	name = p->entry + 1;
	cp = strchr (name, '/');
	if (cp != NULL
	    && strlen (arg) == cp - name
	    && strncmp (arg, name, cp - name) == 0)
	{
	    timefield = strchr (cp + 1, '/') + 1;
	    if (*timefield != UNCHANGED_CHAR && *timefield!=MODIFIED_CHAR && *timefield!=DATE_CHAR)
	    {
		cp = timefield + strlen (timefield);
		cp[1] = '\0';
		while (cp > timefield)
		{
		    *cp = cp[-1];
		    --cp;
		}
		*timefield = UNCHANGED_CHAR;
	    }
	    break;
	}
    }
}

static void serve_is_modified(char *arg)
{
    struct an_entry *p;
    char *name;
    char *cp;
    char *timefield;
    /* Have we found this file in "entries" yet.  */
    int found;

    if (outside_dir (arg))
	return;

    /* Rewrite entries file to have `!' in timestamp field.  */
    found = 0;
    for (p = entries; p != NULL; p = p->next)
    {
	name = p->entry + 1;
	cp = strchr (name, '/');
	if (cp != NULL
	    && strlen (arg) == cp - name
	    && strncmp (arg, name, cp - name) == 0)
	{
	    timefield = strchr (cp + 1, '/') + 1;
	    if (*timefield != UNCHANGED_CHAR && *timefield!=MODIFIED_CHAR && *timefield!=DATE_CHAR)
	    {
		cp = timefield + strlen (timefield);
		cp[1] = '\0';
		while (cp > timefield)
		{
		    *cp = cp[-1];
		    --cp;
		}
		*timefield = MODIFIED_CHAR;
	    }
	    if (kopt != NULL)
	    {
			xfree (kopt);
			error(1,0,"protocol error: both Kopt and Entry for %s",
			     arg);
			return;
	    }
	    found = 1;
	    break;
	}
    }
    if (!found)
    {
	/* We got Is-modified but no Entry.  Add a dummy entry.
	   The "D" timestamp is what makes it a dummy.  */
	p = (struct an_entry *) xmalloc (sizeof (struct an_entry));
	if (p == NULL)
	{
		error(1,ENOMEM,"Alloc failed");
	    return;
	}
	p->entry = (char*)xmalloc (strlen (arg) + 80);
	if (p->entry == NULL)
	{
	    xfree (p);
		error(1,ENOMEM,"Alloc failed");
	    return;
	}
	strcpy (p->entry, "/");
	strcat (p->entry, arg);
	strcat (p->entry, "//D/");
	if (kopt != NULL)
	{
	    strcat (p->entry, kopt);
	    xfree (kopt);
	    kopt = NULL;
	}
	strcat (p->entry, "/");
	p->entry_extra = (char*)xmalloc (strlen (arg) + 80);
	if (p->entry_extra == NULL)
	{
	    xfree (p);
		error(1,ENOMEM,"Alloc failed");
	    return;
	}
	strcpy (p->entry_extra, "/");
	strcat (p->entry_extra, arg);
	strcat (p->entry_extra, "/");
	p->next = entries;
	entries = p;
    }
}

static void serve_entry (char *arg)
{
    struct an_entry *p;
    char *cp;

    p = (struct an_entry *) xmalloc (sizeof (struct an_entry));
    if (p == NULL)
    {
		error(1,ENOMEM,"Alloc failed");
		return;
    }
    /* Leave space for serve_unchanged to write '=' if it wants.  */
    cp = (char*)xmalloc (strlen (arg) + 2);
    if (cp == NULL)
    {
		error(1,ENOMEM,"Alloc failed");
		return;
    }
    strcpy (cp, arg);
    p->next = entries;
    p->entry = cp;
	p->entry_extra = (char*)xmalloc (strlen (arg) + 80);
	if (p->entry_extra == NULL)
	{
		error(1,ENOMEM,"Alloc failed");
	    return;
	}
	strcpy (p->entry_extra, "/");
	strcat (p->entry_extra, arg);
	strcat (p->entry_extra, "/");
    entries = p;
}

/* This must be sent directly after the Entry line above to work properly */
static void serve_entry_extra(char *arg)
{
    struct an_entry *p = entries;

	xfree(p->entry_extra);
	p->entry_extra = xstrdup(arg);
}

static void serve_kopt (char *arg)
{
    if (kopt != NULL)
    {
		error(1,0,"protocol error: duplicate Kopt request: %s", arg);
		return;
    }

    /* Do some sanity checks.  In particular, that it is not too long.
       This lets the rest of the code not worry so much about buffer
       overrun attacks.  Probably should call RCS_check_kflag here,
       but that would mean changing RCS_check_kflag to handle errors
       other than via exit(), fprintf(), and such.  */
    if (strlen (arg) > 30)
    {
		error(1,0,"protocol error: invalid Kopt request: %s", arg);
		return;
    }
	if(arg[0]=='-' && arg[1]=='k')
		arg+=2;

    kopt = (char*)xmalloc (strlen (arg) + 1);
    if (kopt == NULL)
    {
		error(1,ENOMEM,"Alloc failed");
		return;
    }
    strcpy (kopt, arg);
}

static kflag serve_file_kopt (char *arg)
{
    struct an_entry *p;
    char *name;
    char *cp;
    char *timefield, *optfield;
	kflag kf;

	if(kopt)
	{
		RCS_get_kflags(kopt,false,kf);
		return kf;
	}

    if (outside_dir (arg))
	{
        RCS_get_kflags(NULL,false,kf);
		return kf;
	}

    /* Find entry and return kopt */
    for (p = entries; p != NULL; p = p->next)
    {
		name = p->entry + 1;
		cp = strchr (name, '/');
		if (cp != NULL && strlen (arg) == cp - name && strncmp (arg, name, cp - name) == 0)
		{
		    timefield = strchr (cp + 1, '/') + 1;
			optfield = timefield?strchr(timefield,'/')+1:NULL;
			if(optfield)
			{
				cp = strchr(optfield,'/');
				if(cp) *cp='\0';
				if(optfield[0]=='-' && optfield[1]=='k')
					optfield+=2;
				RCS_get_kflags(optfield,false,kf);
				if(cp) *cp='/';
				return kf;
			}
		}
	    break;
	}
	RCS_get_kflags(wrap_rcsoption(arg),false,kf);
	return kf;
}

static void serve_checkin_time (char *arg)
{
    if (checkin_time_valid)
    {
		error(1,0,"protocol error: duplicate Checkin-time request: %s",
		     arg);
		return;
    }

    checkin_time = get_date (arg, NULL);
    if (checkin_time == (time_t)-1)
    {
		error(1,0,"cannot parse date %s", arg);
		return;
    }
    checkin_time_valid = 1;
}

static void serve_checksum (char *arg)
{
    if (checksum_valid)
    {
		error(1,0,"protocol error: duplicate Checksum request: %s", arg);
		return;
    }
	if(!arg || strlen(arg)!=32)
	{
		error(1,0,"protocol error: invalid Checksum request: %s",arg);
		return;
	}

	strcpy(stored_checksum,arg);
    checksum_valid = 1;
}

static void server_write_entries()
{
    FILE *f,*fx;
    struct an_entry *p;
    struct an_entry *q;

    if (entries == NULL)
		return;

    f = fx = NULL;
    /* Note that we free all the entries regardless of errors.  */
	/* We open in append mode because we don't want to clobber an
		existing Entries file.  If we are checking out a module
		which explicitly lists more than one file in a particular
		directory, then we will wind up calling
		server_write_entries for each such file.  */
	f = CVS_FOPEN (CVSADM_ENT, "a");
	if (f == NULL)
	{
		error(1,errno,"cannot open %s", CVSADM_ENT);
	}

	fx = CVS_FOPEN (CVSADM_ENTEXT, "a");
	if (fx == NULL)
	{
		error(1,errno,"cannot open %s", CVSADM_ENTEXT);
	}
    for (p = entries; p != NULL;)
    {
	    if (fprintf (f, "%s\n", p->entry) < 0)
	    {
			error(1,errno,"cannot write to %s", CVSADM_ENT);
	    }
	    if (fprintf (fx, "%s\n", p->entry_extra) < 0)
	    {
			error(1,errno,"cannot write to %s", CVSADM_ENTEXT);
	    }
		xfree (p->entry);
		q = p->next;
		xfree (p);
		p = q;
    }
    entries = NULL;
    if (f != NULL && fclose (f) == EOF)
    {
		error(1,errno,"cannot close %s", CVSADM_ENT);
    }
    if (fx != NULL && fclose (fx) == EOF)
    {
		error(1,errno,"cannot close %s", CVSADM_ENTEXT);
    }
}

static void server_write_renames()
{
    FILE *f;
    rename_struct *p;
    rename_struct *q;

    if (server_renames)
	{
		f = CVS_FOPEN (CVSADM_RENAME, "w");
		if (f == NULL)
		{
			error(1,errno,"cannot open %s", CVSADM_RENAME);
		}

		for (p = server_renames; p != NULL;)
		{
			if (fprintf (f, "%s\n", p->from) < 0)
			{
				error(1,errno,"cannot write to %s", CVSADM_RENAME);
			}
			if (fprintf (f, "%s\n", p->to) < 0)
			{
				error(1,errno,"cannot write to %s", CVSADM_RENAME);
			}
			xfree (p->from);
			xfree (p->to);
			q = p->next;
			xfree (p);
			p = q;
		}
		server_renames = NULL;
		if (f != NULL && fclose (f) == EOF)
		{
			error(1,errno,"cannot close %s", CVSADM_RENAME);
		}
	}

	if(virtual_repository)
	{
		f = CVS_FOPEN (CVSADM_VIRTREPOS, "w");
		if (f == NULL)
		{
			error(1,errno,"cannot open %s", CVSADM_VIRTREPOS);
		}

		fprintf(f,"%s\n", virtual_repository);
		xfree(virtual_repository);

		if (f != NULL && fclose (f) == EOF)
		{
			error(1,errno,"cannot close %s", CVSADM_VIRTREPOS);
		}
	}
}

struct notify_note {
    /* Directory in which this notification happens.  malloc'd*/
    char *dir;

    /* malloc'd.  */
    char *filename;

    /* The following three all in one malloc'd block, pointed to by TYPE.
       Each '\0' terminated.  */
    /* "E" or "U".  */
    char *type;
	char *time;
	char *hostname;
	char *pathname;
    char *watches;
	char *tag;
	char *flags;
	char *bugid;
	char *message;

	/* User doing notify */
	char *user;

    struct notify_note *next;
};

static struct notify_note *notify_list;
/* Used while building list, to point to the last node that already exists.  */
static struct notify_note *last_node;

static char *notify_user;

static void serve_notify_user(char *arg)
{
	if(!verify_admin())
	  error(1,0,"Only repository administrators can unedit other users.");
	notify_user = xstrdup(arg);
}

static void serve_notify (char *arg)
{
    struct notify_note *pnew = NULL;
    char *data = NULL;
    int status;

    if (outside_dir (arg))
	return;

    if (dir_name == NULL)
	goto error;

    pnew = (struct notify_note *) xmalloc (sizeof (struct notify_note));
    if (pnew == NULL)
    {
		error(1,ENOMEM,"Alloc failed");
    }
    pnew->dir = (char*)xmalloc (strlen (dir_name) + 1);
    pnew->filename = (char*)xmalloc (strlen (arg) + 1);
    if (pnew->dir == NULL || pnew->filename == NULL)
    {
		if (pnew->dir != NULL)
			xfree (pnew->dir);
		xfree (pnew);
		error(1,ENOMEM,"Alloc failed");
		return;
    }
    strcpy (pnew->dir, dir_name);
    strcpy (pnew->filename, arg);
	pnew->user = notify_user?xstrdup(notify_user):xstrdup(getcaller());

    status = server_read_line (buf_from_net, &data, (int *) NULL);
    if (status != 0)
    {
		if (status == -2)
			error(1,ENOMEM,"Alloc failed");
		else
		{
			if (status == -1)
				error(1,0,"end of file reading notification for %s", arg);
			else
				error(1,0,"error reading notification for %s", arg);
		}
		xfree (pnew->filename);
		xfree (pnew->dir);
		xfree (pnew->user);
		xfree (pnew);
    }
    else
    {
	char *cp;

	pnew->type = data;

	if (data[1] != '\t')
	    goto error;
	data[1] = '\0';
	cp = data + 2;
	pnew->time = cp;
	cp = strchr (cp, '\t');
	if (cp == NULL)
	    goto error;
	*(cp++)='\0';
	pnew->hostname = cp;
	cp = strchr (cp, '\t');
	if (cp == NULL)
	    goto error;
	*(cp++)='\0';
	pnew->pathname = cp;
	cp = strchr (cp, '\t');
	if (cp == NULL)
	    goto error;
	*cp++ = '\0';
	pnew->watches = cp;
	cp = strchr (cp, '\t');
	if (cp != NULL) /* Old-style notifies stopped here */
	{
		*(cp++)='\0';
		pnew->tag = cp;
		cp = strchr (cp, '\t');
		if (cp == NULL)
			goto error;
		*(cp++)='\0';
		pnew->flags = cp;
		cp = strchr (cp, '\t');
		if (cp == NULL)
			goto error;
		*cp++ = '\0';
		pnew->bugid = cp;
		cp = strchr (cp, '\t');
		if (cp == NULL)
			goto error;
		*cp++ = '\0';
		pnew->message = cp;
		cp = strchr (cp, '\t');
		if (cp != NULL)
		{
			*(cp++)='\0';
		}
		if(!*pnew->bugid)
			pnew->bugid=NULL;
		if(!*pnew->tag)
			pnew->tag=NULL;
	}
	else
	{
		pnew->flags = NULL;
		pnew->tag = NULL;
		pnew->bugid = NULL;
		pnew->message = NULL;
	}

	pnew->next = NULL;

	if (last_node == NULL)
	{
	    notify_list = pnew;
	}
	else
	    last_node->next = pnew;
	last_node = pnew;
    }
    return;
  error:
	xfree (data);
	xfree (pnew);
	error(1,0,"Protocol error; misformed Notify request");
}

/* Process all the Notify requests that we have stored up.  Returns 0
   if successful, if not prints error message (via error()) and
   returns negative value.  */
static int server_notify ()
{
    struct notify_note *p;
    char *repos;
	const char *mapped_repos;
	int can_notify;
	int err = 0;

	if(!notify_list)
	   return 0;

	can_notify = check_command_legal_p("edit"); /* Readers can't notify */
	err = 0;

	/* For edit/unedit these are sent without a prior command */
	if(!server_command_name)
		server_command_name=(*notify_list->type=='U')?"unedit":"edit";

	while (notify_list != NULL)
    {
		if ( CVS_CHDIR (notify_list->dir) < 0)
		{
			error (0, errno, "cannot change to %s", fn_root(notify_list->dir));
			return -1;
		}
		repos = Name_Repository (NULL, NULL);
		mapped_repos = map_repository(repos);
		xfree(repos);
		repos = (char*)mapped_repos;

		if (can_notify)
		{
			lock_dir_for_write (repos);

			fileattr_startdir (repos);

			err += notify_do (*notify_list->type, notify_list->filename, notify_list->user,
				notify_list->time, notify_list->hostname, notify_list->pathname, notify_list->watches, repos, notify_list->tag, notify_list->flags, notify_list->bugid, notify_list->message);

		}
		buf_output0 (buf_to_net, "Notified ");
		{
			char *dir = notify_list->dir + strlen (server_temp_dir) + 1;
			if (dir[0] == '\0')
				buf_append_char (buf_to_net, '.');
			else
				server_buf_output0 (buf_to_net, dir);
			buf_append_char (buf_to_net, '/');
			buf_append_char (buf_to_net, '\n');
		}
		server_buf_output0 (buf_to_net, repos);
		buf_append_char (buf_to_net, '/');
		server_buf_output0 (buf_to_net, notify_list->filename);
		buf_append_char (buf_to_net, '\n');
		xfree (repos);

		if (can_notify)
		{
			fileattr_write ();
			fileattr_free ();

			Lock_Cleanup ();
		}

		p = notify_list->next;
		xfree (notify_list->type); /* Start of buffer, all other pointers are relative to this */
		xfree (notify_list);
		notify_list = p;
    }

    last_node = NULL;
	xfree(notify_user);

    /* The code used to call fflush (stdout) here, but that is no
       longer necessary.  The data is now buffered in buf_to_net,
       which will be flushed by the caller, do_cvs_command.  */

    return err;
}

static int argument_count;
static char **argument_vector;
static int argument_vector_size;

static void serve_argument (char *arg)
{
    char *p;
    
    if (argument_vector_size <= argument_count)
    {
	argument_vector_size *= 2;
	argument_vector =
	    (char **) xrealloc((char *)argument_vector,
			       argument_vector_size * sizeof (char *));
	if (argument_vector == NULL)
	{
		error(1,ENOMEM,"Alloc failed");
	    return;
	}
    }
    p = (char*)xmalloc (strlen (arg) + 1);
    if (p == NULL)
    {
		error(1,ENOMEM,"Alloc failed");
		return;
    }
    strcpy (p, arg);
    argument_vector[argument_count++] = p;
}

static void serve_argumentx (char *arg)
{
    char *p;
    
	if(argument_count<=1)
	{
		error(1,0,"Argumentx protocol violation");
	}

    p = argument_vector[argument_count - 1];
    p = (char*)xrealloc(p, strlen (p) + 1 + strlen (arg) + 1);
    if (p == NULL)
    {
		error(1,ENOMEM,"Alloc failed");
		return;
    }
    strcat (p, "\n");
    strcat (p, arg);
    argument_vector[argument_count - 1] = p;
}

static void serve_global_option (char *arg)
{
    if (arg[0] != '-' || arg[1] == '\0' || arg[2] != '\0')
    {
    error_return:
		error(1,0,"Protocol error: bad global option %s",
		     arg);
		return;
    }
    switch (arg[1])
    {
	case 'n':
	    noexec = 1;
	    break;
	case 'q':
	    quiet = 1;
	    break;
	case 'r':
	    cvswrite = 0;
	    break;
	case 'Q':
	    really_quiet = 1;
	    break;
	case 'l':
	    break;
	case 't':
	    trace++;
		CServerIo::loglevel(trace);
	    break;
	default:
	    goto error_return;
    }
}

static void serve_set (char *arg)
{
    variable_set (arg);
}

static void serve_questionable (char *arg)
{
    if (dir_name == NULL)
    {
	buf_output0 (buf_to_net, "E Protocol error: 'Directory' missing");
	return;
    }

    if (outside_dir (arg))
	return;

    if (!ign_name (arg))
    {
	char *update_dir;

	buf_output (buf_to_net, "M ? ", 4);
	update_dir = dir_name + strlen (server_temp_dir) + 1;
	if (!(update_dir[0] == '.' && update_dir[1] == '\0'))
	{
	    server_buf_output0 (buf_to_net, update_dir);
	    buf_output (buf_to_net, "/", 1);
	}
	server_buf_output0 (buf_to_net, arg);
	buf_output (buf_to_net, "\n", 1);
    }
}

static void serve_utf8 (char *arg)
{
}

static void authentication_requested(char *flag)
{
	if(encryption_level == 1 && server_protocol && server_protocol->wrap)
		*flag = 1;
	else
		*flag = 0;
}

static void encryption_requested(char *flag)
{
	if(encryption_level == 2 && server_protocol && server_protocol->wrap)
		*flag = 1;
	else
		*flag = 0;
}

static void authentication_required(char *flag)
{
	if(encryption_level == 3)
		*flag = 1;
	else
		*flag = 0;
}

static void encryption_required(char *flag)
{
	if(encryption_level == 4)
		*flag = 1;
	else
		*flag = 0;
}

static void compression_requested(char *flag)
{
	if(compression_level == 1)
		*flag = 1;
	else
		*flag = 0;
}

static void compression_required(char *flag)
{
	if(compression_level == 2)
		*flag = 1;
	else
		*flag = 0;
}

static void server_can_rename(char *flag)
{
	*flag = 1;
}

static void outbuf_memory_error(struct buffer *buf)
{
    static const char msg[] = "E Fatal server error\n\
error ENOMEM Virtual memory exhausted.\n";
    /*
     * We have arranged things so that printing this now either will
     * be legal, or the "E fatal error" line will get glommed onto the
     * end of an existing "E" or "M" response.
     */

    /* If this gives an error, not much we could do.  syslog() it?  */
    write (STDOUT_FILENO, msg, sizeof (msg) - 1);
#ifdef HAVE_SYSLOG_H
    syslog (LOG_DAEMON | LOG_ERR, "virtual memory exhausted");
#endif
    error_exit ();
}

/* If command is legal, return 1.
 * Else if command is illegal and croak_on_illegal is set, then die.
 * Else just return 0 to indicate that command is illegal.
 */
static int check_command_legal_p (const char *cmd_name)
{
/*
 * GAIJIN: disable watch
 */
if (cmd_name && (strcmp(cmd_name, "edit")==0 || strcmp(cmd_name, "unedit")==0 || strcmp(cmd_name, "watch")==0))
    return 0;

    /* Right now, only pserver notices illegal commands -- namely,
     * write attempts by a read-only user.  Therefore, if CVS_Username
     * is not set, this just returns 1, because CVS_Username unset
     * means pserver is not active.
     */
#ifdef SERVER_SUPPORT
    if (CVS_Username == NULL)
        return 1;

    if (lookup_command_attribute (cmd_name) & CVS_CMD_MODIFIES_REPOSITORY)
    {
        /* This command has the potential to modify the repository, so
         * we check if the user have permission to do that.
         *
         * (Only relevant for remote users -- local users can do
         * whatever normal Unix file permissions allow them to do.)
         *
         * The decision method:
         *
         *    If $CVSROOT/CVSADMROOT_READERS exists and user is listed
         *    in it, then read-only access for user.
         *
         *    Or if $CVSROOT/CVSADMROOT_WRITERS exists and user NOT
         *    listed in it, then also read-only access for user.
         *
         *    Else read-write access for user.
         */

		/* Local repositories obey the read write flags from the server configuration */
		/* Remote repositories don't - it's up to the remote server to reject if required */
		  if(current_parsed_root->type==RootTypeStandard)
		  {
			  if(read_only_server) /* Server is running read only */
				  return 0;

			if(!current_parsed_root->readwrite) /* Repository is running read only */
			  return 0;
		  }

         char *linebuf = NULL;
         int num_red = 0;
         size_t linebuf_len = 0;
         char *fname;
         size_t flen;
         FILE *fp;
         int found_it = 0;
         
         /* else */
         flen = strlen (current_parsed_root->directory)
                + strlen (CVSROOTADM)
                + strlen (CVSROOTADM_READERS)
                + 3;

         fname = (char*)xmalloc (flen);
         (void) sprintf (fname, "%s/%s/%s", current_parsed_root->directory,
			CVSROOTADM, CVSROOTADM_READERS);

         fp = fopen (fname, "r");

         if (fp == NULL)
	 {
	     if (!existence_error (errno))
	     {
		 /* Need to deny access, so that attackers can't fool
		    us with some sort of denial of service attack.  */
		 error (0, errno, "cannot open %s", fname);
		 xfree (fname);
		 return 0;
	     }
	 }
         else  /* successfully opened readers file */
         {
             while ((num_red = getline (&linebuf, &linebuf_len, fp)) >= 0)
             {
                 /* Hmmm, is it worth importing my own readline
                    library into CVS?  It takes care of chopping
                    leading and trailing whitespace, "#" comments, and
                    newlines automatically when so requested.  Would
                    save some code here...  -kff */

                 /* Chop newline by hand, for strcmp()'s sake. */
                 if (linebuf[num_red - 1] == '\n')
                     linebuf[num_red - 1] = '\0';

                 if (fncmp (linebuf, CVS_Username) == 0)
                     goto handle_illegal;
             }
	     if (num_red < 0 && !feof (fp))
		 error (0, errno, "cannot read %s", fname);

             /* If not listed specifically as a reader, then this user
                has write access by default unless writers are also
                specified in a file . */
	     if (fclose (fp) < 0)
		 error (0, errno, "cannot close %s", fname);
         }
	 xfree (fname);

	 /* Now check the writers file.  */

         flen = strlen (current_parsed_root->directory)
                + strlen (CVSROOTADM)
                + strlen (CVSROOTADM_WRITERS)
                + 3;

         fname = (char*)xmalloc (flen);
         (void) sprintf (fname, "%s/%s/%s", current_parsed_root->directory,
			CVSROOTADM, CVSROOTADM_WRITERS);

         fp = fopen (fname, "r");

         if (fp == NULL)
         {
	     if (linebuf)
	         xfree (linebuf);
	     if (existence_error (errno))
	     {
		 /* Writers file does not exist, so everyone is a writer,
		    by default.  */
		 xfree (fname);
		 return 1;
	     }
	     else
	     {
		 /* Need to deny access, so that attackers can't fool
		    us with some sort of denial of service attack.  */
		 error (0, errno, "cannot read %s", fname);
		 xfree (fname);
		 return 0;
	     }
         }

         found_it = 0;
         while ((num_red = getline (&linebuf, &linebuf_len, fp)) >= 0)
         {
             /* Chop newline by hand, for strcmp()'s sake. */
             if (linebuf[num_red - 1] == '\n')
                 linebuf[num_red - 1] = '\0';
           
             if (fncmp (linebuf, CVS_Username) == 0)
             {
                 found_it = 1;
                 break;
             }
         }
	 if (num_red < 0 && !feof (fp))
	     error (0, errno, "cannot read %s", fname);

         if (found_it)
         {
             if (fclose (fp) < 0)
		 error (0, errno, "cannot close %s", fname);
             if (linebuf)
                 xfree (linebuf);
	     xfree (fname);
             return 1;
         }
         else   /* writers file exists, but this user not listed in it */
         {
         handle_illegal:
             if (fclose (fp) < 0)
		 error (0, errno, "cannot close %s", fname);
             if (linebuf)
                 xfree (linebuf);
	     xfree (fname);
	     return 0;
         }
    }
#endif /* SERVER_SUPPORT */

    /* If ever reach end of this function, command must be legal. */
    return 1;
}

static void do_cvs_command (const char *cmd_name, int (*command)(int argc, char **argv))
{
    int errs;
	CTriggerLibrary lib;

    server_write_entries();
	server_write_renames();

    /* Global `command_name' is probably "server" right now -- only
       serve_export() sets it to anything else.  So we will use local
       parameter `cmd_name' to determine if this command is legal for
       this user.  */
    if (!check_command_legal_p (cmd_name))
    {
	buf_output0 (buf_to_net, "E ");
	buf_output0 (buf_to_net, program_name);
	buf_output0 (buf_to_net, " [server aborted]: \"");
	buf_output0 (buf_to_net, cmd_name);
	buf_output0 (buf_to_net, "\" requires write access to the repository\n\
error  \n");
	goto free_args_and_return;
    }

	server_command_name = cmd_name; /* This is set in server_main too... just to be safe */

    server_notify ();

	/* Flush out any pending data.  */
    buf_flush (buf_to_net, 1);

	switch(current_parsed_root->type)
	{
		case RootTypeStandard:
			errs = server_main(cmd_name, command);
			break;
		case RootTypeProxyAll:
			errs = proxy_main(cmd_name, command);
			break;
		case RootTypeProxyWrite:
			if(lookup_command_attribute (cmd_name) & CVS_CMD_MODIFIES_REPOSITORY)
				errs = proxy_main(cmd_name, command);
			else
				errs = server_main(cmd_name, command);
			break;
		default:
			error(1,0, "Unknown root type - cannot continue");
	}


    buf_flush(stderr_buf, 1);
    buf_flush(stdout_buf, 1);

    server_notify ();

	lib.CloseAllTriggers(); /* This command has finished */
	tf_loaded=false;

	if (errs)
		/* We will have printed an error message already.  */
		buf_output0 (buf_to_net, "error  \n");
    else
		buf_output0 (buf_to_net, "ok\n");

free_args_and_return:
    /* Now free the arguments.  */
    {
	/* argument_vector[0] is a dummy argument, we don't mess with it.  */
	char **cp;
	for (cp = argument_vector + 1;
	     cp < argument_vector + argument_count;
	     ++cp)
	    xfree (*cp);

	argument_count = 1;
    }

    /* Flush out any data not yet sent.  */
    set_block (buf_to_net);
    buf_flush (buf_to_net, 1);

    return;
}

/* Called by error_exit() to flush final error messages before closedown */
void server_error_exit()
{
    buf_output0 (buf_to_net, "error  \n");
    set_block (buf_to_net);
    buf_flush (buf_to_net, 1);
}

/* This variable commented in server.h.  */
char *server_dir = NULL;

static void output_dir(const char *update_dir, const char *repository)
{
    /* If we have a CVS root prefix set, don't transmit it back to the
       client. */

    if (server_dir != NULL)
	{
		server_buf_output0(buf_to_net,client_where(server_dir));
		buf_output0(buf_to_net,"/");
	}

    if (update_dir[0] == '\0')
		buf_output0(buf_to_net,".");
    else
		buf_output0(buf_to_net,client_where(update_dir));
	buf_output0(buf_to_net,"/\n");

	server_buf_output0(buf_to_net,current_parsed_root->unparsed_directory);
	if(strlen(repository)>strlen(current_parsed_root->directory))
		server_buf_output0(buf_to_net,repository+strlen(current_parsed_root->directory));
	buf_output0(buf_to_net,"/");
}

/* This feeds temporary renames back to the client */
int send_rename_to_client(const char *oldfile, const char *newfile)
{
	if(supported_response("Rename"))
	{
		buf_output0(buf_to_net,"Rename ");
		server_buf_output0(buf_to_net,oldfile);
		buf_output0(buf_to_net,"\n");
		server_buf_output0(buf_to_net,newfile);
		buf_output0(buf_to_net,"\n");
	}
	else
	{
		error(0,0,"Client doesn't support rename response.");
	}
	return 0;
}

/* Cause the client to physically rename a file */
int server_rename_file(const char *oldfile, const char *newfile)
{
	if(supported_response("Renamed"))
	{
		buf_output0(buf_to_net,"Renamed ./\n");
		server_buf_output0(buf_to_net,oldfile);
		buf_output0(buf_to_net,"\n");
		server_buf_output0(buf_to_net,newfile);
		buf_output0(buf_to_net,"\n");
	}
	return 0;
}

int client_can_rename()
{
	return supported_response("Renamed");
}

int reset_client_mapping(const char *update_dir, const char *repository)
{
	if(supported_response("Clear-rename"))
	{
		buf_output0(buf_to_net,"Clear-rename ");
	    output_dir (update_dir, repository);
	    buf_output0(buf_to_net,"\n");
	}
	else
	{
		error(0,0,"Client doesn't support clear-rename response.  How did we get here???");
	}
	return 0;
}

/*
 * Entries line that we are squirreling away to send to the client when
 * we are ready.
 */
static char *entries_line;
static char *entries_ex_line;

/*
 * File which has been Scratch_File'd, we are squirreling away that fact
 * to inform the client when we are ready.
 */
static char *scratched_file;

/*
 * The scratched_file will need to be removed as well as having its entry
 * removed.
 */
static int kill_scratched_file;

void server_register(const char *name, const char *version, const char *timestamp, const char *options,
				const char *tag, const char *date, const char *conflict, const char *merge_from_tag_1,
				const char *merge_from_tag_2, time_t rcs_timestamp,
				const char *edit_revision, const char *edit_tag, const char *edit_bugid, const char *md5)
{
    int len;
	char *temp_options;

    if (options == NULL)
		options = "";

	TRACE(1,"server_register(%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)",
			PATCH_NULL(name), PATCH_NULL(version), timestamp ? timestamp : "", PATCH_NULL(options),
			tag ? tag : "", date ? date : "",
			conflict ? conflict : "", 
			merge_from_tag_1 ? merge_from_tag_1 : "",
			merge_from_tag_2 ? merge_from_tag_2 : "",
			edit_revision?edit_revision:"",
			edit_tag?edit_tag:"",
			edit_bugid?edit_bugid:"",
			md5?md5:"");

    if (entries_line != NULL)
    {
	/*
	 * If CVS decides to Register it more than once (which happens
	 * on "cvs update foo/foo.c" where foo and foo.c are already
	 * checked out), use the last of the entries lines Register'd.
	 */
		xfree (entries_line);
    }
    if (entries_ex_line != NULL)
    {
		xfree (entries_ex_line);
    }

    /*
     * I have reports of Scratch_Entry and Register both happening, in
     * two different cases.  Using the last one which happens is almost
     * surely correct; I haven't tracked down why they both happen (or
     * even verified that they are for the same file).
     */
    if (scratched_file != NULL)
    {
	xfree (scratched_file);
	scratched_file = NULL;
    }

    len = (strlen (name) + strlen (version) + strlen (options) + 80);
    if (tag)
	len += strlen (tag);
    if (date)
	len += strlen (date);
	if (timestamp)
	len += strlen (timestamp);
	if(md5)
	len += strlen(md5);

	len += 128; // This is for patching the md5 in later...
    
    entries_line = (char*)xmalloc (len);
	entries_ex_line = (char*)xmalloc(len);
	sprintf (entries_line, "/%s/%s/", name, version);
    if (conflict != NULL)
		strcat (entries_line, "+=");
    strcat (entries_line, "/");
	/* Filter out any unrecognised client options */
	if(options[0])
	{
		temp_options = normalise_options(options,0,name);

		if(temp_options)
		{
			strcat (entries_line, "-k");
			strcat (entries_line, temp_options);
		}
	}
    strcat (entries_line, "/");
    if (tag != NULL)
    {
	strcat (entries_line, "T");
	strcat (entries_line, tag);
    }
    else if (date != NULL)
    {
	strcat (entries_line, "D");
	strcat (entries_line, date);
    }
	
	/* Entries.Extra stuff */
	sprintf (entries_ex_line, "/%s/%s/%s/", name, merge_from_tag_1 ? merge_from_tag_1 : "",merge_from_tag_2 ? merge_from_tag_2 : "");
	if(rcs_timestamp!=(time_t)-1)
		sprintf (entries_ex_line+strlen(entries_ex_line), "%"TIME_T_SPRINTF"d", rcs_timestamp);
	sprintf(entries_ex_line+strlen(entries_ex_line),"/%s/%s/%s/%s/", edit_revision?edit_revision:"",edit_tag?edit_tag:"",edit_bugid?edit_bugid:"",md5?md5:"");
}

void server_scratch (const char *fname)
{
	/*
     * I have reports of Scratch_Entry and Register both happening, in
     * two different cases.  Using the last one which happens is almost
     * surely correct; I haven't tracked down why they both happen (or
     * even verified that they are for the same file).
     *
     * Don't know if this is what whoever wrote the above comment was
     * talking about, but this can happen in the case where a join
     * removes a file - the call to Register puts the '-vers' into the
     * Entries file after the file is removed
     */
    if (entries_line != NULL)
    {
		xfree (entries_line);
		entries_line = NULL;
    }
    if (entries_ex_line != NULL)
    {
		xfree (entries_ex_line);
		entries_ex_line = NULL;
    }

    if (scratched_file != NULL)
    {
		buf_output0(buf_to_net,"E CVS server internal error: duplicate Scratch_Entry\n");
		return;
    }
    scratched_file = xstrdup (fname);
    kill_scratched_file = 1;
}

void
server_scratch_entry_only ()
{
    kill_scratched_file = 0;
}

/* Print a new entries line, from a previous server_register.  */
static void new_entries_line ()
{
    if (entries_line)
    {
		server_buf_output0(buf_to_net,entries_line);
		buf_output0(buf_to_net,"\n");
    }
    else
	{
		/* Return the error message as the Entries line.  */
		buf_output0(buf_to_net,"E CVS server internal error: Register missing\n");
	}
    xfree (entries_line);
    entries_line = NULL;
}

static void new_entries_ex_line ()
{
	if(supported_response("EntriesExtra"))
	{
		if (entries_ex_line)
		{
			buf_output0(buf_to_net,"EntriesExtra ");
			server_buf_output0(buf_to_net,entries_ex_line);
			buf_output0(buf_to_net,"\n");
		}
	}
    xfree (entries_ex_line);
    entries_ex_line = NULL;
}


static void serve_ci (char *arg)
{
    do_cvs_command ("commit", commit);
}

static void checked_in_response (const char *file, const char *update_dir, const char *repository)
{
    if (supported_response ("Mode"))
    {
	struct stat sb;
	char *mode_string;

	if ( CVS_STAT (file, &sb) < 0)
	{
	    /* Not clear to me why the file would fail to exist, but it
	       was happening somewhere in the testsuite.  */
	    if (!existence_error (errno))
		error (0, errno, "cannot stat %s", file);
	}
	else
	{
	    buf_output0(buf_to_net,"Mode ");
	    mode_string = mode_to_string (sb.st_mode);
		buf_output0(buf_to_net,mode_string);
		buf_output0(buf_to_net,"\n");
	    xfree (mode_string);
	}
    }
	new_entries_ex_line();

	buf_output0(buf_to_net,"Checked-in ");
    output_dir (update_dir, repository);
	server_buf_output0(buf_to_net,file);
	buf_output0(buf_to_net,"\n");
    new_entries_line ();
}

void server_checked_in (const char *file, const char *update_dir, const char *repository)
{
    if (noexec)
	return;
    if (scratched_file != NULL && entries_line == NULL)
    {
	/*
	 * This happens if we are now doing a "cvs remove" after a previous
	 * "cvs add" (without a "cvs ci" in between).
	 */
		buf_output0(buf_to_net,"Remove-entry ");
		output_dir (update_dir, repository);
		server_buf_output0(buf_to_net,file);
		buf_output0(buf_to_net,"\n");
		xfree (scratched_file);
		scratched_file = NULL;
    }
    else
	{
		checked_in_response (file, update_dir, repository);
    }
}

void serve_chown (char *arg)
{
   do_cvs_command ("chown", chowner);
}

void serve_rchown (char *arg)
{
   command_name = "rchown";
   do_cvs_command ("rchown", chowner);
}

void serve_chacl (char *arg)
{
   do_cvs_command ("chacl", chacl);
}

void serve_rchacl (char *arg)
{
   command_name = "rchacl";
   do_cvs_command ("rchacl", chacl);
}

void serve_lsacl (char *arg)
{
   do_cvs_command ("lsacl", lsacl);
}

void serve_rlsacl (char *arg)
{
   command_name = "rlsacl";
   do_cvs_command ("rlsacl", lsacl);
}

void serve_passwd (char *arg)
{
   do_cvs_command ("passwd", passwd);
}

static void serve_passwd_md5(char *flag)
{
	*flag = 1;
}

void serve_info (char *arg)
{
   do_cvs_command ("info", info);
}

void server_update_entries (const char *file, const char *update_dir, const char *repository, enum server_updated_arg4 updated)
{
    if (noexec)
		return;
    if (updated == SERVER_UPDATED)
		checked_in_response (file, update_dir, repository);
    else
    {
		if (!supported_response ("New-entry"))
			return;
		buf_output0(buf_to_net,"New-entry ");
		output_dir (update_dir, repository);
		server_buf_output0(buf_to_net,file);
		buf_output0(buf_to_net,"\n");
		new_entries_line ();
    }
}

static void serve_update (char *arg)
{
    do_cvs_command ("update", update);
}

static void serve_diff (char *arg)
{
    do_cvs_command ("diff", diff);
}

static void serve_log (char *arg)
{
    do_cvs_command ("log", cvslog);
}

static void serve_rlog (char *arg)
{
    /* Tell cvslog() to behave like rlog not log.  */
    command_name = "rlog";
    do_cvs_command ("rlog", cvslog);
}

static void serve_add (char *arg)
{
    do_cvs_command ("add", add);
}

static void serve_remove(char *arg)
{
    do_cvs_command("remove", cvsremove);
}

static void serve_status (char *arg)
{
    do_cvs_command ("status", cvsstatus);
}

static void serve_ls (char *arg)
{
    do_cvs_command ("ls", ls);
}

static void serve_rdiff (char *arg)
{
    do_cvs_command ("rdiff", patch);
}

static void serve_tag (char *arg)
{
    do_cvs_command ("tag", cvstag);
}

static void serve_rtag (char *arg)
{
    /* Tell cvstag() to behave like rtag not tag.  */
    command_name = "rtag";
    do_cvs_command ("rtag", cvstag);
}

static void serve_import (char *arg)
{
    do_cvs_command ("import", import);
}

static void serve_admin (char *arg)
{
    do_cvs_command ("admin", admin);
}

static void serve_history (char *arg)
{
    do_cvs_command ("history", history);
}

static void serve_release (char *arg)
{
    do_cvs_command ("release", release);
}

static void serve_watch_on (char *arg)
{
    do_cvs_command ("watch", watch_on);
}

static void serve_watch_off (char *arg)
{
    do_cvs_command ("watch", watch_off);
}

static void serve_watch_add (char *arg)
{
    do_cvs_command ("watch", watch_add);
}

static void serve_watch_remove (char *arg)
{
    do_cvs_command ("watch", watch_remove);
}

static void serve_watchers (char *arg)
{
    do_cvs_command ("watchers", watchers);
}

static void serve_editors (char *arg)
{
    do_cvs_command ("editors", editors);
}

static void serve_editors_edit (char *arg)
{
	serve_argument("-c");
    do_cvs_command ("editors", editors);
}

static void serve_rename(char *arg)
{
    rename_struct *p, *r;
	int status;

    p = (rename_struct *) xmalloc (sizeof (rename_struct));
    if (p == NULL)
    {
		error(1,ENOMEM,"Alloc failed");
		return;
    }
    p->next = NULL;
    p->from = xstrdup(arg);
    if (p->from == NULL)
    {
		error(1,ENOMEM,"Alloc failed");
		return;
    }
	if (isabsolute(p->from) || strstr(p->from,".."))
	{
		error(1,0,"Protocol error: Bad rename %s",p->from);
		xfree(p);
		return;
	}

    status = server_read_line (buf_from_net, &p->to, (int *) NULL);
    if (status == 0)
    {
		if (isabsolute(p->to) || strstr(p->to,".."))
		{
			error(1,0,"Protocol error: Bad rename %s",p->to);
			xfree(p);
			return;
		}
    }
    else if (status == -2)
    {
		xfree(p);
		error(1,ENOMEM,"Alloc failed");
		return;
    }
    else
    {
		if (status == -1)
		{
			xfree(p);
			error(1,0,"end of file reading rename for %s", arg);
		}
		else
		{
			xfree(p);
			error(1,0,"error reading rename for %s", arg);
		}
    }
	if(!server_renames)
		server_renames = p;
	else
	{
		r=server_renames;
		while(r->next)
			r=r->next;
		r->next=p;
	}
}

static void serve_virtual_repository(char *arg)
{
	virtual_repository = xstrdup(arg);
	if (isabsolute(virtual_repository) || strstr(virtual_repository,".."))
	{
		error(1,0,"Protocol error: Bad virtual repository %s",virtual_repository);
		xfree(virtual_repository);
		return;
	}
}

// Duplicate the old modules expansion
// We don't want to call do_module, as it's overkill plus it isn't compatible
// with future core versions, which work entirely differently
static void serve_expand_modules(char *arg)
{
	int err = 0;
	cvs::filename fn;
	std::map<cvs::filename,std::vector<cvs::filename> > alias_map;
	std::map<cvs::filename,cvs::filename> dir_map;
	cvs::sprintf(fn,128,"%s/%s/%s",current_parsed_root->directory,CVSROOTADM,CVSROOTADM_MODULES);
	CFileAccess acc;
	std::vector<cvs::filename> args;

	if(acc.open(fn.c_str(),"r"))
	{
		cvs::string line;
		while(acc.getline(line))
		{
			if(!line.size() || line[0]=='#')
				continue;
			CTokenLine tok(line.c_str());
			if(tok.size()<3)
				continue;
			if(!strcmp(tok[1],"-a"))
			{
				std::vector<cvs::filename>&v = alias_map[tok[0]];
				for(int j=2; j<tok.size(); j++)
					v.push_back(tok[j]);
			}
			else for(int j=1; j<tok.size() && tok[j][0]=='-'; j++)
			{
				if(!strcmp(tok[j],"-d"))
				{
					dir_map[tok[0]]=tok[j+1];
					break;
				}
			}
		}
		acc.close();
	}

	for (int i = 1; i < argument_count; i++)
		args.push_back(argument_vector[i]);

	for (size_t i = 0; i < args.size(); i++)
	{
		cvs::filename frst,nxt;
		size_t p = args[i].find_first_of('/');
		if(p!=cvs::filename::npos)
		{
			frst=args[i].substr(0,p);
			nxt=args[i].substr(p+1);
		}
		else
		{
			frst=args[i];
			nxt="";
		}
		
		/* Alias modules ditch the filename/directory parts of the expansion.  This is consistent
			with cvshome 1.11 behaviour, although I'm not sure it's really correct */
		if(alias_map.find(frst)!=alias_map.end())
		{
			std::vector<cvs::filename>&v = alias_map[frst];
			args.insert(args.begin()+i+1,v.begin(),v.end());
		}
		else if(dir_map.find(frst)!=dir_map.end())
		{
			buf_output0 (buf_to_net, "Module-expansion ");
			server_buf_output0 (buf_to_net, dir_map[frst].c_str());
			if(nxt.size())
			{
				buf_output0 (buf_to_net, "/");
				server_buf_output0 (buf_to_net, nxt.c_str());
			}
			buf_output0 (buf_to_net, "\n");
		}
		else
		{
			/* Note that we don't check for module existence here... that is done
				in the main checkout routine. */
			buf_output0 (buf_to_net, "Module-expansion ");
			server_buf_output0 (buf_to_net, args[i].c_str());
			buf_output0 (buf_to_net, "\n");
		}
	}

    /* argument_vector[0] is a dummy argument, we don't mess with it.  */
	/* Reset the argument vector */
    char **cp;
    for (cp = argument_vector + 1; cp < argument_vector + argument_count; ++cp)
        xfree (*cp);

    argument_count = 1;
    buf_output0 (buf_to_net, "ok\n");

    /* The client is waiting for the module expansions, so we must
       send the output now.  */
    buf_flush (buf_to_net, 1);
 }

static void serve_noop (char *arg)
{

    server_write_entries();
	server_write_renames();
	if(server_notify ())
		buf_output0 (buf_to_net, "error \n");
	else
		buf_output0 (buf_to_net, "ok\n");
    buf_flush (buf_to_net, 1);
}

static void serve_version (char *arg)
{
    do_cvs_command ("version", version);
}

static void serve_read_cvsrc(char *arg)
{
    char *cvsrc;
	
	cvsrc = (char*)xmalloc (strlen (current_parsed_root->directory)
			    + sizeof (CVSROOTADM)
			    + sizeof (CVSROOTADM_CVSRC)
			    + 3);

    sprintf (cvsrc, "%s/%s/%s", current_parsed_root->directory,
			    CVSROOTADM, CVSROOTADM_CVSRC);

	cvs_flushout();
    if(isfile (cvsrc))
	{
		FILE *f = fopen(cvsrc,"r");
		if(f)
		{
			char *buf;
			off_t len;

			CVS_FSEEK(f,0,SEEK_END);
			len=CVS_FTELL(f);
			if(len>0)
			{
				CVS_FSEEK(f,0,SEEK_SET);
				buf=(char*)xmalloc(len+1);
				fread(buf,1,len,f);
				if(buf[len-1]!='\n')
					buf[len++]='\n';

				buf_output(buf_to_net,buf, len);
				xfree(buf);
			}
			fclose(f);
		}
	}
	buf_output0(buf_to_net,"end-cvsrc\n");
	cvs_flushout();
	xfree(cvsrc);
}

static void serve_read_generic(char *file)
{
    char *cvsfile;
	
	cvsfile = (char*)xmalloc (strlen (current_parsed_root->directory)
			    + sizeof (CVSROOTADM)
			    + strlen(file)
			    + 3);

    sprintf (cvsfile, "%s/%s/%s", current_parsed_root->directory,
			    CVSROOTADM, file);

	cvs_flushout();
    if(isfile (cvsfile))
	{
		FILE *f = fopen(cvsfile,"r");
		if(f)
		{
			char *buf;
			char num[16];
			off_t len;

			fseek(f,0,SEEK_END);
			len=ftell(f);
			if(len>0)
			{
				fseek(f,0,SEEK_SET);
				buf=(char*)xmalloc(len+1);
				fread(buf,1,len,f);
				if(buf[len-1]!='\n')
					buf[len++]='\n';

				snprintf(num,sizeof(num),"%lu\n",(long)len);
				buf_output0(buf_to_net, num);

				buf_output(buf_to_net,buf, len);
				xfree(buf);
			}
			else
				buf_output0(buf_to_net,"0\n");

			fclose(f);
		}
		else
			buf_output0(buf_to_net,"0\n");
	}
	else
		buf_output0(buf_to_net,"0\n");
	cvs_flushout();
	xfree(cvsfile);
}

static void serve_read_cvsrc2(char *arg)
{
	serve_read_generic(CVSROOTADM_CVSRC);
}

static void serve_server_codepage(char *arg)
{
#if defined(_WIN32) && defined(_UNICODE)
	if(win32_global_codepage==CP_UTF8)
		buf_output0(buf_to_net,"UTF-8");
	else
#endif
	buf_output0(buf_to_net,CCodepage::GetDefaultCharset());
	buf_output0(buf_to_net,"\n");
	buf_flush(buf_to_net,0);
}

static void serve_client_version(char *arg)
{
	buf_output0(buf_to_net,"CVSNT "CVSNT_PRODUCTVERSION_STRING"\n");
	buf_flush(buf_to_net,0);
	serv_client_version = xstrdup(arg);

 	TRACE(99,"Server: CVSNT "CVSNT_PRODUCTVERSION_STRING);
 	TRACE(99,"Client: %s",PATCH_NULL(serv_client_version));

	// At the time of writing SmartCVS is hardcoded to (I think) ISO8859-1.  This means that it can't
	// effectively use multinational filenames.  If a future version fixes this it can send the
	// server_codepage command to tell cvsnt it's character set aware and avoid this hack.
	if(!server_codepage_sent && strstr(serv_client_version,"SmartCVS"))
		default_client_codepage = xstrdup("ISO8859-1");
}

static void serve_read_cvswrappers(char *arg)
{
	serve_read_generic(CVSROOTADM_WRAPPER);
}

static void serve_read_cvsignore(char *arg)
{
	serve_read_generic(CVSROOTADM_IGNORE);
}

static void serve_error_if_reader(char *arg)
{
  if(!check_command_legal_p("edit"))
    error(1,0,"%s",arg);
}

static void serve_init (char *arg)
{
    do_cvs_command ("init", init);
}

static void serve_annotate (char *arg)
{
    do_cvs_command ("annotate", annotate);
}

static void serve_rannotate (char *arg)
{
    /* Tell annotate() to behave like rannotate not annotate.  */
    command_name = "rannotate";
    do_cvs_command ("rannotate", annotate);
}

static void serve_co (char *arg)
{
    char *tempdir;
    int status;

    if (!isdir (CVSADM))
    {
	/*
	 * The client has not sent a "Repository" line.  Check out
	 * into a pristine directory.
	 */
	tempdir = (char*)xmalloc (strlen (server_temp_dir) + 80);
	if (tempdir == NULL)
	{
	    buf_output0 (buf_to_net, "E Out of memory\n");
	    return;
	}
	strcpy (tempdir, server_temp_dir);
	strcat (tempdir, "/checkout-dir");
	status = mkdir_p (tempdir);
	if (status != 0 && status != EEXIST)
	{
	    buf_output0 (buf_to_net, "E Cannot create ");
	    server_buf_output0 (buf_to_net, fn_root(tempdir));
	    buf_append_char (buf_to_net, '\n');
	    print_error (errno);
	    xfree (tempdir);
	    return;
	}

	if ( CVS_CHDIR (tempdir) < 0)
	{
	    buf_output0 (buf_to_net, "E Cannot change to directory ");
	    server_buf_output0 (buf_to_net, fn_root(tempdir));
	    buf_append_char (buf_to_net, '\n');
	    print_error (errno);
	    xfree (tempdir);
	    return;
	}
	xfree (tempdir);
    }

    /* Compensate for server_export()'s setting of command_name.
     *
     * [It probably doesn't matter if do_cvs_command() gets "export"
     *  or "checkout", but we ought to be accurate where possible.]
     */
    do_cvs_command ((strcmp (command_name, "export") == 0) ?
                    "export" : "checkout",
                    checkout);
}

static void serve_export (char *arg)
{
    /* Tell checkout() to behave like export not checkout.  */
    command_name = "export";
    serve_co (arg);
}

void server_copy_file (const char *file, const char *update_dir, const char *repository, const char *newfile)
{
    /* At least for now, our practice is to have the server enforce
       noexec for the repository and the client enforce it for the
       working directory.  This might want more thought, and/or
       documentation in cvsclient.texi (other responses do it
       differently).  */

    if (!supported_response ("Copy-file"))
		return;
	buf_output0(buf_to_net,"Copy-file ");
    output_dir (update_dir, repository);
	server_buf_output0(buf_to_net,file);
	buf_output0(buf_to_net,"\n");
	server_buf_output0(buf_to_net,newfile);
	buf_output0(buf_to_net,"\n");
}

/* See server.h for description.  */

void server_modtime (struct file_info *finfo, Vers_TS *vers_ts)
{
    char date[MAXDATELEN];
    char outdate[MAXDATELEN];

    assert (vers_ts->vn_rcs != NULL);

    if (!supported_response ("Mod-time"))
	return;

    if (RCS_getrevtime (finfo->rcs, vers_ts->vn_rcs, date, 0) == (time_t) -1)
	/* FIXME? should we be printing some kind of warning?  For one
	   thing I'm not 100% sure whether this happens in non-error
	   circumstances.  */
	return;
    date_to_internet (outdate, date);
	buf_output0(buf_to_net,"Mod-time ");
	buf_output0(buf_to_net,outdate);
	buf_output0(buf_to_net,"\n");
}

/* See server.h for description.  */
void server_updated (
    struct file_info *finfo,
    Vers_TS *vers,
    enum server_updated_arg4 updated,
    mode_t mode,
    CMD5Calc* md5,
    struct buffer *filebuf)
{
    char *mode_string;

	TRACE(3,"server_updated(%s,%04o)",PATCH_NULL(finfo->file),mode);
    if (noexec)
    {
	/* Hmm, maybe if we did the same thing for entries_file, we
	   could get rid of the kludges in server_register and
	   server_scratch which refrain from warning if both
	   Scratch_Entry and Register get called.  Maybe.  */
	if (scratched_file)
	{
	    xfree (scratched_file);
	    scratched_file = NULL;
	}
	return;
    }

    if (entries_line != NULL && scratched_file == NULL)
    {
	FILE *f;
	struct buffer_data *list, *last;
	unsigned long size;

	/* The contents of the file will be in one of filebuf,
	   list/last, or here.  */
	unsigned char *file;
	size_t file_allocated;
	size_t file_used;

	if (filebuf != NULL)
	{
	    size = buf_length (filebuf);
	    if (mode == (mode_t) -1)
		error (1, 0, "CVS server internal error: no mode in server_updated");
	}
	else
	{
	    struct stat sb;

	    if ( CVS_STAT (finfo->file, &sb) < 0)
	    {
		if (existence_error (errno))
		{
		    /* If we have a sticky tag for a branch on which
		       the file is dead, and cvs update the directory,
		       it gets a T_CHECKOUT but no file.  So in this
		       case just forget the whole thing.  */
		    xfree (entries_line);
		    entries_line = NULL;
		    goto done;
		}
		error (1, errno, "reading %s", fn_root(finfo->fullname));
	    }
	    size = sb.st_size;
	    if (mode == (mode_t) -1)
	    {
		/* FIXME: When we check out files the umask of the
		   server (set in .bashrc if rsh is in use) affects
		   what mode we send, and it shouldn't.  */
		mode = sb.st_mode;
	    }
	}

	if (md5)
	{
	    static int checksum_supported = -1;

	    if (checksum_supported == -1)
	    {
			checksum_supported = supported_response ("Checksum");
	    }

	    if (checksum_supported && updated != SERVER_UPDATED)
	    {
			buf_output0(buf_to_net,"Checksum ");
			buf_output0(buf_to_net,md5->Final());
			buf_output0(buf_to_net,"\n");
	    }

		// Space was reserved earlier for this
		if(entries_ex_line && strlen(entries_ex_line))
			sprintf(entries_ex_line+strlen(entries_ex_line)-1,"%s/",md5->Final());
	}

	new_entries_ex_line();

	if (updated == SERVER_UPDATED)
	{
	    Node *node;
	    Entnode *entnode;

	    if (!(supported_response ("Created")
		  && supported_response ("Update-existing")))
		  buf_output0(buf_to_net,"Updated ");
	    else
	    {
			assert (vers != NULL);
			if (vers->ts_user == NULL)
				buf_output0(buf_to_net,"Created ");
			else
				buf_output0(buf_to_net,"Update-existing ");
	    }

	    /* Now munge the entries to say that the file is unmodified,
	       in case we end up processing it again (e.g. modules3-6
	       in the testsuite).  */
	    node = findnode_fn (finfo->entries, finfo->file);
	    entnode = (Entnode *)node->data;
	    xfree (entnode->timestamp);
	    entnode->timestamp = xstrdup (UNCHANGED_CHAR_S);
	}
	else if (updated == SERVER_MERGED)
		buf_output0(buf_to_net,"Merged ");
	else if (updated == SERVER_PATCHED)
		buf_output0(buf_to_net,"Patched ");
	else if (updated == SERVER_RCS_DIFF)
		buf_output0(buf_to_net,"Rcs-diff ");
	else
		error(1,0,"Internal error - invalid update type");
	
	output_dir (finfo->update_dir, finfo->repository);
	server_buf_output0(buf_to_net,finfo->file);
	buf_output0(buf_to_net,"\n");

	new_entries_line ();

    mode_string = mode_to_string (mode);
	buf_output0(buf_to_net,mode_string);
	buf_output0(buf_to_net,"\n");
	xfree (mode_string);

	list = last = NULL;

	file = NULL;
	file_allocated = 0;
	file_used = 0;

	if (size > 0)
	{
	    /* Throughout this section we use binary mode to read the
	       file we are sending.  The client handles any line ending
	       translation if necessary.  */

	    if (filebuf == NULL)
	    {
			long status;

			f = CVS_FOPEN (finfo->file, "rb");
			if (f == NULL)
				error (1, errno, "reading %s", fn_root(finfo->fullname));
			status = buf_read_file (f, size, &list, &last);
			if (status != 0)
				error (1, ferror (f) ? errno : 0, "reading %s",
				   fn_root(finfo->fullname));
			if (fclose (f) == EOF)
				error (1, errno, "reading %s", fn_root(finfo->fullname));
	    }
	}

	{
		char text[16];
		sprintf(text,"%lu\n",size);
		buf_output0(buf_to_net,text);
	}

#ifdef _WIN32
	// Statistics tracking
	kflag kf;
	if(vers && vers->options)
		RCS_get_kflags(vers->options,false,kf);

	if(kf.flags&KFLAG_BINARY)
	{
		int binarycount=0,binaryavg=0;
		CGlobalSettings::GetGlobalValue("cvsnt","PServer","BinaryCount",binarycount);
		CGlobalSettings::GetGlobalValue("cvsnt","PServer","BinaryAverage",binaryavg);
		binarycount++;
		binaryavg=(binaryavg+size)/2;
		CGlobalSettings::SetGlobalValue("cvsnt","PServer","BinaryCount",binarycount);
		CGlobalSettings::SetGlobalValue("cvsnt","PServer","BinaryAverage",binaryavg);
	}
	else
	{
		int textcount=0,textavg=0;
		CGlobalSettings::GetGlobalValue("cvsnt","PServer","TextCount",textcount);
		CGlobalSettings::GetGlobalValue("cvsnt","PServer","TextAverage",textavg);
		textcount++;
		textavg=(textavg+size)/2;
		CGlobalSettings::SetGlobalValue("cvsnt","PServer","TextCount",textcount);
		CGlobalSettings::SetGlobalValue("cvsnt","PServer","TextAverage",textavg);
	}
#endif

	if (file != NULL)
	{
	    buf_output (buf_to_net, (char *) file, file_used);
	    xfree (file);
	    file = NULL;
	}
	else if (filebuf == NULL)
	    buf_append_data (buf_to_net, list, last);
	else
	{
	    buf_append_buffer (buf_to_net, filebuf);
	    buf_free (filebuf);
	}
	/* Note we only send a newline here if the file ended with one.  */

	/*
	 * Avoid using up too much disk space for temporary files.
	 * A file which does not exist indicates that the file is up-to-date,
	 * which is now the case.  If this is SERVER_MERGED, the file is
	 * not up-to-date, and we indicate that by leaving the file there.
	 * I'm thinking of cases like "cvs update foo/foo.c foo".
	 */
	if ((updated == SERVER_UPDATED
	     || updated == SERVER_PATCHED
	     || updated == SERVER_RCS_DIFF)
	    && filebuf == NULL
	    /* But if we are joining, we'll need the file when we call
	       join_file.  */
	    && !joining ())
	{
	    if (CVS_UNLINK (finfo->file) < 0)
		error (0, errno, "cannot remove temp file for %s",
		       fn_root(finfo->fullname));
	}
    }
    else if (scratched_file != NULL && entries_line == NULL)
    {
	if (strcmp (scratched_file, finfo->file) != 0)
	    error (1, 0,
		   "CVS server internal error: `%s' vs. `%s' scratched",
		   scratched_file,
		   finfo->file);
	xfree (scratched_file);
	scratched_file = NULL;

	if (kill_scratched_file)
		buf_output0(buf_to_net,"Removed ");
	else
		buf_output0(buf_to_net,"Remove-entry ");
	output_dir (finfo->update_dir, finfo->repository);
	server_buf_output0(buf_to_net,finfo->file);
	buf_output0(buf_to_net,"\n");
	/* keep the vers structure up to date in case we do a join
	 * - if there isn't a file, it can't very well have a version number, can it?
	 *
	 * we do it here on the assumption that since we just told the client
	 * to remove the file/entry, it will, and we want to remember that.
	 * If it fails, that's the client's problem, not ours
	 */
	if (vers && vers->vn_user != NULL)
	{
	    xfree (vers->vn_user);
	    vers->vn_user = NULL;
	}
	if (vers && vers->ts_user != NULL)
	{
	    xfree (vers->ts_user);
	    vers->ts_user = NULL;
	}
    }
    else if (scratched_file == NULL && entries_line == NULL)
    {
	/*
	 * This can happen with death support if we were processing
	 * a dead file in a checkout.
	 */
    }
    else
	error (1, 0,
	       "CVS server internal error: Register *and* Scratch_Entry.\n");
  done:;
}

/* Return whether we should send patches in RCS format.  */

int server_use_rcs_diff ()
{
    return supported_response ("Rcs-diff");
}

void server_set_entstat (const char *update_dir, const char *repository)
{
    static int set_static_supported = -1;
    if (set_static_supported == -1)
	set_static_supported = supported_response ("Set-static-directory");
    if (!set_static_supported) return;

    buf_output0(buf_to_net,"Set-static-directory ");
    output_dir(update_dir, repository);
    buf_output0(buf_to_net,"\n");
}

void server_clear_entstat (const char *update_dir, const char *repository)
{
    static int clear_static_supported = -1;
    if (clear_static_supported == -1)
	clear_static_supported = supported_response ("Clear-static-directory");
    if (!clear_static_supported) return;

    if (noexec)
		return;

    buf_output0(buf_to_net,"Clear-static-directory ");
    output_dir (update_dir, repository);
    buf_output0(buf_to_net,"\n");
}

void server_set_sticky (const char *update_dir, const char *repository, const char *tag, const char *date, int nonbranch, const char *version)
{
    static int set_sticky_supported = -1;

    assert (update_dir != NULL);

    if (set_sticky_supported == -1)
		set_sticky_supported = supported_response ("Set-sticky");
    if (!set_sticky_supported) return;

    if (noexec)
		return;

    if (tag == NULL && date == NULL && version==NULL)
    {
		buf_output0(buf_to_net,"Clear-sticky ");
		output_dir (update_dir, repository);
		buf_output0(buf_to_net,"\n");
    }
    else 
    {
		buf_output0(buf_to_net,"Set-sticky ");
		output_dir (update_dir, repository);
		buf_output0(buf_to_net,"\n");
		if (tag != NULL)
		{
			if (nonbranch)
				buf_output0(buf_to_net,"N");
			else
				buf_output0(buf_to_net,"T");
			buf_output0(buf_to_net,tag);
			if(!nonbranch && version)
			{
				buf_output0(buf_to_net,":");
				buf_output0(buf_to_net,version);
			}
		}
		else if(date != NULL)
		{
			buf_output0(buf_to_net,"D");
			buf_output0(buf_to_net,date);
		}
		else
		{
			buf_output0(buf_to_net,"T:");
			buf_output0(buf_to_net,version);
		}
		buf_output0(buf_to_net,"\n");
    }
}

struct template_proc_data
{
    const char *directory;
	const char *repository;
	const char *update_dir;
};

static int template_proc (void *params, const trigger_interface *cb)
{
    struct template_proc_data *data = (template_proc_data *)params;

	TRACE(1,"template_proc(%s)",PATCH_NULL(data->directory));
	if(cb->get_template)
	{
		const char *template_ptr = NULL;

	    if (!supported_response ("Template"))
		{
			/* Might want to warn the user that the rcsinfo feature won't work.  */
			TRACE(3,"Client does not support Template (rcsinfo) feature.");
			return 0;
		}
		if(!cb->get_template(cb, data->directory, &template_ptr) && template_ptr)
		{
			long template_ptr_len=(long)strlen(template_ptr);
			TRACE(3,"get_template returned success, so send the %d long template to client.",(int)template_ptr_len);
			TRACE(4,"get_template sending \"%s\".",template_ptr);
			char buf[32];
			buf_output0(buf_to_net,"Template ");
			output_dir (data->update_dir, data->repository);
			buf_output0(buf_to_net,"\n");
			snprintf(buf,sizeof(buf),"%ld\n",template_ptr_len);
			buf_output0(buf_to_net,buf);
			buf_output0(buf_to_net,template_ptr);
		}
		else
			TRACE(1,"get_template returned failure");
	}
	return 0;
}

void server_template (const char *update_dir, const char *repository)
{
    struct template_proc_data data;
    data.directory = Short_Repository(repository);
	data.repository = repository;
	data.update_dir = update_dir;
	TRACE(3,"run template proc");
	run_trigger(&data,template_proc);
}

static void serve_gzip_stream (char *arg)
{
    int level;
    level = atoi (arg);
    if (level == 0)
	level = 6;

	if(protocol_compression_enabled)
		return;

	/* All further communication with the client will be compressed.  */
	buf_flush(stderr_buf, 1);
	buf_flush(stdout_buf, 1);

    buf_to_net = compress_buffer_initialize (buf_to_net, 0, level,
					     buf_to_net->memory_error);
    buf_from_net = compress_buffer_initialize (buf_from_net, 1, level,
					       buf_from_net->memory_error);

	cvs_protocol_wrap_set_buffer(stdout_buf, buf_to_net);
	cvs_protocol_wrap_set_buffer(stderr_buf, buf_to_net);

	protocol_compression_enabled = 1;
}

/* Tell the client about RCS options set in CVSROOT/cvswrappers. */
static void serve_wrapper_sendme_rcs_options(char *arg)
{
    /* Actually, this is kind of sdrawkcab-ssa: the client wants
     * verbatim lines from a cvswrappers file, but the server has
     * already parsed the cvswrappers file into the wrap_list struct.
     * Therefore, the server loops over wrap_list, unparsing each
     * entry before sending it.
     */
	cvs::string wrapper_line;

    /* We do this here & make the command RQ_ROOTLESS because all
     * commands send before encryption is enabled must be rootless - 
     * old clients sent this command before enabling encryption (doh!)
     */
    if (current_parsed_root == NULL)
    	error(1,0,"Protocol error: Root request missing");

	bool first=true;
	while(wrap_unparse_rcs_options(wrapper_line,first))
    {
		buf_output0 (buf_to_net, "Wrapper-rcsOption ");
		server_buf_output0 (buf_to_net, wrapper_line.c_str());
		buf_output0 (buf_to_net, "\n");;
    }

    buf_output0 (buf_to_net, "ok\n");

    /* The client is waiting for us, so we better send the data now.  */
    buf_flush (buf_to_net, 1);
}


static void serve_ignore (char *arg)
{
    /*
     * Just ignore this command.  This is used to support the
     * update-patches command, which is not a real command, but a signal
     * to the client that update will accept the -u argument.
     */
}

#endif // SERVER_SUPPORT

static void serve_valid_requests(char *arg);

/*
 * Parts of this table are shared with the client code,
 * but the client doesn't need to know about the handler
 * functions.
 */
/* The code lets all RQ_ROOTLESS requests pass through unencrypted
 * and unsigned even when message encryption/authentication is required
 * (option "MessageProtection"). This was done for compatibility with
 * CVS 1.11.1.1p1 and earlier (which send the encryption/authentication
 * request after Root, UseUnchanged, and the gzip requests). Those requests
 * are "safe enough" to not require encryption or authentication (although
 * CVSNT 1.11.1.4+ will try to encrypt the Root request because I suppose
 * it might be sensitive). If any RQ_ROOTLESS requests are added
 * that need to be encrypted, additional flags will need to be introduced
 * to identify which commands may be safely processed without integrity
 * checks. Similarly, if CVS 1.11.1.1p1 compatibility is not required,
 * then the additional flags could be added to require that Root, etc.
 * come after the encryption/authentication request.
 */
struct request requests[] =
{
#ifdef SERVER_SUPPORT
#define REQ_LINE(n, f, s) {n, f, s}
#else
#define REQ_LINE(n, f, s) {n, s}
#endif
  REQ_LINE("Root", serve_root, RQ_ESSENTIAL | RQ_ROOTLESS),
  REQ_LINE("Valid-responses", serve_valid_responses,
	   RQ_ESSENTIAL | RQ_ROOTLESS),
  REQ_LINE("valid-requests", serve_valid_requests,
	   RQ_ESSENTIAL | RQ_ROOTLESS),
  REQ_LINE("Directory", serve_directory, RQ_ESSENTIAL),
  REQ_LINE("Max-dotdot", serve_max_dotdot, 0),
  REQ_LINE("Static-directory", serve_static_directory, 0),
  REQ_LINE("Sticky", serve_sticky, 0),
  REQ_LINE("Entry", serve_entry, RQ_ESSENTIAL),
  REQ_LINE("EntryExtra", serve_entry_extra, 0),
  REQ_LINE("Kopt", serve_kopt, 0),
  REQ_LINE("Checkin-time", serve_checkin_time, 0),
  REQ_LINE("Checksum", serve_checksum, 0),
  REQ_LINE("Modified", serve_modified, RQ_ESSENTIAL),
  REQ_LINE("Binary-transfer", serve_binary_transfer, 0),
  REQ_LINE("Is-modified", serve_is_modified, 0),
  REQ_LINE("UseUnchanged", serve_enable_unchanged, RQ_ENABLEME | RQ_ROOTLESS),
  REQ_LINE("Unchanged", serve_unchanged, RQ_ESSENTIAL),
  REQ_LINE("Notify", serve_notify, 0),
  REQ_LINE("NotifyUser", serve_notify_user, 0),
  REQ_LINE("Questionable", serve_questionable, 0),
  REQ_LINE("Utf8", serve_utf8, RQ_ROOTLESS), /* Depreciated, but checked for option support by server */
  REQ_LINE("Case", NULL, 0), /* Deprecated but we send it for older clients */
  REQ_LINE("Argument", serve_argument, RQ_ESSENTIAL),
  REQ_LINE("Argumentx", serve_argumentx, RQ_ESSENTIAL),
  REQ_LINE("Global_option", serve_global_option, RQ_ROOTLESS),
  REQ_LINE("Gzip-stream", serve_gzip_stream, RQ_ROOTLESS),
  REQ_LINE("wrapper-sendme-rcsOptions", serve_wrapper_sendme_rcs_options, RQ_ROOTLESS),
  REQ_LINE("Set", serve_set, RQ_ROOTLESS),
  REQ_LINE("Rename", serve_rename, 0),
  REQ_LINE("VirtualRepository", serve_virtual_repository, 0),
  REQ_LINE("expand-modules", serve_expand_modules, 0),
  REQ_LINE("ci", serve_ci, RQ_ESSENTIAL),
  REQ_LINE("co", serve_co, RQ_ESSENTIAL),
  REQ_LINE("chown", serve_chown, 0),
  REQ_LINE("rchown", serve_rchown, 0),
  REQ_LINE("chacl2", serve_chacl, 0),
  REQ_LINE("rchacl2", serve_rchacl, 0),
  REQ_LINE("lsacl", serve_lsacl, 0),
  REQ_LINE("rlsacl", serve_rlsacl, 0),
  REQ_LINE("passwd", serve_passwd, 0),
  REQ_LINE("passwd-md5", serve_passwd_md5, RQ_SERVER_REQUEST),
  REQ_LINE("info", serve_info, 0),
  REQ_LINE("update", serve_update, RQ_ESSENTIAL),
  REQ_LINE("diff", serve_diff, 0),
  REQ_LINE("log", serve_log, 0),
  REQ_LINE("rlog", serve_rlog, 0),
  REQ_LINE("add", serve_add, 0),
  REQ_LINE("remove", serve_remove, 0),
  REQ_LINE("update-patches", serve_ignore, 0),
  REQ_LINE("status", serve_status, 0),
  REQ_LINE("ls", serve_ls, 0),
  REQ_LINE("rls", serve_ls, 0),
  REQ_LINE("rdiff", serve_rdiff, 0),
  REQ_LINE("tag", serve_tag, 0),
  REQ_LINE("rtag", serve_rtag, 0),
  REQ_LINE("import", serve_import, 0),
  REQ_LINE("admin", serve_admin, 0),
  REQ_LINE("export", serve_export, 0),
  REQ_LINE("history", serve_history, 0),
  REQ_LINE("release", serve_release, 0),
  REQ_LINE("watch-on", serve_watch_on, 0),
  REQ_LINE("watch-off", serve_watch_off, 0),
  REQ_LINE("watch-add", serve_watch_add, 0),
  REQ_LINE("watch-remove", serve_watch_remove, 0),
  REQ_LINE("watchers", serve_watchers, 0),
  REQ_LINE("editors", serve_editors, 0),
  REQ_LINE("editors-edit", serve_editors_edit, 0),
  REQ_LINE("init", serve_init, 0),
  REQ_LINE("annotate", serve_annotate, 0),
  REQ_LINE("rannotate", serve_rannotate, 0),
  REQ_LINE("noop", serve_noop, RQ_ROOTLESS),
  REQ_LINE("version", serve_version, RQ_ROOTLESS),

  /* The client should never actually send this request */
  REQ_LINE("Rootless-stream-modification", serve_noop, 0),
  
  REQ_LINE("Kerberos-encrypt", serve_protocol_encrypt, RQ_ROOTLESS),
  REQ_LINE("Gssapi-encrypt", serve_protocol_encrypt, RQ_ROOTLESS),
  REQ_LINE("Protocol-encrypt", serve_protocol_encrypt, RQ_ROOTLESS),
  REQ_LINE("Gssapi-authenticate", serve_protocol_authenticate, RQ_ROOTLESS),
  REQ_LINE("Protocol-authenticate", serve_protocol_authenticate, RQ_ROOTLESS),
  REQ_LINE("read-cvsrc", serve_read_cvsrc, 0),
  REQ_LINE("read-cvsrc2", serve_read_cvsrc2, 0),
  REQ_LINE("read-cvsignore", serve_read_cvsignore, 0),
  REQ_LINE("read-cvswrappers", serve_read_cvswrappers, 0),
  REQ_LINE("Error-If-Reader", serve_error_if_reader, 0),
  REQ_LINE("server-codepage", serve_server_codepage, 0),
  REQ_LINE("client-version", serve_client_version, 0),

  REQ_LINE("Authentication-Requested", authentication_requested, RQ_SERVER_REQUEST),
  REQ_LINE("Authentication-Required", authentication_required, RQ_SERVER_REQUEST),
  REQ_LINE("Encryption-Requested", encryption_requested, RQ_SERVER_REQUEST),
  REQ_LINE("Encryption-Required", encryption_required, RQ_SERVER_REQUEST),
  REQ_LINE("Compression-Requested", compression_requested, RQ_SERVER_REQUEST),
  REQ_LINE("Compression-Required", compression_required, RQ_SERVER_REQUEST),

  REQ_LINE("Can-Rename", server_can_rename, RQ_SERVER_REQUEST),

  REQ_LINE("Valid-RcsOptions", serve_valid_rcsoptions, 0),

  REQ_LINE(NULL,NULL,0)
#undef REQ_LINE
};

#ifdef SERVER_SUPPORT

static void serve_valid_requests (char *arg)
{
    struct request *rq;

	buf_output0 (buf_to_net, "Valid-requests");
    for (rq = requests; rq->name != NULL; rq++)
    {
		if (rq->func != NULL)
		{
			char flag = 0;

			buf_append_char (buf_to_net, ' ');
			if(rq->flags & RQ_SERVER_REQUEST)
			{
				rq->func(&flag);
				if(!flag)
					continue;
			}
			buf_output0 (buf_to_net, rq->name);
		}
    }
    buf_output0 (buf_to_net, "\nok\n");

    /* The client is waiting for the list of valid requests, so we
       must send the output now.  */
    buf_flush (buf_to_net, 1);
}

void server_cleanup (int sig)
{
    /* Do "rm -rf" on the temp directory.  */
    int status;
    int save_noexec;

    if (buf_to_net != NULL)
    {
	/* FIXME: If this is not the final call from server, this
	   could deadlock, because the client might be blocked writing
	   to us.  This should not be a problem in practice, because
	   we do not generate much output when the client is not
	   waiting for it.  */
	set_block (buf_to_net);
	buf_flush (buf_to_net, 1);

	/* The calls to buf_shutdown are currently only meaningful
	   when we are using compression.  First we shut down
	   BUF_FROM_NET.  That will pick up the checksum generated
	   when the client shuts down its buffer.  Then, after we have
	   generated any final output, we shut down BUF_TO_NET.  */

	status = buf_shutdown (buf_from_net);
	buf_from_net = NULL;
	if (status != 0)
	{
	    error (0, status, "shutting down buffer from client");
	    buf_flush (buf_to_net, 1);
	}
    }

    if (dont_delete_temp)
    {
	if (buf_to_net != NULL)
	    (void) buf_shutdown (buf_to_net);
	return;
    }

    CVS_CHDIR (Tmpdir);
    /* Temporarily clear noexec, so that we clean up our temp directory
       regardless of it (this could more cleanly be handled by moving
       the noexec check to all the unlink_file_dir callers from
       unlink_file_dir itself).  */
    save_noexec = noexec;
    noexec = 0;
    /* FIXME?  Would be nice to not ignore errors.  But what should we do?
       We could try to do this before we shut down the network connection,
       and try to notify the client (but the client might not be waiting
       for responses).  We could try something like syslog() or our own
       log file.  */
	trace = 0;
	if(orig_server_temp_dir)
		unlink_file_dir (orig_server_temp_dir);
    noexec = save_noexec;

    if (buf_to_net != NULL)
	{
		buf_shutdown (buf_to_net);
		buf_to_net = NULL;
	}

	CProtocolLibrary lib;
	lib.UnloadProtocol(server_protocol);
}

int server_active = 0;
int pserver_active = 0;
int proxy_active = 0;

int server (int argc, char **argv)
{
	const char *log = CProtocolLibrary::GetEnvironment("CVS_SERVER_LOG");

    if (argc == -1)
    {
		static const char *const msg[] =
		{
			"Usage: %s %s\n",
			"  Normally invoked by a cvs client on a remote machine.\n",
			NULL
		};
		usage (msg);
    }
    /* Ignore argc and argv.  They might be from .cvsrc.  */

	if(!buf_to_net)
		buf_to_net = fd_buffer_initialize (STDOUT_FILENO, 0, outbuf_memory_error);
	if(!buf_from_net)
		buf_from_net = stdio_buffer_initialize (stdin, 1, outbuf_memory_error);

    /* Set up logfiles, if any. */
    if (log)
    {
	int len = strlen (log);
	char *buf = (char*)xmalloc (len + 64);
	char *p;
	FILE *fp;

	sprintf(buf,"%s-%d",log,(int)getpid());
	p = buf + strlen(buf);

	/* Open logfiles in binary mode so that they reflect
	   exactly what was transmitted and received (that is
	   more important than that they be maximally
	   convenient to view).  */
	strcpy (p, ".out");
	fp = open_file (buf, "wb");
        if (fp == NULL)
	    error (0, errno, "opening to-server logfile %s", buf);
	else
	    buf_to_net = log_buffer_initialize (buf_to_net, fp, 0,
					       (BUFMEMERRPROC) NULL);

	strcpy (p, ".in");
	fp = open_file (buf, "wb");
        if (fp == NULL)
	    error (0, errno, "opening from-server logfile %s", buf);
	else
	    buf_from_net = log_buffer_initialize (buf_from_net, fp, 1,
						 (BUFMEMERRPROC) NULL);

	xfree (buf);
    }

	stdout_buf = cvs_protocol_wrap_buffer_initialize(buf_to_net, 'M');
	stderr_buf = cvs_protocol_wrap_buffer_initialize(buf_to_net, 'E');

    /* OK, now figure out where we stash our temporary files.  */
    {
	char *p;

	/* The code which wants to chdir into server_temp_dir is not set
	   up to deal with it being a relative path.  So give an error
	   for that case.  */
	if (!isabsolute (Tmpdir))
	{
		error(1,0,"Value of %s for TMPDIR is not absolute", Tmpdir);
	}
	else
	{
	    int status;
	    int i = 0;

	    server_temp_dir = (char*)xmalloc (strlen (Tmpdir) + 256);
	    if (server_temp_dir == NULL)
	    {
			error(1,ENOMEM,"Fatal server error, aborting.");
	    }
	    strcpy (server_temp_dir, Tmpdir);

	    /* Remove a trailing slash from TMPDIR if present.  */
	    p = server_temp_dir + strlen (server_temp_dir) - 1;
	    if (*p == '/')
		*p = '\0';

	    /*
	     * I wanted to use cvs-serv/PID, but then you have to worry about
	     * the permissions on the cvs-serv directory being right.  So
	     * use cvs-servPID.
	     */
	    strcat (server_temp_dir, "/cvs-serv");

	    p = server_temp_dir + strlen (server_temp_dir);
	    sprintf (p, "%ld", (long) getpid ());

	    orig_server_temp_dir = server_temp_dir;

	    /* Create the temporary directory, and set the mode to
               700, to discourage random people from tampering with
               it.  */
	    while ((status = mkdir_p (server_temp_dir)) == EEXIST)
	    {
	        static const char suffix[] = "abcdefghijklmnopqrstuvwxyz";

	        if (i >= sizeof suffix - 1) break;
			if (i == 0) p = server_temp_dir + strlen (server_temp_dir);
			p[0] = suffix[i++];
			p[1] = '\0';
	    }
	    if (status != 0)
	    {
			error(1,errno,"can't create temporary directory %s",server_temp_dir);
	    }
#ifndef CHMOD_BROKEN
	    else if (chmod (server_temp_dir, S_IRWXU) < 0)
	    {
			error(1,errno,"cannot change permissions on temporary directory %s",
						fn_root(server_temp_dir));
	    }
#endif
	    else if (CVS_CHDIR (server_temp_dir) < 0)
	    {
			error(1,errno,"cannot change to temporary directory %s",
						fn_root(server_temp_dir));
	    }
	}
    }

#ifdef SIGABRT
    (void) SIG_register (SIGABRT, server_cleanup);
#endif
#ifdef SIGHUP
    (void) SIG_register (SIGHUP, server_cleanup);
#endif
#ifdef SIGINT
    (void) SIG_register (SIGINT, server_cleanup);
#endif
#ifdef SIGQUIT
    (void) SIG_register (SIGQUIT, server_cleanup);
#endif
#ifdef SIGPIPE
    (void) SIG_register (SIGPIPE, server_cleanup);
#endif
#ifdef SIGTERM
    (void) SIG_register (SIGTERM, server_cleanup);
#endif

    /* Now initialize our argument vector (for arguments from the client).  */

    /* Small for testing.  */
    argument_vector_size = 1;
    argument_vector =
	(char **) xmalloc (argument_vector_size * sizeof (char *));
    if (argument_vector == NULL)
    {
	/*
	 * Strictly speaking, we're not supposed to output anything
	 * now.  But we're about to exit(), give it a try.
	 */
	printf ("E Fatal server error, aborting.\nerror ENOMEM Virtual memory exhausted.\n");

	/* I'm doing this manually rather than via error_exit ()
	   because I'm not sure whether we want to call server_cleanup.
	   Needs more investigation....  */

#ifdef SYSTEM_CLEANUP
	/* Hook for OS-specific behavior, for example socket subsystems on
	   NT and OS2 or dealing with windows and arguments on Mac.  */
	SYSTEM_CLEANUP ();
#endif

	CCvsgui::Close(EXIT_FAILURE);
	exit (EXIT_FAILURE);
    }

    argument_count = 1;
    /* This gets printed if the client supports an option which the
       server doesn't, causing the server to print a usage message.
       FIXME: probably should be using program_name here.
       FIXME: just a nit, I suppose, but the usage message the server
       prints isn't literally true--it suggests "cvs server" followed
       by options which are for a particular command.  Might be nice to
       say something like "client apparently supports an option not supported
       by this server" or something like that instead of usage message.  */
    argument_vector[0] = "cvs server";

    while (1)
    {
	char *cmd, *orig_cmd;
	struct request *rq;
	int status;
	
	status = server_read_line (buf_from_net, &cmd, (int *) NULL);
	if (status == -2)
	{
	    buf_output0 (buf_to_net, "E Fatal server error, aborting.\n\
error ENOMEM Virtual memory exhausted.\n");
	    break;
	}
	if (status != 0)
	    break;
	if(!*cmd)
	    continue;
    
	orig_cmd = cmd;
	for (rq = requests; rq->name != NULL; ++rq)
	    if (strncmp (cmd, rq->name, strlen (rq->name)) == 0)
	    {
            int len = strlen (rq->name);
            if (cmd[len] == '\0')
	            cmd += len;
            else if (cmd[len] == ' ')
	            cmd += len + 1;
            else
	            /*
	             * The first len characters match, but it's a different
	             * command.  e.g. the command is "cooperate" but we matched
	             * "co".
	             */
	            continue;

			/* No longer supported by the server */
			if(!rq->func)
				continue;

            if (!(rq->flags & RQ_ROOTLESS))
            {
	            if (current_parsed_root == NULL)
                    error(1,0,"Protocol error: Root request missing");
			}

			/* By convention 'commands' start with lowercase, and 'flags' start with
			   uppercase.  The extra check allows us to look for things like
			   'version' and 'init */
			/* valid-requests is an oddity */
            if (!(rq->flags & RQ_ROOTLESS) || ((rq->name[0]>='a' && rq->name[0]<='z') && strcmp(rq->name,"valid-requests")))
            {
				char buf[64];
				if(!default_client_codepage && !server_codepage_sent && !CGlobalSettings::GetGlobalValue("cvsnt","PServer","DefaultClientCodepage",buf,sizeof(buf)))
					default_client_codepage = xstrdup(buf); // Yeah we leak this at shutdown, but it doesn't really matter

				if(encryption_level==4 && protocol_encryption_enabled!=PROTOCOL_ENCRYPTION)
					error(1,0,"This server requires an encrypted connection");
				if(encryption_level==3 && protocol_encryption_enabled==0)
					error(1,0,"This server reqires a signed or encrypted connection");
				if(compression_level==2 && protocol_compression_enabled==0)
					error(1,0,"This server requries a compressed connection");

				if(allowed_clients && *allowed_clients && strcmp(rq->name,"server-codepage") && strcmp(rq->name,"client-version")) // This check happens late
				{
					static int check_done=0;
					if(!check_done)
					{
						cvs::wildcard_filename ac = serv_client_version?serv_client_version:"";
						if(!ac.length() && compat_level==1)
							ac="CVSNT";
						if(!ac.matches_regexp(allowed_clients))
						{
							TRACE(3,"Failed allowed client match: %s != %s",allowed_clients,serv_client_version);
							error(1,0,"This server does not allow %s clients to connect.",serv_client_version?serv_client_version:compat_level?"old cvsnt":"legacy cvs");
						}
						check_done=1;
					}
				}
            }
            (*rq->func) (cmd);
			buf_send_output(stderr_buf);
			buf_send_output(stdout_buf);
		break;
	    }
	if (rq->name == NULL)
	{	
        buf_output0 (buf_to_net, "error  unrecognized request `");
		buf_output0 (buf_to_net, cmd);
		buf_append_char (buf_to_net, '\'');
		buf_append_char (buf_to_net, '\n');
	}
	xfree (orig_cmd);
    }
    server_cleanup (0);
    return 0;
}

#if defined (SERVER_SUPPORT)

static void switch_to_user(const char *, int username_switch);

#ifndef _WIN32 // Win32 has a totally different authentication mechanism

static void do_chroot()
{
      const char *d;

      if(!chroot_done)
      {
        if(chroot_base && strlen(chroot_base))
        {
          if(ISDIRSEP(chroot_base[strlen(chroot_base)-1]))
            chroot_base[strlen(chroot_base)-1]='\0';

          if(current_parsed_root)
          {
            if(fnncmp(current_parsed_root->directory,chroot_base,strlen(chroot_base)) || strlen(chroot_base)>strlen(current_parsed_root->directory))
            {
              printf("error 0 Repository is not within chroot\n");
              fflush(stdout);
              error_exit();
            }

            d = current_parsed_root->directory;
            if(fncmp(current_parsed_root->directory,chroot_base))
              current_parsed_root->directory = xstrdup(d+strlen(chroot_base));
            else
              current_parsed_root->directory = xstrdup("/");

            xfree(d);
          }

          if(CVS_CHDIR(chroot_base)<0)
          {
              printf("error 0 Cannot change directory to chroot\n");
              fflush(stdout);
              error_exit();
          }

          if(chroot(chroot_base) < 0)
          {
              printf("error 0 chroot failed: %s\n", strerror (errno));
              fflush(stdout);
              error_exit();
          }
          chroot_done = 1;
        }
      }
}

static void switch_to_user (const char *username, int username_switch, void *user_token)
{
    struct passwd *pw;

    pw = getpwnam ((char*)username);
    if (pw == NULL)
    {
	/* Normally this won't be reached; check_password contains
	   a similar check.  */

	printf ("E Fatal error, aborting.\nerror 0 %s: no such user\n", username);
	fflush(stdout);
	/* Don't worry about server_cleanup; server_active isn't set yet.  */
	error_exit ();
    }

#if HAVE_INITGROUPS
    if (initgroups (pw->pw_name, pw->pw_gid) < 0
#  ifdef EPERM
	/* At least on the system I tried, initgroups() only works as root.
	   But we do still want to report ENOMEM and whatever other
	   errors initgroups() might dish up.  */
	&& errno != EPERM
#  endif
	)
    {
	/* This could be a warning, but I'm not sure I see the point
	   in doing that instead of an error given that it would happen
	   on every connection.  We could log it somewhere and not tell
	   the user.  But at least for now make it an error.  */
	printf ("error 0 initgroups failed: %s\n", strerror (errno));
	fflush(stdout);
	/* Don't worry about server_cleanup; server_active isn't set yet.  */
	error_exit ();
    }
#endif /* HAVE_INITGROUPS */
	
	do_chroot();

#ifdef SETXID_SUPPORT
    /* honor the setgid bit iff set*/
    if (getgid() != getegid())
    {
		if (setgid (getegid ()) < 0)
		{
			/* See comments at setuid call below for more discussion.  */
			printf ("error 0 setgid failed: %s\n", strerror (errno));
			fflush(stdout);
			/* Don't worry about server_cleanup;
			server_active isn't set yet.  */
			error_exit ();
		}
    }
    else
#endif
    {
	if (setgid (pw->pw_gid) < 0)
	{
	    /* See comments at setuid call below for more discussion.  */
	    printf ("error 0 setgid failed: %s\n", strerror (errno));
	    /* Don't worry about server_cleanup;
	       server_active isn't set yet.  */
	    error_exit ();
	}
    }
    
    if (setuid (pw->pw_uid) < 0)
    {
	/* Note that this means that if run as a non-root user,
	   CVSROOT/passwd must contain the user we are running as
	   (e.g. "joe:FsEfVcu:cvs" if run as "cvs" user).  This seems
	   cleaner than ignoring the error like CVS 1.10 and older but
	   it does mean that some people might need to update their
	   CVSROOT/passwd file.  */
	printf ("error 0 setuid failed: %s\n", strerror (errno));
	fflush(stdout);

	/* Don't worry about server_cleanup; server_active isn't set yet.  */
	error_exit ();
    }

    /* We don't want our umask to change file modes.  The modes should
       be set by the modes used in the repository, and by the umask of
       the client.  */
    umask (0);

#ifdef SERVER_SUPPORT
    /* Make sure our CVS_Username has been set. */
    if (CVS_Username == NULL)
#ifdef _WIN32
		CVS_Username = sanitise_username(username);
#else
		CVS_Username = xstrdup(username);
#endif
#endif
      
#if HAVE_PUTENV
    /* Set LOGNAME, USER and CVS_USER in the environment, in case they
       are already set to something else.  */
    {
	const char *cvs_user;

	cvs_putenv("LOGNAME", username);
	cvs_putenv("USER", username);

    cvs_user = NULL != CVS_Username ? CVS_Username : "";
	cvs_putenv("CVS_USER", cvs_user);
    }
#endif /* HAVE_PUTENV */
}

#else

static void do_chroot() { }

/* If username_switch is set, then we don't used any cached credential information.  This
   lets us become any arbitrary user */
static void switch_to_user (const char *username, int username_switch, void *user_token)
{
	if(!username_switch && server_protocol->impersonate)
	{
		TRACE(3,"Impersonating preauthenticated user using protocol specific impersionation");
		if(server_protocol->impersonate(client_protocol, username, NULL)!=CVSPROTO_SUCCESS)
		{
			printf ("E Fatal error, aborting.\nerror 0 %s: Impersonation for :%s: protocol failed.\n", username, server_protocol->name);
			error_exit();
		}
	}
	else
	{
		switch(win32switchtouser(username, user_token))
		{
		case 4: // No such domain
			printf ("E Fatal error, aborting.\nerror 0 %s: no such domain\n", username);
			error_exit ();
			break;
		case 3: // Account disabled
			printf ("E Fatal error, aborting.\nerror 0 %s: user account disabled\n", username);
			error_exit ();
			break;
		case 2: // No such user
			printf ("E Fatal error, aborting.\nerror 0 %s: no such user\n", username);
			error_exit ();
			break;
		case 1: // Not enough privileges
			printf ("E Fatal error, aborting.\nerror 0 %s: Switch to user failed due to configuration error.  Contact your System Administrator.\n", username);
			error_exit ();
			break;
		case 0: // Ok
			break;
		}
	}
#ifdef SERVER_SUPPORT
    /* Make sure our CVS_Username has been set. */
    if (CVS_Username == NULL)
		CVS_Username = xstrdup(username);
#endif
      
#ifdef HAVE_PUTENV
    /* Set LOGNAME, USER and CVS_USER in the environment, in case they
       are already set to something else.  */
    {
	const char *cvs_user;

	cvs_putenv("LOGNAME", username);
	cvs_putenv("USER", username);

    cvs_user = NULL != CVS_Username ? CVS_Username : "";
	cvs_putenv("CVS_USER", cvs_user);
    }
#endif /* HAVE_PUTENV */
}

#endif /* _WIN32 */

/* 
 * 0 means no entry found for this user.
 * 1 means entry found and password matches (or found password is empty)
 * 2 means entry found, but password does not match.
 *
 * If 1, host_user_ptr will be set to point at the system
 * username (i.e., the "real" identity, which may or may not be the
 * CVS username) of this user; caller may free this.  Global
 * CVS_Username will point at an allocated copy of cvs username (i.e.,
 * the username argument below).
 * kff todo: FIXME: last sentence is not true, it applies to caller.
 */
static int check_repository_password (const char *username, const char *password, const char *repository, char **host_user_ptr, void **user_token)
{
    int retval = 0;
    FILE *fp;
    char *filename;
    char *linebuf = NULL;
    size_t linebuf_len;
    int found_it = 0;
    int namelen;
	
    /* We don't use current_parsed_root->directory because it hasn't been set yet
     * -- our `repository' argument came from the authentication
     * protocol, not the regular CVS protocol.
     */

    filename = (char*)xmalloc (strlen (repository)
			+ 1
			+ strlen (CVSROOTADM)
			+ 1
			+ strlen (CVSROOTADM_PASSWD)
			+ 1);

    (void) sprintf (filename, "%s/%s/%s", repository,
                    CVSROOTADM, CVSROOTADM_PASSWD);

    fp = CVS_FOPEN (filename, "r");
    if (fp == NULL)
    {
	if (!existence_error (errno))
	    error (0, errno, "cannot open %s", filename);
	return 0;
    }

    /* Look for a relevant line -- one with this user's name. */
    namelen = strlen (username);
    while (getline (&linebuf, &linebuf_len, fp) >= 0)
    {
	if ((strncmp (linebuf, username, namelen) == 0)
	    && ((linebuf[namelen] == ':') || linebuf[namelen] == '\n' || linebuf[namelen] == '\0'))
        {
	    found_it = 1;
	    break;
        }
    }
    if (ferror (fp))
	error (0, errno, "cannot read %s", filename);
    if (fclose (fp) < 0)
	error (0, errno, "cannot close %s", filename);

    /* If found_it, then linebuf contains the information we need. */
    if (found_it)
    {
	char *found_password, *host_user_tmp;
        char *non_cvsuser_portion;

        /* We need to make sure lines such as 
         *
         *    "username::sysuser\n"
         *    "username:\n"
         *    "username:  \n"
         *
         * all result in a found_password of NULL, but we also need to
         * make sure that
         *
         *    "username:   :sysuser\n"
         *    "username: <whatever>:sysuser\n"
         *
         * continues to result in an impossible password.  That way,
         * an admin would be on safe ground by going in and tacking a
         * space onto the front of a password to disable the account
         * (a technique some people use to close accounts
         * temporarily).
         */

        /* Make `non_cvsuser_portion' contain everything after the CVS
           username, but null out any final newline. */
	non_cvsuser_portion = linebuf + namelen;
        strtok (non_cvsuser_portion, "\n");

        /* If there's a colon now, we just want to inch past it. */
        if (strchr (non_cvsuser_portion, ':') == non_cvsuser_portion)
            non_cvsuser_portion++;

        /* Okay, after this conditional chain, found_password and
           host_user_tmp will have useful values: */

        if ((non_cvsuser_portion == NULL)
            || (strlen (non_cvsuser_portion) == 0)
            || ((strspn (non_cvsuser_portion, " \t"))
                == strlen (non_cvsuser_portion)))
        {
            found_password = NULL;
            host_user_tmp = NULL;
        }
        else if (strncmp (non_cvsuser_portion, ":", 1) == 0)
        {
            found_password = NULL;
            host_user_tmp = non_cvsuser_portion + 1;
            if (strlen (host_user_tmp) == 0)
                host_user_tmp = NULL;
        }
        else
        {
            found_password = strtok (non_cvsuser_portion, ":");
            host_user_tmp = strtok (NULL, ":");
        }

        /* Of course, maybe there was no system user portion... */
	if (host_user_tmp == NULL)
            host_user_tmp = (char*)username;

        /* Verify blank passwords directly, otherwise use crypt(). */
        if ((found_password == NULL) || (password==NULL)
#ifdef _WIN32 // NTServer mode sets password==NULL for authentication
	    || (found_password[0]=='!' && win32_valid_user(username,password,found_password+1, user_token))
#endif
		|| !CCrypt::compare(password, found_password))
        {
            /* Give host_user_ptr permanent storage. */
            *host_user_ptr = xstrdup (host_user_tmp);
	    retval = 1;
        }
	else
        {
            *host_user_ptr = NULL;
	    retval         = 2;
        }
    }
    else     /* Didn't find this user, so deny access. */
    {
	*host_user_ptr = NULL;
	retval = 0;
    }

    xfree (filename);
    if (linebuf)
        xfree (linebuf);

    return retval;
}

#ifdef HAVE_PAM
struct cvs_pam_userdata
{
  char *username;
  char *password;
};

#if defined(sun) || defined(__hpux)
/* Solaris has a different definition of this function */
int cvs_conv(int num_msg, struct pam_message **msgm,
            struct pam_response **response, void *appdata_ptr)
#else
int cvs_conv(int num_msg, const struct pam_message **msgm,
            struct pam_response **response, void *appdata_ptr)
#endif
{
    int count=0;
    struct pam_response *reply;
    struct cvs_pam_userdata *udp = (struct cvs_pam_userdata *)appdata_ptr;

    if (num_msg <= 0)
       return PAM_CONV_ERR;

    reply = (struct pam_response *) xmalloc(num_msg*sizeof(struct pam_response));
    if (reply == NULL)
       return PAM_CONV_ERR;

    if (udp == NULL)
    {
       error(0,0,"PAM on this system is broken - appdata_ptr == NULL !");
       return PAM_CONV_ERR;
    }

    memset(reply, '\0', sizeof(struct pam_response) * num_msg);

    for (count=0; count < num_msg; ++count)
    {
       char *string=NULL;

       switch (msgm[count]->msg_style)
       {
           case PAM_PROMPT_ECHO_ON:
               string = xstrdup(udp->username);
               break;
           case PAM_PROMPT_ECHO_OFF:
               string = xstrdup(udp->password);
               break;
         case PAM_TEXT_INFO:
         case PAM_ERROR_MSG:
             break;
           default:
             xfree(reply);
             return PAM_CONV_ERR;
       }

       if (string) /* must add to reply array */
       {
           /* add string to list of responses */
           reply[count].resp_retcode = 0;
           reply[count].resp = string;
           string = NULL;
       }
    }

    *response = reply;
    reply = NULL;

    return PAM_SUCCESS;
}

static struct pam_conv conv = {
    cvs_conv,
    NULL
};

/* Modelled very closely on the example code in "The Linux-PAM
   Application Developers' Guide" by Andrew G. Morgan. */
/* Return a hosting username if password matches, else NULL. */
static char *check_pam_password (const char *username, const char *password)
{
    pam_handle_t *pamh = NULL;
    int retval;
    char *host_user = NULL;
    struct cvs_pam_userdata ud;

    ud.username = (char*)username;
    ud.password = (char*)password;

    conv.appdata_ptr = &ud;

    retval = pam_start("cvsnt", username, &conv, &pamh);

    if (retval == PAM_SUCCESS)
       retval = pam_authenticate(pamh, 0);    /* is user really user? */

    if (retval == PAM_SUCCESS)
       retval = pam_acct_mgmt(pamh, 0);       /* permitted access? */

    /* This is where we have been authorized or not. */

    switch(retval)
    {
       case PAM_SUCCESS:
           host_user = xstrdup(username);
           break;
       case PAM_AUTH_ERR:
       default:
           host_user = NULL;
           break;
    }

    /* now close PAM */
    if (pam_end(pamh,retval) != PAM_SUCCESS)
    {
      pamh = NULL;
      error(1, 0, "PAM failed to release authenticator\n");
    }

    return host_user;
}

#else /* HAVE_PAM */

static char *check_system_password(const char *username, const char *password, void **user_token)
{
    const char *found_passwd = NULL;
    struct passwd *pw;
    char *host_user = NULL;

#ifdef HAVE_GETSPNAM
    struct spwd *spw;

    spw = getspnam (username);
    if (spw != NULL)
    {
      found_passwd = spw->sp_pwdp;
    }
#endif /* HAVE_GETSPNAM */

    if (found_passwd == NULL && (pw = getpwnam (username)) != NULL)
    {
        found_passwd = pw->pw_passwd;
    }

    if(found_passwd && *found_passwd)
    {
#ifdef _WIN32
      host_user = win32_valid_user(username,password,NULL,user_token)
                         ? xstrdup(username) : NULL;
#else
      /* user exists and has a password */
		host_user = CCrypt::compare(password, found_passwd) ? NULL : xstrdup(username);
#endif
    }
    else if (found_passwd && password && *password)
    {
      /* user exists and has no system password, but we got
             one as parameter */
      host_user = xstrdup (username);
    }
    return host_user;
}

#endif /* HAVE_PAM */

/* Return a hosting username if password matches, else NULL. */
static char *check_password (const char *username, const char *password, const char *repository, void **user_token)
{
    int rc;
    char *host_user = NULL;

	if(user_token)
		*user_token = NULL;

    /* First we see if this user has a password in the CVS-specific
       password file.  If so, that's enough to authenticate with.  If
       not, we'll check /etc/passwd. */

    rc = check_repository_password (username, password, repository,
				    &host_user, user_token);

    if (rc == 2)
    {
		TRACE(3,"User in CVSROOT/passwd, but password incorect");
		return NULL;
    }

    /* else */

    if (rc == 1)
    {
	TRACE(3,"User in CVSROOT/passwd, password correct");
        /* host_user already set by reference, so just return. */
        goto handle_return;
    }
    else if (rc == 0 && system_auth && !password)
    {
       /* ntserver/sspi uses password=NULL */
       host_user=xstrdup(username);
       TRACE(3,"System password already validated by authentication mechanism");
       goto handle_return;
    }
    else if (rc == 0 && system_auth)
    {
#ifdef HAVE_PAM
	TRACE(3,"Checking password using PAM");
	host_user = check_pam_password (username, password);
#else
	TRACE(3,"Checking password using passwd/shadow files");
	host_user = check_system_password (username, password, user_token);
#endif
    }
    else if (rc == 0)
    {
	TRACE(3,"No systemauth, and user not in CVSROOT/passwd");
	/* Note that the message _does_ distinguish between the case in
	   which we check for a system password and the case in which
	   we do not.  It is a real pain to track down why it isn't
	   letting you in if it won't say why, and I am not convinced
	   that the potential information disclosure to an attacker
	   outweighs this.  */
	printf ("error 0 no such user %s in CVSROOT/passwd\n", username);
	fflush(stdout);


	/* I'm doing this manually rather than via error_exit ()
	   because I'm not sure whether we want to call server_cleanup.
	   Needs more investigation....  */

#ifdef SYSTEM_CLEANUP
	/* Hook for OS-specific behavior, for example socket subsystems on
	   NT and OS2 or dealing with windows and arguments on Mac.  */
	SYSTEM_CLEANUP ();
#endif
	CCvsgui::Close(EXIT_FAILURE);
	exit (EXIT_FAILURE);
    }
    else
    {
	/* Something strange happened.  We don't know what it was, but
	   we certainly won't grant authorization. */
	TRACE(3,"Strange authentication branch reached.  Failing");
	host_user = NULL;
        goto handle_return;
    }

handle_return:

    if (host_user)
    {
        /* Set CVS_Username here, in allocated space. 
           It might or might not be the same as host_user. */
        CVS_Username = xstrdup(username);
    }

    return host_user;
}

/* Read username and password from client (i.e., stdin).
   If correct, then switch to run as that user and send an ACK to the
   client via stdout, else send NACK and die. */
void server_authenticate_connection ()
{
    char *tmp = NULL;
	void *user_token = NULL;
    char *host_user;
    const char *real_repository = NULL;

	/* Make sure the protocol starts off on the right foot... */
	if(io_getline(server_io_socket, &tmp, PATH_MAX)<0)
	/* FIXME: what?  We could try writing error/eof, but chances
	are the network connection is dead bidirectionally.  log it
	somewhere?  */
		TRACE(3,"io_getline failed");
	/* We want errors to go over the server protocol channel, so create a fake
		io buffer */
	buf_from_net = client_protocol_buffer_initialize (NULL, 1, outbuf_memory_error);
	buf_to_net = client_protocol_buffer_initialize (NULL, 0, outbuf_memory_error);

	TRACE(3,"Client sent '%s'",tmp);

	bool badauth;
	CProtocolLibrary lib;

	server_protocol = lib.FindProtocol(tmp,badauth,server_io_socket,encryption_level==4,&temp_protocol);
	if(badauth)
		goto i_hate_you;

	if(!server_protocol)
	{
		TRACE(3,"Couldn't find a matching authentication protocol");
		error (1, 0, "bad auth protocol start: %s", tmp);
	}
	xfree (tmp);
	temp_protocol = NULL;
	
	TRACE(3,"Authentication protocol :%s: returned user %s",PATCH_NULL(server_protocol->name),PATCH_NULL(server_protocol->auth_username));

#ifdef _WIN32
	win32_sanitize_username((const char **)&server_protocol->auth_username);
#endif

	/* If we're now in 'wrap' mode, setup a wrap buffer for the relevant I/O */
	((client_protocol_data*)buf_from_net->closure)->protocol=server_protocol;
	((client_protocol_data*)buf_to_net->closure)->protocol=server_protocol;

	if(server_protocol->auth_repository)
	{
		// If no repository is sent, we can't work out where the passwd, config, etc. file
		//   are until the first 'Root' command
		const root_allow_struct *found_root = NULL;

		if (!root_allow_ok (server_protocol->auth_repository,found_root,real_repository,true))
		{
			printf ("error 0 %s: no such repository\n", server_protocol->auth_repository);
			fflush(stdout);

			CServerIo::log(CServerIo::logAuth,"login failure for %s on %s", server_protocol->auth_username, server_protocol->auth_repository);
			goto i_hate_you;
		}

		if(!found_root->online)
		{
			printf ("error 0 %s: repository is offline\n", server_protocol->auth_repository);
			fflush(stdout);

			CServerIo::log(CServerIo::logAuth,"login failure for %s on %s", server_protocol->auth_username, server_protocol->auth_repository);
			goto i_hate_you;
		}

		// OK, now parse the config file, so we can use it to control how
		//   to check passwords.  If there was an error parsing the config
		//   file, parse_config already printed an error.  We keep going.
		//   Why?  Because if we didn't, then there would be no way to check
		//   in a new CVSROOT/config file to fix the broken one! 
		parse_config (real_repository);

		// We need the real cleartext before we hash it. 
		host_user = check_password (server_protocol->auth_username, server_protocol->auth_password,
									real_repository, &user_token);
	}
	else 
	{
		if(server_protocol->auth_username)
			host_user = xstrdup(server_protocol->auth_username);
	}

    if (host_user == NULL)
    {
		TRACE(3,"Host user not set - login fail");
		CServerIo::log(CServerIo::logAuth,"login failure for %s on %s", server_protocol->auth_username, server_protocol->auth_repository);
    i_hate_you:
		TRACE(3,"I HATE YOU");
		printf ("I HATE YOU\n");
		fflush (stdout);

	/* Don't worry about server_cleanup, server_active isn't set
	   yet.  */
		error_exit ();
    }
    TRACE(3,"Host user is %s",host_user);

#ifdef _WIN32
	// Statistics tracking
	int usercount,trash;
	if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","UserCount",usercount))
		usercount=0;
	if(CGlobalSettings::GetGlobalValue("cvsnt","UserCache",server_protocol->auth_username,trash))
	{
		if(!CGlobalSettings::SetGlobalValue("cvsnt","UserCache",server_protocol->auth_username,1))
		{
			usercount++;
			CGlobalSettings::SetGlobalValue("cvsnt","PServer","UserCount",usercount);
		}
	}
#endif

	/* Switch to run as this user. */
    if(runas_user && *runas_user)
		switch_to_user (runas_user, 1, NULL);
    else
		switch_to_user (host_user, 0, user_token);
    xfree (host_user);

    TRACE(3,"I LOVE YOU");
    printf ("I LOVE YOU\n");
    fflush (stdout);
    
	if(!CVS_Username)
	{
		if(host_user)
			CVS_Username = xstrdup(host_user);
		else
			CVS_Username = xstrdup(getcaller());
	}

    /* Don't go any farther if we're just responding to "cvs login". */
	/* Moved from above as I'm not sure it makes sense to be able to
	   login then not do anything (because you don't have a valid
	   alias) */
    if (server_protocol->verify_only)
    {
#ifdef SYSTEM_CLEANUP
	/* Hook for OS-specific behavior, for example socket subsystems on
	   NT and OS2 or dealing with windows and arguments on Mac.  */
	SYSTEM_CLEANUP ();
#endif
	CCvsgui::Close(0);

	exit (0);
    }
}

#endif /* SERVER_SUPPORT  */

#endif /* SERVER_SUPPORT */ 

/* This global variable is non-zero if the user requests encryption on
   the command line.  */
int cvsencrypt;

/* This global variable is non-zero if the user requests authentication on
   the command line.  */
int cvsauthenticate;

/* An buffer interface.  This is built on top of a
   packetizing buffer.  */

/* This structure is the closure field of the protocol wrapping.  */

struct cvs_encrypt_wrap_data
{
	int encrypt; /* 1 = wrap & encrypt, 0 = wrap only */
	const struct protocol_interface *protocol;
};

struct cvs_protocol_wrap_data
{
	char prefix; /* 'E' or 'M' */
	int last_was_linefeed;
};

static int cvs_encrypt_wrap_input(void *, const char *, char *, int);
static int cvs_encrypt_wrap_output(void *, const char *, char *, int, int *);

static int cvs_protocol_wrap_output(void *, const char *, char *, int, int *);

/* Create a protocol wrapping buffer.  We use a packetizing buffer. */

struct buffer *cvs_encrypt_wrap_buffer_initialize (const struct protocol_interface *protocol, struct buffer *buf,
     int input, int encrypt, void (*memory)(struct buffer *))
{
    struct cvs_encrypt_wrap_data *ed;

    ed = (struct cvs_encrypt_wrap_data *) xmalloc (sizeof *ed);
    ed->encrypt = encrypt;
	ed->protocol = protocol;

    return (packetizing_buffer_initialize
            (buf,
             input ? cvs_encrypt_wrap_input : NULL,
             input ? NULL : cvs_encrypt_wrap_output,
             ed,
             memory));
}

/* Unwrap data using GSSAPI.  */

static int cvs_encrypt_wrap_input (void *fnclosure, const char *input, char *output, int size)
{
    struct cvs_encrypt_wrap_data *ed =
        (struct cvs_encrypt_wrap_data *) fnclosure;
	const struct protocol_interface *protocol = ed->protocol; 
	int newsize;

	if(protocol && protocol->wrap)
	{
		if(protocol->wrap(protocol, 1, ed->encrypt, input, size, output, &newsize))
		{
			error(1, 0, "cvs_encrypt_wrap_input failed: error");
		}

		if (newsize > size)
		{
			error(1, 0, "cvs_encrypt_wrap_input failed: newsize > size");
		}
	}
	else
	{
		/* We're being called from 'cvs server', which means encryption is being handled by
		   an external protocol (probably ssh).  Just act as a passthrough. */
		memcpy(output,input,size);
	}

    return 0;
}

/* Wrap data using GSSAPI.  */

static int
cvs_encrypt_wrap_output (void *fnclosure, const char *input, char *output, int size, int *translated)
{
    struct cvs_encrypt_wrap_data *ed =
        (struct cvs_encrypt_wrap_data *) fnclosure;
	const struct protocol_interface *protocol = ed->protocol; 
	int newsize;


	if(protocol)
	{
		if(protocol->wrap(protocol, 0, ed->encrypt, input, size, output, &newsize))
		{
			error(1, 0, "cvs_encrypt_wrap_output failed: error");
		}

		if (newsize > size + 100)
		{
			error(1, 0, "cvs_encrypt_wrap_output failed: newsize > size + 100");
		}
	}
	else
	{
		/* External protocol - just passthrough */
		memcpy(output,input,size);
		newsize=size;
	}

    *translated = newsize;

    return 0;
}

struct buffer *cvs_protocol_wrap_buffer_initialize (struct buffer *buf, char prefix)
{
    struct cvs_protocol_wrap_data *ed;

    ed = (struct cvs_protocol_wrap_data *) xmalloc (sizeof *ed);
    ed->prefix=prefix;
	ed->last_was_linefeed = 1;

    return (nonpacketizing_buffer_initialize
            (buf,
             NULL,
             cvs_protocol_wrap_output,
             ed,
             NULL));
}

static void cvs_protocol_wrap_set_buffer(struct buffer *buf, struct buffer *wrap)
{
	packetizing_buffer_set_wrap(buf,wrap);
}

static int cvs_protocol_wrap_output (void *fnclosure, const char *input,
			char *output, int size, int *translated)
{
    struct cvs_protocol_wrap_data *ed = (struct cvs_protocol_wrap_data *) fnclosure;
	const char *p=input;
	char *q=output;
	int lf = ed->last_was_linefeed;

	for(; p<input+size; p++,q++)
	{
		if(lf)
		{
			*(q++)=ed->prefix;
			*(q++)=' ';
			lf=0;
		}
		*q=*p;
		if(*p=='\n')
			lf = 1;

		if((q-output) > size + PACKET_SLOP)
		{
			abort(); // Can't print an error here as we're in the output calling path
			//error(1,0, "cvs_protocol_wrap_output overflowed the output buffer");
		}
	}
	ed->last_was_linefeed = lf;

    *translated = q-output;

    return 0;
}

#ifdef SERVER_SUPPORT
int cvs_output_raw(const char *str, size_t len, bool flush)
{
    if (len == 0)
		len = strlen (str);
	if(!len)
		return 0;

	if(temp_protocol && temp_protocol->server_write_data)
		len = temp_protocol->server_write_data(temp_protocol,str,len);
    else if (server_active)
	{
		buf_output (buf_to_net, str, len);
		if(flush) buf_flush(buf_to_net,1);
	}
	else
	{
		error(1,0,"cvs_output_raw called on local connection");
	}
	return len;
}
#endif

/* Output LEN bytes at STR.  If LEN is zero, then output up to (not including)
   the first '\0' byte.  */

int cvs_output (const char *str, size_t len)
{
	cvs_flusherr();
    if (len == 0)
		len = strlen (str);
	if(!len)
		return 0;
#ifdef SERVER_SUPPORT
	if(temp_protocol && temp_protocol->server_write_data)
		len = temp_protocol->server_write_data(temp_protocol,str,len?len:strlen(str));
    else if (server_active)
    {
		char *ostr=NULL;
		size_t olen=0;
		if(!server_codepage_sent && default_client_codepage)
		{
			// This isn't particularly efficient, especially for some uses of cvs output, but
			// is probably the only way if the client can't be manually set to use something common
			// like utf8.
			const char *server_codepage;
#if defined(_WIN32) && defined(_UNICODE)
			if(win32_global_codepage==CP_UTF8)
				server_codepage="UTF-8";
			else
#endif
				server_codepage = CCodepage::GetDefaultCharset();
			int ret = CCodepage::TranscodeBuffer(server_codepage,default_client_codepage,str,len,(void*&)ostr,olen);
			// Note that whilst it would seem nice to warn the user here, any such warning would
			// end up recursively calling back into this function, plus it gets called in the middle
			// of protocol calls and sticking 'E xxxx' in the middle stands a good chance of breaking it totally.
			if(ret>0)
			{
				CServerIo::trace(3,"Translation from server codepage '%s' to client codepage '%s' lost characters",server_codepage, default_client_codepage);
				// Should we disable here?  Either client codepage is wrong, or no translation is possible (which could be solved by putting the server in utf8).
			}
			else if(ret<0)
			{
				CServerIo::trace(3,"Translation between '%s' and '%s' not possible - disabling",server_codepage, default_client_codepage);
				server_codepage_sent=true; // Slight cheat, but it has the effect of disabling further translation
			}
			str=ostr;
			len=olen;
		}
 		buf_output (stdout_buf?stdout_buf:buf_to_net, str, len);
 		if(str[len-1]=='\n')
 			buf_send_output(stdout_buf?stdout_buf:buf_to_net);
		if(ostr) xfree(ostr);
    }
	else
#endif
	{
#if defined(_WIN32) && !defined(CVS95)
	// Convert the UTF8 string to Unicode/ANSI for console output
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		DWORD NumberOfCharsWritten;
		if(GetFileType(hConsole)==FILE_TYPE_CHAR)
		{
			int wlen = (len)*4;
			wchar_t *wstr = (wchar_t*)xmalloc(wlen+4);
			wlen = MultiByteToWideChar(win32_global_codepage,0,str,len,wstr,wlen);
			WriteConsoleW(hConsole, wstr, wlen, &NumberOfCharsWritten, NULL);
			xfree(wstr);
			return len;
		}
#endif
		if(CCvsgui::Active())
			len = CCvsgui::write(str,len,false,false);
		else
		{
			size_t written;
			size_t to_write = len;
			const char *p = str;

			/* For symmetry with cvs_outerr we would call fflush (stderr)
			here.  I guess the assumption is that stderr will be
			unbuffered, so we don't need to.  That sounds like a sound
			assumption from the manpage I looked at, but if there was
			something fishy about it, my guess is that calling fflush
			would not produce a significant performance problem.  */
			while (to_write > 0)
			{
				written = fwrite (p, 1, to_write, stdout);
				if (written == 0)
				break;
				p += written;
				to_write -= written;
			}
			fflush(stdout);
		}
    }
	return len;
}

#ifdef SERVER_SUPPORT
int cvs_no_translate_begin()
{
	if(!server_active)
		return 0;
	if(supported_response("NoTranslateBegin"))
		buf_output0 (buf_to_net, "NoTranslateBegin\n");
	return 0;
}

int cvs_no_translate_end()
{
	if(!server_active)
		return 0;
	if(supported_response("NoTranslateEnd"))
		buf_output0 (buf_to_net, "NoTranslateEnd\n");
	return 0;
}
#endif

/* Output LEN bytes at STR in binary mode.  If LEN is zero, then
   output zero bytes.  */

int cvs_output_binary (char *str, size_t len)
{
	cvs_flusherr();
#ifdef SERVER_SUPPORT
    if (server_active)
    {
		char size_text[40];

		if (!supported_response ("Mbinary"))
		{
			error (0, 0, "\
	this client does not support writing binary files to stdout");
			return 0;
		}

		buf_output0 (buf_to_net, "Mbinary\n");
		sprintf (size_text, "%lu\n", (unsigned long) len);
		buf_output0 (buf_to_net, size_text);

		/* Not sure what would be involved in using buf_append_data here
		   without stepping on the toes of our caller (which is responsible
		   for the memory allocation of STR).  */
		buf_output (buf_to_net, str, len);
		return len;
    }
	else if(temp_protocol && temp_protocol->server_write_data)
		return temp_protocol->server_write_data(temp_protocol,str,len?len:strlen(str));
    else
#endif
	if(CCvsgui::Active())
		return CCvsgui::write(str,len,false,true);
	else
    {
	size_t written;
	size_t to_write = len;
	const char *p = str;

	/* For symmetry with cvs_outerr we would call fflush (stderr)
	   here.  I guess the assumption is that stderr will be
	   unbuffered, so we don't need to.  That sounds like a sound
	   assumption from the manpage I looked at, but if there was
	   something fishy about it, my guess is that calling fflush
	   would not produce a significant performance problem.  */
#ifdef USE_SETMODE_STDOUT
	int oldmode;

	/* It is possible that this should be the same ifdef as
	   USE_SETMODE_BINARY but at least for the moment we keep them
	   separate.  Mostly this is just laziness and/or a question
	   of what has been tested where.  Also there might be an
	   issue of setmode vs. _setmode.  */
	/* The Windows doc says to call setmode only right after startup.
	   I assume that what they are talking about can also be helped
	   by flushing the stream before changing the mode.  */
	fflush (stdout);
	oldmode = _setmode (_fileno (stdout), OPEN_BINARY);
	if (oldmode < 0)
	    error (0, errno, "failed to setmode on stdout");
#endif

	while (to_write > 0)
	{
	    written = fwrite (p, 1, to_write, stdout);
	    if (written == 0)
		break;
	    p += written;
	    to_write -= written;
	}
#ifdef USE_SETMODE_STDOUT
	fflush (stdout);
	if (_setmode (_fileno (stdout), oldmode) != OPEN_BINARY)
	    error (0, errno, "failed to setmode on stdout");
#endif
	return len - to_write;
    }
}

/* Like CVS_OUTPUT but output is for stderr not stdout.  */

int cvs_outerr (const char *str, size_t len)
{
	/* Make sure that output appears in order if stdout and stderr
	   point to the same place.  For the server case this is taken
	   care of by the fact that saved_outerr always holds less
	   than a line.  */
	cvs_flushout();

    if (len == 0)
		len = strlen (str);
	if(!len)
		return 0;

#ifdef SERVER_SUPPORT
    if(temp_protocol && temp_protocol->server_write_data)
	return temp_protocol->server_write_data(temp_protocol,str,len);
    else if (server_active && (stderr_buf || buf_to_net))
    {
		char *ostr=NULL;
		size_t olen=0;
		if(!server_codepage_sent && default_client_codepage)
		{
			// This isn't particularly efficient, especially for some uses of cvs output, but
			// is probably the only way if the client can't be manually set to use something common
			// like utf8.
			const char *server_codepage;
#if defined(_WIN32) && defined(_UNICODE)
			if(win32_global_codepage==CP_UTF8)
				server_codepage="UTF-8";
			else
#endif
				server_codepage = CCodepage::GetDefaultCharset();
			int ret = CCodepage::TranscodeBuffer(server_codepage,default_client_codepage,str,len,(void*&)ostr,olen);
			// Note that whilst it would seem nice to warn the user here, any such warning would
			// end up recursively calling back into this function, plus it gets called in the middle
			// of protocol calls and sticking 'E xxxx' in the middle stands a good chance of breaking it totally.
			if(ret>0)
			{
				CServerIo::trace(3,"Translation from server codepage '%s' to client codepage '%s' lost characters",server_codepage, default_client_codepage);
				// Should we disable here?  Either client codepage is wrong, or no translation is possible (which could be solved by putting the server in utf8).
			}
			else if(ret<0)
			{
				CServerIo::trace(3,"Translation between '%s' and '%s' not possible - disabling",server_codepage, default_client_codepage);
				server_codepage_sent=true; // Slight cheat, but it has the effect of disabling further translation
			}
			str=ostr;
			len=olen;
		}

 		buf_output (stderr_buf?stderr_buf:buf_to_net, str, len);
 		if(str[len-1]=='\n')
 			buf_send_output(stderr_buf?stderr_buf:buf_to_net);

		if(ostr) xfree(ostr);

	}
	else
#endif
	{
#if defined(_WIN32) && !defined(CVS95)
		// Convert the UTF8 string to Unicode/ANSI for console output
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		DWORD NumberOfCharsWritten;
		if(GetFileType(hConsole)==FILE_TYPE_CHAR)
		{
			int wlen = (len)*4;
			wchar_t *wstr = (wchar_t*)xmalloc(wlen+4);
			wlen = MultiByteToWideChar(win32_global_codepage,0,str,len,wstr,wlen);
			WriteConsoleW(hConsole, wstr, wlen, &NumberOfCharsWritten, NULL);
			xfree(wstr);
			return len;
		}
#endif
		if(CCvsgui::Active())
			return CCvsgui::write(str,len,true,false);
		else
		{
			size_t written;
			size_t to_write = len;
			const char *p = str;

			while (to_write > 0)
			{

				written = fwrite (p, 1, to_write, stderr);
				if (written == 0)
				break;
				p += written;
				to_write -= written;
			}
			return len - to_write;
		}
	}
	return len;
}

/* Flush stderr.  stderr is normally flushed automatically, of course,
   but this function is used to flush information from the server back
   to the client.  */

void
cvs_flusherr ()
{
#ifdef SERVER_SUPPORT
    if (server_active)
    {
	/* skip the actual stderr flush in this case since the parent process
	 * on the server should only be writing to stdout anyhow
	 */
	/* Flush what we can to the network, but don't block.  */
	if(stderr_buf)
		buf_flush (stderr_buf, 0);
    }
	else if(temp_protocol && temp_protocol->server_flush_data)
		temp_protocol->server_flush_data(temp_protocol);
    else
#endif
	fflush (stderr);
}

/* Make it possible for the user to see what has been written to
   stdout (it is up to the implementation to decide exactly how far it
   should go to ensure this).  */

void
cvs_flushout ()
{
#ifdef SERVER_SUPPORT
	if(temp_protocol && temp_protocol->server_flush_data)
		temp_protocol->server_flush_data(temp_protocol);
    else if (server_active)
    {
		/* Flush what we can to the network, but don't block.  */
		if(stdout_buf)
			buf_flush (stdout_buf, 0);
    }
    else
#endif
		fflush (stdout);
}

/* Output TEXT, tagging it according to TAG.  There are lots more
   details about what TAG means in cvsclient.texi but for the simple
   case (e.g. non-client/server), TAG is just "newline" to output a
   newline (in which case TEXT must be NULL), and any other tag to
   output normal text.

   Note that there is no way to output either \0 or \n as part of TEXT.  */

void cvs_output_tagged (const char *tag, const char *text)
{
    if (text != NULL && strchr (text, '\n') != NULL)
	/* Uh oh.  The protocol has no way to cope with this.  For now
	   we dump core, although that really isn't such a nice
	   response given that this probably can be caused by newlines
	   in filenames and other causes other than bugs in CVS.  Note
	   that we don't want to turn this into "MT newline" because
	   this case is a newline within a tagged item, not a newline
	   as extraneous sugar for the user.  */
	assert (0);

    /* Start and end tags don't take any text, per cvsclient.texi.  */
    if (tag[0] == '+' || tag[0] == '-')
		assert (text == NULL);

#ifdef SERVER_SUPPORT
    if (server_active && supported_response ("MT"))
    {
		buf_output0 (buf_to_net, "MT ");
		buf_output0 (buf_to_net, tag);
		if (text != NULL)
		{
			buf_output (buf_to_net, " ", 1);
			buf_output0 (buf_to_net, text);
		}
		buf_output (buf_to_net, "\n", 1);
    }
    else
#endif
    {
		if (strcmp (tag, "newline") == 0)
			cvs_output ("\n", 1);
		else if (text != NULL)
			cvs_output (text, 0);
    }
}

#if defined(SERVER_SUPPORT)
int server_main(const char *cmd_name, int (*command)(int argc, char **argv))
{
	int exitstatus = -1;

	TRACE(3,"server_main started");
	/*
	 * Set this in .bashrc if you want to give yourself time to attach
	 * to the subprocess with a debugger.
	 */
	if (CProtocolLibrary::GetEnvironment ("CVS_SERVER_SLEEP"))
	{
	    int secs = atoi (CProtocolLibrary::GetEnvironment ("CVS_SERVER_SLEEP"));
	    sleep (secs);
	}

	server_command_name = cmd_name;

	precommand_args_t args;
	args.argc = argument_count-1;
	args.argv = (const char **)argument_vector+1;
	args.command = server_command_name;
	args.retval = 0;
	TRACE(3,"run precommand proc server");
	if(run_trigger(&args, precommand_proc))
	{
		error (0, 0, "Pre-command check failed");
		exitstatus = 1;
	}
	else
		exitstatus = (*command) (argument_count, argument_vector);

	TRACE(3,"run postcommand proc server");
	args.retval = exitstatus;
	run_trigger(&args, postcommand_proc);
	xfree(last_repository);

	return exitstatus;
}

/* Send the request off to the proxy server instead */
int proxy_main(const char *cmd_name, int (*command)(int argc, char **argv))
{
	int exitstatus = -1;

	TRACE(3,"proxy_main started");
	/*
	 * Set this in .bashrc if you want to give yourself time to attach
	 * to the subprocess with a debugger.
	 */
	if (CProtocolLibrary::GetEnvironment ("CVS_SERVER_SLEEP"))
	{
	    int secs = atoi (CProtocolLibrary::GetEnvironment ("CVS_SERVER_SLEEP"));
	    sleep (secs);
	}


	server_command_name = cmd_name;

	cvsroot *oldroot = current_parsed_root;

	/* Construct a root as if we were the client */
	TRACE(3,"proxy main -Construct a root as if we were the client- phys repos=(%s,%s,%s)."
				,PATCH_NULL(oldroot->directory)
				,PATCH_NULL(oldroot->proxy_repository_root)
				,PATCH_NULL(oldroot->unparsed_directory));

	current_parsed_root = new_cvsroot_t();
	current_parsed_root->original = xstrdup(oldroot->directory);
	current_parsed_root->directory = xstrdup(oldroot->unparsed_directory);
	//current_parsed_root->directory = xstrdup(oldroot->proxy_repository_root);
	current_parsed_root->unparsed_directory = xstrdup(oldroot->unparsed_directory);
	current_parsed_root->isremote = true;
	strcpy(current_parsed_root->method,"sync");
	current_parsed_root->hostname = xstrdup(oldroot->remote_server);
	if(!strncmp(oldroot->remote_passphrase,"$1$",3))
		current_parsed_root->password = xstrdup(oldroot->remote_passphrase);
	else
	{
		CCrypt crypt;
		current_parsed_root->password = xstrdup(crypt.crypt(oldroot->remote_passphrase));
	}
	current_parsed_root->username = xstrdup(CVS_Username);
	current_parsed_root->optional_1 = xstrdup(oldroot->unparsed_directory);
	current_parsed_root->optional_2 = xstrdup(CVS_Username);
	current_parsed_root->optional_3 = xstrdup("proxy");
	current_parsed_root->remote_repository = xstrdup(oldroot->remote_repository);
	current_parsed_root->proxy_repository_root = xstrdup(oldroot->proxy_repository_root);

	CProtocolLibrary lib;
	client_protocol = lib.LoadProtocol("sync");
	if(!client_protocol)
	{
		error(0, 0, "Sync protocol not available.  Cannot start proxy");
		exitstatus = 1;
		goto finish_command;
	}

	lib.SetupServerInterface(current_parsed_root, server_io_socket);

	precommand_args_t args;
	args.argc = argument_count-1;
	args.argv = (const char **)argument_vector+1;
	args.command = server_command_name;
	args.retval = 0;
	if(have_local_root)
	{
		TRACE(3,"run precommand proc server");
		if(run_trigger(&args, precommand_proc))
		{
			error (0, 0, "Pre-command check failed");
			exitstatus = 1;
			goto finish_command;
		}
	}

	start_server(2);
	proxy_active = 1;

	/* Run the command in client mode */
	/* The condition server_active == true and current_parsed_root->isremote == true is unique to the proxy */
	exitstatus = (*command) (argument_count, argument_vector);

finish_command:
	TRACE(3,"proxy main - change current parsed root to the 'original' one.");
	current_parsed_root = oldroot;
	lib.SetupServerInterface(current_parsed_root, server_io_socket);

	if(have_local_root)
	{
		args.retval = exitstatus;
		TRACE(3,"run postcommand proc server (proxy)");
		run_trigger(&args, postcommand_proc);
		xfree(last_repository);
	}

	return exitstatus;
}

#endif /* SERVER_SUPPORT */

char *normalise_options(const char *options, int quiet, const char *file)
{
#ifdef SERVER_SUPPORT
	char *o;
	const kflag_t *p;
	const char *v = valid_rcsoptions;
	char *temp_options, *tmp;

	if(!server_active)
#endif
		return xstrdup(options);
#ifdef SERVER_SUPPORT

	if(!options || !*options)
		return NULL;

	if(!v)
	{
		if(supported_response("EntriesExtra")) /* This is a hack, but basically any CVSNT client will support this,
							so you can add things like unicode and -kL support */
			v="butkvloL";
		else
			v="bkvlo";
	}

	temp_options = xstrdup(options);
	tmp=temp_options;
	if(*tmp=='{' && !strchr(tmp,'}'))
	{
		/* Can't really process this, but it's better not to fall over... */
		tmp++;
	}
	if(*tmp=='{' && !strchr(v,'{'))
	{
		if(!quiet)
		{
			error(0,0,"`%s' is marked with an expanded encoding {}.",file);
			error(0,0,"Your client cannot understand this option");
			error(0,0,"so the checked out file will not be an");
			error(0,0,"accurate representation of the original");
			error(0,0,"file.  Please contact the repository");
			error(0,0,"administrator if you think that this is");
			error(0,0,"in error");
		}

		strcpy(tmp,strchr(tmp,'}')+1);
	}
	else if(*tmp=='{')
		tmp=strchr(tmp,'}')+1;
	for(p=kflag_encoding;p->flag;p++)
	{
		if((p->type&KFLAG_LEGACY) || (p->type&KFLAG_INTERNAL)) 
			continue;
		o=strchr(tmp,p->flag);
		if(o && !strchr(v,p->flag))
		{
			if(!quiet && p->type&KFLAG_ESSENTIAL)
			{
				error(0,0,"`%s' is marked with expansion option `%c'.",file,p->flag);
				error(0,0,"Your client cannot understand this option");
				error(0,0,"so the checked out file will not be an");
				error(0,0,"accurate representation of the original");
				error(0,0,"file.  Please contact the repository");
				error(0,0,"administrator if you think that this is");
				error(0,0,"in error");
			}
			if(p->alternate)
				*o=p->alternate;
			else
				strcpy(o,o+1);
		}
	}
	for(p=kflag_flags;p->flag;p++)
	{
		if((p->type&KFLAG_LEGACY) || (p->type&KFLAG_INTERNAL)) 
			continue;
		o=strchr(tmp,p->flag);
		if(o && !strchr(v,p->flag))
		{
			if(!quiet && p->type&KFLAG_ESSENTIAL)
			{
				error(0,0,"`%s' is marked with expansion option `%c'.",file,p->flag);
				error(0,0,"Your client cannot understand this option");
				error(0,0,"so the checked out file will not be an");
				error(0,0,"accurate representation of the original");
				error(0,0,"file.  Please contact the repository");
				error(0,0,"administrator if you think that this is");
				error(0,0,"in error");
			}
			if(p->alternate)
				*o=p->alternate;
			else
				strcpy(o,o+1);
		}
	}
	if(!*temp_options)
		xfree(temp_options); /* None left */
	return temp_options;
#endif /* Server_support */
}

#ifdef SERVER_SUPPORT
void server_send_baserev(struct file_info *finfo, const char *basefile, const char *type)
{
	FILE *f;
	unsigned long len;
	buf_output0(buf_to_net,"Update-baserev ");
	output_dir (finfo->update_dir, finfo->repository);
	server_buf_output0(buf_to_net,finfo->file);
	buf_output0(buf_to_net,"\n");
	buf_output0(buf_to_net,type);
	buf_output0(buf_to_net,"\n");
	if(type[0]=='U')
	{
		f = fopen(basefile,"rb");
		if(!f)
			buf_output0(buf_to_net,"0\n");
		else
		{
			char tmp[BUFSIZ];
			fseek(f,0,SEEK_END);
			len = (unsigned long)ftell(f);
			fseek(f,0,0);
			snprintf(tmp,sizeof(tmp),"%lu\n",len);
			buf_output0(buf_to_net,tmp);
			while((len=fread(tmp,1,sizeof(tmp),f))>0)
				buf_output(buf_to_net,tmp,(int)len);
			fclose(f);
		}
	}
}
#endif

int precommand_proc(void *param, const trigger_interface *cb)
{
	int ret = 0;
	precommand_args_t *args = (precommand_args_t*)param;

	TRACE(1,"precommand_proc()");

	if(cb->precommand)
		ret = cb->precommand(cb,args->argc,args->argv);
	return ret;
}

int postcommand_proc(void *param, const trigger_interface *cb)
{
	int ret = 0;
	precommand_args_t *args = (precommand_args_t*)param;

	TRACE(1,"postcommand_proc()");

	if(cb->postcommit && !strcmp(args->command,"commit"))
	{
		ret = cb->postcommit(cb,last_repository?Short_Repository(last_repository):"");
#if (CVSNT_SPECIAL_BUILD_FLAG != 0)
		TRACE(3,"commit server_active=%s, CVSNT_SPECIAL_BUILD=%s",(server_active)?"yes":"no",CVSNT_SPECIAL_BUILD);
#else
		TRACE(3,"commit server_active=%s",(server_active)?"yes":"no");
#endif
#ifdef CVSSPAM
#ifdef _WIN32
		if (server_active)
		{
#if (CVSNT_SPECIAL_BUILD_FLAG != 0)
			if (strcasecmp(CVSNT_SPECIAL_BUILD,"Suite")!=0)
#endif
			{
				error(0, 0, "Committed on the Free edition of March Hare Software CVSNT Server\n           Upgrade to CVS Suite for more features and support:\n           http://march-hare.com/cvsnt/");
			}
		}
#endif
#endif

	}
	if(!ret && cb->postcommand)
		ret = cb->postcommand(cb,last_repository?Short_Repository(last_repository):"",args->retval);
	return ret;
}

#ifdef SERVER_SUPPORT
// Wrap buf_read_line for lines which potentially have filenames in them.  Don't use for others as there's overhead to using this.
int server_read_line(struct buffer *buf, char **line, int *lenp)
{
	int ret = buf_read_line(buf,line,lenp);
	if(ret) return ret;

	if(!server_codepage_sent && default_client_codepage)
	{
		char *iline=NULL;
		size_t ilen=0;

		// This isn't particularly efficient,
		// is probably the only way if the client can't be manually set to use something common
		// like utf8.
		const char *server_codepage;
#if defined(_WIN32) && defined(_UNICODE)
		if(win32_global_codepage==CP_UTF8)
			server_codepage="UTF-8";
		else
#endif
			server_codepage = CCodepage::GetDefaultCharset();
		int ret = CCodepage::TranscodeBuffer(default_client_codepage,server_codepage,*line,lenp?*lenp:0,(void*&)iline,ilen);
		// Note that whilst it would seem nice to warn the user here, any such warning would
		// end up recursively calling back into this function, plus it gets called in the middle
		// of protocol calls and sticking 'E xxxx' in the middle stands a good chance of breaking it totally.
		if(ret>0)
		{
			CServerIo::trace(3,"Translation from client codepage '%s' to server codepage '%s' lost characters",default_client_codepage, server_codepage);
			// Should we disable here?  Either client codepage is wrong, or no translation is possible (which could be solved by putting the server in utf8).
		}
		else if(ret<0)
		{
			CServerIo::trace(3,"Translation between '%s' and '%s' not possible - disabling",default_client_codepage, server_codepage);
			server_codepage_sent=true; // Slight cheat, but it has the effect of disabling further translation
		}
		xfree(*line);
		*line=iline;
		if(lenp) *lenp=ilen;
	}
	return 0;
}

void server_buf_output0(buffer *buf, const char *string)
{
	return server_buf_output(buf,string,strlen(string));
}

void server_buf_output(buffer *buf, const char *data, int len)
{
	char *ostr=NULL;
	size_t olen=0;
	if(!server_codepage_sent && default_client_codepage)
	{
		// This isn't particularly efficient, especially for some uses of cvs output, but
		// is probably the only way if the client can't be manually set to use something common
		// like utf8.
		const char *server_codepage;
#if defined(_WIN32) && defined(_UNICODE)
		if(win32_global_codepage==CP_UTF8)
			server_codepage="UTF-8";
		else
#endif
			server_codepage = CCodepage::GetDefaultCharset();
		int ret = CCodepage::TranscodeBuffer(server_codepage,default_client_codepage,data,len,(void*&)ostr,olen);

		// Note that whilst it would seem nice to warn the user here, any such warning would
		// end up recursively calling back into this function, plus it gets called in the middle
		// of protocol calls and sticking 'E xxxx' in the middle stands a good chance of breaking it totally.
		if(ret>0)
		{
			CServerIo::trace(3,"Translation from server codepage '%s' to client codepage '%s' lost characters",server_codepage, default_client_codepage);
			// Should we disable here?  Either client codepage is wrong, or no translation is possible (which could be solved by putting the server in utf8).
		}
		else if(ret<0)
		{
			CServerIo::trace(3,"Translation between '%s' and '%s' not possible - disabling",server_codepage, default_client_codepage);
			server_codepage_sent=true; // Slight cheat, but it has the effect of disabling further translation
		}
		data=ostr;
		len=olen;
	}
	buf_output(buf,data,len);
	if(ostr) xfree(ostr);
}
#endif
