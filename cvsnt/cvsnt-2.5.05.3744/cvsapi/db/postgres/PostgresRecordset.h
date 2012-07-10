/*
	CVSNT Generic API
    Copyright (C) 2005 Tony Hoyle and March-Hare Software Ltd

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

#ifndef POSTGRESRECORDSET__H
#define POSTGRESRECORDSET__H

#include <vector>

class CPostgresRecordset;

class CPostgresField : public CSqlField
{
	friend class CPostgresRecordset;
public:
	CPostgresField();
	virtual ~CPostgresField();

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
	cvs::string name;
	int field;
	int type;
	CPostgresRecordset *rs;
	cvs::wstring wdata;
};


class CPostgresRecordset : public CSqlRecordset
{
	friend class CPostgresConnection;
	friend class CPostgresField;
public:
	CPostgresRecordset();
	virtual ~CPostgresRecordset();

	virtual bool Close();
	virtual bool Closed() const;
	virtual bool Eof() const;
	virtual bool Next();
	virtual CSqlField* operator[](size_t item) const;
	virtual CSqlField* operator[](int item) const;
	virtual CSqlField* operator[](const char *item) const;

protected:
	PGresult *m_pStmt;
	int m_num_fields;
	unsigned long m_num_rows;
	unsigned long m_current_row;
	std::vector<CPostgresField> m_sqlfields;
	wchar_t wdata;

	bool Init(PGconn *pDb, PGresult *pStmt);
};

#endif


