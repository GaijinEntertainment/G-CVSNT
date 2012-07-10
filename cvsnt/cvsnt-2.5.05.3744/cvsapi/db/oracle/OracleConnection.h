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

#ifndef ORACLECONNECTION__H
#define ORACLECONNECTION__H

#include "OracleRecordset.h"

#include <vector>
#include <map>

class COracleConnection : public CSqlConnection
{
	friend class COracleRecordset;
public:
	COracleConnection();
	virtual ~COracleConnection();

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
	OCIEnv *m_hEnv;
	OCIError *m_hErr;
	OCIServer *m_hSrv;
	OCISvcCtx *m_hSvcCtx;
	OCISession *m_hSess;
	bool m_bAutoCommit;

	struct valStruct
	{
		union
		{
			wchar_t wc;
			short s;
			long l;
#ifdef __WIN32
			__int64 ll;
#else
			long long ll;
#endif
		};
		cvs::wstring ws;
		cvs::string cstr;
	};

	short m_lasterror;
	short m_nchar_utf8;
	cvs::string m_lasterrorString;
	std::map<int,CSqlVariant> m_bindVars;
	std::map<int,valStruct> m_sqlv;

	CSqlRecordsetPtr _Execute(const char *string, unsigned* identity);
	bool CheckError(OCIError *hErr, sb4 status);
};

#endif


