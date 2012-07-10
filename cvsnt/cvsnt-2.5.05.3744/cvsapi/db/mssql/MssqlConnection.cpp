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
// MS Sql
// Win32 specific
#ifdef _WIN32
// Microsoft braindamage reversal.  
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <config.h>
#include <cvsapi.h>

#include <sql.h>
#include <sqlext.h>

#include "MssqlConnection.h"
#include "MssqlRecordset.h"
#include "MssqlConnectionInformation.h"

#ifdef _WIN32
  #define SQL_EXPORT __declspec(dllexport)
#elif defined(HAVE_GCC_VISIBILITY)
  #define SQL_EXPORT __attribute__ ((visibility("default")))
#else
  #define SQL_EXPORT 
#endif

extern "C" SQL_EXPORT CSqlConnection *CreateConnection()
{
        return new CMssqlConnection;
}

#undef SQL_EXPORT

CMssqlConnection::CMssqlConnection()
{
	m_hEnv=NULL;
	m_hDbc=NULL;
	m_lasterror=SQL_SUCCESS;
}

CMssqlConnection::~CMssqlConnection()
{
	Close();
}

bool CMssqlConnection::Create(const char *host, const char *database, const char *username, const char *password)
{
	CMssqlConnectionInformation *pCI = static_cast<CMssqlConnectionInformation*>(GetConnectionInformation());
	pCI->hostname=host?host:"localhost";
	pCI->database=database?database:"";
	pCI->username=username?username:"";
	pCI->password=password?password:"";

	return Create();
}

bool CMssqlConnection::Create()
{
	CMssqlConnectionInformation *pCI = static_cast<CMssqlConnectionInformation*>(GetConnectionInformation());
	cvs::string db = pCI->database;
	pCI->database="master";  
	if(!Open())
		return false;
	pCI->database=db;
	Execute("create database %s",db.c_str());
	if(Error())
		return false;
	Close();
	return Open();
}

bool CMssqlConnection::Open(const char *host, const char *database, const char *username, const char *password)
{
	CMssqlConnectionInformation *pCI = static_cast<CMssqlConnectionInformation*>(GetConnectionInformation());
	pCI->hostname=host?host:"localhost";
	pCI->database=database?database:"";
	pCI->username=username?username:"";
	pCI->password=password?password:"";

	return Open();
}

bool CMssqlConnection::Open()
{
	CMssqlConnectionInformation *pCI = static_cast<CMssqlConnectionInformation*>(GetConnectionInformation());

	m_lasterror = SQLAllocHandle(SQL_HANDLE_ENV,SQL_NULL_HANDLE,&m_hEnv);
	if(!SQL_SUCCEEDED(m_lasterror))
		return false;

	m_lasterror = SQLSetEnvAttr(m_hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
	if(!SQL_SUCCEEDED(m_lasterror))
		return false;

	m_lasterror = SQLAllocHandle(SQL_HANDLE_DBC,m_hEnv,&m_hDbc);
	if(!SQL_SUCCEEDED(m_lasterror))
		return false;

	cvs::wstring strConn;
	cvs::swprintf(strConn,80,L"DATABASE={%s};DRIVER={SQL Server};Pwd={%s};Server={%s};Uid={%s};",
		(const wchar_t*)cvs::wide(pCI->database.c_str()),(const wchar_t*)cvs::wide(pCI->password.c_str()),(const wchar_t*)cvs::wide(pCI->hostname.c_str()),(const wchar_t*)cvs::wide(pCI->username.c_str()));
	m_lasterror = SQLDriverConnect(m_hDbc,NULL,(SQLWCHAR*)strConn.c_str(),(SQLSMALLINT)strConn.size(),NULL,0,NULL,SQL_DRIVER_NOPROMPT);
	if(!SQL_SUCCEEDED(m_lasterror))
		return false;

	m_lasterror = SQLSetConnectAttr(m_hDbc,SQL_ATTR_AUTOCOMMIT,(SQLPOINTER)SQL_AUTOCOMMIT_ON,0);
	if(!SQL_SUCCEEDED(m_lasterror))
		return false;

	return true;
}

bool CMssqlConnection::Close()
{
	if(m_hEnv)
	{
		SQLDisconnect(m_hDbc);
		SQLFreeConnect(m_hDbc);
		SQLFreeEnv(m_hEnv);
	}
	m_hEnv = NULL;
	m_hDbc = NULL;
	m_lastrsError=L"";
	return true;
}

bool CMssqlConnection::IsOpen()
{
	if(m_hEnv)
		return true;
	return false;
}

CSqlRecordsetPtr CMssqlConnection::Execute(const char *string, ...)
{
	cvs::string str;
	va_list va;

	va_start(va,string);
	cvs::vsprintf(str,64,string,va);
	va_end(va);

	return _Execute(str.c_str());
}

unsigned CMssqlConnection::ExecuteAndReturnIdentity(const char *string, ...)
{
	cvs::string str;
	va_list va;

	va_start(va,string);
	cvs::vsprintf(str,64,string,va);
	va_end(va);

	_Execute(str.c_str());
	if(Error())
		return 0;

	CSqlRecordsetPtr rs = _Execute("select @@IDENTITY");

	if(Error() || rs->Closed() || rs->Eof())
		return 0;
	return *rs[0];
}

CSqlRecordsetPtr CMssqlConnection::_Execute(const char *string)
{
	cvs::string str = string;
	SQLRETURN ret;

	CMssqlRecordset *rs = new CMssqlRecordset();

	CServerIo::trace(3,"%s",str.c_str());

	bool inquote = false;
	for(size_t n=0; n<str.size(); n++)
	{
		if(str[n]=='\'')
		{
			inquote = !inquote;
			if(inquote)
				str.insert(n++,1,'N');
		}
	}

	HSTMT hStmt;

	if(!SQL_SUCCEEDED(m_lasterror=SQLAllocHandle(SQL_HANDLE_STMT,m_hDbc,&hStmt)))
	{
		SQLFreeStmt(hStmt,SQL_DROP);
		return rs;
	}

	for(std::map<int,CSqlVariant>::iterator i = m_bindVars.begin(); i!=m_bindVars.end(); ++i)
	{
		switch(i->second.type())
		{
		case CSqlVariant::vtNull:
			m_sqli[i->first]=SQL_NULL_DATA;
			ret = SQLBindParameter(hStmt,i->first+1,SQL_PARAM_INPUT,SQL_C_CHAR,SQL_CHAR,0,0,NULL,0,&m_sqli[i->first]);
			break;
		case CSqlVariant::vtChar:
			CServerIo::trace(3,"Bind vtChar (SQL Server) = \"%c\"",m_sqlv[i->first].c);
			m_sqli[i->first]=0;
			m_sqlv[i->first].c=i->second;
			ret = SQLBindParameter(hStmt,i->first+1,SQL_PARAM_INPUT,SQL_C_CHAR,SQL_CHAR,1,0,&m_sqlv[i->first].c,1,&m_sqli[i->first]);
			break;
		case CSqlVariant::vtUChar:
			m_sqli[i->first]=0;
			m_sqlv[i->first].c=i->second;
			ret = SQLBindParameter(hStmt,i->first+1,SQL_PARAM_INPUT,SQL_C_CHAR,SQL_CHAR,1,0,&m_sqlv[i->first].c,1,&m_sqli[i->first]);
			break;
		case CSqlVariant::vtShort:
			m_sqli[i->first]=0;
			m_sqlv[i->first].s=i->second;
			ret = SQLBindParameter(hStmt,i->first+1,SQL_PARAM_INPUT,SQL_C_SSHORT,SQL_INTEGER,0,0,&m_sqlv[i->first].s,0,&m_sqli[i->first]);
			break;
		case CSqlVariant::vtUShort:
			m_sqli[i->first]=0;
			m_sqlv[i->first].s=i->second;
			ret = SQLBindParameter(hStmt,i->first+1,SQL_PARAM_INPUT,SQL_C_USHORT,SQL_INTEGER,0,0,&m_sqlv[i->first].s,0,&m_sqli[i->first]);
			break;
		case CSqlVariant::vtInt:
		case CSqlVariant::vtLong:
			m_sqli[i->first]=0;
			m_sqlv[i->first].l=i->second;
			ret = SQLBindParameter(hStmt,i->first+1,SQL_PARAM_INPUT,SQL_C_SLONG,SQL_INTEGER,0,0,&m_sqlv[i->first].l,0,&m_sqli[i->first]);
			break;
		case CSqlVariant::vtUInt:
		case CSqlVariant::vtULong:
			m_sqli[i->first]=0;
			m_sqlv[i->first].l=i->second;
			ret = SQLBindParameter(hStmt,i->first+1,SQL_PARAM_INPUT,SQL_C_ULONG,SQL_INTEGER,0,0,&m_sqlv[i->first].l,0,&m_sqli[i->first]);
			break;
		case CSqlVariant::vtLongLong:
			m_sqli[i->first]=0;
			m_sqlv[i->first].ll=i->second;
			ret = SQLBindParameter(hStmt,i->first+1,SQL_PARAM_INPUT,SQL_C_SBIGINT,SQL_BIGINT,0,0,&m_sqlv[i->first].ll,0,&m_sqli[i->first]);
			break;
		case CSqlVariant::vtULongLong:
			m_sqli[i->first]=0;
			m_sqlv[i->first].ll=i->second;
			ret = SQLBindParameter(hStmt,i->first+1,SQL_PARAM_INPUT,SQL_C_UBIGINT,SQL_BIGINT,0,0,&m_sqlv[i->first].ll,0,&m_sqli[i->first]);
			break;
		case CSqlVariant::vtString:
			CServerIo::trace(3,"Bind vtString (SQL Server)"); 

			if (m_sqlv[i->first].ws.length()+1<256)
			{
				CServerIo::trace(3,"Bind vtString as type SQL_NTS (Null Terminated String)");
			m_sqli[i->first]=SQL_NTS;
			m_sqlv[i->first].ws=cvs::wide(i->second);
				CServerIo::trace(3,"Bind vtString as SQL_VARCHAR/text C_WCHAR");
				ret = SQLBindParameter(hStmt,i->first+1,SQL_PARAM_INPUT,SQL_C_WCHAR,SQL_VARCHAR,(SQLINTEGER)m_sqlv[i->first].ws.size()+1,0,(SQLPOINTER)m_sqlv[i->first].ws.c_str(),(SQLINTEGER)m_sqlv[i->first].ws.size(),&m_sqli[i->first]);
			}
			else
			{
				CServerIo::trace(3,"Bind vtString as type LEN_DATA_AT_EXEC");
				m_sqli[i->first]=SQL_LEN_DATA_AT_EXEC((SQLINTEGER)((m_sqlv[i->first].ws.size()+1)*sizeof(wchar_t)));
				m_sqlv[i->first].ws=cvs::wide(i->second);
				CServerIo::trace(3,"Bind vtString as SQL_WLONGVARCHAR/text BLOB");
				ret = SQLBindParameter(hStmt,i->first+1,SQL_PARAM_INPUT,SQL_C_WCHAR,SQL_WLONGVARCHAR,(SQLINTEGER)((m_sqlv[i->first].ws.size()+1)*sizeof(wchar_t)),0,(VOID *)NULL,0,&m_sqli[i->first]);
			}
			break;
		case CSqlVariant::vtWString:
			CServerIo::trace(3,"Bind vtWString (SQL Server)"); 
			m_sqli[i->first]=SQL_NTS;
			m_sqlv[i->first].ws=i->second;
			CServerIo::trace(3,"Bind vtWString (SQL Server)");
			ret = SQLBindParameter(hStmt,i->first+1,SQL_PARAM_INPUT,SQL_C_WCHAR,SQL_WVARCHAR,(SQLINTEGER)m_sqlv[i->first].ws.size()+1,0,(SQLPOINTER)m_sqlv[i->first].ws.c_str(),(SQLINTEGER)m_sqlv[i->first].ws.size()+1,&m_sqli[i->first]);
			break;
		}
	}

	CServerIo::trace(3,"%s",str.c_str());

	rs->Init(this,hStmt,str.c_str()); // Ignore return... it's handled by the error routines

	m_bindVars.clear();

	return rs;
}

bool CMssqlConnection::Error() const
{
	if(SQL_SUCCEEDED(m_lasterror))
		return false;
	return true;
}

const char *CMssqlConnection::ErrorString()
{
	SQLWCHAR state[6];
	SQLINTEGER error;
	SQLSMALLINT size = 512,len;
	cvs::wstring err;
	err.resize((int)size);
	SQLWCHAR *pmsg = (SQLWCHAR*)err.data();

	if(m_lastrsError.size())
	{
		wcscpy((wchar_t *)pmsg,m_lastrsError.c_str());
		pmsg+=m_lastrsError.size();
		size-=(SQLSMALLINT)m_lastrsError.size();
		m_lastrsError=L"";
	}
	if(m_hDbc)
	{
		for(int i=1; SQL_SUCCEEDED(SQLGetDiagRec(SQL_HANDLE_DBC, m_hDbc, i, state, &error, pmsg, size, &len)); i++)
		{
			size-=len;
			pmsg+=len;
		}
	}
	if(m_hEnv)
	{
		for(int i=1; SQL_SUCCEEDED(SQLGetDiagRec(SQL_HANDLE_ENV, m_hEnv, i, state, &error, pmsg, size, &len)); i++)
		{
			size-=len;
			pmsg+=len;
		}
	}
	err.resize(512-size);
	m_lasterrorString=cvs::narrow(err.c_str());
	return m_lasterrorString.c_str();
}

unsigned CMssqlConnection::GetInsertIdentity(const char *table_hint)
{
	HSTMT hStmt;

	m_lasterror=SQLAllocStmt(m_hDbc,&hStmt);
	if(!SQL_SUCCEEDED(m_lasterror))
		return 0;

	m_lasterror=SQLExecDirect(hStmt,(SQLWCHAR*)L"SELECT @@IDENTITY",SQL_NTS);
	if(!SQL_SUCCEEDED(m_lasterror))
	{
		SQLFreeStmt(hStmt,SQL_DROP);
		return 0;
	}

	long id;
	SQLINTEGER len;

	m_lasterror=SQLBindCol(hStmt,1,SQL_C_LONG,&id,sizeof(id),&len);
	if(!SQL_SUCCEEDED(m_lasterror))
	{
		SQLFreeStmt(hStmt,SQL_DROP);
		return 0;
	}

	m_lasterror=SQLFetch(hStmt);
	if(!SQL_SUCCEEDED(m_lasterror))
		return 0;

	SQLFreeStmt(hStmt,SQL_DROP);

	return (unsigned)id;
}

bool CMssqlConnection::BeginTrans()
{
	m_lasterror = SQLSetConnectAttr(m_hDbc,SQL_ATTR_AUTOCOMMIT,(SQLPOINTER)SQL_AUTOCOMMIT_OFF,0);

	if(!SQL_SUCCEEDED(m_lasterror))
		return false;

	return true;
}

bool CMssqlConnection::CommitTrans()
{
	m_lasterror = SQLEndTran(SQL_HANDLE_DBC,m_hDbc,SQL_COMMIT);

	if(!SQL_SUCCEEDED(m_lasterror))
		return false;

	m_lasterror = SQLSetConnectAttr(m_hDbc,SQL_ATTR_AUTOCOMMIT,(SQLPOINTER)SQL_AUTOCOMMIT_ON,0);
	if(!SQL_SUCCEEDED(m_lasterror))
		return false;

	return true;
}

bool CMssqlConnection::RollbackTrans()
{
	m_lasterror = SQLEndTran(SQL_HANDLE_DBC,m_hDbc,SQL_ROLLBACK);

	if(!SQL_SUCCEEDED(m_lasterror))
		return false;

	m_lasterror = SQLSetConnectAttr(m_hDbc,SQL_ATTR_AUTOCOMMIT,(SQLPOINTER)SQL_AUTOCOMMIT_ON,0);
	if(!SQL_SUCCEEDED(m_lasterror))
		return false;

	return true;
}

bool CMssqlConnection::Bind(int variable, CSqlVariant value)
{
	m_bindVars[variable]=value;
	return true;
}

CSqlConnectionInformation* CMssqlConnection::GetConnectionInformation()
{
	if(!m_ConnectionInformation) m_ConnectionInformation = new CMssqlConnectionInformation();
	return m_ConnectionInformation;
}

const char *CMssqlConnection::parseTableName(const char *szName)
{
	CMssqlConnectionInformation *pCI = static_cast<CMssqlConnectionInformation*>(GetConnectionInformation());

	if(!szName)
		return NULL;

	if(!pCI->prefix.size())
		return szName;

	return cvs::cache_static_string((pCI->prefix+szName).c_str());
}
