/* filesubr.c --- subroutines for dealing with files
   Jim Blandy <jimb@cyclic.com>

   This file is part of GNU CVS.

   GNU CVS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

/* These functions were moved out of subr.c because they need different
   definitions under operating systems (like, say, Windows NT) with different
   file system semantics.  */

#include "cvs.h"
#include <zlib.h>

#ifdef MAC_HFS_STUFF
	#include "mac_copy_file.h"
#endif

/* This is defined in zlib.h but we have to redefine it here... seems to be
   a gcc bug as no other compiler has this issue */
extern "C" uLong deflateBound(z_streamp,uLong);

static int deep_remove_dir (const char *path);

/*
 * Copies "from" to "to".
 */
int copy_file (const char *from, const char *to, int force_overwrite, int must_exist)
{
    struct stat sb;
    struct utimbuf t;
    int fdin, fdout;
#ifdef UTIME_EXPECTS_WRITABLE
    int change_it_back = 0;
#endif

	TRACE(1,"copy(%s,%s)",PATCH_NULL(from),PATCH_NULL(to));
    if (noexec)
		return 0;

    /* If the file to be copied is a link or a device, then just create
       the new link or device appropriately. */
    if (islink (from))
    {
		char *source = xreadlink (from);
		symlink (source, to);
		xfree (source);
		return 0;
    }

    if (isdevice (from))
    {
#if defined(HAVE_MKNOD) && defined(HAVE_STRUCT_STAT_ST_RDEV)
	if (stat (from, &sb) < 0)
	    error (1, errno, "cannot stat %s", fn_root(from));
	mknod (to, sb.st_mode, sb.st_rdev);
#else
	error (1, 0, "cannot copy device files on this system (%s)", fn_root(from));
#endif
    }
    else
    {
#ifdef MAC_HFS_STUFF
  /* Only use the mac specific copying routine in the client, since
     the server shouldn't have any need to handle resource forks
     (all the resource fork handling is done in the client - 
      the server only handles flat files) */
	if ( !server_active ) {
		if (stat (from, &sb) < 0)
		{
			if(must_exist)
				error (1, errno, "cannot stat %s", fn_root(from));
			else
				return -1;
		}
		mac_copy_file(from, to, force_overwrite, must_exist);
	} else {
#endif
	/* Not a link or a device... probably a regular file. */
	if ((fdin = CVS_OPEN (from, O_RDONLY)) < 0)
	{
		if(must_exist)
			error (1, errno, "cannot open %s for copying", fn_root(from));
		else
			return -1;
	}
	if (fstat (fdin, &sb) < 0)
	{
		if(must_exist)
			error (1, errno, "cannot fstat %s", fn_root(from));
		else
		{
			close(fdin);
			return -1;
		}
	}
	if (force_overwrite && unlink_file (to) && !existence_error (errno))
	    error (1, errno, "unable to remove %s", to);

	if ((fdout = creat (to, (int) sb.st_mode & 07777)) < 0)
	    error (1, errno, "cannot create %s for copying", fn_root(to));
	if (sb.st_size > 0)
	{
	    char buf[BUFSIZ];
	    int n;
	    
	    for (;;) 
	    {
		n = read (fdin, buf, sizeof(buf));
		if (n == -1)
		{
#ifdef EINTR
		    if (errno == EINTR)
			continue;
#endif
		    error (1, errno, "cannot read file %s for copying", fn_root(from));
		}
		else if (n == 0) 
		    break;
		
		if (write(fdout, buf, n) != n) {
		    error (1, errno, "cannot write file %s for copying", fn_root(to));
		}
	    }

#ifdef HAVE_FSYNC
	    if (fsync (fdout)) 
		error (1, errno, "cannot fsync file %s after copying", fn_root(to));
#endif
	}

	if (close (fdin) < 0) 
	    error (0, errno, "cannot close %s", fn_root(from));
	if (close (fdout) < 0)
	    error (1, errno, "cannot close %s", fn_root(to));
#ifdef MAC_HFS_STUFF
  }
#endif
  }


#ifdef UTIME_EXPECTS_WRITABLE
	if (!iswritable (to))
	{
		xchmod (to, 1);
		change_it_back = 1;
	}
#endif  /* UTIME_EXPECTS_WRITABLE  */
    /* now, set the times for the copied file to match those of the original */
    memset ((char *) &t, 0, sizeof (t));
    t.actime = sb.st_atime;
    t.modtime = sb.st_mtime;
    (void) utime (to, &t);
#ifdef UTIME_EXPECTS_WRITABLE
	if (change_it_back)
		xchmod (to, 0);
#endif  /*  UTIME_EXPECTS_WRITABLE  */
	return 0;
}

/*
 * Copies "from" to "to", compressing "to" on the fly
 */
int copy_and_zip_file (const char *from, const char *to, int force_overwrite, int must_exist)
{
    struct stat sb;
    struct utimbuf t;
    int fdin, fdout;
    z_stream zstr = {0};
    int zstatus;
    char buf[BUFSIZ*10];
    char zbuf[BUFSIZ*20];
    int n,zlen;
#ifdef UTIME_EXPECTS_WRITABLE
    int change_it_back = 0;
#endif

	TRACE(1,"copy_and_zip(%s,%s)",PATCH_NULL(from),PATCH_NULL(to));
    if (noexec)
		return 0;

    /* If the file to be copied is a link or a device, then just create
       the new link or device appropriately. */
    if (islink (from))
    {
		char *source = xreadlink (from);
		symlink (source, to);
		xfree (source);
		return 0;
    }

    if (isdevice (from))
    {
#if defined(HAVE_MKNOD) && defined(HAVE_STRUCT_STAT_ST_RDEV)
	if (stat (from, &sb) < 0)
	    error (1, errno, "cannot stat %s", from);
	mknod (to, sb.st_mode, sb.st_rdev);
#else
	error (1, 0, "cannot copy device files on this system (%s)", fn_root(from));
#endif
    }
    else
    {
	/* Not a link or a device... probably a regular file. */
	if ((fdin = CVS_OPEN (from, O_RDONLY)) < 0)
	{
		if(must_exist)
			error (1, errno, "cannot open %s for copying", fn_root(from));
		else
			return -1;
	}
	if (fstat (fdin, &sb) < 0)
	{
		if(must_exist)
			error (1, errno, "cannot fstat %s", fn_root(from));
		else
		{
			close(fdin);
			return -1;
		}
	}
	if (force_overwrite && unlink_file (to) && !existence_error (errno))
	    error (1, errno, "unable to remove %s", fn_root(to));

	if ((fdout = creat (to, (int) sb.st_mode & 07777)) < 0)
	    error (1, errno, "cannot create %s for copying", fn_root(to));

	zstatus = deflateInit2 (&zstr, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);
	if(zstatus != Z_OK)
	   error(1, 0, "compression error (INIT): (%d)%s", zstatus,zstr.msg);
     
	if (sb.st_size > 0)
	{
	    for (;;) 
	    {
		n = read (fdin, buf, sizeof(buf));
		if (n == -1)
		{
#ifdef EINTR
		    if (errno == EINTR)
			continue;
#endif
		    error (1, errno, "cannot read file %s for copying", fn_root(from));
		}
		else if (n == 0) 
		    break;

		zlen = deflateBound(&zstr,n);
		if(zlen>sizeof(zbuf))
			error(1,0,"compression error: zlen=%d (> %d)",zlen,(int)sizeof(zbuf));
		zstr.next_in = (Bytef*)buf;
		zstr.avail_in = n;	
		zstr.next_out = (Bytef*)zbuf;
		zstr.avail_out = zlen;
		zstatus = deflate (&zstr, Z_SYNC_FLUSH);
		if(zstatus != Z_OK)	
		  error(1,0, "compression error (Z_SYNC_FLUSH): (%d)%s", zstatus,zstr.msg);
		
		n = zlen-zstr.avail_out;	
		if (n && write(fdout, zbuf, n) != n) {
		    error (1, errno, "cannot write file %s for copying", fn_root(to));
		}
	    }

#ifdef HAVE_FSYNC
	    if (fsync (fdout)) 
		error (1, errno, "cannot fsync file %s after copying", fn_root(to));
#endif
	
	}

	zstr.next_in = (Bytef*)buf;
	zstr.avail_in = 0;
	zstr.next_out = (Bytef*)zbuf;
	zstr.avail_out = sizeof(zbuf);
	zstatus = deflate (&zstr, Z_FINISH);
	if(zstatus != Z_OK && zstatus != Z_STREAM_END)	
	      error(1,0, "compression error (Z_FINISH): (%d)%s", zstatus,zstr.msg);
	n = sizeof(zbuf)-zstr.avail_out;	
	if (n && write(fdout, zbuf, n) != n) {
	   error (1, errno, "cannot write file %s for copying", fn_root(to));
	}
	
	zstr.next_in = (Bytef*)buf;
	zstr.avail_in = 0;
	zstr.next_out = (Bytef*)zbuf;
	zstr.avail_out = sizeof(zbuf);
	zstatus = deflateEnd(&zstr);
	if(zstatus != Z_OK)
	  error(1,0, "compression error: %s", zstr.msg);

	if (close (fdin) < 0) 
	    error (0, errno, "cannot close %s", fn_root(from));
	if (close (fdout) < 0)
	    error (1, errno, "cannot close %s", fn_root(to));
    }

#ifdef UTIME_EXPECTS_WRITABLE
	if (!iswritable (to))
	{
		xchmod (to, 1);
		change_it_back = 1;
	}
#endif  /* UTIME_EXPECTS_WRITABLE  */
    /* now, set the times for the copied file to match those of the original */
    memset ((char *) &t, 0, sizeof (t));
    t.actime = sb.st_atime;
    t.modtime = sb.st_mtime;
    (void) utime (to, &t);
#ifdef UTIME_EXPECTS_WRITABLE
	if (change_it_back)
		xchmod (to, 0);
#endif  /*  UTIME_EXPECTS_WRITABLE  */
	return 0;
}

/*
 * Copies "from" to "to", decompressing "from" on the fly
 */
int copy_and_unzip_file (const char *from, const char *to, int force_overwrite, int must_exist)
{
    struct stat sb;
    struct utimbuf t;
    int fdin, fdout;
    z_stream zstr = {0};
    int zstatus;
    char buf[BUFSIZ*10];
    char zbuf[BUFSIZ*20];
    int n;
#ifdef UTIME_EXPECTS_WRITABLE
    int change_it_back = 0;
#endif

	TRACE(1,"copy_and_unzip(%s,%s)",PATCH_NULL(from),PATCH_NULL(to));
    if (noexec)
		return 0;

    /* If the file to be copied is a link or a device, then just create
       the new link or device appropriately. */
    if (islink (from))
    {
		char *source = xreadlink (from);
		symlink (source, to);
		xfree (source);
		return 0;
    }

    if (isdevice (from))
    {
#if defined(HAVE_MKNOD) && defined(HAVE_STRUCT_STAT_ST_RDEV)
	if (stat (from, &sb) < 0)
	    error (1, errno, "cannot stat %s", fn_root(from));
	mknod (to, sb.st_mode, sb.st_rdev);
#else
	error (1, 0, "cannot copy device files on this system (%s)", fn_root(from));
#endif
    }
    else
    {
	/* Not a link or a device... probably a regular file. */
	if ((fdin = CVS_OPEN (from, O_RDONLY)) < 0)
	{
		if(must_exist)
			error (1, errno, "cannot open %s for copying", fn_root(from));
		else
			return -1;
	}
	if (fstat (fdin, &sb) < 0)
	{
		if(must_exist)
			error (1, errno, "cannot fstat %s", fn_root(from));
		else
		{
			close(fdin);
			return -1;
		}
	}
	if (force_overwrite && unlink_file (to) && !existence_error (errno))
	    error (1, errno, "unable to remove %s", to);

	if ((fdout = creat (to, (int) sb.st_mode & 07777)) < 0)
	    error (1, errno, "cannot create %s for copying", to);

	zstatus = inflateInit2 (&zstr, 47);
	if(zstatus != Z_OK)
	   error(1, 0, "expansion error (INIT): (%d)%s", zstatus,zstr.msg);
     
	if (sb.st_size > 0)
	{
	    for (;;) 
	    {
		n = read (fdin, buf, sizeof(buf));
		if (n == -1)
		{
#ifdef EINTR
		    if (errno == EINTR)
			continue;
#endif
		    error (1, errno, "cannot read file %s for copying", fn_root(from));
		}
		else if (n == 0) 
		    break;

		zstr.next_in = (Bytef*)buf;
		zstr.avail_in = n;	
		while(zstr.avail_in)
		{
			zstr.next_out = (Bytef*)zbuf;
			zstr.avail_out = sizeof(zbuf);
			zstatus = inflate (&zstr, 0);
			if(zstatus != Z_OK && zstatus != Z_STREAM_END)	
			error(1,0, "expansion error (inflate): (%d)%s", zstatus,zstr.msg);
			
			n = sizeof(zbuf)-zstr.avail_out;	
			if (n && write(fdout, zbuf, n) != n) {
				error (1, errno, "cannot write file %s for copying", to);
			}
		}
		if(zstatus == Z_STREAM_END)
			break;
		}

#ifdef HAVE_FSYNC
	    if (fsync (fdout)) 
		error (1, errno, "cannot fsync file %s after copying", fn_root(to));
#endif
	
	}

	zstr.next_out = (Bytef*)zbuf;
	zstr.avail_out = sizeof(zbuf);
	zstatus = inflate (&zstr, Z_FINISH);
	if(zstatus != Z_OK && zstatus != Z_STREAM_END)	
	      error(1,0, "expansion error (Z_FINISH): (%d)%s", zstatus,zstr.msg);
	n = sizeof(zbuf)-zstr.avail_out;	
	if (n && write(fdout, zbuf, n) != n) {
	   error (1, errno, "cannot write file %s for copying", fn_root(to));
	}
	
	zstr.next_in = (Bytef*)buf;
	zstr.avail_in = 0;
	zstr.next_out = (Bytef*)zbuf;
	zstr.avail_out = sizeof(zbuf);
	zstatus = inflateEnd(&zstr);
	if(zstatus != Z_OK)
	  error(1,0, "expansion error: %s", zstr.msg);

	if (close (fdin) < 0) 
	    error (0, errno, "cannot close %s", fn_root(from));
	if (close (fdout) < 0)
	    error (1, errno, "cannot close %s", fn_root(to));
    }

#ifdef UTIME_EXPECTS_WRITABLE
	if (!iswritable (to))
	{
		xchmod (to, 1);
		change_it_back = 1;
	}
#endif  /* UTIME_EXPECTS_WRITABLE  */
    /* now, set the times for the copied file to match those of the original */
    memset ((char *) &t, 0, sizeof (t));
    t.actime = sb.st_atime;
    t.modtime = sb.st_mtime;
    (void) utime (to, &t);
#ifdef UTIME_EXPECTS_WRITABLE
	if (change_it_back)
		xchmod (to, 0);
#endif  /*  UTIME_EXPECTS_WRITABLE  */
	return 0;
}

/* FIXME-krp: these functions would benefit from caching the char * &
   stat buf.  */

/*
 * Returns non-zero if the argument file is a directory, or is a symbolic
 * link which points to a directory.
 */
int isdir (const char *file)
{
    struct stat sb;

    if (stat (file, &sb) < 0)
	return (0);
    return (S_ISDIR (sb.st_mode));
}

/*
 * Returns non-zero if the argument file is a symbolic link.
 */
int islink (const char *file)
{
#ifdef S_ISLNK
    struct stat sb;

    if (CVS_LSTAT (file, &sb) < 0)
	return (0);
    return (S_ISLNK (sb.st_mode));
#else
    return (0);
#endif
}

/*
 * Returns non-zero if the argument file is a block or
 * character special device.
 */
int isdevice (const char *file)
{
    struct stat sb;

    if (CVS_LSTAT (file, &sb) < 0)
	return (0);
#ifdef S_ISBLK
    if (S_ISBLK (sb.st_mode))
	return 1;
#endif
#ifdef S_ISCHR
    if (S_ISCHR (sb.st_mode))
	return 1;
#endif
    return 0;
}

/*
 * Returns non-zero if the argument file exists.
 */
int isfile (const char *file)
{
    return isaccessible(file, F_OK);
}

/*
 * Returns non-zero if the argument file is readable.
 */
int isreadable (const char *file)
{
    return isaccessible(file, R_OK);
}

/*
 * Returns non-zero if the argument file is writable.
 */
int iswritable (const char *file)
{
    return isaccessible(file, W_OK);
}

/*
 * Returns non-zero if the argument file is accessable according to
 * mode.  If compiled with SETXID_SUPPORT also works if cvs has setxid
 * bits set.
 */
int isaccessible (const char *file, const int mode)
{
#ifdef SETXID_SUPPORT
    struct stat sb;
    int umask = 0;
    int gmask = 0;
    int omask = 0;
    int uid;
    
    if (stat(file, &sb) == -1)
	return 0;
    if (mode == F_OK)
	return 1;

    uid = geteuid();
    if (uid == 0)		/* superuser */
    {
	if (mode & X_OK)
	    return sb.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH);
	else
	    return 1;
    }
	
    if (mode & R_OK)
    {
	umask |= S_IRUSR;
	gmask |= S_IRGRP;
	omask |= S_IROTH;
    }
    if (mode & W_OK)
    {
	umask |= S_IWUSR;
	gmask |= S_IWGRP;
	omask |= S_IWOTH;
    }
    if (mode & X_OK)
    {
	umask |= S_IXUSR;
	gmask |= S_IXGRP;
	omask |= S_IXOTH;
    }

    if (sb.st_uid == uid)
	return (sb.st_mode & umask) == umask;
    else if (sb.st_gid == getegid())
	return (sb.st_mode & gmask) == gmask;
    else
	return (sb.st_mode & omask) == omask;
#else
    return access(file, mode) == 0;
#endif
}

/*
 * Open a file and die if it fails
 */
FILE *open_file (const char *name, const char *mode)
{
    FILE *fp;

    if ((fp = fopen (name, mode)) == NULL)
	error (1, errno, "cannot open %s", name);
    return (fp);
}

/*
 * Make a directory and die if it fails
 */
void make_directory (const char *name)
{
    struct stat sb;

    if (stat (name, &sb) == 0 && (!S_ISDIR (sb.st_mode)))
	    error (0, 0, "%s already exists but is not a directory", fn_root(name));
    if (!noexec && CVS_MKDIR (name, 0777) < 0)
	error (1, errno, "cannot make directory %s", name);
}

/*
 * Make a path to the argument directory, printing a message if something
 * goes wrong.
 */
void make_directories (const char *name)
{
    char *cp;
    char *dir;

    if (noexec)
		return;

    if (CVS_MKDIR (name, 0777) == 0 || errno == EEXIST)
		return;
    if (errno != ENOENT)
    {
		error (0, errno, "cannot make path to %s", fn_root(name));
		return;
    }
    dir = xstrdup(name);
    for(cp=dir+strlen(dir);cp>dir && !ISDIRSEP(*cp); --cp)
	;
    if(cp==dir)
    {
	xfree(dir);
	return;
    }
    *cp = '\0';
    make_directories (dir);
    *cp++ = '/';
    if (*cp == '\0')
    {
	xfree(dir);
	return;
    }
    xfree(dir);
    CVS_MKDIR (name, 0777);
}

/* Create directory NAME if it does not already exist; fatal error for
   other errors.  Returns 0 if directory was created; 1 if it already
   existed.  */
int mkdir_if_needed (const char *name)
{
    if (CVS_MKDIR (name, 0777) < 0)
    {
	if (!(errno == EEXIST
	      || (errno == EACCES && isdir (name))))
	    error (1, errno, "cannot make directory %s", fn_root(name));
	return 1;
    }
    return 0;
}

mode_t modify_mode(mode_t mode, mode_t add, mode_t remove)
{
   mode_t oumask = 0;
   if(!server_active)
   {
     oumask = umask (0);
     umask (oumask);
   }
   mode |= add;
   mode &=~ remove;
   mode &=~ oumask;
   return mode;
}

/*
 * Change the mode of a file, either adding write permissions, or removing
 * all write permissions.  Either change honors the current umask setting.
 *
 * Don't do anything if PreservePermissions is set to `yes'.  This may
 * have unexpected consequences for some uses of xchmod.
 */
void xchmod (const char *fname, int writable)
{
    struct stat sb;
    mode_t mode;

    if (stat (fname, &sb) < 0)
    {
	if (!noexec)
	    error (0, errno, "cannot stat %s", fn_root(fname));
	return;
    }
    if (writable)
    {
	mode = modify_mode(sb.st_mode,
			          ((sb.st_mode & S_IRUSR) ? S_IWUSR : 0)
				| ((sb.st_mode & S_IRGRP) ? S_IWGRP : 0)
				| ((sb.st_mode & S_IROTH) ? S_IWOTH : 0), 0);
    }
    else
    {
	mode = modify_mode(sb.st_mode,0,S_IWUSR|S_IWGRP|S_IWOTH);
    }

    TRACE(1,"chmod(%s,%o)",PATCH_NULL(fname),mode);
    if (noexec)
		return;

    if (chmod (fname, mode) < 0)
	error (0, errno, "cannot change mode of file %s", fn_root(fname));
}

/*
 * Rename a file and die if it fails
 */
void rename_file (const char *from, const char *to)
{
	TRACE(1,"rename(%s,%s)",PATCH_NULL(from),PATCH_NULL(to));
    if (noexec)
		return;

    if (rename (from, to) < 0)
	error (1, errno, "cannot rename file %s to %s", fn_root(from), fn_root(to));
}

/*
 * unlink a file, if possible.
 */
int unlink_file (const char *f)
{
	TRACE(1,"unlink_file(%s)",PATCH_NULL(f));
    if (noexec)
		return (0);

    return (CVS_UNLINK (f));
}

/*
 * Unlink a file or dir, if possible.  If it is a directory do a deep
 * removal of all of the files in the directory.  Return -1 on error
 * (in which case errno is set).
 */
int unlink_file_dir (const char *f)
{
    struct stat sb;

	TRACE(1,"unlink_file_dir(%s)",PATCH_NULL(f));

    if (noexec)
		return (0);

    /* For at least some unices, if root tries to unlink() a directory,
       instead of doing something rational like returning EISDIR,
       the system will gleefully go ahead and corrupt the filesystem.
       So we first call stat() to see if it is OK to call unlink().  This
       doesn't quite work--if someone creates a directory between the
       call to stat() and the call to unlink(), we'll still corrupt
       the filesystem.  Where is the Unix Haters Handbook when you need
       it?  */
    if (stat (f, &sb) < 0)
    {
	if (existence_error (errno))
	{
	    /* The file or directory doesn't exist anyhow.  */
	    return -1;
	}
    }
    else if (S_ISDIR (sb.st_mode))
	return deep_remove_dir (f);

    return CVS_UNLINK (f);
}

/* Remove a directory and everything it contains.  Returns 0 for
 * success, -1 for failure (in which case errno is set).
 */

static int deep_remove_dir (const char *path)
{
    DIR		  *dirp;
    struct dirent *dp;

    if (rmdir (path) != 0)
    {
	if (errno == ENOTEMPTY
	    || errno == EEXIST
	    /* Ugly workaround for ugly AIX 4.1 (and 3.2) header bug
	       (it defines ENOTEMPTY and EEXIST to 17 but actually
	       returns 87).  */
	    || (ENOTEMPTY == 17 && EEXIST == 17 && errno == 87))
	{
	    if ((dirp = opendir (path)) == NULL)
		/* If unable to open the directory return
		 * an error
		 */
		return -1;

	    errno = 0;
	    while ((dp = readdir (dirp)) != NULL)
	    {
		char *buf;

		if (strcmp (dp->d_name, ".") == 0 ||
			    strcmp (dp->d_name, "..") == 0)
		    continue;

		buf = (char*)xmalloc (strlen (path) + strlen (dp->d_name) + 5);
		sprintf (buf, "%s/%s", path, dp->d_name);

		/* See comment in unlink_file_dir explanation of why we use
		   isdir instead of just calling unlink and checking the
		   status.  */
		if (isdir(buf)) 
		{
		    if (deep_remove_dir(buf))
		    {
			closedir(dirp);
			xfree (buf);
			return -1;
		    }
		}
		else
		{
		    if (CVS_UNLINK (buf) != 0)
		    {
			closedir(dirp);
			xfree (buf);
			return -1;
		    }
		}
		xfree (buf);

		errno = 0;
	    }
	    if (errno != 0)
	    {
		int save_errno = errno;
		closedir (dirp);
		errno = save_errno;
		return -1;
	    }
	    closedir (dirp);
	    return rmdir (path);
	}
	else
	    return -1;
    }

    /* Was able to remove the directory return 0 */
    return 0;
}

/* Read NCHARS bytes from descriptor FD into BUF.
   Return the number of characters successfully read.
   The number returned is always NCHARS unless end-of-file or error.  */
static size_t block_read (int fd, char *buf, size_t nchars)
{
    char *bp = buf;
    size_t nread;

    do 
    {
	nread = read (fd, bp, nchars);
	if (nread == (size_t)-1)
	{
#ifdef EINTR
	    if (errno == EINTR)
		continue;
#endif
	    return (size_t)-1;
	}

	if (nread == 0)
	    break; 

	bp += nread;
	nchars -= nread;
    } while (nchars != 0);

    return bp - buf;
} 

    
/*
 * Compare "file1" to "file2". Return non-zero if they don't compare exactly.
 * If FILE1 and FILE2 are special files, compare their salient characteristics
 * (i.e. major/minor device numbers, links, etc.
 */
int xcmp (const char *file1, const char *file2)
{
    char *buf1, *buf2;
    struct stat sb1, sb2;
    int fd1, fd2;
    int ret;

    if (CVS_LSTAT (file1, &sb1) < 0)
	error (1, errno, "cannot lstat %s", fn_root(file1));
    if (CVS_LSTAT (file2, &sb2) < 0)
	error (1, errno, "cannot lstat %s", fn_root(file2));

    /* If FILE1 and FILE2 are not the same file type, they are unequal. */
    if ((sb1.st_mode & S_IFMT) != (sb2.st_mode & S_IFMT))
	return 1;

    /* If FILE1 and FILE2 are symlinks, they are equal if they point to
       the same thing. */
    if (S_ISLNK (sb1.st_mode) && S_ISLNK (sb2.st_mode))
    {
	int result;
	buf1 = xreadlink (file1);
	buf2 = xreadlink (file2);
	result = (strcmp (buf1, buf2) == 0);
	xfree (buf1);
	xfree (buf2);
	return result;
    }

    /* If FILE1 and FILE2 are devices, they are equal if their device
       numbers match. */
    if (S_ISBLK (sb1.st_mode) || S_ISCHR (sb1.st_mode))
    {
#ifdef HAVE_STRUCT_STAT_ST_RDEV
	if (sb1.st_rdev == sb2.st_rdev)
	    return 0;
	else
	    return 1;
#else
	error (1, 0, "cannot compare device files on this system (%s and %s)",
	       file1, file2);
#endif
    }

    if ((fd1 = CVS_OPEN (file1, O_RDONLY)) < 0)
	error (1, errno, "cannot open file %s for comparing", fn_root(file1));
    if ((fd2 = CVS_OPEN (file2, O_RDONLY)) < 0)
	error (1, errno, "cannot open file %s for comparing", fn_root(file2));

    /* A generic file compare routine might compare st_dev & st_ino here 
       to see if the two files being compared are actually the same file.
       But that won't happen in CVS, so we won't bother. */

    if (sb1.st_size != sb2.st_size)
	ret = 1;
    else if (sb1.st_size == 0)
	ret = 0;
    else
    {
	/* FIXME: compute the optimal buffer size by computing the least
	   common multiple of the files st_blocks field */
	size_t buf_size = 8 * 1024;
	size_t read1;
	size_t read2;

	buf1 = (char*)xmalloc (buf_size);
	buf2 = (char*)xmalloc (buf_size);

	do 
	{
	    read1 = block_read (fd1, buf1, buf_size);
	    if (read1 == (size_t)-1)
		error (1, errno, "cannot read file %s for comparing", fn_root(file1));

	    read2 = block_read (fd2, buf2, buf_size);
	    if (read2 == (size_t)-1)
		error (1, errno, "cannot read file %s for comparing", fn_root(file2));

	    /* assert (read1 == read2); */

	    ret = memcmp(buf1, buf2, read1);
	} while (ret == 0 && read1 == buf_size);

	xfree (buf1);
	xfree (buf2);
    }
	
    (void) close (fd1);
    (void) close (fd2);
    return (ret);
}

/* Generate a unique temporary filename.  Returns a pointer to a newly
 * malloc'd string containing the name.  Returns successfully or not at
 * all.
 *
 *     THIS FUNCTION IS DEPRECATED!!!  USE cvs_temp_file INSTEAD!!!
 *
 * and yes, I know about the way the rcs commands use temp files.  I think
 * they should be converted too but I don't have time to look into it right
 * now.
 */
char *cvs_temp_name ()
{
    char *fn;
    FILE *fp;

    fp = cvs_temp_file (&fn);
    if (fp == NULL)
	error (1, errno, "Failed to create temporary file");
    if (fclose (fp) == EOF)
	error (0, errno, "Failed to close temporary file %s", fn_root(fn));
    return fn;
}

/* Generate a unique temporary filename and return an open file stream
 * to the truncated file by that name
 *
 *  INPUTS
 *	filename	where to place the pointer to the newly allocated file
 *   			name string
 *
 *  OUTPUTS
 *	filename	dereferenced, will point to the newly allocated file
 *			name string.  This value is undefined if the function
 *			returns an error.
 *
 *  RETURNS
 *	An open file pointer to a read/write mode empty temporary file with the
 *	unique file name or NULL on failure.
 *
 *  ERRORS
 *	on error, errno will be set to some value either by CVS_FOPEN or
 *	whatever system function is called to generate the temporary file name
 */
/* There are at least four functions for generating temporary
 * filenames.  We use mkstemp (BSD 4.3) if possible, else tempnam (SVID 3),
 * else mktemp (BSD 4.3), and as last resort tmpnam (POSIX).  Reason is that
 * mkstemp, tempnam, and mktemp both allow to specify the directory in which
 * the temporary file will be created.
 *
 * And the _correct_ way to use the deprecated functions probably involves
 * opening file descriptors using O_EXCL & O_CREAT and even doing the annoying
 * NFS locking thing, but until I hear of more problems, I'm not going to
 * bother.
 */
FILE *cvs_temp_file (char **filename)
{
    char *fn;
    FILE *fp;

    /* FIXME - I'd like to be returning NULL here in noexec mode, but I think
     * some of the rcs & diff functions which rely on a temp file run in
     * noexec mode too.
     */

    assert (filename != NULL);

#ifdef HAVE_MKSTEMP

    {
    int fd;

    fn = (char*)xmalloc (strlen (Tmpdir) + 11);
    sprintf (fn, "%s/%s", Tmpdir, "cvsXXXXXX" );
    fd = mkstemp (fn);

    /* a NULL return will be interpreted by callers as an error and
     * errno should still be set
     */
    if (fd == -1) fp = NULL;
    else if ((fp = CVS_FDOPEN (fd, "w+")) == NULL)
    {
	/* attempt to close and unlink the file since mkstemp returned sucessfully and
	 * we believe it's been created and opened
	 */
 	int save_errno = errno;
	if (close (fd))
	    error (0, errno, "Failed to close temporary file %s", fn_root(fn));
	if (CVS_UNLINK (fn))
	    error (0, errno, "Failed to unlink temporary file %s", fn_root(fn));
	errno = save_errno;
    }

    if (fp == NULL) xfree (fn);
    /* mkstemp is defined to open mode 0600 using glibc 2.0.7+ */
    /* FIXME - configure can probably tell us which version of glibc we are
     * linking to and not chmod for 2.0.7+
     */
    else chmod (fn, 0600);

    }

#elif HAVE_TEMPNAM

    /* tempnam has been deprecated due to under-specification */

    fn = tempnam (Tmpdir, "cvs");
    if (fn == NULL) fp = NULL;
    else if ((fp = CVS_FOPEN (fn, "w+")) == NULL) xfree (fn);
    else chmod (fn, 0600);

    /* tempnam returns a pointer to a newly malloc'd string, so there's
     * no need for a xstrdup
     */

#elif HAVE_MKTEMP

    /* mktemp has been deprecated due to the BSD 4.3 specification specifying
     * that XXXXXX will be replaced by a PID and a letter, creating only 26
     * possibilities, a security risk, and a race condition.
     */

    {
    char *ifn;

    ifn = xmalloc (strlen (Tmpdir) + 11);
    sprintf (ifn, "%s/%s", Tmpdir, "cvsXXXXXX" );
    fn = mktemp (ifn);

    if (fn == NULL) fp = NULL;
    else fp = CVS_FOPEN (fn, "w+");

    if (fp == NULL) xfree (ifn);
    else chmod (fn, 0600);

    }

#else	/* use tmpnam if all else fails */

    /* tmpnam is deprecated */

    {
    char ifn[L_tmpnam + 1];

    fn = tmpnam (ifn);

    if (fn == NULL) fp = NULL;
    else if ((fp = CVS_FOPEN (ifn, "w+")) != NULL)
    {
	fn = xstrdup (ifn);
	chmod (fn, 0600);
    }

    }

#endif

    *filename = fn;
    return fp;
}

/* Return non-zero iff FILENAME is absolute.
   Trivial under Unix, but more complicated under other systems.  */
int isabsolute (const char *filename)
{
    return filename[0] == '/';
}

/* Remote server might be anything, so we have to deal with all types of
   path here */
int isabsolute_remote (const char *filename)
{
    return (ISDIRSEP (filename[0])
            || (filename[0] != '\0'
                && filename[1] == ':'
                && ISDIRSEP (filename[2])));
}

/*
 * Return a string (dynamically allocated) with the name of the file to which
 * LINK is symlinked.
 */
char *xreadlink (const char *link)
{
    char *file = NULL;
    char *tfile;
    int buflen = 128;
    int link_name_len;

    if (!islink (link))
	return NULL;

    /* Get the name of the file to which `from' is linked.
       FIXME: what portability issues arise here?  Are readlink &
       ENAMETOOLONG defined on all systems? -twp */
    do
    {
	file = (char*)xrealloc (file, buflen);
	link_name_len = readlink (link, file, buflen - 1);
	buflen *= 2;
    }
    while (link_name_len < 0 && errno == ENAMETOOLONG);

    if (link_name_len < 0)
	error (1, errno, "cannot readlink %s", link);

    file[link_name_len] = '\0';

    tfile = xstrdup (file);
    xfree (file);

    return tfile;
}


/* Return a pointer into PATH's last component.  */
const char *last_component (const char *path)
{
    char *last = strrchr ((char *)path, '/');
    
    if (last && (last != path))
        return last + 1;
    else
        return path;
}

/* Return the home directory.  Returns a pointer to storage
   managed by this function or its callees (currently getenv).
   This function will return the same thing every time it is
   called.  Returns NULL if there is no home directory.

   Note that for a pserver server, this may return root's home
   directory.  What typically happens is that upon being started from
   inetd, before switching users, the code in cvsrc.c calls
   get_homedir which remembers root's home directory in the static
   variable.  Then the switch happens and get_homedir might return a
   directory that we don't even have read or execute permissions for
   (which is bad, when various parts of CVS try to read there).  One
   fix would be to make the value returned by get_homedir only good
   until the next call (which would free the old value).  Another fix
   would be to just always malloc our answer, and let the caller free
   it (that is best, because some day we may need to be reentrant).

   The workaround is to put -f in inetd.conf which means that
   get_homedir won't get called until after the switch in user ID.

   The whole concept of a "home directory" on the server is pretty
   iffy, although I suppose some people probably are relying on it for
   .cvsrc and such, in the cases where it works.  */
char *get_homedir ()
{
    static char *home = NULL;
    const char *env;
    struct passwd *pw;

    if (home != NULL)
	return home;

    if (
	!server_active &&
	(env = CProtocolLibrary::GetEnvironment ("HOME")) != NULL)
	home = xstrdup(env);
    else if ((pw = (struct passwd *) getpwuid (getuid ()))
	     && pw->pw_dir)
	home = xstrdup (pw->pw_dir);
    else
	return 0;

    return home;
}

/* See cvs.h for description.  On unix this does nothing, because the
   shell expands the wildcards.  */
void expand_wild (int argc, char **argv, int *pargc, char ***pargv)
{
    int i;
    *pargc = argc;
    *pargv = (char **) xmalloc (argc * sizeof (char *));
    for (i = 0; i < argc; ++i)
	(*pargv)[i] = xstrdup (argv[i]);
}

/* On case insensitive systems, find the real case */
/* On case sensitive ones, just return */
char *normalize_path(char *arg)
{
	return arg;
}
