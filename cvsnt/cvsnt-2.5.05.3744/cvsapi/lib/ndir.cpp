/*
	CVSNT Generic API
    Copyright (C) 2004 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* This was originally based on a file distributed with CVS.  Almost no code
   from that original file now remains */

/* Win32 port for CVS/NT by Tony Hoyle, January 2000 */
/* Adapted for Unicode/UTF8 by Tony Hoyle, November 2004 */

#ifdef _WIN32
// Microsoft braindamage reversal.  
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#endif

#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <Windows.h>
#include <errno.h>
#include <tchar.h>
#include <malloc.h>

#include <config.h>
#include "../lib/api_system.h"

#include "ndir.h"
#include "../FileAccess.h"

DIR *opendirA (const char *name)
{
	WIN32_FIND_DATAA fd;
	HANDLE h;
	DIR *dir = NULL;
	DWORD fa = GetFileAttributesA(name);
	char fn[MAX_PATH];

	if(fa==(DWORD)-1 || !(fa&FILE_ATTRIBUTE_DIRECTORY))
    {
       int err = GetLastError();
       switch (err) {
            case ERROR_NO_MORE_FILES:
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
                errno = ENOENT;
                break;

            case ERROR_NOT_ENOUGH_MEMORY:
                errno = ENOMEM;
                break;

            default:
                errno = EINVAL;
                break;
        }
		return NULL;
    }

	strcpy(fn,name);
	strcat(fn,"\\*.*");
	h=FindFirstFileA(fn,&fd);
	if(h!=INVALID_HANDLE_VALUE)
    {
		dir = (DIR *)malloc(sizeof(DIR));
		dir->current=NULL;
		do
		{
			struct direct *ent = (struct direct *)malloc(sizeof(struct direct)+strlen(fd.cFileName)+1);
			memset(ent,0,sizeof(struct direct));
			strcpy(ent->d_name,fd.cFileName);
			if(!dir->current)
				dir->first=ent;
			else
				dir->current->next = ent;
			dir->current = ent;
		} while(FindNextFileA(h,&fd));
		FindClose(h);
		dir->current = NULL;
	}
	else
	{
	   errno=EIO;
	   return NULL;
	}
	return dir;
}

DIR *opendirW (const char *name)
{
	WIN32_FIND_DATAW fd;
	HANDLE h;
	DIR *dir = NULL;
	wchar_t fn[MAX_PATH];

	CFileAccess::Win32Wide u_name(name);

	DWORD fa = GetFileAttributesW(u_name);

	if(fa==(DWORD)-1 || !(fa&FILE_ATTRIBUTE_DIRECTORY))
    {
       int err = GetLastError();
       switch (err) {
            case ERROR_NO_MORE_FILES:
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
                errno = ENOENT;
                break;

            case ERROR_NOT_ENOUGH_MEMORY:
                errno = ENOMEM;
                break;

            default:
                errno = EINVAL;
                break;
        }
		return NULL;
    }

	wcscpy(fn,u_name);
	wcscat(fn,L"\\*.*");
	h=FindFirstFileW(fn,&fd);
	if(h!=INVALID_HANDLE_VALUE)
    {
		dir = (DIR *)malloc(sizeof(DIR));
		dir->current=NULL;
		do
		{
			CFileAccess::Win32Narrow a_name(fd.cFileName);
			struct direct *ent = (struct direct *)malloc(sizeof(struct direct)+strlen(a_name)+1);
			memset(ent,0,sizeof(struct direct));
			strcpy(ent->d_name,a_name);
			if(fd.dwFileAttributes&(FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_HIDDEN))
				ent->d_hidden=1;
			else
				ent->d_hidden=0;
			if(!dir->current)
				dir->first=ent;
			else
				dir->current->next = ent;
			dir->current = ent;
		} while(FindNextFileW(h,&fd));
		FindClose(h);
		dir->current = NULL;
	}
	else
	{
		errno = EIO;
		return NULL;
	}
	return dir;
}

void closedir (DIR *dirp)
{
	struct direct *ent = dirp->first, *next;
	while(ent)
	{
		next=ent->next;
		free(ent);
		ent=next;
	}
	free(dirp);
}


struct direct *readdir (DIR *dirp)
{
	struct direct *ent = dirp->current;
	if(!ent)
		ent=dirp->first;
	else
		ent=ent->next;
	dirp->current = ent;
	return ent;
}

void seekdir (DIR *dirp, long off)
{
	struct direct *ent = dirp->first;

	if(!off)
		dirp->current = NULL;
	else
	{
		while(ent && --off)
			ent=ent->next;
		dirp->current = ent;
	}
}

long telldir (DIR *dirp)
{
	long off = 0;
	struct direct *ent = dirp->first;
	struct direct *cur = dirp->current;

	while(ent && ent!=cur)
	{
		ent=ent->next;
		off++;
	}
	if(ent==cur)
		return off;
	else
		return 0;
}

