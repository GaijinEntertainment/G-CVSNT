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
#ifndef SQLCONNECTION__H
#define SQLCONNECTION__H

#include "SqlVariant.h"
#include "SqlRecordset.h"
#include "SqlConnectionInformation.h"

#include <vector>

class CSqlConnection
{
public:
	typedef std::vector<std::pair<cvs::string, cvs::string> > connectionList_t;

	CSqlConnection() { m_ConnectionInformation = NULL; }
	virtual ~CSqlConnection() { if(m_ConnectionInformation) delete m_ConnectionInformation; }

	static CVSAPI_EXPORT CSqlConnection* CreateConnection(const char *db, const char *library_dir);
	static CVSAPI_EXPORT bool GetConnectionList(connectionList_t& list, const char *library_dir);

	virtual bool Create(const char *hostname, const char *database, const char *username, const char *password) =0;
	virtual bool Create() =0;
	virtual bool Open(const char *hostname, const char *database, const char *username, const char *password) =0;
	virtual bool Open() =0;
	virtual bool IsOpen() =0;
	virtual bool Close() =0;
	virtual bool Bind(int variable, CSqlVariant value) =0;
	virtual CSqlRecordsetPtr Execute(const char *string, ...) =0;
	virtual unsigned ExecuteAndReturnIdentity(const char *string, ...) =0;
	virtual bool Error() const =0;
	virtual const char *ErrorString() =0;
	virtual unsigned GetInsertIdentity(const char *table_hint) =0;
	virtual bool BeginTrans() = 0;
	virtual bool CommitTrans() = 0;
	virtual bool RollbackTrans() = 0;

	virtual CSqlConnectionInformation* GetConnectionInformation() =0;
	virtual const char *parseTableName(const char *szName) = 0;

protected:
	CSqlConnectionInformation *m_ConnectionInformation;
};

#endif
