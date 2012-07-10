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
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#define snprintf _snprintf

#include <stdlib.h>

#include <config.h>
#include <cvsapi.h>

#include <string>

#include <sql.h>
#include <sqlext.h>

#include "MssqlConnection.h"
#include "MssqlRecordset.h"

CMssqlField::CMssqlField()
{
	data = NULL;
}

CMssqlField::~CMssqlField()
{
	if(data)
		free(data);
}

CMssqlField::operator int()
{
	switch(ctype)
	{
		case SQL_C_WCHAR:
			{ int ret=0; swscanf((const wchar_t *)data,L"%d",&ret); return ret; }
		case SQL_C_LONG:
			return (int)*(long*)data;
		case SQL_C_DOUBLE:
			return (int)*(double*)data;
		default:
			CServerIo::trace(1,"Bogus value return for field %s",name.c_str());
			return 0;
	}
}

CMssqlField::operator long()
{
	switch(ctype)
	{
		case SQL_C_WCHAR:
			{ long ret=0; swscanf((const wchar_t *)data,L"%ld",&ret); return ret; }
		case SQL_C_LONG:
			return (long)*(long*)data;
		case SQL_C_DOUBLE:
			return (long)*(double*)data;
		default:
			CServerIo::trace(1,"Bogus value return for field %s",name.c_str());
			return 0;
	}
}

CMssqlField::operator unsigned()
{
	switch(ctype)
	{
		case SQL_C_WCHAR:
			{ unsigned ret=0; swscanf((const wchar_t *)data,L"%u",&ret); return ret; }
		case SQL_C_LONG:
			return (unsigned)*(long*)data;
		case SQL_C_DOUBLE:
			return (unsigned)*(double*)data;
		default:
			CServerIo::trace(1,"Bogus value return for field %s",name.c_str());
			return 0;
	}
}

CMssqlField::operator unsigned long()
{
	switch(ctype)
	{
		case SQL_C_WCHAR:
			{ unsigned long ret=0; swscanf((const wchar_t *)data,L"%lu",&ret); return ret; }
		case SQL_C_LONG:
			return (unsigned long)*(long*)data;
		case SQL_C_DOUBLE:
			return (unsigned long)*(double*)data;
		default:
			CServerIo::trace(1,"Bogus value return for field %s",name.c_str());
			return 0;
	}
}

CMssqlField::operator __int64()
{
	switch(ctype)
	{
		case SQL_C_WCHAR:
			{ __int64 ret=0; swscanf((const wchar_t *)data,L"%I64d",&ret); return ret; }
		case SQL_C_LONG:
			return (__int64)*(long*)data;
		case SQL_C_DOUBLE:
			return (__int64)*(double*)data;
		default:
			CServerIo::trace(1,"Bogus value return for field %s",name.c_str());
			return 0;
	}
}

CMssqlField::operator const char *()
{
	switch(ctype)
	{
		case SQL_C_WCHAR:
			tmpstr = cvs::narrow((const wchar_t*)data);
			return tmpstr.c_str();
		case SQL_C_LONG:
			cvs::sprintf(tmpstr,32,"%ld",*(long*)data);
			return tmpstr.c_str();
		case SQL_C_DOUBLE:
			cvs::sprintf(tmpstr,32,"%lf",*(double*)data);
			return tmpstr.c_str();
		default:
			CServerIo::trace(1,"Bogus value return for field %s",name.c_str());
			return 0;
	}
}

CMssqlField::operator const wchar_t *()
{
	switch(ctype)
	{
		case SQL_C_WCHAR:
			return (const wchar_t*)data;
		case SQL_C_LONG:
			cvs::swprintf(tmpwstr,32,L"%ld",*(long*)data);
			return tmpwstr.c_str();
		case SQL_C_DOUBLE:
			cvs::swprintf(tmpwstr,32,L"%lf",*(double*)data);
			return tmpwstr.c_str();
		default:
			CServerIo::trace(1,"Bogus value return for field %s",name.c_str());
			return 0;
	}
}

/**********************************************************************/

CMssqlRecordset::CMssqlRecordset()
{
	m_hStmt = NULL;
	m_bEof = true;
}

CMssqlRecordset::~CMssqlRecordset()
{
	Close();
}

bool CMssqlRecordset::Init(CMssqlConnection *parent, HSTMT hStmt, const char *command)
{
	m_bEof = false;
	m_parent = parent;
	m_hStmt = hStmt;

	m_parent->m_lasterror=SQLExecDirect(m_hStmt,(SQLWCHAR*)(const wchar_t *)cvs::wide(command),SQL_NTS);
	CServerIo::trace(1,"MSSQL Execute Done");

	if((!SQL_SUCCEEDED(m_parent->m_lasterror))&&(m_parent->m_lasterror!=SQL_NEED_DATA))
	{
		GetStmtError();
		return false;
	}

	if(m_parent->m_lasterror==SQL_NEED_DATA)
	{
		SQLPOINTER pParmID;
		SQLRETURN retcode=SQL_SUCCESS, putret;
		char *dataptr, *dataput;
		SQLINTEGER dataoff, datasiz;
		SQLINTEGER chunk=1024;

		CServerIo::trace(1,"MSSQL Execute requires more data");
		retcode = SQLParamData(m_hStmt, &pParmID);
		if (retcode == SQL_NEED_DATA)
		{
		for(std::map<int,CSqlVariant>::iterator i = parent->m_bindVars.begin(); i!=parent->m_bindVars.end(); ++i)
		{
			switch(i->second.type())
			{
			case CSqlVariant::vtString:
				if (parent->m_sqlv[i->first].ws.length()+1<256)
						CServerIo::trace(1,"MSSQL Execute this parameter is too small to be a BLOB");
				else
				{
					//dataput = (char *)((const char *)parent->m_sqlv[i->first].cs.c_str());
					//dataoff=0; datasiz = (SQLINTEGER)(parent->m_sqlv[i->first].cs.size()+1);
					dataput = (char *)((const char *)parent->m_sqlv[i->first].ws.c_str());
					dataoff=0; datasiz = (SQLINTEGER)(parent->m_sqlv[i->first].ws.size()+1);

					//CServerIo::trace(1,"MSSQL Needs data - so put the data %d, size %d",(int)i->first,(int)parent->m_sqlv[i->first].cs.size()+1);
					CServerIo::trace(1,"MSSQL Needs data - so put the data %d, size %d",(int)i->first,(int)parent->m_sqlv[i->first].ws.size()+1);
					for (dataptr=dataput;dataoff<datasiz;dataoff+=chunk)
					{
						CServerIo::trace(1,"MSSQL put data %d offset %d bytes N\"%0.25s...%0.25s\"",dataoff,
												(dataoff+chunk>datasiz)?datasiz-dataoff:chunk,
												(const char *)cvs::narrow((const wchar_t *)(dataptr+dataoff)),
												((const char *)cvs::narrow((const wchar_t *)(dataptr+dataoff))+(((dataoff+chunk>datasiz)?datasiz-dataoff:chunk)+1-25)));
						putret=SQLPutData(m_hStmt, (SQLPOINTER)(dataptr+(sizeof(wchar_t)*dataoff)), (dataoff+chunk>datasiz)?(sizeof(wchar_t)*(datasiz-dataoff)):(sizeof(wchar_t)*chunk));
						if(!SQL_SUCCEEDED(putret))
	{
							m_parent->m_lasterror = putret;
							GetStmtError();
							return false;
						}
						/*switch (putret)
						{
						case SQL_SUCCESS:
							CServerIo::trace(1,"SQL Put Data returned SQL_SUCCESS");
							break;
						case SQL_SUCCESS_WITH_INFO:
							CServerIo::trace(1,"SQL Put Data returned SQL_SUCCESS_WITH_INFO");
							break;
						case SQL_STILL_EXECUTING:
							CServerIo::trace(1,"SQL Put Data returned SQL_STILL_EXECUTING");
							break;
						case SQL_ERROR:
							CServerIo::trace(1,"SQL Put Data returned SQL_ERROR");
							break;
						case SQL_INVALID_HANDLE:
							CServerIo::trace(1,"SQL Put Data returned SQL_INVALID_HANDLE");
							break;
						default:
							CServerIo::trace(1,"SQL Put Data returned some other error.");
						}*/
					}
					CServerIo::trace(1,"MSSQL call ParamData again");
					retcode = SQLParamData(m_hStmt, &pParmID);
					if (retcode==SQL_SUCCESS)
						CServerIo::trace(1,"MSSQL call ParamData returned OK - no more data");
					else if (retcode==SQL_NEED_DATA)
						CServerIo::trace(1,"MSSQL call ParamData returned need more data");
				}
				break;
			default:
				break;
			}
		}
		} else {
			retcode=SQL_SUCCESS;
		}
		if (retcode==SQL_SUCCESS)
			CServerIo::trace(1,"MSSQL call ParamData returned OK - no more data");
		else if (retcode==SQL_NEED_DATA)
			CServerIo::trace(1,"MSSQL call ParamData returned need more data");
		else
		if(!SQL_SUCCEEDED(retcode))
		{
			CServerIo::trace(1,"MSSQL call ParamData returned some sort of failure so returning...");
			m_parent->m_lasterror = retcode;
			GetStmtError();
			return false;
		}
	}

	CServerIo::trace(1,"MSSQL Execute all complete now get the Number of Result Columns");
	if(!SQL_SUCCEEDED(m_parent->m_lasterror = SQLNumResultCols(m_hStmt,&m_num_fields)))
	{
		GetStmtError();
		return false;
	}

	m_sqlfields.resize(m_num_fields);
	for(SQLSMALLINT n=0; n<m_num_fields; n++)
	{
		SQLRETURN rc;

		SQLWCHAR szCol[128];
		SQLSMALLINT len = sizeof(szCol);
		rc = m_parent->m_lasterror = SQLDescribeCol(hStmt,n+1,szCol,sizeof(szCol),&len,&m_sqlfields[n].type,&m_sqlfields[n].size,&m_sqlfields[n].decimal,&m_sqlfields[n].null);
		if(!SQL_SUCCEEDED(rc))
		{
			GetStmtError();
			return false;
		}
		szCol[len]='\0';
		m_sqlfields[n].field = n;
		m_sqlfields[n].hStmt = m_hStmt;
		m_sqlfields[n].name = szCol;

		SQLINTEGER fldlen = 0;
		SQLSMALLINT ctype;
		switch(m_sqlfields[n].type)
		{
		case SQL_UNKNOWN_TYPE:
			CServerIo::trace(1,"Unable to bind column %s as it is SQL_UNKNOWN_TYPE",(const char *)szCol);
			break; // Don't bind
		case SQL_CHAR:
		case SQL_VARCHAR:
			ctype = SQL_C_WCHAR;
			fldlen = m_sqlfields[n].size;
			break;
		case SQL_DECIMAL:
			ctype = SQL_C_WCHAR;
			fldlen = m_sqlfields[n].size + m_sqlfields[n].decimal + 1;
			break;
		case SQL_NUMERIC:
		case SQL_INTEGER:
		case SQL_SMALLINT:
			ctype = SQL_C_LONG;
			fldlen = sizeof(long);
			break;
		case SQL_FLOAT:
		case SQL_REAL:
		case SQL_DOUBLE:
			ctype = SQL_C_DOUBLE;
			fldlen = sizeof(double);
			break;
		case SQL_DATETIME:
			ctype = SQL_C_WCHAR;
			fldlen = 64;
			break;
		}
		m_sqlfields[n].ctype = ctype;
		m_sqlfields[n].fldlen = fldlen;
		if(m_sqlfields[n].fldlen)
		{
			m_sqlfields[n].data = malloc(m_sqlfields[n].fldlen);
			if(!SQL_SUCCEEDED(m_parent->m_lasterror = SQLBindCol(m_hStmt,n+1,m_sqlfields[n].ctype,m_sqlfields[n].data,m_sqlfields[n].fldlen,&m_sqlfields[n].datalen)))
			{
				GetStmtError();
				CServerIo::trace(1,"Unable to bind column %s due to error",(const char*)szCol);
				return false;
			}
		}
	}

	if(m_num_fields)
	{
		if(!Next() && !m_bEof)
			return false;
	}

	return true;
}

bool CMssqlRecordset::Close()
{
	if(m_hStmt)
		SQLFreeStmt(m_hStmt,SQL_DROP);
	m_hStmt = NULL;
	m_bEof = true;
	return true;
}

bool CMssqlRecordset::Closed() const
{
	if(!m_hStmt)
		return false;
	return true;
}

bool CMssqlRecordset::Eof() const
{
	return m_bEof;
}

bool CMssqlRecordset::Next()
{
	SQLRETURN res;

	if(m_bEof)
		return false;
	m_parent->m_lasterror = res = SQLFetch(m_hStmt);
	if(res == SQL_NO_DATA_FOUND)
	{
		m_bEof = true;
		return false;
	}

	if(!SQL_SUCCEEDED(res))
	{
		GetStmtError();
		return false;
	}

	return true;
}

CSqlField* CMssqlRecordset::operator[](size_t item) const
{
	if(item>=(size_t)m_num_fields)
		return NULL;
	return (CSqlField*)&m_sqlfields[item];
}

CSqlField* CMssqlRecordset::operator[](int item) const
{
	if(item<0 || item>=m_num_fields)
		return NULL;
	return (CSqlField*)&m_sqlfields[item];
}

CSqlField* CMssqlRecordset::operator[](const char *item) const
{
	cvs::wide _item(item);
	for(size_t n=0; n<(size_t)m_num_fields; n++)
		if(!wcsicmp(m_sqlfields[n].name.c_str(),_item))
			return (CSqlField*)&m_sqlfields[n];
	CServerIo::error("Database error - field '%s' not found in recordset.",item);
	return NULL;
}

void CMssqlRecordset::GetStmtError()
{
	SQLWCHAR state[6];
	SQLINTEGER error;
	SQLSMALLINT size = 512,len;
	m_parent->m_lastrsError.resize(size);
	SQLWCHAR *pmsg = (SQLWCHAR*)m_parent->m_lastrsError.data();

	if(m_hStmt)
	{
		for(int i=1; SQL_SUCCEEDED(SQLGetDiagRec(SQL_HANDLE_STMT, m_hStmt, i, state, &error, pmsg, size, &len)); i++)
		{
			size-=len;
			pmsg+=len;
		}
	}
	m_parent->m_lastrsError.resize(512-size);
}
