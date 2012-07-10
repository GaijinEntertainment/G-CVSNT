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
/* _EXPORT */
#ifndef SQLRECORDSET__H
#define SQLRECORDSET__H

#include "cvs_smartptr.h"

class CSqlField
{
public:
	CSqlField() { }
	virtual ~CSqlField() { }

	virtual operator int() =0;
	virtual operator long() =0;
	virtual operator unsigned() =0;
	virtual operator unsigned long() =0;
#if defined(_WIN32) || defined(_WIN64)
	virtual operator __int64() =0;
#else
	virtual operator long long() =0;
#endif
	virtual operator const char *() =0;
	virtual operator const wchar_t *() = 0;
};

class CSqlRecordset
{
public:
	CSqlRecordset() { }
	virtual ~CSqlRecordset() { }

	virtual bool Close() =0;
	virtual bool Closed() const =0;
	virtual bool Eof() const =0;
	virtual bool Next() =0;
	virtual CSqlField* operator[](size_t item) const =0;
	virtual CSqlField* operator[](int item) const =0;
	virtual CSqlField* operator[](const char *item) const =0;
};

typedef cvs::smartptr<CSqlRecordset,CSqlField*> CSqlRecordsetPtr;

#endif
