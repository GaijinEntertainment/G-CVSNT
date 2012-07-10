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

#ifndef SQLITERECORDSET__H
#define SQLITERECORDSET__H

#include <vector>

class CSQLiteField : public CSqlField
{
	friend class CSQLiteRecordset;
public:
	CSQLiteField();
	virtual ~CSQLiteField();

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
	sqlite3_stmt *pStmt;
};


class CSQLiteRecordset : public CSqlRecordset
{
	friend class CSQLiteConnection;
public:
	CSQLiteRecordset();
	virtual ~CSQLiteRecordset();

	virtual bool Close();
	virtual bool Closed() const;
	virtual bool Eof() const;
	virtual bool Next();
	virtual CSqlField* operator[](size_t item) const;
	virtual CSqlField* operator[](int item) const;
	virtual CSqlField* operator[](const char *item) const;

protected:
	sqlite3_stmt *m_pStmt;
	bool m_bEof;
	int m_num_fields;
	std::vector<CSQLiteField> m_sqlfields;

	bool Init(sqlite3 *pDb, sqlite3_stmt *pStmt);
};

#endif


