/*
	CVSNT Generic API
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd

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
/* Win32 specific */
#include <config.h>
#include "../lib/api_system.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <softpub.h>
#include <wintrust.h>

#include "../cvs_string.h"
#include "../FileAccess.h"
#include "../ServerIO.h"
#include "../LibraryAccess.h"

CLibraryAccess::CLibraryAccess(const void *lib /*= NULL*/)
{
	m_lib = (void*)lib;
}

CLibraryAccess::~CLibraryAccess()
{
	Unload();
}

bool CLibraryAccess::Load(const char *name, const char *directory)
{
	if(m_lib)
		Unload();

	cvs::filename fn;
	if(directory)
		cvs::sprintf(fn,256,"%s/%s",directory,name);
	else
		fn = name;
	CServerIo::trace(3,"CLibraryAccess::Load loading %s",fn.c_str());

	VerifyTrust(fn.c_str(),false);

	UINT uMode = SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
	m_lib = (void*)LoadLibrary(CFileAccess::Win32Wide(fn.c_str()));
	SetErrorMode(uMode);

	if(!m_lib)
	{
		CServerIo::trace(3,"LibraryAccess::Load failed for '%s', Win32 error %08x",fn.c_str(),GetLastError());
		return false;
	}

	return true;
}

bool CLibraryAccess::Unload()
{
	if(!m_lib)
		return true;

	FreeLibrary((HMODULE)m_lib);
	m_lib = NULL;
	return true;
}

void *CLibraryAccess::GetProc(const char *name)
{
	if(!m_lib)
		return NULL;

	return (void*)GetProcAddress((HMODULE)m_lib,name);
}

void *CLibraryAccess::Detach()
{
	void *lib = m_lib;
	m_lib = NULL;
	return lib;
}

void CLibraryAccess::VerifyTrust(const char *module, bool must_exist)
{
	TCHAR szModule[MAX_PATH];
	// For some reason VS2005 compiles this wrong, so you get a call to cvs::wide(NULL)
	// HMODULE hModule = GetModuleHandle(module?cvs::wide(module):NULL);
	HMODULE hModule;
	if(module)
		hModule = GetModuleHandle(cvs::wide(module));
	else
		hModule = GetModuleHandle(NULL);

	if(!hModule && module)
	{
		if(!SearchPath(NULL,cvs::wide(module),_T(".dll"),sizeof(szModule)/sizeof(szModule[0]),szModule,NULL))
		{
			if(must_exist)
			{
				CServerIo::error("Unable to find %s - aborting",module);
				exit(-1);
				abort();
			}
			return;
		}
	}
	else
		GetModuleFileName(hModule,szModule,sizeof(szModule));

	WINTRUST_FILE_INFO FileData = {sizeof(WINTRUST_FILE_INFO)};
	WINTRUST_DATA WinTrustData = {sizeof(WINTRUST_DATA)};
    FileData.pcwszFilePath = szModule;

    GUID WVTPolicyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;

    WinTrustData.dwUIChoice = WTD_UI_NONE;
    WinTrustData.dwUnionChoice = WTD_CHOICE_FILE;
    WinTrustData.dwProvFlags = WTD_SAFER_FLAG | WTD_REVOCATION_CHECK_NONE | WTD_CACHE_ONLY_URL_RETRIEVAL;
    WinTrustData.pFile = &FileData;
	WinTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;

    DWORD dwTrustErr = WinVerifyTrust(NULL, &WVTPolicyGUID, &WinTrustData);
	if(dwTrustErr)
	{
		if(dwTrustErr==TRUST_E_NOSIGNATURE)
		{
#ifdef COMMERCIAL_RELEASE
			CServerIo::error("File '%s' Module '%s' trust verification failed - image not signed\n",(const char *)cvs::narrow(szModule),(module)?module:"*none*");
#else
			CServerIo::trace(1,"Community file '%s' Module '%s' trust verification failed - executable image not signed\n",(const char *)cvs::narrow(szModule),(module)?module:"*none*");
			return;
#endif
		}
		else if(dwTrustErr==TRUST_E_SUBJECT_NOT_TRUSTED)
			CServerIo::error("File '%s' Module '%s' trust verification failed - invalid signature\n",(const char *)cvs::narrow(szModule),(module)?module:"*none*");
		else
			CServerIo::error("File '%s' Module '%s' trust verification failed - error %08x\n",(const char *)cvs::narrow(szModule),(module)?module:"*none*",GetLastError());
		exit(-1);
		abort();
	}
}

