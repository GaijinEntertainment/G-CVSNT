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

#include <libpq-fe.h>

#include <string>

#include "PostgresRecordset.h"

CPostgresField::CPostgresField()
{
}

CPostgresField::~CPostgresField()
{
}

CPostgresField::operator int()
{
	int r = 0;
	sscanf(PQgetvalue(rs->m_pStmt,rs->m_current_row,field),"%ld",&r);
	return r;
}

CPostgresField::operator long()
{
	long r = 0;
	sscanf(PQgetvalue(rs->m_pStmt,rs->m_current_row,field),"%ld",&r);
	return r;
}

CPostgresField::operator unsigned()
{
	unsigned r = 0;
	sscanf(PQgetvalue(rs->m_pStmt,rs->m_current_row,field),"%u",&r);
	return r;
}

CPostgresField::operator unsigned long()
{
	unsigned long r = 0;
	sscanf(PQgetvalue(rs->m_pStmt,rs->m_current_row,field),"%lu",&r);
	return r;
}

#if defined(_WIN32) || defined(_WIN64)
CPostgresField::operator __int64()
{
	__int64 r = 0;
	sscanf(PQgetvalue(rs->m_pStmt,rs->m_current_row,field),"%I64d",&r);
	return r;
}
#else
CPostgresField::operator long long()
{
	long long r = 0;
	sscanf(PQgetvalue(rs->m_pStmt,rs->m_current_row,field),"%Ld",&r);
	return r;
}
#endif

CPostgresField::operator const char *()
{
	return PQgetvalue(rs->m_pStmt,rs->m_current_row,field);
}

CPostgresField::operator const wchar_t *()
{
	wdata=cvs::wide(operator const char *());
	return wdata.c_str();
}

/**********************************************************************/

CPostgresRecordset::CPostgresRecordset()
{
	m_pStmt = NULL;
}

CPostgresRecordset::~CPostgresRecordset()
{
	Close();
}

bool CPostgresRecordset::Init(PGconn *pDb, PGresult *pStmt)
{
	m_pStmt = pStmt;

	m_num_fields = PQnfields(m_pStmt);
	m_sqlfields.resize(m_num_fields);
	for(int n=0; n<m_num_fields; n++)
	{
		m_sqlfields[n].field = n;
		m_sqlfields[n].rs = this;
		m_sqlfields[n].name = PQfname(m_pStmt,n);
		m_sqlfields[n].type = PQftype(m_pStmt,n);
	}

	m_num_rows = PQntuples(m_pStmt);
	CServerIo::trace(3,"PG_rs: m_num_fields=%d; m_num_rows=%d",m_num_fields, m_num_rows);
	m_current_row = 0;

	return true;
}

bool CPostgresRecordset::Close()
{
	if(m_pStmt)
		PQclear(m_pStmt);
	m_pStmt = NULL;
	return true;
}

bool CPostgresRecordset::Closed() const
{
	if(m_pStmt)
		return false;
	return true;
}

bool CPostgresRecordset::Eof() const
{
	return m_current_row>=m_num_rows;
}

bool CPostgresRecordset::Next()
{
	if(m_current_row >= m_num_rows)
		return false;
	return (++m_current_row >= m_num_rows);
}

CSqlField* CPostgresRecordset::operator[](size_t item) const
{
	if(item>=(size_t)m_num_fields)
		return NULL;
	return (CSqlField*)&m_sqlfields[item];
}

CSqlField* CPostgresRecordset::operator[](int item) const
{
	if(item<0 || item>=m_num_fields)
		return NULL;
	return (CSqlField*)&m_sqlfields[item];
}

CSqlField* CPostgresRecordset::operator[](const char *item) const
{
	for(size_t n=0; n<(size_t)m_num_fields; n++)
		if(!strcasecmp(m_sqlfields[n].name.c_str(),item))
			return (CSqlField*)&m_sqlfields[n];
	CServerIo::error("Database error - field '%s' not found in recordset.",item);
	return NULL;
}

