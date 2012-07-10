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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "api_system.h"
#include "fncmp.h"

static int g_case_sensitive  = 0;

CVSAPI_EXPORT int __case_sensitive()
{
	return g_case_sensitive;
}

CVSAPI_EXPORT void __set_case_sensitive(int cs)
{
	g_case_sensitive = cs;
}

int __cfc(char c, char d, int cs)
{
	if(c==d) return 0;
	if(IsSeparator(c) && IsSeparator(d)) return 0; 
	return cs?c-d:tolower((unsigned char)c)-tolower((unsigned char)d);
}

int __fncmp(const char *a, const char *b)
{
	int r;
	while(*a && *b)
	{
		if((r = __cfc(*a,*b,FsCaseSensitive))!=0)
			return r;
		a++;
		b++;
	}
	return (*a)-(*b);
}

int __fnncmp(const char *a, const char *b, size_t len)
{
	int r;
	while(len && *a && *b)
	{
		if((r = __cfc(*a,*b,FsCaseSensitive))!=0)
			return r;
		a++;
		b++;
		len--;
	}
	return len?(*a)-(*b):0;
}
