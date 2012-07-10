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

#ifndef ORACLERECORDSET__H
#define ORACLERECORDSET__H

#include <vector>

class COracleConnection;

class COracleField : public CSqlField
{
	friend class COracleRecordset;
public:
	COracleField();
	virtual ~COracleField();

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
	OCIStmt *hStmt;
	cvs::wstring name;
	ub2 type;
	ub2 csid;
	OCITypeCode typecode;
	b1 precision;
	sb1 scale;
	ub2 size;
	ub2 fldlen,datalen[1],fldtype;
	void *data;
	int field;
	cvs::wstring tmpwstr;
	cvs::string tmpstr;
};


class COracleRecordset : public CSqlRecordset
{
	friend class COracleConnection;
public:
	COracleRecordset();
	virtual ~COracleRecordset();

	virtual bool Close();
	virtual bool Closed() const;
	virtual bool Eof() const;
	virtual bool Next();
	virtual CSqlField* operator[](size_t item) const;
	virtual CSqlField* operator[](int item) const;
	virtual CSqlField* operator[](const char *item) const;

protected:
	OCIStmt *m_hStmt;
	bool m_bEof;
	ub4 m_num_fields;
	std::vector<COracleField> m_sqlfields;
	COracleConnection *m_parent;

	bool Init(COracleConnection *parent, OCIStmt *hStmt, bool select);
};

#endif


