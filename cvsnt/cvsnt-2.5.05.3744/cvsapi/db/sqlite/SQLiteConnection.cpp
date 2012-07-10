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
#endif
#include <config.h>
#include <cvsapi.h>

#include <sqlite3.h>

#include "SQLiteConnection.h"
#include "SQLiteRecordset.h"
#include "SQLiteConnectionInformation.h"

#ifdef _WIN32
  #define SQL_EXPORT __declspec(dllexport)
#elif defined(HAVE_GCC_VISIBILITY)
  #define SQL_EXPORT __attribute__ ((visibility("default")))
#else
  #define SQL_EXPORT 
#endif

extern "C" SQL_EXPORT CSqlConnection *CreateConnection()
{
        return new CSQLiteConnection;
}

#undef SQL_EXPORT

CSQLiteConnection::CSQLiteConnection()
{
	m_pDb=NULL;
}

CSQLiteConnection::~CSQLiteConnection()
{
	Close();
}

bool CSQLiteConnection::Create(const char *host, const char *database, const char *username, const char *password)
{
	CSQLiteConnectionInformation *pCI = static_cast<CSQLiteConnectionInformation*>(GetConnectionInformation());
	pCI->hostname=host?host:"localhost";
	pCI->database=database?database:"";
	pCI->username=username?username:"";
	pCI->password=password?password:"";

	return Create();
}

bool CSQLiteConnection::Create()
{
	CSQLiteConnectionInformation *pCI = static_cast<CSQLiteConnectionInformation*>(GetConnectionInformation());

	if(CFileAccess::exists(pCI->database.c_str()))
		return false;

	if(sqlite3_open(pCI->database.c_str(), &m_pDb))
		return false;

	return true;
}

bool CSQLiteConnection::Open(const char *host, const char *database, const char *username, const char *password)
{
	CSQLiteConnectionInformation *pCI = static_cast<CSQLiteConnectionInformation*>(GetConnectionInformation());
	pCI->hostname=host?host:"localhost";
	pCI->database=database?database:"";
	pCI->username=username?username:"";
	pCI->password=password?password:"";

	return Open();
}

bool CSQLiteConnection::Open()
{
	CSQLiteConnectionInformation *pCI = static_cast<CSQLiteConnectionInformation*>(GetConnectionInformation());

	if(!CFileAccess::exists(pCI->database.c_str()))
		return false;

	if(sqlite3_open(pCI->database.c_str(), &m_pDb))
		return false;

	return true;
}

bool CSQLiteConnection::Close()
{
	if(m_pDb)
		sqlite3_close(m_pDb);

	m_pDb=NULL;
	return true;
}

bool CSQLiteConnection::IsOpen()
{
	if(m_pDb)
		return true;
	return false;
}

CSqlRecordsetPtr CSQLiteConnection::Execute(const char *string, ...)
{
	cvs::string str;
	va_list va;

	va_start(va,string);
	cvs::vsprintf(str,64,string,va);
	va_end(va);

	return _Execute(str.c_str());
}

unsigned CSQLiteConnection::ExecuteAndReturnIdentity(const char *string, ...)
{
	cvs::string str;
	va_list va;

	va_start(va,string);
	cvs::vsprintf(str,64,string,va);
	va_end(va);

	CSqlRecordsetPtr rs = _Execute(str.c_str());

	return (unsigned)sqlite3_last_insert_rowid(m_pDb);
}

CSqlRecordsetPtr CSQLiteConnection::_Execute(const char *string)
{
	CSQLiteRecordset *rs = new CSQLiteRecordset();

	CServerIo::trace(3,"%s",string);

	sqlite3_stmt *pStmt;
	const char *zTail = NULL;

	if(sqlite3_prepare(m_pDb,string,(int)strlen(string),&pStmt,&zTail))
		return rs;

	for(std::map<int,CSqlVariant>::iterator i = m_bindVars.begin(); i!=m_bindVars.end(); ++i)
	{
		switch(i->second.type())
		{
		case CSqlVariant::vtNull:
			sqlite3_bind_null(pStmt,i->first+1);
			break;
		case CSqlVariant::vtChar:
		case CSqlVariant::vtShort:
		case CSqlVariant::vtInt:
		case CSqlVariant::vtLong:
		case CSqlVariant::vtUChar:
		case CSqlVariant::vtUShort:
		case CSqlVariant::vtUInt:
		case CSqlVariant::vtULong:
			sqlite3_bind_int(pStmt,i->first+1,(int)i->second);
			break;
		case CSqlVariant::vtLongLong:
		case CSqlVariant::vtULongLong:
#ifdef _WIN32
			sqlite3_bind_int64(pStmt,i->first+1,(__int64)i->second);
#else
			sqlite3_bind_int64(pStmt,i->first+1,(long long)i->second);
#endif
			break;
		case CSqlVariant::vtString:
			sqlite3_bind_text(pStmt,i->first+1,(const char *)i->second,-1,SQLITE_STATIC);
			break;
		case CSqlVariant::vtWString:
			sqlite3_bind_text16(pStmt,i->first+1,(const wchar_t *)i->second,-1,SQLITE_STATIC);
			break;
		}
	}

	rs->Init(m_pDb,pStmt);
	
	m_bindVars.clear();

	return rs;
}

bool CSQLiteConnection::Error() const
{
	int err = sqlite3_errcode(m_pDb);
	if(err && err<100)
		return true;
	return false;
}

const char *CSQLiteConnection::ErrorString() 
{
	if(!m_pDb)
		return "Open failed";
	return sqlite3_errmsg(m_pDb);
}

unsigned CSQLiteConnection::GetInsertIdentity(const char *table_hint) 
{
	return (unsigned)sqlite3_last_insert_rowid(m_pDb);
}

bool CSQLiteConnection::BeginTrans()
{
	if(sqlite3_exec(m_pDb,"BEGIN",NULL,NULL,NULL)==SQLITE_OK)
		return true;
	return false;
}

bool CSQLiteConnection::CommitTrans()
{
	if(sqlite3_exec(m_pDb,"COMMIT",NULL,NULL,NULL)==SQLITE_OK)
		return true;
	return false;
}

bool CSQLiteConnection::RollbackTrans()
{
	if(sqlite3_exec(m_pDb,"ROLLBACK",NULL,NULL,NULL)==SQLITE_OK)
		return true;
	return false;
}

bool CSQLiteConnection::Bind(int variable, CSqlVariant value)
{
	m_bindVars[variable]=value;
	return true;
}

CSqlConnectionInformation* CSQLiteConnection::GetConnectionInformation()
{
	if(!m_ConnectionInformation) m_ConnectionInformation = new CSQLiteConnectionInformation();
	return m_ConnectionInformation;
}

const char *CSQLiteConnection::parseTableName(const char *szName)
{
	CSQLiteConnectionInformation *pCI = static_cast<CSQLiteConnectionInformation*>(GetConnectionInformation());

	if(!szName)
		return NULL;

	if(!pCI->prefix.size())
		return szName;

	return cvs::cache_static_string((pCI->prefix+szName).c_str());
}
