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

/* We Use Mysql 3.23 for the API.  Linking to Mysql 4.x would force all calling applications
   to be GPL.  If you make a version that does this please rename the DLL & make it plain that it's
   GPL only. */

#ifndef MYSQLRECORDSET__H
#define MYSQLRECORDSET__H

#include <vector>
#include <string>

class CMySqlField : public CSqlField
{
	friend class CMySqlRecordset;
public:
	CMySqlField();
	virtual ~CMySqlField();

	virtual operator int();
	virtual operator long();
	virtual operator unsigned();
	virtual operator unsigned long();
#if defined(_WIN32) || defined(_WIN64)
	virtual operator __int64();
#else
	virtual operator long long();
#endif
	virtual operator const char *();
	virtual operator const wchar_t *();

protected:
	MYSQL_FIELD *field;
	void *data;
	char num_string[64];
	std::wstring wdata;
	unsigned long length;
	char is_null;
};


class CMySqlRecordset : public CSqlRecordset
{
	friend class CMySqlConnection;
public:
	CMySqlRecordset();
	virtual ~CMySqlRecordset();

	virtual bool Close();
	virtual bool Closed() const;
	virtual bool Eof() const;
	virtual bool Next();
	virtual CSqlField* operator[](size_t item) const;
	virtual CSqlField* operator[](int item) const;
	virtual CSqlField* operator[](const char *item) const;

protected:
	MYSQL_RES *m_result;
	MYSQL_FIELD *m_fields;
	int m_num_fields;
	bool m_bEof;
	std::vector<CMySqlField> m_sqlfields;

	bool Init();
};

#endif


