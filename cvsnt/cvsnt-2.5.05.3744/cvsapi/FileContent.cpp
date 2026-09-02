/*
	CVSNT Generic API - file content classification
    Copyright (C) 2026 Gaijin Entertainment

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
#include <config.h>
#include "lib/api_system.h"

#include <stdio.h>
#include <string.h>

#include "cvs_string.h"
#include "FileAccess.h"

/* The 8000-byte window and the NUL rule are what git uses for the same
   question: a binary file with no NUL in its first 8 KB is rare, a text
   file with one rarer still.  An unreadable file is reported as text so
   the caller sees the real open error later.  */
bool CFileAccess::looks_binary(const char *file)
{
	FILE *fp = fopen(file, "rb");
	if(!fp)
		return false;
	unsigned char buf[8000];
	size_t n = fread(buf, 1, sizeof(buf), fp);
	fclose(fp);
	if(n < 2)
		return false;
	/* UTF-16/32 text carries NULs by design; its BOM says so up front.  */
	if((buf[0]==0xff && buf[1]==0xfe) || (buf[0]==0xfe && buf[1]==0xff))
		return false;
	if(n >= 4 && buf[0]==0 && buf[1]==0 && buf[2]==0xfe && buf[3]==0xff)
		return false;
	return memchr(buf, 0, n) != NULL;
}
