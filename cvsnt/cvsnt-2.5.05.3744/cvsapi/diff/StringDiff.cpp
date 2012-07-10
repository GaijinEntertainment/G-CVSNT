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
#include <config.h>
#include "../lib/api_system.h"

#include "DiffBase.h"
#include "StringDiff.h"

#include <stdio.h>
#include <string.h>

CStringDiff::CStringDiff()
{
}

CStringDiff::~CStringDiff()
{
}

int CStringDiff::ExecuteDiff(const char *s1, const char *s2)
{
	return CDiffBase::ExecuteDiff(s1,0,(int)strlen(s1),s2,0,(int)strlen(s2),0);
}

const void *CStringDiff::IndexFn(const void *s, int idx)
{
	return ((const unsigned char *)s)+idx;
}

int CStringDiff::CompareFn(const void *a, const void *b)
{
	return *(const unsigned char *)a - *(const unsigned char *)b;
}

void CStringDiff::DebugDump()
{
	static const char *op[] = { "??", "Match","Delete","Insert" };

	printf("String1: %s\n",(const char *)m_bufa);
	printf("String2: %s\n",(const char *)m_bufb);
	for(size_t n=0; n<m_ses.size(); n++)
	{
		printf("%s %d %d\n",op[m_ses[n].op],m_ses[n].off,m_ses[n].len);
	}
}
