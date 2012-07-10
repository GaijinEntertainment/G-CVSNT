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
/* Win32 specific */
#include <config.h>
#include "../lib/api_system.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#define SECURITY_WIN32
#include <security.h>
#include <lm.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "../lib/api_system.h"
#include "../cvs_string.h"
#include "../SSPIHandler.h"

CSSPIHandler::CSSPIHandler()
{
	m_secPackInfo = NULL;
}

CSSPIHandler::~CSSPIHandler()
{
}

bool CSSPIHandler::init(const char *protocol /* = NULL */)
{
	HKEY hk;
	
	if(!protocol) 
	{
		if(!InitProtocol("Negotiate") && !InitProtocol("NTLM"))
			return false;
	}
	else
	{
		if(!InitProtocol(protocol))
			return false;
	}

	/* Check for BFS mode */
	m_broken_file_sharing = false;
	if(!RegOpenKeyEx(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Control\\Lsa",0,KEY_QUERY_VALUE,&hk))
	{
		DWORD dwType,dwVal,dwLen=sizeof(DWORD);
		if(!RegQueryValueEx(hk,L"ForceGuest",NULL,&dwType,(LPBYTE)&dwVal,&dwLen))
		{
			if(dwVal)
				m_broken_file_sharing = true;
		}
		CloseHandle(hk);
	}
	return true;
}

bool CSSPIHandler::InitProtocol(const char *protocol)
{
	if(QuerySecurityPackageInfoA( (char*)protocol, &m_secPackInfo ) != SEC_E_OK)
		return false;
	if(!m_secPackInfo)
		return false;

	m_currentProtocol = protocol;
	return true;
}

bool CSSPIHandler::ClientStart(bool encrypt, bool& more, const char *name, const char *pwd, const char *tokenSource /* = NULL */)
{
	char username[256];
	char domain[256];
	SECURITY_STATUS rc;
	TimeStamp useBefore;

	memset(&m_nameAndPwd,0,sizeof(m_nameAndPwd));

	if (name && *name)
	{
		strncpy(username,name,sizeof(username));
		char *p = strpbrk(username,"\\/");
		if(!p)
			domain[0]='\0';
		else
		{
			*p='\0';
			strcpy(domain,username);
			strcpy(username,p+1);
		}

		
		m_nameAndPwd.Domain = (BYTE*)domain;
		m_nameAndPwd.DomainLength = (DWORD)strlen(domain);
		m_nameAndPwd.User = (BYTE*)username;
		m_nameAndPwd.UserLength = (DWORD)strlen(username);
		m_nameAndPwd.Password = (BYTE*)(pwd?pwd:"");
		m_nameAndPwd.PasswordLength = (DWORD)(pwd?strlen(pwd):0); 
		m_nameAndPwd.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;
	}
	else if(m_broken_file_sharing)
	{
		m_nameAndPwd.UserLength = sizeof(username);
		GetUserNameA(username,&m_nameAndPwd.UserLength);
		m_nameAndPwd.User= (BYTE*)username;
		m_nameAndPwd.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;
	}

	rc = AcquireCredentialsHandleA( NULL, (char*)m_currentProtocol.c_str(), SECPKG_CRED_OUTBOUND, NULL, m_nameAndPwd.UserLength?&m_nameAndPwd:NULL, NULL, NULL, &m_credHandle, &useBefore );
	if(rc)
	{
		m_rc = rc;
		return false;
	}

	m_haveContext = false;
	m_tokenSource = tokenSource;
	m_ctxReq = ISC_REQ_MUTUAL_AUTH | ISC_REQ_DELEGATE | ISC_REQ_STREAM | ISC_REQ_EXTENDED_ERROR;
	if(encrypt)
		m_ctxReq |= ISC_REQ_REPLAY_DETECT  | ISC_REQ_SEQUENCE_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_REQ_INTEGRITY;

	return ClientStep(more,NULL,0);
}

bool CSSPIHandler::ClientStep(bool& more, const char *inputBuffer, size_t inputSize)
{
	SECURITY_STATUS rcISC;
	DWORD ctxAttr;
	// input and output buffers
	TimeStamp useBefore;
	SecBufferDesc obd,ibd;
	SecBuffer ob[1], ib[2];

	m_outputBuffer.resize(m_secPackInfo->cbMaxToken);

	obd.ulVersion = SECBUFFER_VERSION;
	obd.cBuffers = 1;
	obd.pBuffers = ob; // just one buffer
	ob[0].cbBuffer = m_secPackInfo->cbMaxToken;
	ob[0].pvBuffer = (void*)m_outputBuffer.data();
	ob[0].BufferType = SECBUFFER_TOKEN; // preping a token here

	if(inputBuffer)
	{
		// prepare to get the server's response
		ibd.ulVersion = SECBUFFER_VERSION;
		ibd.cBuffers = 1;
		ibd.pBuffers = ib; // just one buffer

		ib[0].cbBuffer=(DWORD)inputSize;
		ib[0].pvBuffer =(void*)inputBuffer;
		ib[0].BufferType = SECBUFFER_TOKEN; // preping a token here
	}

	rcISC = InitializeSecurityContextA( &m_credHandle, m_haveContext? &m_contextHandle:NULL,
		(char*)m_tokenSource, m_ctxReq, 0, SECURITY_NATIVE_DREP, m_haveContext?&ibd:NULL,
		0, &m_contextHandle, &obd, &ctxAttr, &useBefore );

	if ( rcISC == SEC_I_COMPLETE_AND_CONTINUE || rcISC == SEC_I_COMPLETE_NEEDED )
	{
		CompleteAuthToken( &m_contextHandle, &obd );
		if ( rcISC == SEC_I_COMPLETE_NEEDED )
			rcISC = SEC_E_OK;
		else if ( rcISC == SEC_I_COMPLETE_AND_CONTINUE )
			rcISC = SEC_I_CONTINUE_NEEDED;
	}

	if(rcISC<0)
	{
		m_rc = rcISC;
		more=false;
		return false;
	}

	m_outputBuffer.resize(ob[0].cbBuffer);

	if ( rcISC != SEC_I_CONTINUE_NEEDED )
	{
		m_rc = rcISC;
		if ( rcISC != SEC_E_OK )
			m_haveContext = false;
		else
		{
			QueryContextAttributes(&m_contextHandle,SECPKG_ATTR_SIZES,&m_secSizes);
			m_haveContext = true;
		}

		more=false;
		return m_haveContext;
	}

	m_haveContext = true;
	more=true;
	return true;
}
