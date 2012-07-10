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

#ifndef STRINGDIFF__H
#define STRINGDIFF__H

#include "DiffBase.h"

/* Diff two strings, for testing... not much use for anything else */
class CStringDiff : protected CDiffBase
{
public:
	CVSAPI_EXPORT CStringDiff();
	CVSAPI_EXPORT virtual ~CStringDiff();

	CVSAPI_EXPORT int ExecuteDiff(const char *s1, const char *s2);
	CVSAPI_EXPORT void DebugDump();

protected:
	virtual const void *IndexFn(const void *s, int idx);
	virtual int CompareFn(const void *a, const void *b);
};

#endif 
