/* xmalloc.c -- malloc with out of memory checking
   Copyright (C) 1990-1997, 98, 99 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>

#if STDC_HEADERS
# include <stdlib.h>
#else
void *calloc ();
void *malloc ();
void *realloc ();
void free ();
#endif

#include "xalloc.h"
#define XTERN extern
#include "common.h"
#include "util.h"
#undef XTERN
#define XTERN
#include <Utils/TortoiseException.h>

#ifndef EXIT_FAILURE
# define EXIT_FAILURE 1
#endif

/* Exit value when the requested amount of memory is not available.
   The caller may set it to some other value.  */
int xalloc_exit_failure = EXIT_FAILURE;

/* If non NULL, call this function when memory is exhausted. */
void (*xalloc_fail_func) () = 0;

/* If XALLOC_FAIL_FUNC is NULL, or does return, display this message
   before exiting when memory is exhausted.   */
char *const xalloc_msg_memory_exhausted = "Memory exhausted";

static void
xalloc_fail (void)
{
  if (xalloc_fail_func)
    (*xalloc_fail_func) ();
  fatal(xalloc_msg_memory_exhausted);
}

/* Allocate N bytes of memory dynamically, with error checking.  */

char *
xmalloc (size_t n)
{
   char * p = reinterpret_cast<char*>(malloc(n));
   if (p == 0)
      xalloc_fail ();
   return p;
}

/* Change the size of an allocated block of memory P to N bytes,
   with error checking.
   If P is NULL, run xmalloc.  */

char *
xrealloc (char *p, size_t n)
{
   p = reinterpret_cast<char*>(realloc (p, n));
   if (p == 0)
      xalloc_fail ();
   return p;
}

/* Allocate memory for N elements of S bytes, with error checking.  */

char *
xcalloc (size_t n, size_t s)
{
  char *p = reinterpret_cast<char*>(calloc (n, s));
  if (p == 0)
    xalloc_fail ();
  return p;
}
