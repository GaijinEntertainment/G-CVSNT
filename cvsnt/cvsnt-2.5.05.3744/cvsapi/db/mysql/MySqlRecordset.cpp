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
#define strcasecmp stricmp
#endif

#include <config.h>
#include <cvsapi.h>

#ifdef _WIN32
#include "mysql-3.23/mysql.h"
#else
#include <mysql.h>
#endif

#include <string>

#include "MySqlRecordset.h"

CMySqlField::CMySqlField()
{
	field=NULL;
	data=NULL;
}

CMySqlField::~CMySqlField()
{
}

CMySqlField::operator int()
{
	return(atoi(*(char**)data));
}

CMySqlField::operator long()
{
	return (long)operator unsigned long();
}

CMySqlField::operator unsigned()
{
	return (unsigned)operator unsigned long();
}

CMySqlField::operator unsigned long()
{
	return strtoul(*(char**)data,NULL,10);
}

#if defined(_WIN32) || defined(_WIN64)
CMySqlField::operator __int64()
{
	__int64 s;
	if(!sscanf(*(char**)data,"%I64u",&s))
		return 0;
	return s;
}
#else
CMySqlField::operator long long()
{
	long long s;
	if(!sscanf(*(char**)data,"%Lu",&s))
		return 0;
	return s;
}
#endif

CMySqlField::operator const char *()
{
	return *(const char **)data;
}

CMySqlField::operator const wchar_t *()
{
	wdata=cvs::wide(operator const char *());
	return wdata.c_str();
}

/**********************************************************************/

CMySqlRecordset::CMySqlRecordset()
{
	m_result = NULL;
	m_bEof=false;
}

CMySqlRecordset::~CMySqlRecordset()
{
	Close();
}

bool CMySqlRecordset::Init()
{
	m_bEof=false;

	m_fields = mysql_fetch_fields(m_result);
	if (!m_fields)
		return false;
	m_num_fields = mysql_num_fields(m_result);

	m_sqlfields.resize(m_num_fields);
	for(int n=0; n<m_num_fields; n++)
	{
		m_sqlfields[n].field = m_fields+n;
		m_sqlfields[n].data = NULL;
	}

	Next();
	return true;
};

bool CMySqlRecordset::Close()
{
	if(m_result)
		mysql_free_result(m_result);
	m_result=NULL;
	return true;
}

bool CMySqlRecordset::Closed() const
{
	if(!m_result)
		return true;
	return false;
}

bool CMySqlRecordset::Eof() const
{
	return m_bEof;
}

bool CMySqlRecordset::Next()
{
	MYSQL_ROW row = mysql_fetch_row(m_result);
	if(row==NULL)
		m_bEof=true;
	for(int n=0; n<m_num_fields; n++)
		m_sqlfields[n].data = row+n;
	return !m_bEof;
}

CSqlField* CMySqlRecordset::operator[](size_t item) const
{
	if(item>=(size_t)m_num_fields)
		return NULL;
	return (CSqlField*)&m_sqlfields[item];
}

CSqlField* CMySqlRecordset::operator[](int item) const
{
	if(item<0 || item>=m_num_fields)
		return NULL;
	return (CSqlField*)&m_sqlfields[item];
}

CSqlField* CMySqlRecordset::operator[](const char *item) const
{
	for(size_t n=0; n<(size_t)m_num_fields; n++)
		if(!strcasecmp(m_fields[n].name,item))
			return (CSqlField*)&m_sqlfields[n];
	return NULL;
}

