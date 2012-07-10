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

#include <libpq-fe.h>

#include "PostgresConnection.h"
#include "PostgresRecordset.h"
#include "PostgresConnectionInformation.h"

#ifdef _WIN32
  #define SQL_EXPORT __declspec(dllexport)
#elif defined(HAVE_GCC_VISIBILITY)
  #define SQL_EXPORT __attribute__ ((visibility("default")))
#else
  #define SQL_EXPORT 
#endif

extern "C" SQL_EXPORT CSqlConnection *CreateConnection()
{
        return new CPostgresConnection;
}

#undef SQL_EXPORT

CPostgresConnection::CPostgresConnection()
{
	m_pDb=NULL;
	m_lasterr=PGRES_COMMAND_OK;
}

CPostgresConnection::~CPostgresConnection()
{
	Close();
}

bool CPostgresConnection::Create(const char *host, const char *database, const char *username, const char *password)
{
	CPostgresConnectionInformation *pCI = static_cast<CPostgresConnectionInformation*>(GetConnectionInformation());
	pCI->hostname=host?host:"localhost";
	pCI->database=database?database:"";
	pCI->username=username?username:"";
	pCI->password=password?password:"";

	return Create();
}

bool CPostgresConnection::Create()
{
	CPostgresConnectionInformation *pCI = static_cast<CPostgresConnectionInformation*>(GetConnectionInformation());
	cvs::string db = pCI->database;
	pCI->database="template1";  
	if(!Open())
		return false;
	pCI->database=db;
	Execute("create database %s",db.c_str());
	if(Error())
		return false;
	Close();
	return Open();
}

bool CPostgresConnection::Open(const char *host, const char *database, const char *username, const char *password)
{
	CPostgresConnectionInformation *pCI = static_cast<CPostgresConnectionInformation*>(GetConnectionInformation());
	pCI->hostname=host?host:"localhost";
	pCI->database=database?database:"";
	pCI->username=username?username:"";
	pCI->password=password?password:"";

	return Open();
}

bool CPostgresConnection::Open()
{
	CPostgresConnectionInformation *pCI = static_cast<CPostgresConnectionInformation*>(GetConnectionInformation());

	cvs::string tmp;
	cvs::sprintf(tmp,128,"host = '%s' dbname = '%s' user = '%s' password = '%s'",pCI->hostname.c_str(),pCI->database.c_str(),pCI->username.c_str(),pCI->password.c_str());

	m_pDb = PQconnectdb(tmp.c_str());
	if(!m_pDb)
		return false;

	if(PQstatus(m_pDb)==CONNECTION_BAD)
		return false;

	PQsetClientEncoding(m_pDb,"UNICODE");

	return true;
}

bool CPostgresConnection::Close()
{
	if(m_pDb)
		PQfinish(m_pDb);

	m_pDb=NULL;
	return true;
}

bool CPostgresConnection::IsOpen()
{
	if(m_pDb && PQstatus(m_pDb)!=CONNECTION_BAD)
		return true;
	return false;
}

CSqlRecordsetPtr CPostgresConnection::Execute(const char *string, ...)
{
	cvs::string str;
	va_list va;

	va_start(va,string);
	cvs::vsprintf(str,64,string,va);
	va_end(va);

	return _Execute(str.c_str());
}

unsigned CPostgresConnection::ExecuteAndReturnIdentity(const char *string, ...)
{
	cvs::string str,table;
	va_list va;

	va_start(va,string);
	cvs::vsprintf(str,64,string,va);
	va_end(va);

	if(strncasecmp(str.c_str(),"insert into ",12))
	{
		m_lasterr = PGRES_FATAL_ERROR;
		m_lasterrmsg = "Postgres driver only supports identity return from 'insert into' statements";
		return 0;
	}

	char *p=(char*)str.c_str()+12;
	char *q=p;
	while(*q && !isspace(*q) && *q!='(')
		q++;
	table.assign(p,q-p);

	CServerIo::trace(3,"Postgres: table name is %s",table.c_str());

	_Execute(str.c_str());

	if(Error())
	{
		CServerIo::trace(3,"Postgres: Initial command returned error");
		return 0;
	}

	cvs::sprintf(str,80,"select currval('%s_id_seq')",table.c_str());
	CSqlRecordsetPtr rs = _Execute(str.c_str());
	if(Error())
	{
		CServerIo::trace(3,"Postgres: select currval returned error");
		return 0;
	}
	if(rs->Closed())
	{
		CServerIo::trace(3,"Postgres: select currval returned closed");
		return 0;
	}
	if(rs->Eof())
	{
		CServerIo::trace(3,"Postgres: select currval returned eof");
		return 0;
	}
	unsigned id = *rs[0];
	CServerIo::trace(3,"Postgres: new id is %u",id);
	return id;
}

CSqlRecordsetPtr CPostgresConnection::_Execute(const char *string)
{
	cvs::string str = string;
	CPostgresRecordset *rs = new CPostgresRecordset();

	/* postgres doesn't support the standard questionmark operator, instead using '$n'.
	   preparse the string looking for these and replacing them.  This routine is probably
	   not very robust. */
	size_t pos = 0;
	int var = 1;
	char varstr[32];
	bool quote = false;
	while(pos<str.size())
	{
		char c=str[pos];
		if(!quote && c=='\'')
			quote=true;
		else if(quote && c=='\'')
			quote=false;
		else if(!quote && c=='?')
		{
			snprintf(varstr,sizeof(varstr),"$%d",var++);
			str.replace(pos,1,varstr);
		}
		pos++;
	}

	CServerIo::trace(3,"%s",str.c_str());

	PGresult *rslt;
	int numParams = (int)m_bindVars.size();
	Oid *paramTypes = NULL;
	char **paramValues = NULL;
	int *paramLength = NULL;
	int *paramFormats = NULL;
	// Appears to be impossible to define in VC.. Probably an MS bug.
	typedef char __dummy[16];
	__dummy *paramVars = NULL;
	if(numParams)
	{
		paramTypes = new Oid[numParams];
		paramValues = new char *[numParams];
		paramLength = new int[numParams];
		paramFormats = new int[numParams];
		paramVars = new char[numParams][16];

		int n = 0;
		for(std::map<int,CSqlVariant>::iterator i = m_bindVars.begin(); i!=m_bindVars.end(); ++i)
		{
			paramFormats[n]=1;
			switch(i->second.type())
			{
			case CSqlVariant::vtNull:
				paramTypes[n]=0; 
				paramValues[n]=0;
				paramLength[n]=0;
				break;
			case CSqlVariant::vtChar:
			case CSqlVariant::vtUChar:
				paramTypes[n]=18;
				*(char*)paramVars[n]=(char)i->second;
				paramValues[n]=paramVars[n];
				paramLength[n]=sizeof(char);
				break;
			case CSqlVariant::vtShort:
			case CSqlVariant::vtUShort:
				paramTypes[n]=21;
				*(short*)paramVars[n]=(short)i->second;
				paramValues[n]=paramVars[n];
				paramLength[n]=sizeof(short);
				break;
			case CSqlVariant::vtInt:
			case CSqlVariant::vtUInt:
			case CSqlVariant::vtLong:
			case CSqlVariant::vtULong:
				if(sizeof(int)==4)
				{
					paramTypes[n]=23; 
					*(int*)paramVars[n]=(int)i->second;
					paramValues[n]=paramVars[n];
					paramLength[n]=sizeof(int);
					break;
				}
			/* fall through (64bit systems) */
			case CSqlVariant::vtLongLong:
			case CSqlVariant::vtULongLong:
				paramTypes[n]=20;
	#ifdef _WIN32
				*(__int64*)paramVars[n]=(__int64)i->second;
				paramValues[n]=paramVars[n];
				paramLength[n]=sizeof(__int64);
	#else
				*(long long*)paramVars[n]=(long long)i->second;
				paramValues[n]=paramVars[n];
				paramLength[n]=sizeof(long long);
	#endif
				break;
			case CSqlVariant::vtString:
			case CSqlVariant::vtWString:
				{
				paramTypes[n]=25;
				const char *s = i->second;
				paramLength[n]=(int)strlen(s);
				paramValues[n]=(char*)s;
				}
				break;
			}
			n++;
		}
	}


	rslt = PQexecParams(m_pDb, str.c_str(), numParams, paramTypes, paramValues, paramLength, paramFormats, 0);

	if(paramTypes)
		delete[] paramTypes;
	if(paramValues)
		delete[] paramValues;
	if(paramLength)
		delete[] paramLength;
	if(paramFormats)
		delete[] paramFormats;
	if(paramVars)
		delete[] paramVars;

	if(!rslt)
	{
		m_lasterr = PGRES_FATAL_ERROR;
		return rs;
	}

	m_lasterr = PQresultStatus(rslt);
	if(m_lasterr == PGRES_BAD_RESPONSE || m_lasterr == PGRES_NONFATAL_ERROR || m_lasterr == PGRES_FATAL_ERROR)
	{
		if(rslt)
			m_lasterrmsg = PQresultErrorMessage(rslt);
		else
			m_lasterrmsg = "";
		if(m_lasterrmsg.size() && m_lasterrmsg[m_lasterrmsg.size()-1]=='\n')
			m_lasterrmsg.resize(m_lasterrmsg.size()-1);
		PQclear(rslt);
		return rs;
	}

	rs->Init(m_pDb,rslt);

	m_bindVars.clear();

	return rs;
}

bool CPostgresConnection::Error() const
{
	if(!m_pDb) return true;
	if(PQstatus(m_pDb)==CONNECTION_BAD) return true;
	if(m_lasterr == PGRES_BAD_RESPONSE || m_lasterr == PGRES_NONFATAL_ERROR || m_lasterr == PGRES_FATAL_ERROR)
		return true;
	return false;
}

const char *CPostgresConnection::ErrorString() 
{
	if(!m_pDb)
		return "Database not created or couldn't find libpq.dll";
	if(PQstatus(m_pDb)!=CONNECTION_OK)
		return PQerrorMessage(m_pDb);
	if(m_lasterrmsg.size())
		return m_lasterrmsg.c_str();
	return PQresStatus((ExecStatusType)m_lasterr);
}

unsigned CPostgresConnection::GetInsertIdentity(const char *table_hint) 
{
	cvs::string str;
	cvs::sprintf(str,80,"select currval('%s_id_seq')",table_hint);
	PGresult *rslt = PQexec(m_pDb, str.c_str());
	if(!PQntuples(rslt) || !PQnfields(rslt))
	{
		CServerIo::trace(1,"Postgres GetInsertIdentity(%s) failed",table_hint);
		return 0;
	}

	unsigned long ident;
	if(sscanf(PQgetvalue(rslt,0,0),"%lu",&ident)!=1)
	{
		CServerIo::trace(1,"Postgres GetInsertIdentity(%s) failed (bogus value)",table_hint);
		return 0;
	}

	PQclear(rslt);

	return ident;
}

bool CPostgresConnection::BeginTrans()
{
	PGresult *rslt = PQexec(m_pDb, "BEGIN TRANSACTION");
	m_lasterr = PQresultStatus(rslt);
	PQclear(rslt);
	if(m_lasterr == PGRES_BAD_RESPONSE || m_lasterr == PGRES_NONFATAL_ERROR || m_lasterr == PGRES_FATAL_ERROR)
		return false;
	return true;
}

bool CPostgresConnection::CommitTrans()
{
	PGresult *rslt = PQexec(m_pDb, "COMMIT TRANSACTION");
	m_lasterr = PQresultStatus(rslt);
	PQclear(rslt);
	if(m_lasterr == PGRES_BAD_RESPONSE || m_lasterr == PGRES_NONFATAL_ERROR || m_lasterr == PGRES_FATAL_ERROR)
		return false;
	return true;
}

bool CPostgresConnection::RollbackTrans()
{
	PGresult *rslt = PQexec(m_pDb, "ROLLBACK TRANSACTION");
	m_lasterr = PQresultStatus(rslt);
	PQclear(rslt);
	if(m_lasterr == PGRES_BAD_RESPONSE || m_lasterr == PGRES_NONFATAL_ERROR || m_lasterr == PGRES_FATAL_ERROR)
		return false;
	return true;
}

bool CPostgresConnection::Bind(int variable, CSqlVariant value)
{
	m_bindVars[variable]=value;
	return true;
}

CSqlConnectionInformation* CPostgresConnection::GetConnectionInformation()
{
	if(!m_ConnectionInformation) m_ConnectionInformation = new CPostgresConnectionInformation();
	return m_ConnectionInformation;
}

const char *CPostgresConnection::parseTableName(const char *szName)
{
	CPostgresConnectionInformation *pCI = static_cast<CPostgresConnectionInformation*>(GetConnectionInformation());

	if(!szName)
		return NULL;

	if(!pCI->schema.size())
		return szName;

	return cvs::cache_static_string((pCI->schema+"."+szName).c_str());
}
