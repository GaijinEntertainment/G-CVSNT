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

#ifndef MSSQLCONNECTION__H
#define MSSQLCONNECTION__H

#include "MssqlRecordset.h"

#include <vector>
#include <map>

class CMssqlConnection : public CSqlConnection
{
	friend class CMssqlRecordset;
public:
	CMssqlConnection();
	virtual ~CMssqlConnection();

	virtual bool Create(const char *host, const char *database, const char *username, const char *password);
	virtual bool Create();
	virtual bool Open(const char *host, const char *database, const char *username, const char *password);
	virtual bool Open();
	virtual bool Close();
	virtual bool IsOpen();
	virtual bool Bind(int variable, CSqlVariant value);
	virtual CSqlRecordsetPtr Execute(const char *string, ...);
	virtual unsigned ExecuteAndReturnIdentity(const char *string, ...);
	virtual bool Error() const;
	virtual const char *ErrorString();
	virtual unsigned GetInsertIdentity(const char *table_hint);
	virtual bool BeginTrans();
	virtual bool CommitTrans();
	virtual bool RollbackTrans();

	virtual CSqlConnectionInformation* GetConnectionInformation();
	virtual const char *parseTableName(const char *szName);

protected:
	HENV m_hEnv;
	HDBC m_hDbc;
	SQLRETURN m_lasterror;
	struct valStruct
	{
		union
		{
			char c;
			short s;
			long l;
			__int64 ll;
		};
		cvs::wstring ws;
		cvs::string cs;
	};

	cvs::string m_lasterrorString;
	cvs::wstring m_lastrsError;
	std::map<int,CSqlVariant> m_bindVars;
	std::map<int,SQLINTEGER> m_sqli;
	std::map<int,valStruct> m_sqlv;

	CSqlRecordsetPtr _Execute(const char *string);
};

#endif


