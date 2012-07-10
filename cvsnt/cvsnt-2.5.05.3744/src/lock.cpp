/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Set Lock
 * 
 * Lock file support for CVS.
 */

/* The node Concurrency in doc/cvs.texinfo has a brief introduction to
   how CVS locks function, and some of the user-visible consequences of
   their existence.  Here is a summary of why they exist (and therefore,
   the consequences of hacking CVS to read a repository without creating
   locks):

   There are two uses.  One is the ability to prevent there from being
   two writers at the same time.  This is necessary for any number of
   reasons (fileattr code, probably others).  Commit needs to lock the
   whole tree so that nothing happens between the up-to-date check and
   the actual checkin.

   The second use is the ability to ensure that there is not a writer
   and a reader at the same time (several readers are allowed).  Reasons
   for this are:

   * Readlocks ensure that once CVS has found a collection of rcs
   files using Find_Names, the files will still exist when it reads
   them (they may have moved in or out of the attic).

   * Readlocks provide some modicum of consistency, although this is
   kind of limited--see the node Concurrency in cvs.texinfo.

   * Readlocks ensure that the RCS file does not change between
   RCS_parse and RCS_reparsercsfile time.  This one strikes me as
   important, although I haven't thought up what bad scenarios might
   be.

   * Readlocks ensure that we won't find the file in the state in
   which it is in between the calls to add_rcs_file and RCS_checkin in
   commit.c (when a file is being added).  This state is a state in
   which the RCS file parsing routines in rcs.c cannot parse the file.

   * Readlocks ensure that a reader won't try to look at a
   half-written fileattr file (fileattr is not updated atomically).

   (see also the description of anonymous read-only access in
   "Password authentication security" node in doc/cvs.texinfo).

   While I'm here, I'll try to summarize a few random suggestions
   which periodically get made about how locks might be different:

   1.  Check for EROFS.  Maybe useful, although in the presence of NFS
   EROFS does *not* mean that the file system is unchanging.

   2.  Provide a means to put the cvs locks in some directory apart from
   the repository (CVSROOT/locks; a -l option in modules; etc.).

   3.  Provide an option to disable locks for operations which only
   read (see above for some of the consequences).

   4.  Have a server internally do the locking.  Probably a good
   long-term solution, and many people have been working hard on code
   changes which would eventually make it possible to have a server
   which can handle various connections in one process, but there is
   much, much work still to be done before this is feasible.

   5.  Like #4 but use shared memory or something so that the servers
   merely need to all be on the same machine.  This is a much smaller
   change to CVS (it functions much like #2; shared memory might be an
   unneeded complication although it presumably would be faster).  */

#include "cvs.h"

#ifdef _WIN32
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif

#include <stdarg.h>

struct lock {
    /* This is the directory in which we may have a lock named by the
       readlock variable, a lock named by the writelock variable, and/or
       a lock named CVSLCK.  The storage is not allocated along with the
       struct lock; it is allocated by the Reader_Lock caller or in the
       case of writelocks, it is just a pointer to the storage allocated
       for the ->key field.  */
    const char *repository;
    /* Do we have a lock named CVSLCK?  */
    int have_lckdir;
    /* Note there is no way of knowing whether the readlock and writelock
       exist.  The code which sets the locks doesn't use SIG_beginCrSect
       to set a flag like we do for CVSLCK.  */
};

static void remove_locks ();
static int readers_exist (const char *repository);
static int set_lock (struct lock *lock, int will_wait);
static void clear_lock (struct lock *lock);
#ifndef _WIN32
static void set_lockers_name (struct stat *statp);
#else
static void set_lockers_name (const char *file);
#endif
static int set_writelock_proc (Node * p, void *closure);
static int unlock_proc (Node * p, void *closure);
static int write_lock (struct lock *lock);
static void lock_simple_remove (struct lock *lock);
static void lock_wait (const char *repository);
static void lock_obtained (const char *repository);

/* Malloc'd array containing the username of the whoever has the lock.
   Will always be non-NULL in the cases where it is needed.  */
static char *lockers_name;
/* Malloc'd array specifying name of a readlock within a directory.
   Or NULL if none.  */
static char *readlock;
/* Malloc'd array specifying name of a writelock within a directory.
   Or NULL if none.  */
static char *writelock;
/* Malloc'd array specifying the name of a CVSLCK file (absolute pathname).
   Will always be non-NULL in the cases where it is used.  */
static char *masterlock;
static List *locklist;

#define L_OK		0		/* success */
#define L_ERROR		1		/* error condition */
#define L_LOCKED	2		/* lock owned by someone else */

/* This is the (single) readlock which is set by Reader_Lock.  The
   repository field is NULL if there is no such lock.  */
static struct lock global_readlock;

/* List of locks set by lock_tree_for_write.  This is redundant
   with locklist, sort of.  */
static List *lock_tree_list;

/* If we set locks with lock_dir_for_write, then locked_dir contains
   the malloc'd name of the repository directory which we have locked.
   locked_list is the same thing packaged into a list and is redundant
   with locklist the same way that lock_tree_list is.  */
static char *locked_dir;
static List *locked_list;

/* LockDir/Server from CVSROOT/config.  */
char *lock_dir;
char *lock_server;

int lock_server_socket = -1;

static char *lock_name (const char *repository, const char *name);

static int lock_server_command(char *line, int line_len, const char *cmd, ...);

void lock_register_client(const char *username, const char *root)
{
	char line[1024];

	/* Using file locks for some unknown reason */
	if(!lock_server)
		return;

	if(lock_server_socket == -1)
	{
		const char *serv;
		char *_root = xstrdup(root);
		const char *roo = normalize_path(_root);

		/* First time - connect to lock server & register ourselves */
		char *p=strchr(lock_server,':');
		int l;
		if(p) *(p++)='\0';
		if(!strcmp(lock_server,"localhost"))
			serv = "127.0.0.1"; /* XP Home breakage */
		else
			serv = lock_server;
		TRACE(3,"Lock server connect to %s port %s",PATCH_NULL(serv),p?p:"2402");

		if((lock_server_socket=cvs_tcp_connect(serv,p?p:"2402", 0))<0)
			error(1,0,"Couldn't connect to lock server");
		if(p)
			*(--p)=':';
		if((l=recv(lock_server_socket,line,sizeof(line),0))<=0)
		{
			error(1,errno,"Error communicating with lock server");
		}
		do
		{
			line[l--]='\0';
		} while(l && isspace(line[l]));
		// Line now contains signon string, including protocol version. 

		{
			int ver1,ver2;
			if(!strchr(line,' ') || sscanf(strchr(line,' ')+1,"%d.%d",&ver1,&ver2)!=2)
			{
				error(1,0,"Lockserver returned invalid version string: %s",line+4);
			}
			if(ver1<2 || ver2<2)
			{
				error(1,0,"Invalid Lockserver version - got %d.%d, wanted 2.2",ver1,ver2);
			}
		}

		switch(lock_server_command(line,sizeof(line),"Client %s|%s|%s\n",username,roo,remote_host_name?remote_host_name:""))
		{
		case 0:
			break;
		default:
			error(1,0,"Couldn't authenticate with lock server: %s",line+4);
		}

		xfree(_root);
	}
}

static int lock_server_command(char *line, int line_len, const char *cmd, ...)
{
	int l;
	va_list va;

	va_start(va,cmd);
	vsnprintf(line,line_len,cmd,va);
	va_end(va);
	if(send(lock_server_socket,line,strlen(line),0)<=0)
	{
		error(1,errno,"Error communicating with lock server (send)");
	}
	if((l=recv(lock_server_socket,line,line_len,0))<=0)
	{
		error(1,errno,"Error communicating with lock server (recv)");
	}
	do
	{
		line[l--]='\0';
	} while(l && isspace(line[l]));
	if(!isdigit(line[0]) || !isdigit(line[1]) || !isdigit(line[2]) || line[3]!=' ')
		return 99; // Unknown
	return atoi(line);
}

/* Connect to the lock server and attain a lock */
static size_t do_lock_server(const char *object, const char *directory, const char *flags, int wait)
{
	char line[MAX_PATH*4],*p,*q, *ob = NULL;
	int bWaited;
	size_t id;
	unsigned helper;

	if(filenames_case_insensitive && object)
	{
		char *ob;
		
		if(directory)
		{
			ob = (char*)xmalloc(strlen(directory)+strlen(object)+10);
			sprintf(ob,"%s/%s",directory,object);
		}
		else
			ob = (char*)xstrdup(object);
		ob = normalize_path(ob);
		object = last_component(ob);
		if(object>ob)
		{
			((char*)object)[-1]='\0';
			directory=ob;
		}
		else if(directory)
			directory="";
	}

	bWaited = 0;
	do
	{
		time_t now;
		char tm[32];
	    time (&now);
		strncpy(tm,ctime(&now)+11,sizeof(tm));
		tm[8]='\0';
		switch(lock_server_command(line,sizeof(line),"Lock %s|%s%s%s\n",flags,directory?directory:"",directory?"/":"",object))
		{
		case 0:
		case 3:
			if(bWaited)
				error(0,0,"[%s] obtained lock in %s", tm, fn_root(object));
			p=strchr(line,'(');
			if(!p)
				error(1,0,"Lockserver did not return a lock ID.  Check version.");
			p++;
			q=strchr(p,')');
			if(!q)
				error(1,0,"Lockserver did not return a lock ID.  Check version.");
			*q='\0';
			sscanf(p,"%u",&helper);
			id = helper; /* 64 bit aware */
			xfree(ob);
			return id;
		case 1:
			error(1,0,"Failed to obtain lock on %s: %s",fn_root(object),line+4);
		case 2:
			if(!wait) /* In the case of no wait, return immediately */
			{
				xfree(ob);
				return 0;
			}
			else
			{
				char *owner, *host, *path;
				owner = strchr(line,'|');
				if(owner)
				{
					*(owner++)='\0';
					host = strchr(owner,'|');
				}
				else
					host = NULL;
				if(host)
				{
					*(host++)='\0';
					path = strchr(host,'|');
				}
				else
					path = NULL;

				if(path)
					*(path++)='\0';

				if(!(bWaited%5)) // No need to keep going on about it..
				{
					if(host && *host)
						error(0,0,"[%s] waiting for %s on %s's lock in %s", tm, owner, host, fn_root(path));
					else
						error(0,0,"[%s] waiting for %s's lock in %s", tm, owner, fn_root(path));
				}
				bWaited++;
				if(bWaited==20)
					error(1,0,"Failed to obtain lock on %s",fn_root(object));
			}
			break;
		default:
			error(1,0,"Unknown response from lock server: %s", line+4);
		}
		// Only get here in case 2 == WAIT
		sleep(1);
	} while(1);
	xfree(ob);
	return 0;
}

size_t do_lock_file(const char *file, const char *repository, int write, int wait)
{
	if(!lock_server)
		return (size_t)-1;
	return do_lock_server(file,repository,write?"Write":"Read", wait);
}

/* Cleanup lock(s) */
int do_unlock_file(size_t lockId)
{
 	char line[1024];

	if(!lock_server)
		return 0;

 	switch(lock_server_command(line,sizeof(line),"Unlock %u\n",lockId))
 	{
 	case 0:
 		break;
 	case 1:
 		return -1;
 	default:
 		error(1,0,"Unknown response from lock server: %s", line+4);
 	}
	return 0;
}

int do_lock_version(size_t lockId, const char *branch, char **version)
{
 	char line[1024],*p,*q;

	if(!lock_server)
	{
		if(version)
			*version=NULL;
		return 0;
	}

 	switch(lock_server_command(line,sizeof(line),"Version %u|%s\n",lockId,branch))
 	{
 	case 0:
		if(version)
		{
			p=strchr(line,'(');
			if(!p)
				*version=NULL;
			else
			{
				p++;
				q=strchr(p,')');
				if(!q)
					*version=NULL;
				else
				{
					*version=(char*)xmalloc((q-p)+1);
					strncpy(*version,p,q-p);
					(*version)[q-p]='\0';
				}
			}
		}
 		break;
 	case 1:
		error(1,0,"Version request failed: %s", line+4);
 		return -1;
 	default:
 		error(1,0,"Unknown response from lock server: %s", line+4);
 	}
	return 0;
}

/* Return a newly malloc'd string containing the name of the lock for the
   repository REPOSITORY and the lock file name within that directory
   NAME.  Also create the directories in which to put the lock file
   if needed (if we need to, could save system call(s) by doing
   that only if the actual operation fails.  But for now we'll keep
   things simple).  */
static char *lock_name (const char *repository,const char *name)
{
    char *retval;
    const char *p;
    char *q;
    const char *short_repos;
    mode_t save_umask;
    int saved_umask = 0;

    if (lock_dir == NULL)
    {
	/* This is the easy case.  Because the lock files go directly
	   in the repository, no need to create directories or anything.  */
	retval = (char*)xmalloc (strlen (repository) + strlen (name) + 10);
	(void) sprintf (retval, "%s/%s", repository, name);
    }
    else
    {
	struct stat sb;
	mode_t new_mode = 0;

	/* The interesting part of the repository is the part relative
	   to CVSROOT.  */
	assert (current_parsed_root != NULL);
	assert (current_parsed_root->directory != NULL);
	assert (fnncmp (repository, current_parsed_root->directory,
			 strlen (current_parsed_root->directory)) == 0);
	short_repos = repository + strlen (current_parsed_root->directory) + 1;

	if (fncmp (repository, current_parsed_root->directory) == 0)
	    short_repos = ".";
	else
	    assert (ISDIRSEP(short_repos[-1]));

	retval = (char*)xmalloc (strlen (lock_dir)
			  + strlen (short_repos)
			  + strlen (name)
			  + 10);
	strcpy (retval, lock_dir);
	q = retval + strlen (retval);
	*q++ = '/';

	strcpy (q, short_repos);

	/* In the common case, where the directory already exists, let's
	   keep it to one system call.  */
	if (CVS_STAT (retval, &sb) < 0)
	{
	    /* If we need to be creating more than one directory, we'll
	       get the existence_error here.  */
	    if (!existence_error (errno))
		error (1, errno, "cannot stat directory %s", retval);
	}
	else
	{
	    if (S_ISDIR (sb.st_mode))
		goto created;
	    else
		error (1, 0, "%s is not a directory", fn_root(retval));
	}

	/* Now add the directories one at a time, so we can create
	   them if needed.

	   The idea behind the new_mode stuff is that the directory we
	   end up creating will inherit permissions from its parent
	   directory (we re-set new_mode with each EEXIST).  CVSUMASK
	   isn't right, because typically the reason for LockDir is to
	   use a different set of permissions.  We probably want to
	   inherit group ownership also (but we don't try to deal with
	   that, some systems do it for us either always or when g+s is on).

	   We don't try to do anything about the permissions on the lock
	   files themselves.  The permissions don't really matter so much
	   because the locks will generally be removed by the process
	   which created them.  */

	if (CVS_STAT (lock_dir, &sb) < 0)
	    error (1, errno, "cannot stat %s", lock_dir);
	new_mode = sb.st_mode;
	save_umask = umask (0000);
	saved_umask = 1;

	p = short_repos;
	while (1)
	{
	    while (!ISDIRSEP (*p) && *p != '\0')
		++p;
	    if (ISDIRSEP (*p))
	    {
		strncpy (q, short_repos, p - short_repos);
		q[p - short_repos] = '\0';
		if (!ISDIRSEP (q[p - short_repos - 1])
		    && CVS_MKDIR (retval, new_mode) < 0)
		{
		    int saved_errno = errno;
		    if (saved_errno != EEXIST)
			error (1, errno, "cannot make directory %s", retval);
		    else
		    {
			if (CVS_STAT (retval, &sb) < 0)
			    error (1, errno, "cannot stat %s", retval);
			new_mode = sb.st_mode;
		    }
		}
		++p;
	    }
	    else
	    {
		strcpy (q, short_repos);
		if (CVS_MKDIR (retval, new_mode) < 0
		    && errno != EEXIST)
		    error (1, errno, "cannot make directory %s", retval);
		goto created;
	    }
	}
    created:;

	strcat (retval, "/");
	strcat (retval, name);

	if (saved_umask)
	{
	    assert (umask (save_umask) == 0000);
	    saved_umask = 0;
	}
    }
    return retval;
}

void Lock_Cleanup_Directory()
{
	/* Called by recurse after single directory lock */
	/* Don't kill the lockserver here */
	if(lock_server)
		return; 
	Lock_Cleanup();
}

/*
 * Clean up all outstanding locks
 */
void Lock_Cleanup ()
{
    /* FIXME: error handling here is kind of bogus; we sometimes will call
       error, which in turn can call us again.  For the moment work around
       this by refusing to reenter this function (this is a kludge).  */
    /* FIXME-reentrancy: the workaround isn't reentrant.  */
    static int in_lock_cleanup = 0;

    if (in_lock_cleanup)
		return;
    in_lock_cleanup = 1;

	if(lock_server)
	{
		char line[256];

		/* Do lock server stuff */
		if(lock_server_socket!=-1)
		{
			lock_server_command(line,256,"Close\n");
			cvs_tcp_close(lock_server_socket);
		}
		lock_server_socket=-1;

		in_lock_cleanup = 0;
		return;
	}

    remove_locks ();

    dellist (&lock_tree_list);

    if (locked_dir != NULL)
    {
	dellist (&locked_list);
	xfree (locked_dir);
	locked_dir = NULL;
	locked_list = NULL;
    }

    in_lock_cleanup = 0;
}

/*
 * Remove locks without discarding the lock information
 */
static void
remove_locks ()
{
    /* clean up simple locks (if any) */
    if (global_readlock.repository != NULL)
    {
	lock_simple_remove (&global_readlock);
	global_readlock.repository = NULL;
    }

    /* clean up multiple locks (if any) */
    if (locklist != (List *) NULL)
    {
	(void) walklist (locklist, unlock_proc, NULL);
	locklist = (List *) NULL;
    }
}

/*
 * walklist proc for removing a list of locks
 */
static int unlock_proc (Node *p, void *closure)
{
    lock_simple_remove ((struct lock *)p->data);
    return (0);
}

/* Remove the lock files.  */
static void lock_simple_remove (struct lock *lock)
{
    char *tmp;

    /* If readlock is set, the lock directory *might* have been created, but
       since Reader_Lock doesn't use SIG_beginCrSect the way that set_lock
       does, we don't know that.  That is why we need to check for
       existence_error here.  */
    if (readlock != NULL)
    {
	tmp = lock_name (lock->repository, readlock);
	if ( CVS_UNLINK (tmp) < 0 && ! existence_error (errno))
	    error (0, errno, "failed to remove lock %s", tmp);
	xfree (tmp);
    }

    /* If writelock is set, the lock directory *might* have been created, but
       since write_lock doesn't use SIG_beginCrSect the way that set_lock
       does, we don't know that.  That is why we need to check for
       existence_error here.  */
    if (writelock != NULL)
    {
	tmp = lock_name (lock->repository, writelock);
	if ( CVS_UNLINK (tmp) < 0 && ! existence_error (errno))
	    error (0, errno, "failed to remove lock %s", fn_root(tmp));
	xfree (tmp);
    }

    if (lock->have_lckdir)
    {
	tmp = lock_name (lock->repository, CVSLCK);
	if (CVS_RMDIR (tmp) < 0)
	    error (0, errno, "failed to remove lock dir %s", tmp);
	lock->have_lckdir = 0;
	xfree (tmp);
    }
}

/*
 * Create a lock file for readers
 */
int Reader_Lock (const char *xrepository)
{
    int err = 0;
    FILE *fp;
    char *tmp;

    if (noexec)
		return (0);

	if(lock_server)
	{
		/* No recursive locks */
		return 0;
	}

    /* we only do one directory at a time for read locks! */
    if (global_readlock.repository != NULL)
    {
	error (0, 0, "Reader_Lock called while read locks set - Help!");
	return (1);
    }

    if (readlock == NULL)
    {
#ifdef _WIN32
	if(server_active && CVS_Username)
	{
	  readlock = (char*)xmalloc (strlen (hostname) + strlen(CVS_Username) + sizeof (CVSRFL) + 40);
	  (void) sprintf (readlock, 
		"%s.%s(%s).%ld", CVSRFL, hostname, CVS_Username,
			(long) getpid ());
	}
	else
	{
	  readlock = (char*)xmalloc (strlen (hostname) + strlen(getlogin()) + sizeof (CVSRFL) + 40);
	  (void) sprintf (readlock, 
		"%s.%s(%s).%ld", CVSRFL, hostname, getlogin(),
			(long) getpid ());
	}
#else
	readlock = (char*)xmalloc (strlen (hostname) + sizeof (CVSRFL) + 40);
	(void) sprintf (readlock, 
#ifdef HAVE_LONG_FILE_NAMES
			"%s.%s.%ld", CVSRFL, hostname,
#else
			"%s.%ld", CVSRFL,
#endif
			(long) getpid ());
#endif
    }

    /* remember what we're locking (for Lock_Cleanup) */
    global_readlock.repository = xrepository;

    /* get the lock dir for our own */
    if (set_lock (&global_readlock, 1) != L_OK)
    {
	error (0, 0, "failed to obtain dir lock in repository `%s'",
	       xrepository);
	if (readlock != NULL)
	    xfree (readlock);
	readlock = NULL;
	/* We don't set global_readlock.repository to NULL.  I think this
	   only works because recurse.c will give a fatal error if we return
	   a nonzero value.  */
	return (1);
    }

    /* write a read-lock */
    tmp = lock_name (xrepository, readlock);
    if ((fp = CVS_FOPEN (tmp, "w+")) == NULL || fclose (fp) == EOF)
    {
	error (0, errno, "cannot create read lock in repository `%s'",
	       fn_root(xrepository));
	if (readlock != NULL)
	    xfree (readlock);
	readlock = NULL;
	err = 1;
    }
    xfree (tmp);

    /* free the lock dir */
    clear_lock (&global_readlock);

    return (err);
}

/*
 * Lock a list of directories for writing
 */
static char *lock_error_repos;
static int lock_error;

static int Writer_Lock (List *list)
{
    char *wait_repos;

    if (noexec)
	return (0);

    /* We only know how to do one list at a time */
    if (locklist != (List *) NULL)
    {
	error (0, 0, "Writer_Lock called while write locks set - Help!");
	return (1);
    }

    wait_repos = NULL;
    for (;;)
    {
	/* try to lock everything on the list */
	lock_error = L_OK;		/* init for set_writelock_proc */
	lock_error_repos = (char *) NULL; /* init for set_writelock_proc */
	locklist = list;		/* init for Lock_Cleanup */
	if (lockers_name != NULL)
	    xfree (lockers_name);
	lockers_name = xstrdup ("unknown");

	(void) walklist (list, set_writelock_proc, NULL);

	switch (lock_error)
	{
	    case L_ERROR:		/* Real Error */
		if (wait_repos != NULL)
		    xfree (wait_repos);
		Lock_Cleanup ();	/* clean up any locks we set */
		error (0, 0, "lock failed - giving up");
		return (1);

	    case L_LOCKED:		/* Someone already had a lock */
		remove_locks ();	/* clean up any locks we set */
		lock_wait (lock_error_repos); /* sleep a while and try again */
		wait_repos = xstrdup (lock_error_repos);
		continue;

	    case L_OK:			/* we got the locks set */
	        if (wait_repos != NULL)
		{
		    lock_obtained (wait_repos);
		    xfree (wait_repos);
		}
		return (0);

	    default:
		if (wait_repos != NULL)
		    xfree (wait_repos);
		error (0, 0, "unknown lock status %d in Writer_Lock",
		       lock_error);
		return (1);
	}
    }
}

/*
 * walklist proc for setting write locks
 */
static int set_writelock_proc (Node *p, void *closure)
{
    /* if some lock was not OK, just skip this one */
    if (lock_error != L_OK)
	return (0);

    /* apply the write lock */
    lock_error_repos = p->key;
    lock_error = write_lock ((struct lock *)p->data);
    return (0);
}

/*
 * Create a lock file for writers returns L_OK if lock set ok, L_LOCKED if
 * lock held by someone else or L_ERROR if an error occurred
 */
static int write_lock (struct lock *lock)
{
    int status;
    FILE *fp;
    char *tmp;

    if (writelock == NULL)
    {
#ifdef _WIN32
	if(server_active && CVS_Username)
	{
	  writelock = (char*)xmalloc (strlen (hostname) + strlen(CVS_Username) + sizeof (CVSWFL) + 40);
	  (void) sprintf (writelock, 
		"%s.%s(%s).%ld", CVSWFL, hostname, CVS_Username,
			(long) getpid ());
	}
	else
	{
	  writelock = (char*)xmalloc (strlen (hostname) + strlen(getlogin()) + sizeof (CVSWFL) + 40);
	  (void) sprintf (writelock, 
		"%s.%s(%s).%ld", CVSWFL, hostname, getlogin(),
			(long) getpid ());
	}
#else
	writelock = (char*)xmalloc (strlen (hostname) + sizeof (CVSWFL) + 40);
	(void) sprintf (writelock,
#ifdef HAVE_LONG_FILE_NAMES
			"%s.%s.%ld", CVSWFL, hostname,
#else
			"%s.%ld", CVSRFL,
#endif
			(long) getpid ());
#endif
    }

    /* make sure the lock dir is ours (not necessarily unique to us!) */
    status = set_lock (lock, 0);
    if (status == L_OK)
    {
	/* we now own a writer - make sure there are no readers */
	if (readers_exist (lock->repository))
	{
	    /* clean up the lock dir if we created it */
	    if (status == L_OK)
	    {
		clear_lock (lock);
	    }

	    /* indicate we failed due to read locks instead of error */
	    return (L_LOCKED);
	}

	/* write the write-lock file */
	tmp = lock_name (lock->repository, writelock);
	if ((fp = CVS_FOPEN (tmp, "w+")) == NULL || fclose (fp) == EOF)
	{
	    int xerrno = errno;

	    if ( CVS_UNLINK (tmp) < 0 && ! existence_error (errno))
		error (0, errno, "failed to remove lock %s", fn_root(tmp));

	    /* free the lock dir if we created it */
	    if (status == L_OK)
	    {
		clear_lock (lock);
	    }

	    /* return the error */
	    error (0, xerrno, "cannot create write lock in repository `%s'",
		   fn_root(lock->repository));
	    xfree (tmp);
	    return (L_ERROR);
	}
	xfree (tmp);
	return (L_OK);
    }
    else
	return (status);
}

/*
 * readers_exist() returns 0 if there are no reader lock files remaining in
 * the repository; else 1 is returned, to indicate that the caller should
 * sleep a while and try again.
 */
static int readers_exist (const char *repository)
{
    char *line;
    DIR *dirp;
    struct dirent *dp;
    struct stat sb;
    int ret = 0;

#ifdef CVS_FUDGELOCKS
again:
#endif

    if ((dirp = opendir (repository)) == NULL)
	{
		error (1, 0, "cannot open directory %s", repository);
	}

    errno = 0;
    while ((dp = readdir (dirp)) != NULL)
    {
	if (CVS_FNMATCH (CVSRFLPAT, dp->d_name, CVS_CASEFOLD) == 0)
	{
#ifdef CVS_FUDGELOCKS
	    time_t now;
	    (void) time (&now);
#endif

	    line = (char*)xmalloc (strlen (repository) + strlen (dp->d_name) + 5);
	    (void) sprintf (line, "%s/%s", repository, dp->d_name);
	    if ( CVS_STAT (line, &sb) != -1)
	    {
#ifdef CVS_FUDGELOCKS
		/*
		 * If the create time of the file is more than CVSLCKAGE 
		 * seconds ago, try to clean-up the lock file, and if
		 * successful, re-open the directory and try again.
		 */
		if (now >= (sb.st_ctime + CVSLCKAGE) && CVS_UNLINK (line) != -1)
		{
		    (void) closedir (dirp);
		    xfree (line);
		    goto again;
		}
#endif
#ifdef _WIN32
		/* Win32 has no uid/gid support, so we need the file path to see who owns it */
		set_lockers_name (line);
#else
		set_lockers_name (&sb);
#endif
	    }
	    else
	    {
		/* If the file doesn't exist, it just means that it disappeared
		   between the time we did the readdir and the time we did
		   the stat.  */
		if (!existence_error (errno))
		    error (0, errno, "cannot stat %s", line);
	    }
	    errno = 0;
	    xfree (line);

	    ret = 1;
	    break;
	}
	errno = 0;
    }
    if (errno != 0)
	error (0, errno, "error reading directory %s", repository);

    closedir (dirp);
    return (ret);
}

#ifndef _WIN32
/*
 * Set the static variable lockers_name appropriately, based on the stat
 * structure passed in.
 */
static void set_lockers_name (struct stat *statp)
{
    struct passwd *pw;

    if (lockers_name != NULL)
	xfree (lockers_name);
    if ((pw = (struct passwd *) getpwuid ((uid_t)statp->st_uid)) !=
	(struct passwd *) NULL)
    {
	lockers_name = xstrdup (pw->pw_name);
    }
    else
    {
	lockers_name = (char*)xmalloc (20);
	(void) sprintf (lockers_name, "uid%lu", (unsigned long) statp->st_uid);
    }
}
#else
/*
 * Set the static variable lockers_name appropriately, based on the
 * filename passed in.
 */
static void set_lockers_name (const char *file)
{
	lockers_name = xstrdup(win32getfileowner(file));
}
#endif /* _WIN32 */

/*
 * Persistently tries to make the directory "lckdir",, which serves as a
 * lock. If the create time on the directory is greater than CVSLCKAGE
 * seconds old, just try to remove the directory.
 */
static int set_lock (struct lock *lock, int will_wait)
{
    int waited;
    struct stat sb;
    mode_t omask;
#ifdef CVS_FUDGELOCKS
    time_t now;
#endif

    if (masterlock != NULL)
	xfree (masterlock);
    masterlock = lock_name (lock->repository, CVSLCK);

    /*
     * Note that it is up to the callers of set_lock() to arrange for signal
     * handlers that do the appropriate things, like remove the lock
     * directory before they exit.
     */
    waited = 0;
    lock->have_lckdir = 0;
    for (;;)
    {
	int status = -1;
	omask = umask (cvsumask);
	if (CVS_MKDIR (masterlock, 0777) == 0)
	{
	    lock->have_lckdir = 1;
	    status = L_OK;
	    if (waited)
	        lock_obtained (lock->repository);
	    goto out;
	}
      out:
	(void) umask (omask);
	if (status != -1)
	    return status;

	if (errno != EEXIST)
	{
	    error (0, errno,
		   "failed to create lock directory for `%s' (%s)",
		   fn_root(lock->repository), masterlock);
	    return (L_ERROR);
	}

	/* Find out who owns the lock.  If the lock directory is
	   non-existent, re-try the loop since someone probably just
	   removed it (thus releasing the lock).  */
	if (CVS_STAT (masterlock, &sb) < 0)
	{
	    if (existence_error (errno))
		continue;

	    error (0, errno, "couldn't stat lock directory `%s'", fn_root(masterlock));
	    return (L_ERROR);
	}

#ifdef CVS_FUDGELOCKS
	/*
	 * If the create time of the directory is more than CVSLCKAGE seconds
	 * ago, try to clean-up the lock directory, and if successful, just
	 * quietly retry to make it.
	 */
	(void) time (&now);
	if (now >= (sb.st_ctime + CVSLCKAGE))
	{
	    if (CVS_RMDIR (masterlock) >= 0)
		continue;
	}
#endif

#ifdef _WIN32
		/* Win32 has no uid/gid support, so we need the file path to see who owns it */
		set_lockers_name (masterlock);
#else
	set_lockers_name (&sb);
#endif

	/* if he wasn't willing to wait, return an error */
	if (!will_wait)
	    return (L_LOCKED);
	lock_wait (lock->repository);
	waited = 1;
    }
}

/*
 * Clear master lock.  We don't have to recompute the lock name since
 * clear_lock is never called except after a successful set_lock().
 */
static void clear_lock (struct lock *lock)
{
    if (CVS_RMDIR (masterlock) < 0)
	error (0, errno, "failed to remove lock dir `%s'", masterlock);
    lock->have_lckdir = 0;
}

/*
 * Print out a message that the lock is still held, then sleep a while.
 */
static void lock_wait (const char *repos)
{
    time_t now;
    char *msg;

    (void) time (&now);
    msg = (char*)xmalloc (100 + strlen (lockers_name) + strlen (repos));
    sprintf (msg, "[%8.8s] waiting for %s's lock in %s", ctime (&now) + 11,
	     lockers_name, repos);
    error (0, 0, "%s", msg);
    /* Call cvs_flusherr to ensure that the user sees this message as
       soon as possible.  */
    cvs_flusherr ();
    xfree (msg);
    (void) sleep (CVSLCKSLEEP);
}

/*
 * Print out a message when we obtain a lock.
 */
static void lock_obtained (const char *repos)
{
    time_t now;
    char *msg;

    (void) time (&now);
    msg = (char*)xmalloc (100 + strlen (repos));
    sprintf (msg, "[%8.8s] obtained lock in %s", ctime (&now) + 11, repos);
    error (0, 0, "%s", msg);
    /* Call cvs_flusherr to ensure that the user sees this message as
       soon as possible.  */
    cvs_flusherr ();
    xfree (msg);
}

static int lock_filesdoneproc (void *callerdat, int err,
				      char *repository, char *update_dir,
				      List *entries);

/*
 * Create a list of repositories to lock
 */
/* ARGSUSED */
static int lock_filesdoneproc (void *callerdat, int err, char *repository, char *update_dir, List *entries)
{
    Node *p;

	if(lock_dir)
	{
		/* Shortcut to avoid locking the lock directory */
		if(!fncmp(repository,lock_dir))
			return 0;
	}

    p = getnode ();
    p->type = LOCK;
    p->key = xstrdup (repository);
    p->data = (char*)xmalloc (sizeof (struct lock));
    ((struct lock *)p->data)->repository = p->key;
    ((struct lock *)p->data)->have_lckdir = 0;

    /* FIXME-KRP: this error condition should not simply be passed by. */
    if (p->key == NULL || addnode (lock_tree_list, p) != 0)
		freenode (p);

    return (err);
}

void lock_tree_for_write (int argc, char **argv, int local, int which, int aflag)
{
    int err;
    /*
     * Run the recursion processor to find all the dirs to lock and lock all
     * the dirs
     */
	if(lock_server)
		return; /* No recursive locks */
    lock_tree_list = getlist ();
    err = start_recursion ((FILEPROC) NULL, lock_filesdoneproc,
			   (PREDIRENTPROC) NULL, (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL, argc,
			   argv, local, which, aflag, 0, (char *) NULL, NULL, 0,
			   NULL, NULL);
    sortlist (lock_tree_list, fsortcmp);
    if (Writer_Lock (lock_tree_list) != 0)
		error (1, 0, "lock failed - giving up");
}

/* Lock a single directory in REPOSITORY.  It is OK to call this if
   a lock has been set with lock_dir_for_write; the new lock will replace
   the old one.  If REPOSITORY is NULL, don't do anything.  */
void lock_dir_for_write (const char *repository)
{
	if(lock_server)
	{
		// Do lock server stuff
//		do_lock_server(repository,"Directory Write Advisory", NULL);
		return;
	}
    if (repository != NULL
	&& (locked_dir == NULL
	    || fncmp (locked_dir, repository) != 0))
    {
	Node *node;

	if (locked_dir != NULL)
	    Lock_Cleanup ();

	locked_dir = xstrdup (repository);
	locked_list = getlist ();
	node = getnode ();
	node->type = LOCK;
	node->key = xstrdup (repository);
	node->data = (char*)xmalloc (sizeof (struct lock));
	((struct lock *)node->data)->repository = node->key;
	((struct lock *)node->data)->have_lckdir = 0;

	(void) addnode (locked_list, node);
	Writer_Lock (locked_list);
    }
}

int do_modified(size_t lockId, const char *version, const char *oldversion, const char *branch, char type)
{
	if(lock_server)
	{
		char line[1024];
		switch(lock_server_command(line,sizeof(line),"Modified %s|%d|%s|%s|%s\n",(type=='A')?"Added":(type=='D')?"Deleted":"",lockId,branch,version,oldversion))
		{
			case 0:
				break;
			default:
				error(1,0,"Couldn't send Modified to lock server: %s",line+4);
		}
	}
	return 0;
}

int local_lockserver()
{
#ifdef _WIN32
	HANDLE hSem = OpenSemaphore(GENERIC_READ,FALSE,_T("CVSNT_Lockserver"));
	if(hSem)
	{
		CloseHandle(hSem);
		return 1;
	}
	return 0;
#else
/* Stupidly, this is impossible in Unix. */
/* Return 1 to say it's running anways. */
	return 1;
#endif
}
