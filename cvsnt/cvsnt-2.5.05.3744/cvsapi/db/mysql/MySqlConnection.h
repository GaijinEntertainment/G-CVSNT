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

#ifndef MYSQLCONNECTION__H
#define MYSQLCONNECTION__H

#include "MySqlRecordset.h"

#include <vector>
#include <map>

class CMySqlConnection : public CSqlConnection
{
public:
	CMySqlConnection();
	virtual ~CMySqlConnection();

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
	std::map<int,CSqlVariant> m_bindVars;
	MYSQL *m_connect;

	CSqlRecordsetPtr _Execute(const char *string);

};

#endif

