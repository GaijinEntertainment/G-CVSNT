/* error.c -- error handler for noninteractive utilities
   Copyright (C) 1990-1992 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

/* David MacKenzie */
/* Brian Berliner added support for CVS */

#include "cvs.h"

#include <stdio.h>
#include <stdarg.h>
#define VA_START(args, lastarg) va_start(args, lastarg)

#include <stdlib.h>
#include <string.h>

#ifndef strerror
extern char *strerror ();
#endif

void error_exit ()
{
#ifdef SERVER_SUPPORT
    /* Since we won't return to the caller, we should write a final 'error' to the client */
    if(server_active)
      server_error_exit();
#endif

	CTriggerLibrary lib;
	lib.CloseAllTriggers();
	tf_loaded=false;
    rcs_cleanup (0);
    Lock_Cleanup ();
#ifdef SERVER_SUPPORT
    if (server_active)
		server_cleanup (0);
#endif
#ifdef SYSTEM_CLEANUP
    /* Hook for OS-specific behavior, for example socket subsystems on
       NT and OS2 or dealing with windows and arguments on Mac.  */
    SYSTEM_CLEANUP ();
#endif

	CCvsgui::Close(EXIT_FAILURE);

    exit (EXIT_FAILURE);
}

/* Print the program name and error message MESSAGE, which is a printf-style
   format string with optional args.  This is a very limited printf subset:
   %s, %d, %c, %x and %% only (without anything between the % and the s,
   d, &c).  Callers who want something fancier can use sprintf.

   If ERRNUM is nonzero, print its corresponding system error message.
   Exit with status EXIT_FAILURE if STATUS is nonzero.  If MESSAGE is "",
   no need to print a message.

   error() does not molest errno; some code (e.g. Entries_Open) depends
   on being able to say something like:
      error (0, 0, "foo");
      error (0, errno, "bar");

   */

/* VARARGS */
void error (int status, int errnum, const char *message, ...)
{
	static int in_error = 0;

    int save_errno = errno;

	if(in_error)
	{
		if(status)
			error_exit();
		return;
	}
	in_error = 1;

    if (message[0] != '\0')
    {
	va_list args;
	const char *p;
	char *q;
	char *str;
	int num;
	long lnum;
	unsigned int unum;
	unsigned long ulnum;
	int ch;
	char buf[100];

	cvs_outerr (program_name, 0);
	if (command_name && *command_name)
	{
	    cvs_outerr (" ", 1);
	    if (status != 0)
		cvs_outerr ("[", 1);
	    cvs_outerr (command_name, 0);
	    if (status != 0)
		cvs_outerr (" aborted]", 0);
	}
	cvs_outerr (": ", 2);

	VA_START (args, message);
	p = message;
	while ((q = (char*)strchr (p, '%')) != NULL)
	{
	    static const char msg[] =
		"\ninternal error: bad % in error()\n";
	    if (q - p > 0)
		cvs_outerr (p, q - p);

	    switch (q[1])
	    {
	    case 's':
		str = va_arg (args, char *);
		if(str)
			cvs_outerr (str, strlen (str));
		else
			cvs_outerr ("(null)", 6);
		break;
	    case 'd':
		num = va_arg (args, int);
		sprintf (buf, "%d", num);
		cvs_outerr (buf, strlen (buf));
		break;
	    case 'l':
		if (q[2] == 'd')
		{
		    lnum = va_arg (args, long);
		    sprintf (buf, "%ld", lnum);
		}
		else if (q[2] == 'u')
		{
		    ulnum = va_arg (args, unsigned long);
		    sprintf (buf, "%lu", ulnum);
		}
		else goto bad;
		cvs_outerr (buf, strlen (buf));
		q++;
		break;
	    case 'x':
		unum = va_arg (args, unsigned int);
		sprintf (buf, "%x", unum);
		cvs_outerr (buf, strlen (buf));
		break;
	    case 'c':
		ch = va_arg (args, int);
		buf[0] = ch;
		cvs_outerr (buf, 1);
		break;
	    case '%':
		cvs_outerr ("%", 1);
		break;
	    default:
	    bad:
		cvs_outerr (msg, sizeof (msg) - 1);
		/* Don't just keep going, because q + 1 might point to the
		   terminating '\0'.  */
		goto out;
	    }
	    p = q + 2;
	}
	cvs_outerr (p, strlen (p));
    out:
	va_end (args);

	if (errnum != 0)
	{
	    cvs_outerr (": ", 2);
	    cvs_outerr (strerror (errnum), 0);
	}
	cvs_outerr ("\n", 1);
    }

    if (status)
	{
		cvs_flushout();
		cvs_flusherr();
		error_exit ();
	}
	cvs_flusherr();
    errno = save_errno;
	in_error = 0;
}

/* Print the program name and error message MESSAGE, which is a printf-style
   format string with optional args to the file specified by FP.
   If ERRNUM is nonzero, print its corresponding system error message.
   Exit with status EXIT_FAILURE if STATUS is nonzero.  */
/* VARARGS */
void fperrmsg (FILE *fp, int status, int errnum, char *message, ...)
{
    va_list args;

    fprintf (fp, "%s: ", program_name);
    VA_START (args, message);
    vfprintf (fp, message, args);
    va_end (args);
    if (errnum)
	fprintf (fp, ": %s", strerror (errnum));
    putc ('\n', fp);
    fflush (fp);
    if (status)
	error_exit ();
}
