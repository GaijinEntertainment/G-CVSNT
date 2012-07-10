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
#include <io.h>
#include <zlib.h>
#include <shellapi.h>

/* Copies "from" to "to".  Note that the functionality here is similar
   to the win32 function CopyFile, but (1) we copy LastAccessTime and
   CopyFile doesn't, (2) we set file attributes to the default set by
   the C library and CopyFile copies them.  Neither #1 nor #2 was intentional
   as far as I know, but changing them could be confusing, unless there
   is some reason they should be changed (this would need more
   investigation).  */
int copy_file (const char *from, const char *to, int force_overwrite, int must_exist)
{
	uc_name n_to = to;
	uc_name n_from = from;
	DWORD fa_to = GetFileAttributes(n_to);
	DWORD fa_from = GetFileAttributes(n_from);
	if(fa_from==0xFFFFFFFF)
	{
		_dosmaperr(GetLastError());
		/* We don't actually handle EPERM here... probably should do */
		if(must_exist || !existence_error(errno))
			error (1, errno, "cannot open %s for copying", from);
		return 0;
	}

	if(fa_to!=0xFFFFFFFF && (fa_to&FILE_ATTRIBUTE_READONLY))
		SetFileAttributes(n_to,fa_to&~FILE_ATTRIBUTE_READONLY);
	if(!CopyFile(n_from,n_to,force_overwrite?FALSE:TRUE))
	{
		TRACE(3,"copy_file: CopyFile returned %08x",GetLastError());
		_dosmaperr(GetLastError());
		error(1,errno,"Couldn't copy file %s to %s",fn_root(from),fn_root(to));
		return -1;
	}

	{
		HANDLE hf;
		FILETIME creat,acc,writ;
		hf=CreateFile(n_from,FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
		GetFileTime(hf,&creat,&acc,&writ);
		CloseHandle(hf);
		hf=CreateFile(n_to,FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
		SetFileTime(hf,&creat,&acc,&writ);
		CloseHandle(hf);
	}

	SetFileAttributes(n_to,fa_from);

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

	TRACE(1,"copy_and_zip(%s,%s)",from,to);
    if (noexec)
		return 0;

    if ((fdin = CVS_OPEN (from, O_RDONLY | O_BINARY,0)) < 0)
	{
		if(must_exist)
			error (1, errno, "cannot open %s for copying", from);
		else
			return -1;
	}
	if (fstat (fdin, &sb) < 0)
	{
		if(must_exist)
			error (1, errno, "cannot fstat %s", from);
		else
		{
			close(fdin);
			return -1;
		}
	}
	if (force_overwrite && unlink_file (to) && !existence_error (errno))
	    error (1, errno, "unable to remove %s", to);

    if ((fdout = CVS_OPEN (to, O_CREAT | O_TRUNC | O_RDWR | O_BINARY, 0600)) < 0)
	    error (1, errno, "cannot create %s for copying", to);

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
		    error (1, errno, "cannot read file %s for copying", from);
		}
		else if (n == 0) 
		    break;

		zlen = deflateBound(&zstr,n);
		if(zlen>sizeof(zbuf))
			error(1,0,"compression error: zlen=%d (> %d)",zlen,sizeof(zbuf));
		zstr.next_in = (Bytef*)buf;
		zstr.avail_in = n;	
		zstr.next_out = (Bytef*)zbuf;
		zstr.avail_out = zlen;
		zstatus = deflate (&zstr, Z_SYNC_FLUSH);
		if(zstatus != Z_OK)	
		  error(1,0, "compression error (deflate): (%d)%s", zstatus,zstr.msg);
		
		n = zlen-zstr.avail_out;	
		if (n && write(fdout, zbuf, n) != n) {
		    error (1, errno, "cannot write file %s for copying", to);
		}
	    }

#ifdef HAVE_FSYNC
	    if (fsync (fdout)) 
		error (1, errno, "cannot fsync file %s after copying", to);
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
	   error (1, errno, "cannot write file %s for copying", to);
	}

	zstr.next_in = (Bytef*)buf;
	zstr.avail_in = 0;
	zstr.next_out = (Bytef*)zbuf;
	zstr.avail_out = sizeof(zbuf);
	zstatus = deflateEnd(&zstr);
	if(zstatus != Z_OK)
	  error(1,0, "compression error (deflateEnd): (%d) %s", zstatus, zstr.msg);

	if (close (fdin) < 0) 
	    error (0, errno, "cannot close %s", from);
	if (close (fdout) < 0)
	    error (1, errno, "cannot close %s", to);

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
    utime (to, &t);
	chmod(to,sb.st_mode);
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

	TRACE(1,"copy_and_unzip(%s,%s)",from,to);
    if (noexec)
		return 0;

    if ((fdin = CVS_OPEN (from, O_RDONLY | O_BINARY,0)) < 0)
	{
		if(must_exist)
			error (1, errno, "cannot open %s for copying", from);
		else
			return -1;
	}
	if (fstat (fdin, &sb) < 0)
	{
		if(must_exist)
			error (1, errno, "cannot fstat %s", from);
		else
		{
			close(fdin);
			return -1;
		}
	}
	if (force_overwrite && unlink_file (to) && !existence_error (errno))
	    error (1, errno, "unable to remove %s", to);

    if ((fdout = CVS_OPEN (to, O_CREAT | O_TRUNC | O_RDWR | O_BINARY, 0600))<0)
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
		    error (1, errno, "cannot read file %s for copying", from);
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
		error (1, errno, "cannot fsync file %s after copying", to);
#endif
	
	}

	zstr.next_out = (Bytef*)zbuf;
	zstr.avail_out = sizeof(zbuf);
	zstatus = inflate (&zstr, Z_FINISH);
	if(zstatus != Z_OK && zstatus != Z_STREAM_END)	
	      error(1,0, "expansion error (Z_FINISH): (%d)%s", zstatus,zstr.msg);
	n = sizeof(zbuf)-zstr.avail_out;	
	if (n && write(fdout, zbuf, n) != n) {
	   error (1, errno, "cannot write file %s for copying", to);
	}
	
	zstr.next_in = (Bytef*)buf;
	zstr.avail_in = 0;
	zstr.next_out = (Bytef*)zbuf;
	zstr.avail_out = sizeof(zbuf);
	zstatus = inflateEnd(&zstr);
	if(zstatus != Z_OK)
	  error(1,0, "expansion error: %s", zstr.msg);

	if (close (fdin) < 0) 
	    error (0, errno, "cannot close %s", from);
	if (close (fdout) < 0)
	    error (1, errno, "cannot close %s", to);

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
    utime (to, &t);
	chmod(to,sb.st_mode);
#ifdef UTIME_EXPECTS_WRITABLE
	if (change_it_back)
		xchmod (to, 0);
#endif  /*  UTIME_EXPECTS_WRITABLE  */
	return 0;
}

/* FIXME-krp: these functions would benefit from caching the char * &
   stat buf.  */

char *repository_name(struct file_info *file)
{
	char *fn = (char*)xmalloc(strlen(file->repository) + strlen(file->file) + 10);

	sprintf(fn,"%s/%s",file->repository,file->file);
	return fn;
}

/*
 * Returns non-zero if the argument file is a directory, or is a symbolic
 * link which points to a directory.
 */
int isdir (const char *file)
{
	uc_name fn = file;
	DWORD fa = GetFileAttributes(fn);

	if(fa!=0xFFFFFFFF && fa&FILE_ATTRIBUTE_DIRECTORY)
		return 1;
	if(fa==0xFFFFFFFF)
		_dosmaperr(GetLastError());
	return 0;
}

/*
 * Returns non-zero if the argument file is a symbolic link.
 */
int islink (const char *file)
{
	return 0;
}

/*
 * Returns non-zero if the argument file exists.
 */
int isfile (const char *file)
{
	uc_name fn = file;
	DWORD fa = GetFileAttributes(fn);
	if(fa==0xFFFFFFFF)
	{
		_dosmaperr(GetLastError());
		return 0;
	}
	return 1;
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
	DWORD fa;
	DWORD access = 0;
	HANDLE hf;
	uc_name fn = file;

	fa = GetFileAttributes(fn);
	if(fa==0xFFFFFFFF)
	{
		_dosmaperr(GetLastError());
		return 0;
	}

	if(fa&FILE_ATTRIBUTE_DIRECTORY)
	{
		return 1;
	}

	if(!(mode&W_OK))
	{
		return 1;
	}


	hf=CreateFile(fn,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
	if(hf==INVALID_HANDLE_VALUE)
	{
		_dosmaperr(GetLastError());
		return 0;
	}
	CloseHandle(hf);
	return 1;
}

/*
 * Open a file and die if it fails
 */
FILE *open_file (const char *name, const char *mode)
{
    FILE *fp;

    if ((fp = CVS_FOPEN (name, mode)) == NULL)
		error (1, errno, "cannot open %s", name);
    return fp;
}

/*
 * Make a directory and die if it fails
 */
void make_directory(const char *name)
{
	TRACE(3,"make_directory(%s)",name);
	uc_name fn = name;
	DWORD fa = GetFileAttributes(fn);

	if(fa!=0xFFFFFFFF)
	{
		if(!(fa&FILE_ATTRIBUTE_DIRECTORY))
		{
			TRACE(3,"make_directory() already exists but is not a directory");
			error (0, 0, "%s already exists but is not a directory", fn_root(name));
		}
		else
		{
			TRACE(3,"make_directory() already exists - return");
			return;
		}
	}

	if (!noexec && !CreateDirectory(fn,NULL))
	{
		TRACE(3,"make_directory() cannot make directory - get last error");
		_dosmaperr(GetLastError());
		TRACE(3,"make_directory() cannot make directory - give error");
		error (1, errno, "cannot make directory %s", fn_root(name));
	}
	TRACE(3,"make_directory() made directory OK");
}

/*
 * Make a path to the argument directory, printing a message if something
 * goes wrong.
 */
static void _tmake_directories(const char *name, const TCHAR *fn)
{
	TRACE(3,"make_directories(%s,%s)",name,(fn)?"fn passed in":"fn not passed in");
	DWORD fa;
	TCHAR *dir=NULL;
	TCHAR *cp=NULL;

    if (noexec)
		return;

	fa = GetFileAttributes(fn);
	if(fa!=0xFFFFFFFF)
	{
		if(!(fa&FILE_ATTRIBUTE_DIRECTORY))
			error (0, 0, "%s already exists but is not a directory", fn_root(name));
		else
		{
			return;
		}
	}
	if (!CreateDirectory(fn,NULL))
	{
		DWORD dwErr = GetLastError();
		if(dwErr!=ERROR_PATH_NOT_FOUND)
		{
			_dosmaperr(dwErr);
			error (0, errno, "cannot make directory %s", fn_root(name));
			return;
		}
	}
	else
		return;
	dir = _tcsdup(fn);
	for(cp=dir+_tcslen(dir);cp>dir && !ISDIRSEP(*cp); --cp)
		;
	if(cp==dir)
	{
		xfree(dir);
		return;
	}
    *cp = '\0';
    _tmake_directories (name,dir);
    *cp++ = '/';
    if (*cp == '\0')
	{
		xfree(dir);
		return;
	}
	if (!CreateDirectory(dir,NULL))
	{
		_dosmaperr(GetLastError());
		error (0, errno, "cannot make directory %s", fn_root(name));
		xfree(dir);
		return;
	}
	xfree(dir);
	TRACE(3,"_tmake_directories() return");
}

void make_directories (const char *name)
{
	TRACE(3,"make_directories(%s)",name);
	uc_name fn = name;
	_tmake_directories(name,fn);
	TRACE(3,"make_directories() return");
}

/* Create directory NAME if it does not already exist; fatal error for
   other errors.  Returns 0 if directory was created; 1 if it already
   existed.  */
int mkdir_if_needed (const char *name)
{
	uc_name fn = name;
	DWORD fa = GetFileAttributes(fn);
	const char *wd = xgetwd();
	if(fa!=0xFFFFFFFF)
	{
		if(!(fa&FILE_ATTRIBUTE_DIRECTORY))
			error (1, EEXIST, "%s already exists but is not a directory", fn_root(name));
		else
			return 1;
	}
	if (!CreateDirectory(fn,NULL))
	{
		_dosmaperr(GetLastError());
		error (1, errno, "cannot make directory %s", fn_root(name));
		return 0;
	}
    return 0;
}

/* Return the mask that results from setting or removing bits */
/* On Win32 we assume a umask of 022 */
mode_t modify_mode(mode_t mode, mode_t add, mode_t remove)
{
	mode_t oumask = 0;

	if(!server_active)
		oumask = 022;

	mode |= add;
	mode &=~ remove;
	mode &=~ oumask;
	return mode;
}

/*
 * Change the mode of a file, either adding write permissions, or removing
 * all write permissions.  Adding write permissions honors the current umask
 * setting.
 */
/*
 * Note that this isn't correct from the point of view of cygwin compat., but is
 * a lot faster than any other way
 */
void xchmod (const char *fname, int writable)
{
	uc_name fn = fname;
	DWORD oldfa,fa = oldfa = GetFileAttributes(fn);

	TRACE(3,"xchmod(%s,%d)",fname,writable);

	if(fa==0xFFFFFFFF)
	{
		if (!noexec)
			error (0, errno, "cannot stat %s", fn_root(fname));
		return;
    }

    if (noexec)
	{
		return;
	}

	if(writable)
		fa&=~FILE_ATTRIBUTE_READONLY;
	else
		fa|=FILE_ATTRIBUTE_READONLY;


	if(oldfa!=fa)
	{
		if(!SetFileAttributes(fn,fa))
		{
			_dosmaperr(GetLastError());
			error (0, errno, "cannot change mode of file %s", fn_root(fname));
		}
	}
}


/* Read the value of a symbolic link.
   Under Windows NT, this function always returns EINVAL.  */
int readlink (char *path, char *buf, int buf_size)
{
    errno = EINVAL;
    return -1;
}

// From win32.cpp
bool validate_filename(const char *path, bool warn);

/* Rename for NT which works for read only files.  */
int wnt_rename (const char *from, const char *to)
{
	if(!validate_filename(from,false))
		return -1;
	if(!validate_filename(to,false))
		return -1;

    int result, save_errno, count;
	uc_name fn_from = from;
	uc_name fn_to = to;
	DWORD fa_from;
	DWORD fa_to = GetFileAttributes(fn_to);

	if(!fncmp(from,to))
	{
		TRACE(3,"Rename to self - %s",from);
		return 0;
	}
	TRACE(3,"wnt_rename(%s,%s)",from,to);

	fa_from = GetFileAttributes(fn_from);
	if(fa_from==0xFFFFFFFF)
	{
		_dosmaperr(GetLastError());
		TRACE(3,"wnt_rename: no file?");
		return -1;
	}

	if(fa_to!=0xFFFFFFFF && (fa_to&FILE_ATTRIBUTE_READONLY))
	{
		if(!SetFileAttributes(fn_to,fa_to&~FILE_ATTRIBUTE_READONLY))
		{
			_dosmaperr(GetLastError());
			TRACE(3,"wnt_rename: couldn't set file attributes");
			return -1;
		}
	}

#ifdef CVS95
	/* Win95 doesn't support atomic rename over existing files */
	DeleteFile(fn_to);
	count=0;
	while(!(result = MoveFile(fn_from,fn_to)))
	{
		save_errno = GetLastError();
		TRACE(3,"MoveFile returned error %08x",save_errno);

		if(save_errno != ERROR_ACCESS_DENIED)
			break;
		Sleep(100);
		count++;
		if(count==100)
		{
			printf("Unable to rename file %s to %s for 10 seconds, giving up...\n",fn_root(from),fn_root(to),count/10,count>10?"s":"");
			break;
		}
		if(!(count%10))
			printf("Unable to rename file %s to %s for %d second%s, still trying...\n",fn_root(from),fn_root(to),count/10,count>10?"s":"");
	}
#else
	count=0;
	while(!(result = MoveFileEx(fn_from,fn_to,MOVEFILE_COPY_ALLOWED|MOVEFILE_REPLACE_EXISTING)))
	{
		save_errno = GetLastError();
		TRACE(3,"MoveFile returned error %08x",save_errno);

		if(save_errno != ERROR_ACCESS_DENIED)
			break;
		Sleep(100);
		count++;
		if(count==100)
		{
			printf("Unable to rename file %s to %s for 10 seconds, giving up...\n",fn_root(from),fn_root(to),count/10,count>10?"s":"");
			break;
		}
		if(!(count%10))
			printf("Unable to rename file %s to %s for %d second%s, still trying...\n",fn_root(from),fn_root(to),count/10,count>10?"s":"");
	}
#endif
	if(result)
		SetFileAttributes(fn_to,fa_from);
	else
	{
		SetFileAttributes(fn_from,fa_from);
		if(fa_to!=0xFFFFFFFF)
			SetFileAttributes(fn_to,fa_to);
		_dosmaperr(save_errno);
	}
    return result?0:-1;
}

/*
 * Rename a file and die if it fails
 */
void rename_file (const char *from, const char *to)
{
	TRACE(1,"rename(%s,%s)",from,to);

    if (noexec)
		return;

    if (CVS_RENAME (from, to) < 0)
	{
		error (1, errno, "cannot rename file %s to %s", fn_root(from), fn_root(to));
	}
}

/*
 * unlink a file, if possible.
 */
int unlink_file (const char *f)
{
	DWORD fa;
	uc_name fn = f;

	TRACE(1,"unlink_file(%s)",f);
	if (noexec)
	{
		return (0);
	}

	fa = GetFileAttributes(fn);
	if(fa==0xFFFFFFFF)
	{
		_dosmaperr(GetLastError());
		return -1;
	}

	if(fa&FILE_ATTRIBUTE_READONLY)
		SetFileAttributes(fn,fa&~FILE_ATTRIBUTE_READONLY);

	if(!DeleteFile(fn))
	{
		_dosmaperr(GetLastError());
		return -1;
	}
	return 0;
}

static BOOL _RecursiveDelete()
{
	WIN32_FIND_DATA wfd;
	HANDLE hFind;

	hFind = FindFirstFile(_T("*.*"),&wfd);
	if(hFind==INVALID_HANDLE_VALUE)
		return FALSE;
	do
	{
		if(wfd.cFileName[0]!='.' || (wfd.cFileName[1]!='.' && wfd.cFileName[1]!='\0'))
		{
			if(wfd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
			{
				SetCurrentDirectory(wfd.cFileName);
				if(!_RecursiveDelete())
				{
					FindClose(hFind);
					return FALSE;
				}
				SetCurrentDirectory(_T(".."));
				if(!RemoveDirectory(wfd.cFileName))
				{
					FindClose(hFind);
					return FALSE;
				}
			}
			else
			{
				if(wfd.dwFileAttributes&FILE_ATTRIBUTE_READONLY)
					SetFileAttributes(wfd.cFileName,wfd.dwFileAttributes&~FILE_ATTRIBUTE_READONLY);
				if(!DeleteFile(wfd.cFileName))
				{
					FindClose(hFind);
					return FALSE;
				}
			}
		}
	} while(FindNextFile(hFind,&wfd));
	FindClose(hFind);
	return TRUE;
}

static BOOL RecursiveDelete(LPCTSTR szDir)
{
	TCHAR oldDir[MAX_PATH];
	GetCurrentDirectory(sizeof(oldDir)/sizeof(oldDir[0]),oldDir);
	if(!SetCurrentDirectory(szDir))
		return FALSE;
	if(!_RecursiveDelete())
		return FALSE;
	SetCurrentDirectory(oldDir);
	return RemoveDirectory(szDir);
}

/*
 * Unlink a file or dir, if possible.  If it is a directory do a deep
 * removal of all of the files in the directory.  Return -1 on error
 * (in which case errno is set).
 */
int unlink_file_dir (const char *f)
{
	DWORD fa;
	uc_name fn = f;

	TRACE(1,"unlink_file_dir(%s)",f);
    if (noexec)
	{
		return (0);
	}

	fa = GetFileAttributes(fn);
	if(fa==0xFFFFFFFF)
	{
		_dosmaperr(GetLastError());
		return -1;
	}

	if(fa&FILE_ATTRIBUTE_READONLY)
		SetFileAttributes(fn,fa&~FILE_ATTRIBUTE_READONLY);

	if(fa&FILE_ATTRIBUTE_DIRECTORY)
	{
		if(!RecursiveDelete(fn))
		{
			_dosmaperr(GetLastError());
			return -1;
		}
		return 0;
	}
	else
	{
		if(!DeleteFile(fn))
		{

			_dosmaperr(GetLastError());
			return -1;
		}

	    /* We were able to remove the file from the disk */
		return 0;
	}
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
 * Use memory mapped files as it's a lot faster than allocating real memory, especially
 * for larger files
 */
int xcmp (const char *file1, const char *file2)
{
	uc_name fn = file1;
	uc_name fn2 = file2;

	HANDLE hFile1 = CreateFile(fn,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
	if(hFile1==INVALID_HANDLE_VALUE)
	{
		_dosmaperr(GetLastError());
		error (1, errno, "cannot open %s for comparing", file1);
	}
	HANDLE hFile2 = CreateFile(fn2,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
	if(hFile2==INVALID_HANDLE_VALUE)
	{
		_dosmaperr(GetLastError());
		CloseHandle(hFile1);
		error (1, errno, "cannot open %s for comparing", file2);
	}

	DWORD dwSize1=GetFileSize(hFile1,NULL);
	if(dwSize1==INVALID_FILE_SIZE && GetLastError()!=NO_ERROR)
	{
		_dosmaperr(GetLastError());
		CloseHandle(hFile1);
		CloseHandle(hFile2);
		error (1, errno, "cannot get file %s size",file1);
	}
	DWORD dwSize2=GetFileSize(hFile2,NULL);
	if(dwSize2==INVALID_FILE_SIZE && GetLastError()!=NO_ERROR)
	{
		_dosmaperr(GetLastError());
		CloseHandle(hFile1);
		CloseHandle(hFile2);
		error (1, errno, "cannot get file %s size",file2);
	}

	if(!dwSize1 && !dwSize2)
	{
		CloseHandle(hFile1);
		CloseHandle(hFile2);
		return 0;
	}

	if(dwSize1!=dwSize2)
	{
		CloseHandle(hFile1);
		CloseHandle(hFile2);
		return dwSize1-dwSize2;
	}

	HANDLE hMap1 = CreateFileMapping(hFile1,0,PAGE_READONLY,0,0,NULL);
	if(hMap1==INVALID_HANDLE_VALUE)
	{
		_dosmaperr(GetLastError());
		CloseHandle(hFile1);
		CloseHandle(hFile2);
		error (1, errno, "cannot get map file %s",file1);
	}

	HANDLE hMap2 = CreateFileMapping(hFile1,0,PAGE_READONLY,0,0,NULL);
	if(hMap2==INVALID_HANDLE_VALUE)
	{
		_dosmaperr(GetLastError());
		CloseHandle(hMap1);
		CloseHandle(hFile1);
		CloseHandle(hFile2);
		error (1, errno, "cannot get map file %s",file2);
	}

	LPVOID lpFile1 = MapViewOfFile(hMap1, FILE_MAP_READ, 0, 0, 0);
	if(!lpFile1)
	{
		_dosmaperr(GetLastError());
		CloseHandle(hMap1);
		CloseHandle(hMap2);
		CloseHandle(hFile1);
		CloseHandle(hFile2);
		error (1, errno, "cannot get map view of file %s",file1);
	}

	LPVOID lpFile2 = MapViewOfFile(hMap1, FILE_MAP_READ, 0, 0, 0);
	if(!lpFile2)
	{
		_dosmaperr(GetLastError());
		UnmapViewOfFile(lpFile1);
		CloseHandle(hMap1);
		CloseHandle(hMap2);
		CloseHandle(hFile1);
		CloseHandle(hFile2);
		error (1, errno, "cannot get map view of file %s",file2);
	}

	int diff = memcmp(lpFile1, lpFile2, dwSize1);

	UnmapViewOfFile(lpFile1);
	UnmapViewOfFile(lpFile2);
	CloseHandle(hMap1);
	CloseHandle(hMap2);
	CloseHandle(hFile1);
	CloseHandle(hFile2);

    return diff;
}


/* Return non-zero iff FILENAME is absolute.
   Trivial under Unix, but more complicated under other systems.  */
int
isabsolute (const char *filename)
{
    /* FIXME: This routine seems to interact poorly with
       strip_trailing_slashes.  For example, specify ":local:r:\" as
       CVSROOT.  The CVS/Root file will contain ":local:r:" and then
       isabsolute will complain about the root not being an absolute
       pathname.  My guess is that strip_trailing_slashes is the right
       place to fix this.  */
    return (ISDIRSEP (filename[0])
            || (filename[0] != '\0'
                && filename[1] == ':'
                && ISDIRSEP (filename[2])));
}

int isabsolute_remote(const char *filename)
{
	return isabsolute(filename);
}

/* Return a pointer into PATH's last component.  */
const char *last_component (const char *path)
{
    const char *scan;
    const char *last = 0;

    for (scan = path; *scan; scan++)
        if (ISDIRSEP (*scan))
	    last = scan;

    if (last && (last != path))
        return last + 1;
    else
        return path;
}


/* NT has two evironment variables, HOMEPATH and HOMEDRIVE, which,
   when combined as ${HOMEDRIVE}${HOMEPATH}, give the unix equivalent
   of HOME.  Some NT users are just too unixy, though, and set the
   HOME variable themselves.  Therefore, we check for HOME first, and
   then try to combine the other two if that fails.

   Looking for HOME strikes me as bogus, particularly if the only reason
   is to cater to "unixy users".  On the other hand, if the reasoning is
   there should be a single variable, rather than requiring people to
   set both HOMEDRIVE and HOMEPATH, then it starts to make a little more
   sense.

   Win95: The system doesn't set HOME, HOMEDRIVE, or HOMEPATH (at
   least if you set it up as the "all users under one user ID" or
   whatever the name of that option is).  Based on thing overheard on
   the net, it seems that users of the pserver client have gotten in
   the habit of setting HOME (if you don't use pserver, you can
   probably get away without having a reasonable return from
   get_homedir.  Of course you lose .cvsrc and .cvsignore, but many
   users won't notice).  So it would seem that we should be somewhat
   careful if we try to change the current behavior.

   NT 3.51 or NT 4.0: I haven't checked this myself, but I am told
   that HOME gets set, but not to the user's home directory.  It is
   said to be set to c:\users\default by default. 

   Win2k:  Apparently this gets set to c:\ by default (ugh).
 */

char *stripslash(char *path)
{
	char *p=path+strlen(path)-1;
	if(!*path)
		return path; // null string, just in case
	if(*p=='\\' || *p=='/')
		*p='\0';
	return path;
}

char *get_homedir ()
{
    static char *pathbuf;
    const char *hd, *hp;

    if (pathbuf != NULL)
		return stripslash(pathbuf);
    else if ((hd = CProtocolLibrary::GetEnvironment ("HOME")))
	{
		pathbuf = xstrdup(hd);
		return stripslash(pathbuf);
	}
    else if ((hd = CProtocolLibrary::GetEnvironment ("HOMEDRIVE")) && (hp = CProtocolLibrary::GetEnvironment ("HOMEPATH")))
    {
		pathbuf = (char*)xmalloc (strlen (hd) + strlen (hp) + 5);
		strcpy (pathbuf, hd);
		strcat (pathbuf, hp);

		return stripslash(pathbuf);
    }
    else
		return NULL;
}

/* See cvs.h for description.  */
void expand_wild (int argc, char **argv, int *pargc, char ***pargv)
{
    int i;
    int new_argc;
    char **new_argv;
    /* Allocated size of new_argv.  We arrange it so there is always room for
	   one more element.  */
    int max_new_argc;
    int ugly_dot_hack;

    new_argc = 0;
    /* Add one so this is never zero.  */
    max_new_argc = argc + 1;
    new_argv = (char **) xmalloc (max_new_argc * sizeof (char *));
    for (i = 0; i < argc; ++i)
    {
	HANDLE h;
	WIN32_FIND_DATAA fdata;

	/* These variables help us extract the directory name from the
           given pathname. */

	char *last_forw_slash, *last_back_slash, *end_of_dirname;
	int dirname_length = 0;

	/* FindFirstFile doesn't return pathnames, so we have to do
	   this ourselves.  Luckily, it's no big deal, since globbing
	   characters under Win32s can only occur in the last segment
	   of the path.  For example,
                /a/path/q*.h                      valid
	        /w32/q*.dir/cant/do/this/q*.h     invalid */

	/* Win32 can handle both forward and backward slashes as
           filenames -- check for both. */
	     
	last_forw_slash = strrchr (argv[i], '/');
	last_back_slash = strrchr (argv[i], '\\');

#define cvs_max(x,y) ((x >= y) ? (x) : (y))

	/* FIXME: this comparing a NULL pointer to a non-NULL one is
	   extremely ugly, and I strongly suspect *NOT* sanctioned by
	   ANSI C.  The code should just use last_component instead.  */
	end_of_dirname = cvs_max (last_forw_slash, last_back_slash);

	if (end_of_dirname == NULL)
	  dirname_length = 0;	/* no directory name */
	else
	  dirname_length = end_of_dirname - argv[i] + 1; /* include slash */

	/* This is an attempt to get around the dotting problem */
	if(!stricmp(argv[i],".") || !stricmp(argv[i],"..")) ugly_dot_hack = 1;
	else ugly_dot_hack = 0;

	h = FindFirstFileA (argv[i], &fdata);
	if (h == INVALID_HANDLE_VALUE)
	{
		/* No match.  The file specified didn't contain a wildcard (in which case
		   we clearly should return it unchanged), or it contained a wildcard which
		   didn't match (in which case it might be better for it to be an error,
		   but we don't try to do that).  */
		new_argv [new_argc++] = xstrdup (argv[i]);
		if (new_argc == max_new_argc)
		{
		    max_new_argc *= 2;
		    new_argv = (char**)xrealloc (new_argv, max_new_argc * sizeof (char *));
		}
	}
	else
	{
	    while (1)
	    {
			if(!strcmp(fdata.cFileName,".") || !strcmp(fdata.cFileName,".."))
				goto skip_dot;
		new_argv[new_argc] =
		    (char *) xmalloc (strlen (fdata.cFileName) + 1
				      + dirname_length);

		/* Copy the directory name, if there is one. */

		if (dirname_length)
		{
		    strncpy (new_argv[new_argc], argv[i], dirname_length);
		    new_argv[new_argc][dirname_length] = '\0';
		}
		else
		    new_argv[new_argc][0] = '\0';

		/* Copy the file name. */
		
			if (ugly_dot_hack ||
				(fncmp (argv[i] + dirname_length, fdata.cFileName) == 0))
		    /* We didn't expand a wildcard; we just matched a filename.
		       Use the file name as specified rather than the filename
		       which exists in the directory (they may differ in case).
		       This is needed to make cvs add on a directory consistently
		       use the name specified on the command line, but it is
		       probably a good idea in other contexts too.  */
		    strcpy (new_argv[new_argc], argv[i]);
		else
		    strcat (new_argv[new_argc], fdata.cFileName);

		new_argc++;

		if (new_argc == max_new_argc)
		{
		    max_new_argc *= 2;
		    new_argv = (char**)xrealloc (new_argv, max_new_argc * sizeof (char *));
		}
skip_dot:
		if (!FindNextFileA (h, &fdata))
		{
		    if (GetLastError () == ERROR_NO_MORE_FILES)
			break;
		    else
			error (1, errno, "cannot find %s", argv[i]);
		}
	    }
	    if (!FindClose (h))
		error (1, GetLastError (), "cannot close %s", argv[i]);
	}
    }
    *pargc = new_argc;
    *pargv = new_argv;
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
		error (0, errno, "Failed to close temporary file %s", fn);
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
	char tempdir[MAX_PATH];
	FILE *f;

	wnt_get_temp_dir(tempdir,sizeof(tempdir));
	*filename=(char*)xmalloc(MAX_PATH);
	GetTempFileNameA(tempdir,"cvs",0,*filename);
	f=CVS_FOPEN(*filename,"wb+");
	return f;
}

static void win32ize_root(char *arg)
{
	char *p;

	if(ISDIRSEP(arg[0]) && ISDIRSEP(arg[2]) && ISDIRSEP(arg[3]))
	{
		strcpy(arg,arg+1);
		arg[1]=':';
	}
	for(p=arg; *p; p++)
		if(*p=='\\')
			*p='/';
}

/* On case insensitive systems, find the real case */
/* On case sensitive ones, just return */
char *normalize_path(char *arg)
{
	if(!arg)
		return NULL;

	if(!filenames_case_insensitive)
		return arg;

	TRACE(3,"normalize_path(%s)",arg);
	win32ize_root(arg);
	TRACE(3,"...returns %s",arg);

	return arg;
}
