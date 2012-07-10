#ifndef NDIR__H
#define NDIR__H

#include "api_system.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#include <sys/types.h>	/* ino_t definition */

#define	rewinddir(dirp)	seekdir(dirp, 0L)

struct direct
{
  _ino_t d_ino;			/* a bit of a farce */
  int d_reclen;			/* more farce */
  int d_namlen;			/* length of d_name */
  int d_hidden;			/* hidden file */
  struct direct *next;
  char d_name[1];			/* garentee null termination */
};

typedef struct 
{
	struct direct *current;
	struct direct *first;
} DIR;

CVSAPI_EXPORT DIR *opendirA(const char *name);
CVSAPI_EXPORT DIR *opendirW(const char *name);

#ifdef _UNICODE
#define opendir opendirW
#else
#define opendir opendirA
#endif

CVSAPI_EXPORT void closedir (DIR *dirp);
CVSAPI_EXPORT void seekdir (DIR *dirp, long off);
CVSAPI_EXPORT long telldir (DIR *dirp);
CVSAPI_EXPORT struct direct *readdir (DIR *dirp);

#ifdef __cplusplus
}
#endif

#endif
