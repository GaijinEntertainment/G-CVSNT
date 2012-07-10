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
#define snprintf _snprintf
#define wcscasecmp wcsicmp
#endif

#if defined ( __APPLE__) 
#include <wctype.h>
#endif
#include <wchar.h>
#include <stdlib.h>

#if defined( __hpux ) || defined ( __APPLE__)
// temporary.. fix properly later
int wcscasecmp(const wchar_t*a, const wchar_t* b)
{
	int res;
	size_t pos;
	wchar_t * _ws1;
	wchar_t * _ws2;
	size_t _ws1len = wcslen(a);
	size_t _ws2len = wcslen(b);

	_ws1 = (wchar_t *) malloc(_ws1len * sizeof(wchar_t));
	_ws2 = (wchar_t *) malloc(_ws2len * sizeof(wchar_t));

	for (pos=0;pos<_ws1len;pos++)
		*_ws1=towupper(*(a+pos));

	for (pos=0;pos<_ws2len;pos++)
		*_ws2=towupper(*(b+pos));

	res = wcscmp(_ws1,_ws2);

	free(_ws1);
	free(_ws2);

	return res;
}
#endif

#include <config.h>
#include <cvsapi.h>

#include <string>

#include <oci.h>

#include "OracleConnection.h"
#include "OracleRecordset.h"

COracleField::COracleField()
{
	/*CServerIo::trace(1,"Instantiate COracleField");*/
	data = NULL;
}

COracleField::~COracleField()
{
	CServerIo::trace(1,"Destroy COracleField");
	if(data)
	{
		CServerIo::trace(1,"Destroy COracleField - free data");
		free(data);
	}
	CServerIo::trace(1,"Destroyed COracleField");
}

COracleField::operator int()
{
	CServerIo::trace(1,"Field type int");
	switch(fldtype)
	{
		case SQLT_INT:
			return (int)*(int*)data;
		case SQLT_FLT:
			return (int)*(double*)data;
		case SQLT_STR:
			{ int ret=0; swscanf((const wchar_t*)data,L"%d",&ret); return ret; }
		default:
			CServerIo::trace(1,"Field type %d unsupported for field %s",fldtype,name.c_str());
			return 0;
	}
}

COracleField::operator long()
{
	CServerIo::trace(1,"Field type long");
	switch(fldtype)
	{
		case SQLT_INT:
			return (long)*(int*)data;
		case SQLT_FLT:
			return (long)*(double*)data;
		case SQLT_STR:
			{ long ret=0; swscanf((const wchar_t*)data,L"%ld",&ret); return ret; }
		default:
			CServerIo::trace(1,"Field type %d unsupported for field %s",fldtype,name.c_str());
			return 0;
	}
}

COracleField::operator unsigned()
{
	CServerIo::trace(1,"Field type unsigned ");
	switch(fldtype)
	{
		case SQLT_INT:
			return (unsigned)*(int*)data;
		case SQLT_FLT:
			return (unsigned)*(double*)data;
		case SQLT_STR:
			{ unsigned ret=0; swscanf((const wchar_t*)data,L"%u",&ret); return ret; }
		default:
			CServerIo::trace(1,"Field type %d unsupported for field %s",fldtype,name.c_str());
			return 0;
	}
}

COracleField::operator unsigned long()
{
	CServerIo::trace(1,"Field type long ");
	switch(fldtype)
	{
		case SQLT_INT:
			return (unsigned long)*(int*)data;
		case SQLT_FLT:
			return (unsigned long)*(double*)data;
		case SQLT_STR:
			{ unsigned long ret=0; swscanf((const wchar_t*)data,L"%lu",&ret); return ret; }
		default:
			CServerIo::trace(1,"Field type %d unsupported for field %s",fldtype,name.c_str());
			return 0;
	}
}

#if defined(_WIN32) || defined(_WIN64)
COracleField::operator __int64()
{
	CServerIo::trace(1,"Field type __int64 ");
	switch(fldtype)
	{
		case SQLT_INT:
			return (__int64)*(int*)data;
		case SQLT_FLT:
			return (__int64)*(double*)data;
		case SQLT_STR:
			{ __int64 ret=0; swscanf((const wchar_t*)data,L"%I64d",&ret); return ret; }
		default:
			CServerIo::trace(1,"Field type %d unsupported for field %s",fldtype,name.c_str());
			return 0;
	}
}
#else
COracleField::operator long long()
{
	CServerIo::trace(1,"Field type long long ");
	switch(fldtype)
	{
		case SQLT_INT:
			return (long long)*(int*)data;
		case SQLT_FLT:
			return (long long)*(double*)data;
		case SQLT_STR:
			{ long long ret=0; swscanf((const wchar_t*)data,L"%lld",&ret); return ret; }
		default:
			CServerIo::trace(1,"Field type %d unsupported for field %s",fldtype,name.c_str());
			return 0;
	}
}
#endif

int ucs2toi(void *datain)
{
	int iresult;
	const unsigned char * instr=(const unsigned char *)datain;
	cvs::string xxxstr;
	xxxstr.resize(50);
	cvs::sprintf(xxxstr,50," %02u",(int)*(instr));
	iresult=atoi(xxxstr.c_str());
	return iresult;
}

int is_little_endian_int(void)
{
	short int aword = 0x001;
	char *abyte = (char *) &aword;
	return (abyte[0] ? 1 : 0);
}

int is_little_endian_ucs2(void)
{
	wchar_t aword[] = L"A\0";
	int iresult = ucs2toi((void *)&aword[0]);
	return (iresult ? 1 : 0);
}

void trace_out_content(const unsigned char * instr, size_t datalength)
{
	unsigned char outstr[500];
	unsigned char chrstr[500];
	unsigned char xxxstr[50];
	short k, l;

	if (instr==NULL)
		return;
	CServerIo::trace(3," trace_out_content %d \"%s\"",(int)datalength,instr);
	outstr[0]='\0';
	chrstr[0]='\0';
	l=0; k=0;
	// currently stops at the first NULL
	// can modify to use datalength...
	// while (*(instr+l)!='\0')
	while ((size_t)k+l< datalength)
	{
	  outstr[0]='\0';
	  chrstr[0]='\0';
	  for (k=0;k<10;k++)
	  {
		if ((size_t)k+l>=datalength)
			break;
		sprintf((char *)&xxxstr[0]," %02x",(int)*(instr+k+l));
		strcat((char *)&outstr[0],(char *)&xxxstr[0]);
		if ( (*(instr+k+l)!='\0') && (*(instr+k+l)!='\n') )
			sprintf((char *)&xxxstr[0],"%c",*(instr+k+l));
		else
			sprintf((char *)&xxxstr[0]," ");
		strcat((char *)&chrstr[0],(char *)&xxxstr[0]);
	  }
	  CServerIo::trace(3,"%04d %s :%s",(int)l,chrstr,outstr);
	  l+=k;
	  k=0;
	}
	if (k!=0)
	  CServerIo::trace(3," %s :%s",chrstr,outstr);
	CServerIo::trace(3," trace_out_content %d",(int)(k+l));
	return;
}

COracleField::operator const char *()
{
	CServerIo::trace(1,"Field type char * ");
	switch(fldtype)
	{
		case SQLT_INT:
			cvs::sprintf(tmpstr,32,"%d",*(int*)data);
			return tmpstr.c_str();
		case SQLT_FLT:
			cvs::sprintf(tmpstr,32,"%lf",*(double*)data);
			return tmpstr.c_str();
		case SQLT_STR:
			CServerIo::trace(1,"Return an ASCII char *, datalen=%d, size=%d",(int)datalen[0],(int)size);

			tmpwstr=L"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
			tmpwstr.resize(datalen[0]+300);
			memcpy((void *)tmpwstr.data(),(const wchar_t *)cvs::cvsucs2((const char *)data,datalen[0]),((datalen[0]+1)*sizeof(wchar_t)));
			//memcpy((void *)tmpwstr.data(),(const wchar_t *)cvs::cvsucs2((const char *)data,size),(datalen[0]*sizeof(wchar_t))+1);
			CServerIo::trace(3,"About to convert tmpwstr to tmpstr with narrow()");
			tmpstr= cvs::narrow(tmpwstr.c_str());
			return (const char *)tmpstr.c_str();
		default:
			CServerIo::trace(1,"Field type %d unsupported for field %s",fldtype,name.c_str());
			return 0;
	}
}

COracleField::operator const wchar_t *()
{
	CServerIo::trace(1,"Field type wchar_t * ");
	switch(fldtype)
	{
		case SQLT_INT:
			cvs::swprintf(tmpwstr,32,L"%d",*(int*)data);
			return tmpwstr.c_str();
		case SQLT_FLT:
			cvs::swprintf(tmpwstr,32,L"%lf",*(double*)data);
			return tmpwstr.c_str();
		case SQLT_STR:
			CServerIo::trace(1,"Return an WIDE wchar_t *, datalen=%d, size=%d",(int)datalen[0],(int)size);
			tmpwstr=L"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
			tmpwstr.resize(datalen[0]+300);
			memcpy((void *)tmpwstr.data(),(const wchar_t *)cvs::cvsucs2((const char *)data,datalen[0]),((datalen[0]+1)*sizeof(wchar_t)));
			//memcpy((void *)tmpwstr.data(),(const wchar_t *)cvs::cvsucs2((const char *)data,size),(datalen[0]*sizeof(wchar_t))+1);
			CServerIo::trace(3,"About to convert tmpwstr to tmpstr with narrow()");
			return (wchar_t*)tmpwstr.c_str();
		default:
			CServerIo::trace(1,"Field type %d unsupported for field %s",fldtype,name.c_str());
			return 0;
	}
}

/**********************************************************************/

COracleRecordset::COracleRecordset()
{
	/*CServerIo::trace(1,"Instantiate COracleRecordset");*/
	m_hStmt = NULL;
	m_bEof = true;
}

COracleRecordset::~COracleRecordset()
{
	/*CServerIo::trace(1,"Destroy COracleRecordset");*/
	Close();
	/*CServerIo::trace(1,"Destroyed COracleRecordset");*/
}

bool COracleRecordset::Init(COracleConnection *parent, OCIStmt *hStmt, bool select)
{
	sb4 status;
	int is_string_data=0;
	m_bEof = false;
	m_parent = parent;
	m_hStmt = hStmt;

	if (select)
	{
	CServerIo::trace(3,"COracleRecordset::Init -  OCIStmtExecute(Describe_Only,select=%d)",select?0:1);
	status=OCIStmtExecute(parent->m_hSvcCtx, hStmt, parent->m_hErr, select?0:1, 0, NULL, NULL, OCI_DESCRIBE_ONLY);
	} else {
	CServerIo::trace(3,"COracleRecordset::Init -  OCIStmtExecute(%s,select=%d)",parent->m_bAutoCommit?"OCI_COMMIT_ON_SUCCESS":"OCI_DEFAULT",select?0:1);
	status=OCIStmtExecute(parent->m_hSvcCtx, hStmt, parent->m_hErr, select?0:1, 0, NULL, NULL, parent->m_bAutoCommit?OCI_COMMIT_ON_SUCCESS:OCI_DEFAULT);
	}
	CServerIo::trace(3,"COracleRecordset::Init -  CheckError(%d)",(int)status);
	if(!parent->CheckError(parent->m_hErr,status))
		return false;

	CServerIo::trace(3,"COracleRecordset::Init -  OCIAttrGet(OCI_ATTR_PARAM_COUNT)");
	status=OCIAttrGet(hStmt, OCI_HTYPE_STMT, &m_num_fields, 0, OCI_ATTR_PARAM_COUNT, parent->m_hErr);
	CServerIo::trace(3,"COracleRecordset::Init -  CheckError(%d) m_num_fields=%d",(int)status,(int)m_num_fields);
	if(!parent->CheckError(parent->m_hErr,status))
		return false;

	CServerIo::trace(3,"COracleRecordset::Init -  resize");
	m_sqlfields.resize(m_num_fields);
	CServerIo::trace(3,"COracleRecordset::Init -  for loop");
	for(size_t n=0; n<m_num_fields; n++)
	{
		OCIParam *hParam;
		wchar_t szCol[128];
		ub4 ubCol = sizeof(szCol);
		unsigned char *szaColPtr;
		ub4 ubaCol = 0;

		CServerIo::trace(3,"COracleRecordset::Init -  for loop %d",(int)n);
		CServerIo::trace(3,"COracleRecordset::Init -  OCIParamGet(OCI_HTYPE_STMT) n==%d",n+1);
		OCIParamGet(hStmt, OCI_HTYPE_STMT, parent->m_hErr, (void**)&hParam, n+1);
		CServerIo::trace(3,"COracleRecordset::Init -  OCIAttrGet(OCI_ATTR_DATA_TYPE)");
		OCIAttrGet(hParam, OCI_DTYPE_PARAM, &m_sqlfields[n].type, NULL, OCI_ATTR_DATA_TYPE, parent->m_hErr);
		CServerIo::trace(3,"COracleRecordset::Init -  OCIAttrGet(OCI_ATTR_TYPECODE)");
		OCIAttrGet(hParam, OCI_DTYPE_PARAM, &m_sqlfields[n].typecode, NULL, OCI_ATTR_TYPECODE, parent->m_hErr);
		switch (m_sqlfields[n].type) {
		case OCI_TYPECODE_REF:
			CServerIo::trace(3,"  - TYPECODE REF");
			break;
		case OCI_TYPECODE_DATE:
			CServerIo::trace(3,"  - TYPECODE DATE");
			break;
		case OCI_TYPECODE_REAL:
			CServerIo::trace(3,"  - TYPECODE REAL");
			break;
		case OCI_TYPECODE_DOUBLE:
			CServerIo::trace(3,"  - TYPECODE DOUBLE");
			break;
		case OCI_TYPECODE_FLOAT:
			CServerIo::trace(3,"  - TYPECODE FLOAT");
			break;
		case OCI_TYPECODE_NUMBER:
			CServerIo::trace(3,"  - TYPECODE NUMBER");
			break;
		case OCI_TYPECODE_DECIMAL:
			CServerIo::trace(3,"  - TYPECODE DECIMAL");
			break;
		case OCI_TYPECODE_OCTET:
			CServerIo::trace(3,"  - TYPECODE OCTET");
			break;
		case OCI_TYPECODE_INTEGER:
			CServerIo::trace(3,"  - TYPECODE INTEGER");
			break;
		case OCI_TYPECODE_SMALLINT:
			CServerIo::trace(3,"  - TYPECODE SMALLINT");
			break;
		case OCI_TYPECODE_RAW:
			CServerIo::trace(3,"  - TYPECODE RAW");
			break;
		case OCI_TYPECODE_VARCHAR2:
			CServerIo::trace(3,"  - TYPECODE VARCHAR2");
			break;
		case OCI_TYPECODE_VARCHAR:
			CServerIo::trace(3,"  - TYPECODE VARCHAR");
			break;
		case OCI_TYPECODE_CHAR:
			CServerIo::trace(3,"  - TYPECODE CHAR");
			break;
		case OCI_TYPECODE_VARRAY:
			CServerIo::trace(3,"  - TYPECODE VARRAY");
			break;
		case OCI_TYPECODE_TABLE:
			CServerIo::trace(3,"  - TYPECODE TABLE");
			break;
		case OCI_TYPECODE_CLOB:
			CServerIo::trace(3,"  - TYPECODE CLOB");
			break;
		case OCI_TYPECODE_BLOB:
			CServerIo::trace(3,"  - TYPECODE BLOB");
			break;
		case OCI_TYPECODE_BFILE:
			CServerIo::trace(3,"  - TYPECODE BFILE");
			break;
		case OCI_TYPECODE_OBJECT:
			CServerIo::trace(3,"  - TYPECODE OBJECT");
			break;
		case OCI_TYPECODE_NAMEDCOLLECTION:
			CServerIo::trace(3,"  - TYPECODE NAMED COLL.");
			break;
		default:
			CServerIo::trace(3,"  - TYPECODE unknown");
		}
		CServerIo::trace(3,"COracleRecordset::Init -  OCIAttrGet(OCI_ATTR_NAME)");
		szaColPtr=NULL;
		OCIAttrGet(hParam, OCI_DTYPE_PARAM, (void**)&szaColPtr, &ubaCol, OCI_ATTR_NAME, parent->m_hErr);
		if (szaColPtr!=NULL)
		CServerIo::trace(3,"  - ATTR_NAME %s (%d).",szaColPtr,(int)ubaCol);
		else
		CServerIo::trace(3,"  - ATTR_NAME NULL (%d).",(int)ubaCol);
		CServerIo::trace(3,"COracleRecordset::Init -  OCIAttrGet(OCI_ATTR_PRECISION)");
		OCIAttrGet(hParam, OCI_DTYPE_PARAM, &m_sqlfields[n].precision, NULL, OCI_ATTR_PRECISION, parent->m_hErr);
		CServerIo::trace(3,"COracleRecordset::Init -  OCIAttrGet(OCI_ATTR_SCALE)");
		OCIAttrGet(hParam, OCI_DTYPE_PARAM, &m_sqlfields[n].scale, NULL, OCI_ATTR_SCALE, parent->m_hErr);
		CServerIo::trace(3,"COracleRecordset::Init -  OCIAttrGet(OCI_ATTR_DATA_SIZE)");
		OCIAttrGet(hParam, OCI_DTYPE_PARAM, &m_sqlfields[n].size, NULL, OCI_ATTR_DATA_SIZE, parent->m_hErr);
		CServerIo::trace(3,"  - ATTR_DATA_SIZE %d.",(int)m_sqlfields[n].size);

		m_sqlfields[n].field = n;
		m_sqlfields[n].hStmt = m_hStmt;
		m_sqlfields[n].name = cvs::wide((char *)szaColPtr);

		switch(m_sqlfields[n].type)
		{
		case OCI_TYPECODE_REAL:
		case OCI_TYPECODE_FLOAT:
		case OCI_TYPECODE_DOUBLE:
			CServerIo::trace(3," ::Init -  OCI_TYPECODE_REAL");
			m_sqlfields[n].fldtype=SQLT_FLT;
			m_sqlfields[n].fldlen=sizeof(double);
			break;
		case OCI_TYPECODE_OCTET:
		case OCI_TYPECODE_INTEGER:
		case OCI_TYPECODE_SMALLINT:
			CServerIo::trace(3," ::Init -  OCI_TYPECODE_OCTET");
			m_sqlfields[n].fldtype=SQLT_INT;
			m_sqlfields[n].fldlen=sizeof(int);
			break;
		case OCI_TYPECODE_NUMBER:
			CServerIo::trace(3," ::Init -  OCI_TYPECODE_NUMBER");
			if(m_sqlfields[n].precision)
			{
				m_sqlfields[n].fldtype=SQLT_FLT;
				m_sqlfields[n].fldlen=sizeof(double);
			}
			else
			{
				m_sqlfields[n].fldtype=SQLT_INT;
				m_sqlfields[n].fldlen=sizeof(int);
			}
			break;
		case OCI_TYPECODE_VARCHAR2:
		case OCI_TYPECODE_VARCHAR:
		case OCI_TYPECODE_CHAR:
		case OCI_TYPECODE_CLOB:
		case OCI_TYPECODE_BLOB:
		case OCI_TYPECODE_DATE:
			CServerIo::trace(3," ::Init -  OCI_TYPECODE_VARCHAR2");
			m_sqlfields[n].fldtype=SQLT_STR;
			m_sqlfields[n].fldlen=m_sqlfields[n].size+1;
			is_string_data=1;
			break;
		case OCI_TYPECODE_REF:
		case OCI_TYPECODE_RAW:
		case OCI_TYPECODE_VARRAY:
		case OCI_TYPECODE_TABLE:
		case OCI_TYPECODE_BFILE:
		case OCI_TYPECODE_OBJECT:
		case OCI_TYPECODE_NAMEDCOLLECTION:
		default:
			CServerIo::trace(3," ::Init -  Unsuppoered datatype");
			//CServerIo::error("Field type %d unsupported for field %s\n",m_sqlfields[n].type,(const char *)cvs::narrow(szCol));
			CServerIo::error("Field type %d unsupported for field %s\n",m_sqlfields[n].type,szaColPtr);
			return false;
		}

		CServerIo::trace(3," ::Init -  end of datatype");
		if(m_sqlfields[n].fldlen)
		{
			OCIDefine *hDef = NULL;
			CServerIo::trace(3," ::Init -  m_sqlfields[n].fldlen=%d",(int)m_sqlfields[n].fldlen);

			if ((m_sqlfields[n].fldtype==SQLT_STR) ||
			    (is_string_data))
			{
			// the result of the OCI DefineByPos is a buffer of 
			// characters.  We could leave just that but
			// this (incorrectly?) assumes the OCI client is
			// going to present this data as UTF8.  In truth
			// it is based on the bind OCI_HTYPE_BIND attribute 
			// OCI_ATTR_CHARSET_ID
			CServerIo::trace(3," ::Init -  OCIAttrGet(OCI_ATTR_CHARSET_ID)");
			OCIAttrGet(hParam, OCI_DTYPE_PARAM, &m_sqlfields[n].csid, NULL, OCI_ATTR_CHARSET_ID, parent->m_hErr);
			if (m_sqlfields[n].csid == OCI_UCS2ID)
				CServerIo::trace(3," ::Init -  UCS2 Charset");
			if (m_sqlfields[n].csid == 2000 /* OCI_UTF16ID */)
				CServerIo::trace(3," ::Init -  UTF16 Charset");
			else
				CServerIo::trace(3," ::Init -  Charset %d (not UCS2)",(int)m_sqlfields[n].csid);
			/* The only valid handle types for setting charset are:
			  Bind Handle 
			  Define Handle
			  Direct Path Loading Handle
			  Direct Path Column Parameter
			  */
			/*
			CServerIo::trace(3,"COracleRecordset::Init -  OCIParamGet(OCI_DTYPE_PARM)");
			OCIParamGet(hParam, OCI_DTYPE_PARM, parent->m_hErr, (void **)&hColDesc, 1);
			  
			CServerIo::trace(3," ::Init - Force Direct Path Loading Handle to UCS2 Charset");
			m_sqlfields[n].csid=OCI_UCS2ID;
			status=OCIAttrSet((void *)hColDesc, OCI_DTYPE_PARAM,(void *)&m_sqlfields[n].csid,0,OCI_ATTR_CHARSET_ID,parent->m_hErr);
			CServerIo::trace(3," ::Init - Force Direct Path Loading Handle to UCS2 Charset (%d)",(int)status);
			if(!parent->CheckError(parent->m_hErr,status)
				CServerIo::trace(1,"Unable to set UCS2 character set \"%s\".",parent->ErrorString());
			OCIDescriptorFree((dvoid *)hColDesc, OCI_DTYPE_PARAM));
			*/
			}
			
			m_sqlfields[n].data = malloc((m_sqlfields[n].fldlen+2)+(is_string_data)?m_sqlfields[n].fldlen+2:0);
			memset(m_sqlfields[n].data,0,m_sqlfields[n].fldlen+(is_string_data)?m_sqlfields[n].fldlen+2:0);
			CServerIo::trace(3," ::Init -  OCIDefineByPos()");
			m_sqlfields[n].datalen[0]=0 /*m_sqlfields[n].fldlen*/ ;
			status=OCIDefineByPos(m_hStmt,&hDef,parent->m_hErr,n+1,(void **)m_sqlfields[n].data,m_sqlfields[n].fldlen,m_sqlfields[n].fldtype,0,(unsigned short *)&m_sqlfields[n].datalen,0,OCI_DEFAULT);
			CServerIo::trace(3," ::Init -  OCIDefineByPos()=%d,%d",status,(int)m_sqlfields[n].datalen[0]);
			if(!parent->CheckError(parent->m_hErr,status))
			{
				CServerIo::trace(1,"Unable to bind column %s due to error \"%s\".",(const char*)szaColPtr,parent->ErrorString());
				return false;
			}
			if (is_string_data)
			{
			  if (m_sqlfields[n].csid<OCI_UCS2ID)
			  {
			    m_sqlfields[n].csid=OCI_UCS2ID;
			    CServerIo::trace(3," ::Init - Define Handle to UCS2 Charset");
			    status=OCIAttrSet((void *)hDef, OCI_HTYPE_DEFINE,(void *)&m_sqlfields[n].csid,0,OCI_ATTR_CHARSET_ID,parent->m_hErr);
			    CServerIo::trace(3," ::Init - Define Handle to UCS2 Charset (%d)",(int)status);
			    if(!parent->CheckError(parent->m_hErr,status))
				CServerIo::trace(1,"Unable to set UCS2 character set \"%s\".",parent->ErrorString());
			  }
			}
			/*
			szaColPtr=NULL; ubaCol=0;
			OCIAttrGet(hParam,  OCI_HTYPE_DEFINE, (void**)&szaColPtr, &ubaCol, OCI_ATTR_NAME, parent->m_hErr);
			if (szaColPtr!=NULL)
			{
			CServerIo::trace(3,"  - ATTR_NAME (Define Handle) %s (%d).",szaColPtr,(int)ubaCol);
			m_sqlfields[n].name = cvs::wide((char *)szaColPtr);
			}
			else
			CServerIo::trace(3,"  - ATTR_NAME (Define Handle) NULL (%d).",(int)ubaCol);
			*/

			CServerIo::trace(3," ::Init -  complete");
		}
	}

	if (select)
	{
	CServerIo::trace(3,"COracleRecordset::Init -  OCIStmtExecute(%s,select=%d)",parent->m_bAutoCommit?"OCI_COMMIT_ON_SUCCESS":"OCI_DEFAULT",select?0:1);
	status=OCIStmtExecute(parent->m_hSvcCtx, hStmt, parent->m_hErr, select?0:1, 0, NULL, NULL, parent->m_bAutoCommit?OCI_COMMIT_ON_SUCCESS:OCI_DEFAULT);
	CServerIo::trace(3,"COracleRecordset::Init -  CheckError(%d)",(int)status);
	if(!parent->CheckError(parent->m_hErr,status))
		return false;
	}

	if(m_num_fields)
	{
		CServerIo::trace(3," ::Init -  m_num_fields=%d",(int)(m_num_fields));
		if(!Next() && !m_bEof)
			return false;
	}

	CServerIo::trace(3," ::Init -  return");
	return true;
}

bool COracleRecordset::Close()
{
	CServerIo::trace(1,"Close COracleRecordset");
	if(m_hStmt)
	{
		CServerIo::trace(1,"Free Statement Handle");
		OCIHandleFree(m_hStmt,OCI_HTYPE_STMT);
		CServerIo::trace(1,"Free'd Statement Handle");
	}
	m_hStmt = NULL;
	m_bEof = true;
	CServerIo::trace(1,"Closed COracleRecordset");
	return true;
}

bool COracleRecordset::Closed() const
{
	if(!m_hStmt)
		return false;
	return true;
}

bool COracleRecordset::Eof() const
{
	return m_bEof;
}

bool COracleRecordset::Next()
{
	b4 res;

	CServerIo::trace(1,"COracleRecordset Next()");
	if(m_bEof)
		return false;
	res = OCIStmtFetch(m_hStmt, m_parent->m_hErr, 1, OCI_FETCH_NEXT, OCI_DEFAULT);
	if(res == OCI_NO_DATA)
	{
		CServerIo::trace(1,"COracleRecordset Next() OCI_NO_DATA");
		m_bEof = true;
		return false;
	}

	if(!m_parent->CheckError(m_parent->m_hErr, res))
		return false;

	CServerIo::trace(1,"COracleRecordset Next(), done - there is more data");
	return true;
}

CSqlField* COracleRecordset::operator[](size_t item) const
{
	CServerIo::trace(1,"Return an array [size_t]");
	if(item>=(size_t)m_num_fields)
		return NULL;

	/*
	COracleField *rsf = new COracleField();
	rsf = m_sqlfields[item];
	return (CSqlField*)rsf;
	*/
	return (CSqlField*)&(m_sqlfields[item]);
}

CSqlField* COracleRecordset::operator[](int item) const
{
	CServerIo::trace(1,"Return an array [int]");
	if(item<0 || item>=(int)m_num_fields)
		return NULL;
	return (CSqlField*)&m_sqlfields[item];
}

CSqlField* COracleRecordset::operator[](const char *item) const
{
	cvs::wide _item(item);
	CServerIo::trace(1,"Return an array [char *]");
	for(size_t n=0; n<(size_t)m_num_fields; n++)
		if(!wcscasecmp(m_sqlfields[n].name.c_str(),_item))
			return (CSqlField*)&m_sqlfields[n];
	CServerIo::error("Database error - field '%s' not found in recordset.",item);
	return NULL;
}

