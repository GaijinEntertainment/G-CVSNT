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
#endif

#include <config.h>
#include <cvsapi.h>

#include <oci.h>

#include "OracleConnection.h"
#include "OracleRecordset.h"
#include "OracleConnectionInformation.h"

#ifdef _WIN32
  #define SQL_EXPORT __declspec(dllexport)
#elif defined(HAVE_GCC_VISIBILITY)
  #define SQL_EXPORT __attribute__ ((visibility("default")))
#else
  #define SQL_EXPORT 
#endif

extern "C" SQL_EXPORT CSqlConnection *CreateConnection()
{
        return new COracleConnection;
}

#undef SQL_EXPORT

COracleConnection::COracleConnection()
{
	CServerIo::trace(3,"Instantiate COracleConnection ");
	m_hEnv = NULL;
	m_hErr = NULL;
	m_hSrv = NULL;
	m_hSvcCtx = NULL;
	m_hSess = NULL;
	m_bAutoCommit = true;
	m_lasterrorString = "";
	m_nchar_utf8=0;
}

COracleConnection::~COracleConnection()
{
	CServerIo::trace(3,"Destroy COracleConnection ");
	Close();
	CServerIo::trace(3,"Destroyed COracleConnection ");
}

bool COracleConnection::Create(const char *host, const char *database, const char *username, const char *password)
{
	COracleConnectionInformation *pCI = static_cast<COracleConnectionInformation*>(GetConnectionInformation());
	pCI->hostname=host?host:"localhost";
	pCI->database=database?database:"";
	pCI->username=username?username:"";
	pCI->password=password?password:"";
	CServerIo::trace(3,"COracleConnection::Create host=%s database=%s",
				pCI->hostname.c_str(), pCI->database.c_str());
	CServerIo::trace(3,"COracleConnection::Create user=%s password=%s",
				pCI->username.c_str(), pCI->password.c_str());

	return Create();
}

bool COracleConnection::Create()
{
	COracleConnectionInformation *pCI = static_cast<COracleConnectionInformation*>(GetConnectionInformation());

	return false; // Oracle doesn't let you do this
}

bool COracleConnection::Open(const char *host, const char *database, const char *username, const char *password)
{
	COracleConnectionInformation *pCI = static_cast<COracleConnectionInformation*>(GetConnectionInformation());
	pCI->hostname=host?host:"localhost";
	pCI->database=database?database:"";
	pCI->username=username?username:"";
	pCI->password=password?password:"";
	CServerIo::trace(3,"COracleConnection::Open host=%s database=%s",
				pCI->hostname.c_str(), pCI->database.c_str());
	CServerIo::trace(3,"COracleConnection::Open user=%s password=%s",
				pCI->username.c_str(), pCI->password.c_str());

	return Open();
}

bool COracleConnection::Open()
{
	sword attres;
	sword sessres;
	sword verres;
	sword infores;
	//sword descres;
	//sword allocres;
	OCIDescribe   *dschp = (OCIDescribe *)0;
	char server_version[2000];
	const unsigned int server_version_siz=2000;
        unsigned short env_charset=0, env_ncharset=0;
        size_t env_charset_siz=sizeof(env_charset);
        size_t env_ncharset_siz=sizeof(env_ncharset);
	unsigned char infoBuf[OCI_NLS_MAXBUFSZ];

	COracleConnectionInformation *pCI = static_cast<COracleConnectionInformation*>(GetConnectionInformation());

	/* Was like this in the example, but we need to specify UTF16 for strings (no utf8 support which would be simpler)
    OCIInitialize(OCI_DEFAULT, NULL, NULL, NULL, NULL );
	OCIEnvInit(&m_hEnv, OCI_DEFAULT, 0, NULL);
	OCIEnvNlsCreate(&m_hEnv, OCI_DEFAULT, NULL, NULL, NULL, NULL, 0, NULL, OCI_UTF16ID, OCI_UTF16ID);
	*/

	/* initialize the mode to be the default (non threaded) environment */
	/* the manual says that: 
		OCIEnvCreate() should be used instead of the OCIInitialize() 
		and OCIEnvInit() calls. OCIInitialize() and OCIEnvInit() 
		calls will be supported for backward compatibility. */
	CServerIo::trace(3," ::Open OCIEnvCreate(OCI_DEFAULT|OCI_OBJECT)");
	OCIEnvCreate(&m_hEnv, OCI_DEFAULT|OCI_OBJECT, NULL, NULL, NULL, NULL, 0, NULL);

	/* allocate a server handle */
	CServerIo::trace(3," ::Open OCIHandleAlloc(OCI_HTYPE_ERROR)");
	OCIHandleAlloc(m_hEnv, (void**)&m_hErr, OCI_HTYPE_ERROR, 0, 0);

	/* allocate an error handle */
	CServerIo::trace(3," ::Open OCIHandleAlloc(OCI_HTYPE_SERVER)");
	OCIHandleAlloc(m_hEnv, (void**)&m_hSrv, OCI_HTYPE_SERVER, 0, 0);

	/* create a server context */
	CServerIo::trace(3," ::CheckError OCIServerAttach()");
	attres=OCIServerAttach(m_hSrv, m_hErr, (OraText*)pCI->hostname.c_str(), pCI->hostname.length(), OCI_DEFAULT);
	if(!CheckError(m_hErr, attres))
		return false;

	/* allocate a service handle */
	CServerIo::trace(3," ::Open OCIHandleAlloc(OCI_HTYPE_SVCCTX)");
	OCIHandleAlloc(m_hEnv, (void**)&m_hSvcCtx, OCI_HTYPE_SVCCTX, 0, 0);

	/* set the server attribute in the service context handle*/
	CServerIo::trace(3," ::Open OCIAttrSet(OCI_HTYPE_SVCCTX)");
	OCIAttrSet(m_hSvcCtx, OCI_HTYPE_SVCCTX, m_hSrv, 0, OCI_ATTR_SERVER, m_hErr);

	/* allocate a user session handle */
	CServerIo::trace(3," ::Open OCIHandleAlloc(OCI_HTYPE_SESSION)");
	OCIHandleAlloc(m_hEnv, (void**)&m_hSess, OCI_HTYPE_SESSION, 0, 0);

	/* set username attribute in user session handle */
	CServerIo::trace(3," ::Open OCIAttrSet(OCI_HTYPE_SESSION, OCI_ATTR_USERNAME)");
	OCIAttrSet(m_hSess, OCI_HTYPE_SESSION, (void*)pCI->username.c_str(), pCI->username.length(), OCI_ATTR_USERNAME, m_hErr);

	/* set password attribute in user session handle */
	CServerIo::trace(3," ::Open OCIAttrSet(OCI_HTYPE_SESSION, OCI_ATTR_PASSWORD)");
	OCIAttrSet(m_hSess, OCI_HTYPE_SESSION, (void*)pCI->password.c_str(), pCI->password.length(), OCI_ATTR_PASSWORD, m_hErr);

	CServerIo::trace(3," ::CheckError OCISessionBegin()");
	sessres=OCISessionBegin( m_hSvcCtx, m_hErr, m_hSess, OCI_CRED_RDBMS, OCI_DEFAULT);
	if(!CheckError(m_hErr, sessres))
		return false;

	/* set the user session attribute in the service context handle*/
	CServerIo::trace(3," ::Open OCIAttrSet(OCI_HTYPE_SVCCTX)");
	OCIAttrSet(m_hSvcCtx, OCI_HTYPE_SVCCTX, m_hSess, 0, OCI_ATTR_SESSION, m_hErr);

	/* get and print in trace the server version */
	CServerIo::trace(3," ::Open OCIServerVersion()");
	server_version[0]='\0';
	verres=OCIServerVersion( m_hSvcCtx, m_hErr, (OraText*)server_version, server_version_siz, OCI_HTYPE_SVCCTX /* or m_hSrv would be OCI_HTYPE_SERVER */);
	if(!CheckError(m_hErr, verres))
		return false;
	CServerIo::trace(3,"%s",server_version);

	/*
	CServerIo::trace(3," ::Open OCIAttrGet (OCI_ATTR_CHARSET_ID)");
	infores=OCIAttrGet( m_hSvcCtx, OCI_HTYPE_SVCCTX, &env_charset, &env_charset_siz, OCI_ATTR_CHARSET_ID, m_hErr );
	if (infores == OCI_SUCCESS)
	CServerIo::trace(3," ::Open OCI_ATTR_CHARSET_ID=%d",(int)env_charset);
	CServerIo::trace(3," ::Open OCIAttrGet (OCI_ATTR_NCHARSET_ID)");
	infores=OCIAttrGet( m_hSvcCtx, OCI_HTYPE_SVCCTX, &env_ncharset, &env_ncharset_siz, OCI_ATTR_NCHARSET_ID, m_hErr );
	if (infores == OCI_SUCCESS)
	CServerIo::trace(3," ::Open OCI_ATTR_NCHARSET_ID=%d",(int)env_ncharset);
	*/

	/*
	  seems like the only way to get the database name is:
	  select sys_context('userenv', 'db_name') from dual;

	  So that leaves me in a catch 22 if I want to find the 
	  Character Set at this point in the code.  So just 
	  comment all this out for now ...

	allocres=OCIHandleAlloc(m_hEnv ,(void **)&dschp, OCI_HTYPE_DESCRIBE, 0, 0);
	if (allocres == OCI_SUCCESS)
	{
	CServerIo::trace(3," ::Open Describe the server");
	server_version[0]='\0';
	descres=OCIDescribeAny(m_hSvcCtx, m_hErr, (void *)&server_version, server_version_siz, OCI_ATTR_VERSION, 0, OCI_PTYPE_DATABASE, dschp);
	if (descres == OCI_SUCCESS)
	CServerIo::trace(3," ::Open OCI_ATTR_VERSION=%s",server_version);
	else
	{
	if (!CheckError(m_hErr, descres))
	CServerIo::trace(3," ::Open OCI_ATTR_VERSION error %s",m_lasterrorString.c_str());
	}

	descres=OCIDescribeAny(m_hSvcCtx, m_hErr, (void *)&env_ncharset, env_ncharset_siz, OCI_ATTR_NCHARSET_ID, 0, OCI_PTYPE_DATABASE, dschp);
	if (descres == OCI_SUCCESS)
	CServerIo::trace(3," ::Open OCI_ATTR_NCHARSET_ID=%d",(int)env_ncharset);
	else
	{
	if (!CheckError(m_hErr, descres))
	CServerIo::trace(3," ::Open OCI_ATTR_NCHARSET_ID error %s",m_lasterrorString.c_str());
	}

	descres=OCIDescribeAny(m_hSvcCtx, m_hErr, (void *)&env_charset, env_charset_siz, OCI_ATTR_CHARSET_ID, OCI_DEFAULT, OCI_PTYPE_DATABASE, dschp);
	if (descres == OCI_SUCCESS)
	CServerIo::trace(3," ::Open OCI_ATTR_CHARSET_ID=%d",(int)env_charset);
	else
	{
	if (!CheckError(m_hErr, descres))
	CServerIo::trace(3," ::Open OCI_ATTR_CHARSET_ID error %s",m_lasterrorString.c_str());
	}

	OCIHandleFree(dschp,OCI_HTYPE_DESCRIBE);
	}
	*/

	infoBuf[0]='\0';
	CServerIo::trace(3," ::Open OCINlsGetInfo(OCI_NLS_LANGUAGE)");
	infores = OCINlsGetInfo(m_hEnv, m_hErr, &infoBuf[0], 
	                           (size_t) OCI_NLS_MAXBUFSZ, 
				   (ub2) OCI_NLS_LANGUAGE);
	if (infores == OCI_SUCCESS)
	CServerIo::trace(3," ::Open Client OCI_NLS_LANGUAGE=%s",infoBuf);

	CServerIo::trace(3," ::Open OCINlsGetInfo(OCI_NLS_CHARACTER_SET)");
	infores = OCINlsGetInfo(m_hEnv, m_hErr, &infoBuf[0], 
	                           (size_t) OCI_NLS_MAXBUFSZ, 
				   (ub2) OCI_NLS_CHARACTER_SET);
	if (infores == OCI_SUCCESS)
	CServerIo::trace(3," ::Open Client OCI_NLS_CHARACTER_SET=%s",infoBuf);

	CServerIo::trace(3," ::Open OCINlsGetInfo(OCI_NLS_LANGUAGE)");
	infores = OCINlsGetInfo(m_hSvcCtx/*m_hSess*/, m_hErr, &infoBuf[0], 
	                           (size_t) OCI_NLS_MAXBUFSZ, 
				   (ub2) OCI_NLS_LANGUAGE);
	if (infores == OCI_SUCCESS)
	CServerIo::trace(3," ::Open Server OCI_NLS_LANGUAGE=%s",infoBuf);

	CServerIo::trace(3," ::Open OCINlsGetInfo(OCI_NLS_CHARACTER_SET)");
	infores = OCINlsGetInfo(m_hSvcCtx/*m_hSess*/, m_hErr, &infoBuf[0], 
	                           (size_t) OCI_NLS_MAXBUFSZ, 
				   (ub2) OCI_NLS_CHARACTER_SET);
	if (infores == OCI_SUCCESS)
	CServerIo::trace(3," ::Open Server OCI_NLS_CHARACTER_SET=%s",infoBuf);

	m_bAutoCommit = true;

	CServerIo::trace(3," ::Open _Execute() - set nls_date_format");
	_Execute("alter session set nls_date_format = 'YYYY-MM-DD HH24:MI:SS'",NULL);

	CServerIo::trace(3," ::Open _Execute() - get nls_database_parameter");
	CSqlRecordsetPtr rs = _Execute("select value as NCHAR from nls_database_parameters where parameter = 'NLS_NCHAR_CHARACTERSET'",NULL);
	if (!Error())
	{
	CSqlField *rsf = rs[0];
	if (rsf!=NULL)
	{
	cvs::string selresult = (const char *)*rsf;
	if (selresult.length()!=0)
	{
	CServerIo::trace(3," ::Open NLS_NCHAR_CHARACTERSET = %s",selresult.c_str());
	if (strstr(selresult.c_str(),"UTF")==NULL)
	{
	CServerIo::trace(1,"***********************************************");
	CServerIo::trace(1," WARNING: Oracle National Character Set");
	CServerIo::trace(1," ");
	CServerIo::trace(1," Character Set \"%s\" is unsuitable.",selresult.c_str());
	CServerIo::trace(1," Please choose UTF8, AL16UTF16 etc ");
	CServerIo::trace(1,"***********************************************");
	m_nchar_utf8=1;
	}
	else
	if (strcmp(selresult.c_str(),"UTF8")==0)
	{
		m_nchar_utf8=1;
		CServerIo::trace(3," Appears to be an Oracle 8 / UTF8 server, will try and take care of this... ");
	}
	else
	m_nchar_utf8=0;
	}
	}
	}

	/* a little debugging just to see how NCHAR comes back... */
	/*
	CServerIo::trace(3," ::Open _Execute() - get physrepos from SessionLog");
	CSqlRecordsetPtr rs2 = _Execute("select physrepos as REPO from CVSNT.SessionLog", NULL);
	if (!Error())
	{
	CSqlField *rsf2 = rs2[0];
	if (rsf2!=NULL)
	{
	const char *selresult2 = (const char *)*rsf2;
	if (selresult2!=NULL)
	{
	CServerIo::trace(3," ::Open phyrepos = %s",selresult2);
	}
	}
	}
	*/

	CServerIo::trace(3," ::Open return(true)");
	return true;
}

bool COracleConnection::Close()
{
	CServerIo::trace(3,"Close COracleConnection ");
	if(m_hEnv)
	{
		CServerIo::trace(3,"Free Handle Env");
		OCIHandleFree(m_hEnv,OCI_HTYPE_ENV);
		CServerIo::trace(3,"Free Handle Error");
		OCIHandleFree(m_hErr,OCI_HTYPE_ERROR);
		CServerIo::trace(3,"Free Handle Server");
		OCIHandleFree(m_hSrv,OCI_HTYPE_SERVER);
		OCIHandleFree(m_hSvcCtx,OCI_HTYPE_SVCCTX);
		CServerIo::trace(3,"Free Handle Service Context");
		OCIHandleFree(m_hSess,OCI_HTYPE_SESSION);
		CServerIo::trace(3,"Free Handle Session");
	}
	m_hEnv = NULL;
	m_hErr = NULL;
	m_hSrv = NULL;
	m_hSvcCtx = NULL;
	m_hSess = NULL;
	CServerIo::trace(3,"Closed COracleConnection ");
	return true;
}

bool COracleConnection::IsOpen()
{
	if(m_hEnv)
		return true;
	return false;
}

CSqlRecordsetPtr COracleConnection::Execute(const char *string, ...)
{
	cvs::string str;
	va_list va;

	va_start(va,string);
	cvs::vsprintf(str,64,string,va);
	va_end(va);

	CServerIo::trace(3,"COracleConnection::Execute _Execute()");
	return _Execute(str.c_str(),NULL);
}

unsigned COracleConnection::ExecuteAndReturnIdentity(const char *string, ...)
{
	cvs::string str;
	va_list va;
	unsigned vId;

	va_start(va,string);
	cvs::vsprintf(str,64,string,va);
	va_end(va);

	str += " returning Id Into :identity";

	CServerIo::trace(3,"COracleConnection:::ExecuteAndReturnIdentity _Execute()");
	_Execute(str.c_str(), &vId);

	if(Error())
		return 0;
	return vId;
}

CSqlRecordsetPtr COracleConnection::_Execute(const char *string, unsigned* identity)
{
	cvs::string tmp;
	cvs::string str;
	cvs::string fldstr, fldstr2;
	cvs::wstring wfldstr;
	int zzz, yyy;
	char myvalchar;

	COracleRecordset *rs = new COracleRecordset();

	CServerIo::trace(3,"%s",string);

	str=string;

	bool inquote = false;
	int bindnum=1;
	CServerIo::trace(3,"COracleConnection::_Execute()");
	for(size_t n=0; n<str.size(); n++)
	{
		if(str[n]=='\'')
			inquote = !inquote;
		if(!inquote && str[n]=='?')
		{
			cvs::sprintf(tmp,16,":%d",bindnum++);
			str.replace(n++,1,tmp);
		}
	}
	CServerIo::trace(3," ::_Execute(str=%s)",str.c_str());

	OCIStmt *hStmt;

	if(!CheckError(m_hErr, OCIHandleAlloc(m_hEnv, (void**)&hStmt, OCI_HTYPE_STMT, 0, 0)))
		return rs;

	CServerIo::trace(3," ::_Execute -  OCIStmtPrepare(%s,%d)",
				str.c_str(),str.length());
	if(!CheckError(m_hErr, OCIStmtPrepare(hStmt, m_hErr, (OraText *)str.c_str(), str.length(), OCI_NTV_SYNTAX, OCI_DEFAULT)))
	{
		OCIHandleFree(hStmt,OCI_HTYPE_STMT);
		return rs;
	}


	OCIBind *hBind = NULL;
	long str_length=0;
	//OCILobLocator * one_lob;
	//void * one_lob_mem;
	ub2 csid = OCI_UCS2ID;
	ub1 formid = SQLCS_NCHAR;
	sb4 col_len = 20; /* some examples show this as sb2 */

	CServerIo::trace(3," ::_Execute - csid = OCI_UCS2ID(%d);  formid = SQLCS_NCHAR(%d)",(int)OCI_UCS2ID,(int)SQLCS_NCHAR);
	/*CServerIo::trace(3," ::_Execute - OCI_UTF16ID(%d)",(int)OCI_UTF16ID);*/

	for(std::map<int,CSqlVariant>::iterator i = m_bindVars.begin(); i!=m_bindVars.end(); ++i)
	{
		hBind = NULL;

		CServerIo::trace(3," ::_Execute -  Prepare to Bind(%d)",i->first+1);
		switch(i->second.type())
		{
		case CSqlVariant::vtNull:
			CServerIo::trace(3," ::_Execute -  Bind Null pos=%d",i->first+1);
			if(!CheckError(m_hErr, OCIBindByPos(hStmt,&hBind, m_hErr, i->first+1, NULL, 0, SQLT_CHR, 0, 0, 0, 0, 0, OCI_DEFAULT)))
			{
				OCIHandleFree(hStmt,OCI_HTYPE_STMT);
				return rs;
			}
			break;
		case CSqlVariant::vtChar:
			CServerIo::trace(3," ::_Execute -  Bind vtChar pos=%d",i->first+1);
			m_sqlv[i->first].wc=(char)i->second;
			if(!CheckError(m_hErr,OCIBindByPos(hStmt,&hBind, m_hErr, i->first+1, &m_sqlv[i->first].wc, sizeof(wchar_t), SQLT_CHR, 0, 0, 0, 0, 0, OCI_DEFAULT)))
			{
				OCIHandleFree(hStmt,OCI_HTYPE_STMT);
				return rs;
			}
			OCIAttrSet(hBind,OCI_HTYPE_BIND,&csid,0,OCI_ATTR_CHARSET_ID,m_hErr);
			col_len=sizeof(wchar_t);
			OCIAttrSet(hBind,OCI_HTYPE_BIND,&col_len,0,OCI_ATTR_MAXDATA_SIZE,m_hErr);
			//OCIAttrSet(hBind,OCI_HTYPE_BIND,&formid,0,OCI_ATTR_CHARSET_FORM,m_hErr);
			break;
		case CSqlVariant::vtUChar:
			CServerIo::trace(3," ::_Execute -  Bind vtUCharpos=%d",i->first+1);
			m_sqlv[i->first].wc=(unsigned char)i->second;
			if(!CheckError(m_hErr,OCIBindByPos(hStmt,&hBind, m_hErr, i->first+1, &m_sqlv[i->first].wc, sizeof(wchar_t), SQLT_CHR, 0, 0, 0, 0, 0, OCI_DEFAULT)))
			{
				OCIHandleFree(hStmt,OCI_HTYPE_STMT);
				return rs;
			}
			OCIAttrSet(hBind,OCI_HTYPE_BIND,&csid,0,OCI_ATTR_CHARSET_ID,m_hErr);
			col_len=sizeof(wchar_t);
			OCIAttrSet(hBind,OCI_HTYPE_BIND,&col_len,0,OCI_ATTR_MAXDATA_SIZE,m_hErr);
			//OCIAttrSet(hBind,OCI_HTYPE_BIND,&formid,0,OCI_ATTR_CHARSET_FORM,m_hErr);
			break;
		case CSqlVariant::vtShort:
			CServerIo::trace(3," ::_Execute -  Bind vtShortpos=%d",i->first+1);
			m_sqlv[i->first].s=i->second;
			if(!CheckError(m_hErr,OCIBindByPos(hStmt,&hBind, m_hErr, i->first+1, &m_sqlv[i->first].s, sizeof(m_sqlv[i->first].s), SQLT_INT, 0, 0, 0, 0, 0, OCI_DEFAULT)))
			{
				OCIHandleFree(hStmt,OCI_HTYPE_STMT);
				return rs;
			}
			break;
		case CSqlVariant::vtUShort:
			CServerIo::trace(3," ::_Execute -  Bind vtUShort pos=%d",i->first+1);
			m_sqlv[i->first].s=i->second;
			if(!CheckError(m_hErr,OCIBindByPos(hStmt,&hBind, m_hErr, i->first+1, &m_sqlv[i->first].s, sizeof(m_sqlv[i->first].s), SQLT_INT, 0, 0, 0, 0, 0, OCI_DEFAULT)))
			{
				OCIHandleFree(hStmt,OCI_HTYPE_STMT);
				return rs;
			}
			break;
		case CSqlVariant::vtInt:
		case CSqlVariant::vtLong:
			CServerIo::trace(3," ::_Execute -  Bind vtInt vtLong pos=%d",i->first+1);
			m_sqlv[i->first].l=i->second;
			if(!CheckError(m_hErr,OCIBindByPos(hStmt,&hBind, m_hErr, i->first+1, &m_sqlv[i->first].l, sizeof(m_sqlv[i->first].l), SQLT_INT, 0, 0, 0, 0, 0, OCI_DEFAULT)))
			{
				OCIHandleFree(hStmt,OCI_HTYPE_STMT);
				return rs;
			}
			break;
		case CSqlVariant::vtUInt:
		case CSqlVariant::vtULong:
			CServerIo::trace(3," ::_Execute -  Bind vtUInt vtULong pos=%d",i->first+1);
			m_sqlv[i->first].l=i->second;
			if(!CheckError(m_hErr,OCIBindByPos(hStmt,&hBind, m_hErr, i->first+1, &m_sqlv[i->first].l, sizeof(m_sqlv[i->first].l), SQLT_UIN, 0, 0, 0, 0, 0, OCI_DEFAULT)))
			{
				OCIHandleFree(hStmt,OCI_HTYPE_STMT);
				return rs;
			}
			break;
		case CSqlVariant::vtLongLong:
			CServerIo::trace(3," ::_Execute -  Bind vtLongLong pos=%d",i->first+1);
			// not supported by oracle.  Devolve to long.. may lose data
			m_sqlv[i->first].l=i->second;
			if(!CheckError(m_hErr,OCIBindByPos(hStmt,&hBind, m_hErr, i->first+1, &m_sqlv[i->first].l, sizeof(m_sqlv[i->first].l), SQLT_INT, 0, 0, 0, 0, 0, OCI_DEFAULT)))
			{
				OCIHandleFree(hStmt,OCI_HTYPE_STMT);
				return rs;
			}
			break;
		case CSqlVariant::vtULongLong:
			CServerIo::trace(3," ::_Execute -  Bind vtULongLong pos=%d",i->first+1);
			// not supported by oracle.  Devolve to long.. may lose data
			m_sqlv[i->first].l=i->second;
			if(!CheckError(m_hErr,OCIBindByPos(hStmt,&hBind, m_hErr, i->first+1, &m_sqlv[i->first].l, sizeof(m_sqlv[i->first].l), SQLT_UIN, 0, 0, 0, 0, 0, OCI_DEFAULT)))
			{
				OCIHandleFree(hStmt,OCI_HTYPE_STMT);
				return rs;
			}
			break;
		case CSqlVariant::vtString:
			/* The wide character data type is Oracle-specific 
			   and not to be confused with the wchar_t defined 
			   by the ANSI/ISO C standard. The Oracle wide 
			   character is always 4 bytes in all platforms, 
			   while wchar_t is implementation- and 
			   platform-dependent. */

			fldstr=cvs::string(i->second);
			wfldstr=cvs::wide(fldstr.c_str());
			str_length=cvs::cvsucs2(wfldstr.c_str(),wfldstr.length());
			col_len=(fldstr.length()*2)+2;
			col_len=(str_length*2)+2;
			m_sqlv[i->first].cstr.reserve(col_len);
			memset((void *)m_sqlv[i->first].cstr.data(),0,col_len);
			memcpy((void *)m_sqlv[i->first].cstr.data(),(const char *)cvs::cvsucs2(wfldstr.c_str(),wfldstr.length()),str_length+2);

			// a little debugging
			yyy=0; fldstr2.resize(fldstr.length()+1);
			myvalchar=m_sqlv[i->first].cstr[1];
			for (zzz=1;(size_t)zzz<(fldstr.length()*2);zzz+=2)
				fldstr2[yyy++]=m_sqlv[i->first].cstr[zzz];
			fldstr2[yyy]='\0';
			// done debugging
			CServerIo::trace(3," ::_Execute -  Bind vtString \"%s\",%d,%d,\"%s\",%d,%d,%d,'%c' pos=%d",fldstr.c_str(),(int)str_length,(int)(col_len),fldstr2.c_str(),zzz,yyy,(int)myvalchar,myvalchar,i->first+1);
			if (fldstr.length()+1<256)
			{
			if(!CheckError(m_hErr,OCIBindByPos(hStmt,&hBind, m_hErr, i->first+1, (void*)m_sqlv[i->first].cstr.data(), col_len, SQLT_STR, 0, 0, 0, 0, 0, OCI_DEFAULT)))
			{
				OCIHandleFree(hStmt,OCI_HTYPE_STMT);
				return rs;
			}
			}
			else
			{
			// the field is too long to be a varchar, so treat it
			// as a NCLOB instead...
			CServerIo::trace(3," ::_Execute - the field (%lu) is too long to be a varchar, so treat it as a NCLOB instead...",(unsigned long)fldstr.length()+1);
			//one_lob=NULL;
			//one_lob_mem=NULL;
			//if(!CheckError(m_hErr,OCIDescriptorAlloc(m_hEnv,&one_lob,OCI_DTYPE_LOB,(size_t)0/*str_length+2*/,(dvoid **)0/*&one_lob_mem)*/)
			//{
			//	return rs;
			//}
			//OCIAttrSet(OCI_ATTR_LOBEMPTY );
			//if(!CheckError(m_hErr,OCIBindByPos(hStmt,&hBind, m_hErr, i->first+1, (void*)&one_lob, sizeof(OCILobLocator *), SQLT_NCLOB, 0, 0, 0, 0, 0, OCI_DEFAULT)))
			if(!CheckError(m_hErr,OCIBindByPos(hStmt,&hBind, m_hErr, i->first+1, (void*)m_sqlv[i->first].cstr.data(), col_len, SQLT_LNG, 0, 0, 0, 0, 0, OCI_DEFAULT)))
			{
				OCIHandleFree(hStmt,OCI_HTYPE_STMT);
				return rs;
			}
			}
			if (m_nchar_utf8)
			{
			// for some reason if you do this on an 8i client
			// to 9i server (server national character set is
			// AL16UTF16) then the server just crashes
			// but if you do NOT do it for an Oracle 8i server
			// ((server national character set is UTF8) then
			// it gives a character set mismatch error
			CServerIo::trace(3," ::_Execute -  OCIAttrSet formid=%d OCI_ATTR_CHARSET_FORM=%d",(int)formid,(int)OCI_ATTR_CHARSET_FORM);
			OCIAttrSet(hBind,OCI_HTYPE_BIND,&formid,0,OCI_ATTR_CHARSET_FORM,m_hErr);
			}
			CServerIo::trace(3," ::_Execute -  OCIAttrSet csid=%d OCI_ATTR_CHARSET_ID=%d",(int)csid,(int)OCI_ATTR_CHARSET_ID);
			OCIAttrSet(hBind,OCI_HTYPE_BIND,&csid,0,OCI_ATTR_CHARSET_ID,m_hErr);
//			col_len=((fldstr.length()*sizeof(wchar_t))*4);
//			col_len=(fldstr.length()*2)+2;
			col_len=fldstr.length();
//			CServerIo::trace(3," ::_Execute -  col_len=%d",(int)(col_len));
//			m_sqlv[i->first].ws.resize(col_len);
//			m_sqlv[i->first].cstr.resize(col_len);
			CServerIo::trace(3," ::_Execute -  OCIAttrSet col_len=%d size=%d, OCI_ATTR_MAXDATA_SIZE=%d",(int)col_len,(int)sizeof(col_len),(int)OCI_ATTR_MAXDATA_SIZE);
			OCIAttrSet(hBind,OCI_HTYPE_BIND,&col_len,0,OCI_ATTR_MAXDATA_SIZE,m_hErr);
			break;
		case CSqlVariant::vtWString:
			CServerIo::trace(3," ::_Execute -  Bind vtWString \"%s\" pos=%d",i->second,i->first+1);
			m_sqlv[i->first].ws=cvs::wide(i->second);
			if(!CheckError(m_hErr,OCIBindByPos(hStmt,&hBind, m_hErr, i->first+1, (void*)m_sqlv[i->first].ws.data(), m_sqlv[i->first].ws.length()*sizeof(wchar_t), SQLT_CHR, 0, 0, 0, 0, 0, OCI_DEFAULT)))
			{
				OCIHandleFree(hStmt,OCI_HTYPE_STMT);
				return rs;
			}
			//CServerIo::trace(3," ::_Execute -  OCIAttrSet formid=%d OCI_ATTR_CHARSET_FORM=%d",(int)formid,(int)OCI_ATTR_CHARSET_FORM);
			//OCIAttrSet(hBind,OCI_HTYPE_BIND,&formid,0,OCI_ATTR_CHARSET_FORM,m_hErr);
			CServerIo::trace(3," ::_Execute -  OCIAttrSet csid=%d OCI_ATTR_CHARSET_ID=%d",(int)csid,(int)OCI_ATTR_CHARSET_ID);
			OCIAttrSet(hBind,OCI_HTYPE_BIND,&csid,0,OCI_ATTR_CHARSET_ID,m_hErr);
			CServerIo::trace(3," ::_Execute -  OCIAttrSet col_len");
			col_len=m_sqlv[i->first].ws.length()*sizeof(wchar_t);
			OCIAttrSet(hBind,OCI_HTYPE_BIND,&col_len,0,OCI_ATTR_MAXDATA_SIZE,m_hErr);
			break;
		}
	}

	if(identity)
	{
			hBind = NULL;
			CServerIo::trace(3," ::_Execute -  BindByName - identity");
			if(!CheckError(m_hErr,OCIBindByName(hStmt,&hBind, m_hErr, (OraText*)":identity", strlen(":identity"), identity, sizeof(*identity), SQLT_UIN, 0, 0, 0, 0, 0, OCI_DEFAULT)))
			{
				OCIHandleFree(hStmt,OCI_HTYPE_STMT);
				return rs;
			}
	}

	CServerIo::trace(3," ::_Execute -  rs->Init()");
	rs->Init(this,hStmt,strncasecmp(string,"select ",7)?false:true); // Ignore return... it's handled by the error routines

	CServerIo::trace(3," ::_Execute -  m_bindVars.clear()");
	m_bindVars.clear();

	CServerIo::trace(3," ::_Execute -  return rs");
	return rs;
}

bool COracleConnection::Error() const
{
	if(m_lasterror==OCI_SUCCESS)
		return false;
	return true;
}

const char *COracleConnection::ErrorString()
{
	return m_lasterrorString.c_str();
}

unsigned COracleConnection::GetInsertIdentity(const char *table_hint) 
{
	return (unsigned)0; // not implemented
}

bool COracleConnection::BeginTrans()
{
	if(!m_bAutoCommit)
	{
		m_lasterrorString="BeginTrans called inside transaction";
		return false;
	}
	m_bAutoCommit = false;
	return true;
}

bool COracleConnection::CommitTrans()
{
	if(m_bAutoCommit)
	{
		m_lasterrorString="CommitTrans called outside transaction";
		return false;
	}
	m_bAutoCommit = true;
	return CheckError(m_hErr,OCITransCommit(m_hSvcCtx, m_hErr, OCI_DEFAULT));
}

bool COracleConnection::RollbackTrans()
{
	if(m_bAutoCommit)
	{
		m_lasterrorString="RallbackTrans called outside transaction";
		return false;
	}
	m_bAutoCommit = true;
	return CheckError(m_hErr,OCITransRollback(m_hSvcCtx, m_hErr, OCI_DEFAULT));
}

bool COracleConnection::Bind(int variable, CSqlVariant value)
{
	m_bindVars[variable]=value;
	return true;
}

CSqlConnectionInformation* COracleConnection::GetConnectionInformation()
{
	if(!m_ConnectionInformation) m_ConnectionInformation = new COracleConnectionInformation();
	return m_ConnectionInformation;
}

const char *COracleConnection::parseTableName(const char *szName)
{
	COracleConnectionInformation *pCI = static_cast<COracleConnectionInformation*>(GetConnectionInformation());

	if(!szName)
		return NULL;

	if(!pCI->schema.size())
		return szName;

	return cvs::cache_static_string((pCI->schema+"."+szName).c_str());
}

bool COracleConnection::CheckError(OCIError *hErr, sb4 status)
{
	cvs::string nws;
	/*cvs::string nws2;*/
	unsigned int recordno=1;
	m_lasterror = status;
	switch(status)
	{
	case OCI_SUCCESS:
		m_lasterrorString="";
		return true;
	case OCI_SUCCESS_WITH_INFO:
		m_lasterrorString="Success with Info";
		return true;
	case OCI_NEED_DATA:
		m_lasterrorString="Need Data";
		return false;
	case OCI_NO_DATA:
		m_lasterrorString="No Data";
		return false;
	case OCI_INVALID_HANDLE:
		m_lasterrorString="Invalid Handle";
		return false;
	case OCI_STILL_EXECUTING:
		m_lasterrorString="Still Executing";
		return true; //??
	case OCI_CONTINUE:
		m_lasterrorString="Continue";
		return true; //??
	case OCI_ERROR:
		nws.resize(512);
		OCIErrorGet(hErr, recordno, NULL, &status, (OraText*)nws.data(), nws.size(), OCI_HTYPE_ERROR);
		/*nws2.resize(512);
		nws=""; nws2=""; recordno=1;
		while (OCIErrorGet(hErr, recordno, NULL, &status, (OraText*)nws.data(), nws.size(), OCI_HTYPE_ERROR)==OCI_SUCCESS)
		{
			if (recordno>1)
				nws2+="\n";
			recordno++;
			nws2.resize(nws2.size()+nws.length()+10);
			nws2+=nws;
		}
		m_lasterrorString = nws2.c_str();
		*/
		m_lasterrorString = nws.c_str();
		return false;
	default:
		m_lasterrorString="Unknown error";
		return false;
	}
}
