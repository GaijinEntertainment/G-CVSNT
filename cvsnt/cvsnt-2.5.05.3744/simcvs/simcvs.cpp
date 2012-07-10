/*  Call cvsnt at its installed location
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd

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
#define WIN32_LEAN_AND_MEAN
#define STRICT
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <stdlib.h>
#include <softpub.h>
#include <wintrust.h>

void simVerifyTrust(const TCHAR *module, bool must_exist)
{
	TCHAR szModule[MAX_PATH];
	// For some reason VS2005 compiles this wrong, so you get a call to cvs::wide(NULL)
	// HMODULE hModule = GetModuleHandle(module?cvs::wide(module):NULL);
	HMODULE hModule;
	if(module)
		hModule = GetModuleHandle(module);
	else
		hModule = GetModuleHandle(NULL);

	if(!hModule && module)
	{
		if(!SearchPath(NULL,module,_T(".dll"),sizeof(szModule)/sizeof(szModule[0]),szModule,NULL))
		{
			if(must_exist)
			{
				printf("Unable to find %S  - aborting\n",module);
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
			printf("Trust verification failed for '%S' - image not signed\n",szModule);
#else
			printf("Trust verification failed for '%S' - community image file image not signed\n",szModule);
			return;
#endif
		}
		else if(dwTrustErr==TRUST_E_SUBJECT_NOT_TRUSTED)
			printf("Trust verification failed for '%S' - invalid signature\n",szModule);
		else
			printf("Trust verification failed for '%S' - error %08x\n",szModule,GetLastError());
		exit(-1);
		abort();
	}
}

static int SimCvsStartup(int argc, const char **argv)
{
	BOOL bIsCVSNT;
	TCHAR CVSNTInstallPath[1024];
	HKEY hKeyLocal,hKeyGlobal;
	TCHAR InstallPath[1024],SearchPath[2048],Env[10240];
	DWORD dwLen,dwType;

	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,L"Software\\CVS\\PServer",0,KEY_QUERY_VALUE,&hKeyGlobal))
		hKeyGlobal = NULL;

	if(RegOpenKeyEx(HKEY_CURRENT_USER,L"Software\\Cvsnt\\PServer",0,KEY_QUERY_VALUE,&hKeyLocal))
		hKeyLocal = NULL;

	dwLen=sizeof(InstallPath);
	if(RegQueryValueEx(hKeyGlobal,L"InstallPath",NULL,&dwType,(LPBYTE)InstallPath,&dwLen) &&
		RegQueryValueEx(hKeyLocal,L"InstallPath",NULL,&dwType,(LPBYTE)InstallPath,&dwLen))
	{
		InstallPath[0]='\0';
	}

	dwLen=sizeof(SearchPath);
	if(RegQueryValueEx(hKeyGlobal,L"SearchPath",NULL,&dwType,(LPBYTE)SearchPath,&dwLen) &&
		RegQueryValueEx(hKeyLocal,L"SearchPath",NULL,&dwType,(LPBYTE)SearchPath,&dwLen))
	{
		SearchPath[0]='\0';
	}
	RegCloseKey(hKeyGlobal);
	RegCloseKey(hKeyLocal);
	
	_tcscpy(Env,InstallPath);
	_tcscat(Env,_T(";"));
	_tcscat(Env,SearchPath);
	_tcscat(Env,_T(";"));

	GetEnvironmentVariable(_T("Path"),Env+_tcslen(Env),sizeof(Env));
	SetEnvironmentVariable(_T("Path"),Env);

	//_tcscpy(InstallPath,_T("cvsnt.exe"));
	GetModuleFileName(NULL,InstallPath,sizeof(InstallPath));
	//printf("load %S\n",InstallPath);
	if(!_tcsicmp(InstallPath+_tcslen(InstallPath)-9,_T("cvsnt.exe")))
	{
		bIsCVSNT = TRUE;
		_tcscpy(CVSNTInstallPath,InstallPath);
		_tcscpy(CVSNTInstallPath+_tcslen(InstallPath)-4,_T(".dll"));
		_tcscpy(InstallPath+_tcslen(InstallPath)-9,_T("cvs.dll"));
	}
	else
	if(!_tcsicmp(InstallPath+_tcslen(InstallPath)-4,_T(".exe")))
		_tcscpy(InstallPath+_tcslen(InstallPath)-4,_T(".dll"));
	else
	if(_tcsicmp(InstallPath+_tcslen(InstallPath)-4,_T(".dll")))
		_tcscat(InstallPath,_T(".dll"));

	HMODULE hModule = LoadLibraryEx(InstallPath,NULL,LOAD_WITH_ALTERED_SEARCH_PATH);

	if(!hModule)
	{
		if (bIsCVSNT)
		{
			hModule = LoadLibraryEx(CVSNTInstallPath,NULL,LOAD_WITH_ALTERED_SEARCH_PATH);
			if(!hModule)
			{
				printf("Unable to load %S\n",InstallPath);
				printf("Unable to load %S\n",CVSNTInstallPath);
				return -1;
			}
			_tcscpy(InstallPath,CVSNTInstallPath);
		}
		else
		{
		printf("Unable to load %S\n",InstallPath);
		return -1;
		}
	}
	simVerifyTrust(InstallPath,true);

	int (*pmain)(int argc, const char **argv);

	pmain = (int (*)(int, const char **))GetProcAddress(hModule,"main");

	if(!pmain)
	{
		printf("%S does not export main()\n",InstallPath);
		return -1;
	}

	return pmain(argc,argv);
}

int wmain(int argc, const wchar_t *argv[])
{
	char **newargv = (char**)malloc(sizeof(char*)*(argc+1)),*buf, *p;
	int n, ret, len;

	len=0;
	for(n=0; n<argc; n++)
	{
		len+= WideCharToMultiByte(CP_UTF8,0,argv[n],-1,NULL,0,NULL,NULL);
	}
	buf=p=(char*)malloc(len);
	for(n=0; n<argc; n++)
	{
		newargv[n]=p;
		p+=WideCharToMultiByte(CP_UTF8,0,argv[n],-1,newargv[n],len,NULL,NULL);
	}
	newargv[n]=NULL;
	ret = SimCvsStartup(argc,(const char **)newargv);
	free(buf);
	free(newargv);
	return ret;
}

