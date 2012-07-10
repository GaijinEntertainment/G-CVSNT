/*  Call installer.dll 
    Copyright (C) 2010 March Hare Software Ltd

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
//#define _CRT_NON_CONFORMING_SWPRINTFS
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <stdlib.h>
#include <softpub.h>
#include <wintrust.h>
#include <msiquery.h>
#include <shlwapi.h>
#include "commctrl.h"

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

static UINT InstallerStartup(MSIHANDLE hmsi, int argc, const char **argv)
{
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
	
	if (_tcslen(InstallPath)>0)
	{
		_tcscpy(Env,InstallPath);
		_tcscat(Env,_T(";"));
	}
	if (_tcslen(SearchPath)>0)
	{
		_tcscat(Env,SearchPath);
		_tcscat(Env,_T(";"));
	}

	GetEnvironmentVariable(_T("Path"),Env+_tcslen(Env),sizeof(Env)-_tcslen(Env));
	SetEnvironmentVariable(_T("Path"),Env);


	GetModuleFileName(NULL,InstallPath,sizeof(InstallPath));

	_tcscpy(InstallPath+_tcslen(InstallPath)-18,_T("installer.dll"));

	HMODULE hModule = LoadLibraryEx(InstallPath,NULL,LOAD_WITH_ALTERED_SEARCH_PATH);

	if(!hModule)
	{
		printf("Unable to load %S\n",InstallPath);
		return -1;
	}
	simVerifyTrust(InstallPath,true);

	UINT (*pmain)(MSIHANDLE hInstall);

	pmain = (UINT (*)(MSIHANDLE ))GetProcAddress(hModule,argv[2]);

	if(!pmain)
	{
		printf("%S does not export %s()\n",InstallPath,argv[2]);
		return -1;
	}

	UINT msires = pmain(hmsi);
	if (msires==ERROR_INSTALL_FAILURE)
	{
		fprintf(stderr,"%s() returns ERROR_INSTALL_FAILURE\n",argv[2]);
		return -1;
	}
	else
	if (msires==ERROR_SUCCESS)
	{
		printf("%s() returns ERROR_SUCCESS\n",argv[2]);
		return -1;
	}
	return 0;
}

int wmain(int argc, const wchar_t *argv[])
{
	INITCOMMONCONTROLSEX icex;
	char **newargv = (char**)malloc(sizeof(char*)*(argc+1)),*buf, *p;
	int n, ret, len;

	if (argc!=3)
	{
		fprintf(stderr,"installer-test msi-database  CustomActionCheck | CustomActionEmail | CustomActionSerial | CustomWelcome | CustomActionInstall | CustomActionUninstall\n");
		return -1;
	}

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

	CoInitialize(NULL);
	icex.dwSize=sizeof(icex);
	icex.dwICC=ICC_STANDARD_CLASSES|ICC_LINK_CLASS;
	InitCommonControlsEx(&icex);

	MSIHANDLE hmsidb = NULL;
	if (MsiOpenDatabase(argv[1],MSIDBOPEN_READONLY,&hmsidb)!=ERROR_SUCCESS)
	{
		fprintf(stderr,"Failed to open %s\n",newargv[1]);
		return -1;
	}

	TCHAR szhmsidb[256];
	_sntprintf(szhmsidb,sizeof(szhmsidb)/sizeof(szhmsidb[0]),_T("#%d"),(int)hmsidb);
		
	MSIHANDLE hmsiprd = NULL;
	if (MsiOpenPackage(szhmsidb,&hmsiprd)!=ERROR_SUCCESS)
	{
		fprintf(stderr,"Failed to open db handle #%d\n",(int)hmsidb);
		return -1;
	}

	ret = InstallerStartup(hmsiprd, argc,(const char **)newargv);

	MsiCloseHandle(hmsiprd);
	MsiCloseHandle(hmsidb);
	free(buf);
	free(newargv);
	return ret;
}

