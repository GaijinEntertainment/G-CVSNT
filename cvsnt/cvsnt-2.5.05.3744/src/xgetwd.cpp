/* xgetwd.c -- return current directory with unlimited length
   Copyright (C) 1992, 1997 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

/* Derived from xgetcwd.c in e.g. the GNU sh-utils.  */

#define MAIN_CVS
#include <config.h>
#include "cvs.h"

#include "system.h"

#include <stdio.h>
#include <errno.h>
#ifndef errno
extern int errno;
#endif
#include <sys/types.h>

/* Amount by which to increase buffer size when allocating more space. */
#define PATH_INCR 32

/* Return the current directory, newly allocated, arbitrarily long.
   Return NULL and set errno on error. */

char *xgetwd ()
{
	return xstrdup(CDirectoryAccess::getcwd());
}
