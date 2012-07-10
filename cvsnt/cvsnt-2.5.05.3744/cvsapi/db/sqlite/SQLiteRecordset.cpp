/*
	CVSNT Generic API
    Copyright (C) 2004 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifdef _WIN32
// Microsoft braindamage reversal.  
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#define snprintf _snprintf
#endif

#include <config.h>
#include <cvsapi.h>

#include <sqlite3.h>

#include <string>

#include "SQLiteRecordset.h"

CSQLiteField::CSQLiteField()
{
}

CSQLiteField::~CSQLiteField()
{
}

CSQLiteField::operator int()
{
	return sqlite3_column_int(pStmt,field);
}

CSQLiteField::operator long()
{
	return (long)sqlite3_column_int64(pStmt,field);
}

CSQLiteField::operator unsigned()
{
	return (unsigned)sqlite3_column_int64(pStmt,field);
}

CSQLiteField::operator unsigned long()
{
	return (unsigned long)sqlite3_column_int64(pStmt,field);
}

#if defined(_WIN32) || defined(_WIN64)
CSQLiteField::operator __int64()
{
	return (__int64)sqlite3_column_int64(pStmt,field);
}
#else
CSQLiteField::operator long long()
{
	return (long long)sqlite3_column_int64(pStmt,field);
}
#endif

CSQLiteField::operator const char *()
{
	return (const char *)sqlite3_column_text(pStmt,field);
}

CSQLiteField::operator const wchar_t *()
{
	return (const wchar_t *)sqlite3_column_text16(pStmt,field);
}

/**********************************************************************/

CSQLiteRecordset::CSQLiteRecordset()
{
	m_pStmt = NULL;
}

CSQLiteRecordset::~CSQLiteRecordset()
{
	Close();
}

bool CSQLiteRecordset::Init(sqlite3 *pDb, sqlite3_stmt *pStmt)
{
	m_pStmt = pStmt;
	m_bEof = false;

	m_num_fields = sqlite3_column_count(m_pStmt);
	m_sqlfields.resize(m_num_fields);
	for(int n=0; n<m_num_fields; n++)
	{
		m_sqlfields[n].field = n;
		m_sqlfields[n].pStmt = m_pStmt;
		m_sqlfields[n].name = sqlite3_column_name(m_pStmt,n);
		m_sqlfields[n].type = sqlite3_column_type(m_pStmt,n);
	}

	Next();

	return true;
}

bool CSQLiteRecordset::Close()
{
	if(m_pStmt)
		sqlite3_finalize(m_pStmt);
	m_pStmt = NULL;
	m_bEof = true;
	return true;
}

bool CSQLiteRecordset::Closed() const
{
	if(m_pStmt)
		return false;
	return true;
}

bool CSQLiteRecordset::Eof() const
{
	return m_bEof;
}

bool CSQLiteRecordset::Next()
{
	int res = sqlite3_step(m_pStmt);
	switch(res)
	{
	case SQLITE_DONE:
		m_bEof = true;
		break;
	case SQLITE_ROW:
	case SQLITE_OK:
		break;
	default:
		// Error
		m_bEof = true;
		CServerIo::trace(3,"sqlite3_step returned %d\n",res);
		break;
	}
	return !m_bEof;
}

CSqlField* CSQLiteRecordset::operator[](size_t item) const
{
	if(item>=(size_t)m_num_fields)
		return NULL;
	return (CSqlField*)&m_sqlfields[item];
}

CSqlField* CSQLiteRecordset::operator[](int item) const
{
	if(item<0 || item>=m_num_fields)
		return NULL;
	return (CSqlField*)&m_sqlfields[item];
}

CSqlField* CSQLiteRecordset::operator[](const char *item) const
{
	for(size_t n=0; n<(size_t)m_num_fields; n++)
		if(!strcasecmp(m_sqlfields[n].name.c_str(),item))
			return (CSqlField*)&m_sqlfields[n];
	CServerIo::error("Database error - field '%s' not found in recordset.",item);
	return NULL;
}

