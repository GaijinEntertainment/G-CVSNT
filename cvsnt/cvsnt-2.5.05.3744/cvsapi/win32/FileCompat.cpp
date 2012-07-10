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
/* Win32 specific */
#include <config.h>
#include "../lib/api_system.h"

#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <windows.h>
#include <stdio.h>
#include <time.h>

/* ANSI C compatibility for timestamps - VC pads with zeros, C standard pads with spaces */

#undef asctime
char *wnt_asctime(const struct tm *tm)
{
	char *buf = asctime(tm);
	if(buf[8]=='0') buf[8]=' ';
	return buf;
}

#undef ctime
char *wnt_ctime(const time_t *t)
{
	static char future[] = "Fri Dec 31 23:59:59 1999";
	char *buf = _ctime64(t);
	if(!buf) /* Y2.038K bug in 32bit... */
		buf=future;
	else if(buf[8]=='0')
		buf[8]=' ';
	return buf;
}
