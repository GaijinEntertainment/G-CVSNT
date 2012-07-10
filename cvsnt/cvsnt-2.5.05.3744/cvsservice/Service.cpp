/*	cvsnt dispatcher
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
// Service.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "ServiceMsg.h"
#include <config.h>
#include <cvsapi.h>
#include <cvstools.h>
#include <malloc.h>
#include <io.h>
#include <vector>
#include <shlwapi.h>
#include <io.h>

#define SECURITY_WIN32
#include <security.h>
#include <ntsecapi.h>
#include <ntdsapi.h>
#include <dsgetdc.h>
#include <lm.h>
#include <activeds.h>
#include <shellapi.h>

typedef __int64 longint;

#include "../version_no.h"
#include "../version_fu.h"

#include <comdef.h>
struct  _LARGE_INTEGER_X {
 struct {
  DWORD LowPart;
  LONG HighPart;
 } u;
};
// This stops the compiler from complaining about negative values in unsigned longs.
#pragma warning(disable:4146)
#pragma warning(disable:4192)
#import <activeds.tlb>  rename("_LARGE_INTEGER","_LARGE_INTEGER_X") rename("GetObject","_GetObject")
#pragma warning(default:4146)
#pragma warning(default:4192)

#define SERVICE_NAMEA "CVSNT"
#define SERVICE_NAME _T("CVSNT")
#define DISPLAY_NAMEA "CVSNT Low Performance Service " CVSNT_PRODUCTVERSION_STRING_A
#define DISPLAY_NAME _T("CVSNT Low Performance Service ") CVSNT_PRODUCTVERSION_STRING_W
#define NTSERVICE_VERSION_STRING _T("This service handles connections from clients and dispatches CVSNT server processes to handle them.  A high performance version is also available in CVS Suite.")

static void CALLBACK ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv);
static void CALLBACK ServiceHandler(DWORD fdwControl);
static BOOL NotifySCM(DWORD dwState, DWORD dwWin32ExitCode, DWORD dwProgress);
static char* basename(const char* str);
static LPCSTR GetErrorString();
static void ReportError(BOOL bError, LPCSTR szError, ...);
static DWORD CALLBACK DoCvsThread(LPVOID lpParam);
static DWORD CALLBACK DoUnisonThread(LPVOID lpParam);
static bool SendStatistics();
static void GetOsVersion(LPTSTR os, LPTSTR servicepack, DWORD& dwMajor, DWORD& dwMinor);

static DWORD   g_dwCurrentState;
static SERVICE_STATUS_HANDLE  g_hService;
static std::vector<SOCKET> g_Sockets;
static BOOL g_bStop = FALSE;
static BOOL g_bTestMode = FALSE;
static LONG g_dwUsers,g_dwMaxUsers,g_dwAverageTime,g_dwSessionCount;

CRITICAL_SECTION g_crit;

void InitServer()
{
	InitializeCriticalSection(&g_crit);
}

void CloseServer()
{
	DeleteCriticalSection(&g_crit);
}

struct ClientLock
{
	ClientLock() { EnterCriticalSection(&g_crit); }
	~ClientLock() { LeaveCriticalSection(&g_crit); }
};

CVSNT_EXPORT int main(int argc, char* argv[])
{
    SC_HANDLE  hSCManager = NULL, hService = NULL;
    TCHAR szImagePath[MAX_PATH];
	HKEY hk;
	DWORD dwType;
	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{ SERVICE_NAME, ServiceMain },
		{ NULL, NULL }
	};
	LPSTR szRoot;

	if(argc==1)
	{
		// Attempt to start service.  If this fails we're probably
		// not running as a service
		if(!StartServiceCtrlDispatcher(ServiceTable)) return 0;
	}
	if(argc<2 || (strcmp(argv[1],"-i") && strcmp(argv[1],"-reglsa") && strcmp(argv[1],"-u") && strcmp(argv[1],"-unreglsa") && strcmp(argv[1],"-test") && strcmp(argv[1],"-v") && strcmp(argv[1],"-sendstats")))
	{
		fprintf(stderr, "CVSNT Low Performance Service Handler\n\n"
                        "Arguments:\n"
                        "\t%s -i [cvsroot]\tInstall\n"
                        "\t%s -reglsa\tRegister LSA helper\n"
                        "\t%s -u\tUninstall\n"
                        "\t%s -unreglsa\tUnregister LSA helper\n"
                        "\t%s -test\tInteractive run\n"
                        "\t%s -v\tReport version number\n"
						"\t%s -sendstats\tSend statistics\n",
                        basename(argv[0]),basename(argv[0]),
                        basename(argv[0]), basename(argv[0]), 
                        basename(argv[0]), basename(argv[0]),
						basename(argv[0])
                        );
		return -1;
	}

	if(!strcmp(argv[1],"-reglsa"))
	{
		TCHAR lsaBuf[10240];
		DWORD dwLsaBuf;

		if(!CGlobalSettings::isAdmin())
		{
			fprintf(stderr,"Must be run as an administrator to use this option\n");
			return -1;
		}

		if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,_T("SYSTEM\\CurrentControlSet\\Control\\Lsa"),0,KEY_ALL_ACCESS,&hk))
		{
			fprintf(stderr,"Couldn't open LSA registry key, error %d\n",GetLastError());
			return -1;
		}
		dwLsaBuf=sizeof(lsaBuf);
		if(RegQueryValueEx(hk,_T("Authentication Packages"),NULL,&dwType,(BYTE*)lsaBuf,&dwLsaBuf))
		{
			fprintf(stderr,"Couldn't read LSA registry key, error %d\n",GetLastError());
			return -1;
		}
		if(dwType!=REG_MULTI_SZ)
		{
			fprintf(stderr,"LSA key isn't REG_MULTI_SZ!!!\n");
			return -1;
		}
		lsaBuf[dwLsaBuf]='\0';
		TCHAR *p = lsaBuf;
		while(*p)
		{
			if(!_tcscmp(p,_T("setuid")))
				break;
			p+=_tcslen(p)+1;
		}
		if(!*p)
		{
			_tcscpy(p,_T("setuid"));
			dwLsaBuf+=_tcslen(p)+1;
			lsaBuf[dwLsaBuf]='\0';
			if(RegSetValueEx(hk,_T("Authentication Packages"),NULL,dwType,(BYTE*)lsaBuf,dwLsaBuf))
			{
				fprintf(stderr,"Couldn't write LSA registry key, error %d\n",GetLastError());
				return -1;
			}
		}
		return 0;
	}

	if(!strcmp(argv[1],"-unreglsa"))
	{
		TCHAR lsaBuf[10240];
		DWORD dwLsaBuf;

		if(!CGlobalSettings::isAdmin())
		{
			fprintf(stderr,"Must be run as an administrator to use this option\n");
			return -1;
		}

		if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,_T("SYSTEM\\CurrentControlSet\\Control\\Lsa"),0,KEY_ALL_ACCESS,&hk))
		{
			fprintf(stderr,"Couldn't open LSA registry key, error %d\n",GetLastError());
			return -1;
		}
		dwLsaBuf=sizeof(lsaBuf);
		if(RegQueryValueEx(hk,_T("Authentication Packages"),NULL,&dwType,(BYTE*)lsaBuf,&dwLsaBuf))
		{
			fprintf(stderr,"Couldn't read LSA registry key, error %d\n",GetLastError());
			return -1;
		}
		if(dwType!=REG_MULTI_SZ)
		{
			fprintf(stderr,"LSA key isn't REG_MULTI_SZ!!!\n");
			return -1;
		}
		lsaBuf[dwLsaBuf]='\0';
		TCHAR *p = lsaBuf;
		while(*p)
		{
			if(!_tcscmp(p,_T("setuid")))
				break;
			p+=_tcslen(p)+1;
		}
		if(*p)
		{
			size_t l = _tcslen(p)+1;
			memcpy(p,p+l,(dwLsaBuf-((p+l)-lsaBuf))+1);
			dwLsaBuf-=l;
			if(RegSetValueEx(hk,_T("Authentication Packages"),NULL,dwType,(BYTE*)lsaBuf,dwLsaBuf))
			{
				fprintf(stderr,"Couldn't write LSA registry key, error %d\n",GetLastError());
				return -1;
			}
		}
		return 0;
	}

    if (!strcmp(argv[1],"-v"))
	{
        _putts(NTSERVICE_VERSION_STRING);
        return 0;
    }

	if(!strcmp(argv[1],"-i"))
	{
		HKEY hk;

		if(!CGlobalSettings::isAdmin())
		{
			fprintf(stderr,"Must be run as an administrator to use this option\n");
			return -1;
		}

		if(RegCreateKeyEx(HKEY_LOCAL_MACHINE,_T("Software\\CVS\\Pserver"),NULL,_T(""),REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&hk,NULL))
		{ 
			fprintf(stderr,"Couldn't create HKLM\\Software\\CVS\\Pserver key, error %d\n",GetLastError());
			return -1;
		}

		if(argc==3)
		{
			szRoot = argv[2];
			if(GetFileAttributesA(szRoot)==(DWORD)-1)
			{
				fprintf(stderr,"Repository directory '%s' not found\n",szRoot);
				return -1;
			}
			dwType=REG_SZ;
			RegSetValueExA(hk,"Repository0",NULL,dwType,(BYTE*)szRoot,strlen(szRoot)+1);
		}
		// connect to  the service control manager
		if((hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE)) == NULL)
		{
			fprintf(stderr,"OpenSCManager Failed\n");
			return -1;
		}

		if((hService=OpenService(hSCManager,SERVICE_NAME,DELETE))!=NULL)
		{
			DeleteService(hService);
			CloseServiceHandle(hService);
		}

		GetModuleFileName(NULL,szImagePath,MAX_PATH);
		if ((hService = CreateService(hSCManager,SERVICE_NAME,DISPLAY_NAME,
						STANDARD_RIGHTS_REQUIRED|SERVICE_CHANGE_CONFIG, SERVICE_WIN32_OWN_PROCESS|SERVICE_INTERACTIVE_PROCESS,
						SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
						szImagePath, NULL, NULL, NULL, NULL, NULL)) == NULL)
		{
			fprintf(stderr,"CreateService Failed: %s\n",GetErrorString());
			return -1;
		}
		{
			BOOL (WINAPI *pChangeServiceConfig2)(SC_HANDLE,DWORD,LPVOID);
			pChangeServiceConfig2=(BOOL (WINAPI *)(SC_HANDLE,DWORD,LPVOID))GetProcAddress(GetModuleHandle(_T("advapi32")),"ChangeServiceConfig2W");
			if(pChangeServiceConfig2)
			{
				SERVICE_DESCRIPTION sd = { NTSERVICE_VERSION_STRING };
				if(!pChangeServiceConfig2(hService,SERVICE_CONFIG_DESCRIPTION,&sd))
				{
					0;
				}
				SERVICE_REQUIRED_PRIVILEGES_INFO sp = { SE_NETWORK_LOGON_NAME _T("\0") SE_IMPERSONATE_NAME _T("\0") };
				if(!pChangeServiceConfig2(hService,SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO,&sp))
				{
					0;
				}
			}
		}
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		ReportError(FALSE,DISPLAY_NAMEA " installed successfully");
		printf(DISPLAY_NAMEA " installed successfully\n");
		RegCloseKey(hk);
	}
	

	if(!strcmp(argv[1],"-u"))
	{
		if(!CGlobalSettings::isAdmin())
		{
			fprintf(stderr,"Must be run as an administrator to use this option\n");
			return -1;
		}

		// connect to  the service control manager
		if((hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE)) == NULL)
		{
			fprintf(stderr,"OpenSCManager Failed\n");
			return -1;
		}

		if((hService=OpenService(hSCManager,SERVICE_NAME,DELETE))==NULL)
		{
			fprintf(stderr,"OpenService Failed: %s\n",GetErrorString());
			return -1;
		}
		if(!DeleteService(hService))
		{
			fprintf(stderr,"DeleteService Failed: %s\n",GetErrorString());
			return -1;
		}
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		ReportError(FALSE,DISPLAY_NAMEA " uninstalled successfully");
		printf(DISPLAY_NAMEA " uninstalled successfully\n");
	}	
	else if(!strcmp(argv[1],"-test"))
	{
		if(!CGlobalSettings::isAdmin())
			fprintf(stderr,"**WARNING** Not running as administrator.  Some functions may fail.\n");

		ServiceMain(999,NULL);
	}
	else if(!strcmp(argv[1],"-sendstats"))
	{
		g_bTestMode=true;
		printf("Sending Statistics\n");
		SendStatistics();
	}
	return 0;
}

void CALLBACK ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
	TCHAR szTmp[8192];
	char szTmpA[8192];
	TCHAR szTmp2[8192];
	char szAuthServer[32], szUnisonServer[32];
	DWORD dwTmp,dwType;
	HKEY hk,hk_env;
	int seq=1;
	LPCSTR szNode;
	int authserver_port = 2401;
	int unison_port = 0;
	bool bUnison;

	if(dwArgc!=999)
	{
		if (!(g_hService = RegisterServiceCtrlHandler(SERVICE_NAME,ServiceHandler))) { ReportError(TRUE,"Unable to start "SERVICE_NAMEA" - RegisterServiceCtrlHandler failed"); return; }
		NotifySCM(SERVICE_START_PENDING, 0, seq++);
	}
	else
	{
		g_bTestMode=TRUE;
		printf(SERVICE_NAMEA" " CVSNT_PRODUCTVERSION_STRING " ("__DATE__") starting in test mode.\n");
	}

	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,_T("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"),NULL,KEY_QUERY_VALUE,&hk_env))
	{ 
		ReportError(TRUE,"Unable to start "SERVICE_NAMEA" - Couldn't open environment key"); 
		if(!g_bTestMode)
			NotifySCM(SERVICE_STOPPED,0,0);
		return;
	}

	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,_T("Software\\CVS\\Pserver"),NULL,KEY_QUERY_VALUE,&hk))
	{
		ReportError(TRUE,"Unable to start "SERVICE_NAMEA" - Couldn't open HKLM\\Software\\CVS\\Pserver key");
		if(!g_bTestMode)
			NotifySCM(SERVICE_STOPPED,0,0);
		return;
	}

	dwTmp=sizeof(szTmp);
	szTmp2[0]='\0';

	if(!RegQueryValueEx(hk,_T("InstallPath"),NULL,&dwType,(BYTE*)szTmp2,&dwTmp))
	{
		_tcscat(szTmp2,_T(";"));
	}

	dwTmp=sizeof(szTmp);
	if(!RegQueryValueEx(hk_env,_T("PATH"),NULL,&dwType,(BYTE*)szTmp,&dwTmp))
		ExpandEnvironmentStrings(szTmp,szTmp2+_tcslen(szTmp2),sizeof(szTmp2)-_tcslen(szTmp2));

	if(!*szTmp2)
	{
		ReportError(TRUE,"Unable to start "SERVICE_NAMEA" - No path");
		if(!g_bTestMode)
			NotifySCM(SERVICE_STOPPED,0,0);
		return;
	}

	SetEnvironmentVariable(_T("PATH"),szTmp2);

	RegCloseKey(hk_env);

	dwTmp=sizeof(szTmp);
	if(RegQueryValueEx(hk,_T("TempDir"),NULL,&dwType,(LPBYTE)szTmp,&dwTmp) &&
	   SHRegGetUSValue(_T("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"),_T("TEMP"),NULL,(LPVOID)szTmp,&dwTmp,TRUE,NULL,0) &&
	   !GetEnvironmentVariable(_T("TEMP"),(LPTSTR)szTmp,sizeof(szTmp)) &&
	   !GetEnvironmentVariable(_T("TMP"),(LPTSTR)szTmp,sizeof(szTmp)))
	{
		_tcscpy(szTmp,_T("C:\\"));
	}

	SetEnvironmentVariable(_T("TEMP"),szTmp);
	SetEnvironmentVariable(_T("TMP"),szTmp);
	SetEnvironmentVariable(_T("HOME"),szTmp);
	if(g_bTestMode)
		_tprintf(_T("TEMP/TMP currently set to %s\n"),szTmp);

	dwTmp=sizeof(DWORD);
	if(!RegQueryValueEx(hk,_T("PServerPort"),NULL,&dwType,(BYTE*)szTmp,&dwTmp))
	{
		authserver_port=*(DWORD*)szTmp;
	}
	itoa(authserver_port,szAuthServer,10);

	bUnison = true;
	dwTmp=sizeof(DWORD);
	if(!RegQueryValueEx(hk,_T("EnableUnison"),NULL,&dwType,(BYTE*)szTmp,&dwTmp))
	{
		if(*(DWORD*)szTmp)
			bUnison=true;
		else
			bUnison=false;
	}

	unison_port = 0;
	dwTmp=sizeof(DWORD);
	if(!RegQueryValueEx(hk,_T("UnisonPort"),NULL,&dwType,(BYTE*)szTmp,&dwTmp))
	{
		unison_port=*(DWORD*)szTmp;
	}
	if(unison_port)
		itoa(unison_port,szUnisonServer,10);
	else
		bUnison=false;

	CSocketIO cvs_sock;
	CSocketIO unison_sock;
	TCHAR unison_path[MAX_PATH];

	if(FindExecutable(_T("unison.exe"),NULL,unison_path)>=(HINSTANCE)32)
	{
		if(g_bTestMode)
			printf("Unison is available\n");
		if(!bUnison)
			printf("Unison is disabled\n");
	}
	else
		bUnison=false;

// Initialisation
	if(!CSocketIO::init())
	{
		ReportError(TRUE,"WSAStartup failed: %s\n",cvs_sock.error());
		if(!g_bTestMode)
			NotifySCM(SERVICE_STOPPED,0,0);
		return;
	}

	dwTmp=sizeof(szTmpA);
	szNode = NULL;
	if(!RegQueryValueExA(hk,"BindAddress",NULL,&dwType,(BYTE*)szTmpA,&dwTmp))
	{
		if(stricmp(szTmpA,"*"))
			szNode = szTmpA;
	}

	if(g_bTestMode)
	{
		printf("Initialising socket...\n");
	}

	if(!cvs_sock.create(szNode,szAuthServer,false))
	{
		ReportError(FALSE,"Failed to create socket: %s\n",cvs_sock.error());
		if(g_bTestMode)
			printf("Failed to create socket: %s\n",cvs_sock.error());
		else
			NotifySCM(SERVICE_STOPPED,0,0);
		return;
	}

	if(bUnison && !unison_sock.create(szNode,szUnisonServer,false))
	{
		ReportError(FALSE,"Failed to create socket: %s\n",cvs_sock.error());
		if(g_bTestMode)
			printf("Failed to create socket: %s\n",cvs_sock.error());
		else
			NotifySCM(SERVICE_STOPPED,0,0);
		return;
	}

	if(!g_bTestMode)
		NotifySCM(SERVICE_START_PENDING, 0, seq++);

	if(g_bTestMode)
	{
		printf("Starting auth server on port %d/tcp...\n",authserver_port);
		if(bUnison)
			printf("Starting unison server on port %d/tcp...\n",unison_port);
	}


	if(!cvs_sock.bind())
	{
		ReportError(TRUE,"Failed to bind socket: %s\n",cvs_sock.error());
		if(g_bTestMode)
			printf("Failed to bind socket: %s\n",cvs_sock.error());
		else
			NotifySCM(SERVICE_STOPPED,0,0);
		return;
	}

	if(bUnison && !unison_sock.bind())
	{
		ReportError(TRUE,"Failed to bind socket: %s\n",cvs_sock.error());
		if(g_bTestMode)
			printf("Failed to bind socket: %s\n",cvs_sock.error());
		else
			NotifySCM(SERVICE_STOPPED,0,0);
		return;
	}

	if(LoadLibrary(_T("NtDsApi")))
	{
		CoInitialize(NULL);
		try
		{
			ActiveDs::IADsADSystemInfoPtr info(CLSID_ADSystemInfo);
			_bstr_t path(info->ComputerName);

			if(g_bTestMode)
			{
				printf("Registering service SPN...\n");
				printf("DSN=%s\n",(const char *)path);
			}

			DWORD nspn = 0;
			LPTSTR *pspn;
			HANDLE hDS = NULL;
			dwTmp = DsBind(NULL,NULL,&hDS);
			if(!dwTmp)
				dwTmp = DsGetSpn(DS_SPN_DNS_HOST,_T("cvs"),NULL,0,0,NULL,NULL,&nspn,&pspn);
			if(g_bTestMode && nspn)
				printf("Adding %S\n",pspn[0]);
			if(!dwTmp)
				dwTmp = DsWriteAccountSpn(hDS,DS_SPN_ADD_SPN_OP ,path,nspn,(LPCTSTR*)pspn);
			if(nspn)
				DsFreeSpnArray(nspn,pspn);
			nspn = 0;
			if(!dwTmp)
				dwTmp = DsGetSpn(DS_SPN_NB_HOST,_T("cvs"),NULL,0,0,NULL,NULL,&nspn,&pspn);
			if(g_bTestMode && nspn)
				printf("Adding %S\n",pspn[0]);
			if(!dwTmp)
				dwTmp = DsWriteAccountSpn(hDS,DS_SPN_ADD_SPN_OP,path,nspn,(LPCTSTR*)pspn);
			if(nspn)
				DsFreeSpnArray(nspn,pspn);
			if(hDS)
				dwTmp = DsUnBind(&hDS);

			if(dwTmp)
			{
				if(g_bTestMode)
					printf("Active Directory registration failed (Error %d)\n",dwTmp);
			}
		}
		catch(_com_error e)
		{
			if(g_bTestMode)
				printf("Couldn't register with active directory: %S\n",e.ErrorMessage());
		}
	}

	// Process running, wait for closedown
	ReportError(FALSE,SERVICE_NAMEA" initialised successfully");
	if(!g_bTestMode)
		NotifySCM(SERVICE_RUNNING, 0, 0);

	time_t servtime;
	time(&servtime);
	longint st = servtime;
	CGlobalSettings::SetGlobalValue("Server",NULL,"StartTime",st);

	int dw = 0;
	CGlobalSettings::GetGlobalValue("Server",NULL,"MaxUsers",dw);
	g_dwMaxUsers=dw;
	
	dw = 0;
	CGlobalSettings::GetGlobalValue("Server",NULL,"AverageTime",dw);
	g_dwAverageTime=dw;

	dw = 0;
	CGlobalSettings::GetGlobalValue("Server",NULL,"SessionCount",dw);
	g_dwSessionCount=dw;

	g_dwUsers=0;
	g_bStop=FALSE;

	InitServer();

	LONG dwOldMaxUsers = g_dwMaxUsers;

	CSocketIO* sock_list[2] = { &cvs_sock, &unison_sock };
	
	while(!g_bStop && CSocketIO::select(5000,bUnison?2:1,sock_list))
	{
		for(size_t n=0; n<cvs_sock.accepted_sockets().size(); n++)
			CloseHandle(CreateThread(NULL,0,DoCvsThread,(void*)cvs_sock.accepted_sockets()[n].Detach(),0,NULL));
		if(bUnison)
		{
			for(size_t n=0; n<unison_sock.accepted_sockets().size(); n++)
				CloseHandle(CreateThread(NULL,0,DoUnisonThread,(void*)unison_sock.accepted_sockets()[n].Detach(),0,NULL));
		}
		if(dwOldMaxUsers<g_dwMaxUsers)
		{
			dwOldMaxUsers=g_dwMaxUsers;
			CGlobalSettings::SetGlobalValue("Server",NULL,"MaxUsers",(int)dwOldMaxUsers);
		}
		CGlobalSettings::SetGlobalValue("Server",NULL,"SessionCount",g_dwSessionCount);
		CGlobalSettings::SetGlobalValue("Server",NULL,"AverageTime",g_dwAverageTime);

		int dwSendStatistics=0;
		CGlobalSettings::GetGlobalValue("Server",NULL,"SendStatistics",dwSendStatistics);

		if (dwSendStatistics)
		{
			longint lastAttempt,lastSuccess;
			if (CGlobalSettings::GetGlobalValue("Server",NULL,"LastAttemptedSend",lastAttempt))
				lastAttempt=0;
			if (CGlobalSettings::GetGlobalValue("Server",NULL,"LastSend",lastSuccess))
				lastSuccess=0;
			CGlobalSettings::SetGlobalValue("Server",NULL,"LastAttemptedSend",lastAttempt);
			time_t nextAttempt,now;
			if(lastAttempt==lastSuccess)
				nextAttempt = lastAttempt + 604800; // 1 week
			else
				nextAttempt = lastAttempt + 28800; // 8 hour retry on failure
			
			time(&now);
			if((nextAttempt<=now))
			{
				if(g_bTestMode)
					printf("Sending Statistics\n");
				bool bSuccess = SendStatistics();
				if(g_bTestMode)
					printf("Sending Statistics %s\n",bSuccess?"succeeded":"failed");
				
				lastAttempt=now;
				CGlobalSettings::SetGlobalValue("Server",NULL,"LastAttemptedSend",lastAttempt);
				if(bSuccess)
					CGlobalSettings::SetGlobalValue("Server",NULL,"LastSend",lastAttempt);
			}
		}
	}

	NotifySCM(SERVICE_STOPPED, 0, 0);
	ReportError(FALSE,SERVICE_NAMEA" stopped successfully");

	RegCloseKey(hk);
}

void CALLBACK ServiceHandler(DWORD fdwControl)
{
	switch(fdwControl)
	{      
	case SERVICE_CONTROL_STOP:
		OutputDebugString(SERVICE_NAME _T(": Stop\n"));
		NotifySCM(SERVICE_STOP_PENDING, 0, 0);
		g_bStop=TRUE;
		return;
	case SERVICE_CONTROL_INTERROGATE:
	default:
		break;
	}
	OutputDebugString(SERVICE_NAME _T(": Interrogate\n"));
	NotifySCM(g_dwCurrentState, 0, 0);
}

BOOL NotifySCM(DWORD dwState, DWORD dwWin32ExitCode, DWORD dwProgress)
{
	SERVICE_STATUS ServiceStatus;

	// fill in the SERVICE_STATUS structure
	ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	ServiceStatus.dwCurrentState = g_dwCurrentState = dwState;
	ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	ServiceStatus.dwWin32ExitCode = dwWin32ExitCode;
	ServiceStatus.dwServiceSpecificExitCode = 0;
	ServiceStatus.dwCheckPoint = dwProgress;
	ServiceStatus.dwWaitHint = 3000;

	// send status to SCM
	return SetServiceStatus(g_hService, &ServiceStatus);
}

char* basename(const char* str)
{
	char*p = ((char*)str)+strlen(str)-1;
	while(p>str && *p!='\\')
		p--;
	if(p>str) return (p+1);
	else return p;
}

LPCSTR GetErrorString()
{
	static char ErrBuf[1024];

	FormatMessageA(
    FORMAT_MESSAGE_FROM_SYSTEM |
	FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    GetLastError(),
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
    (LPSTR) ErrBuf,
    sizeof(ErrBuf),
    NULL );
	return ErrBuf;
};

void ReportError(BOOL bError, LPCSTR szError, ...)
{
	char buf[512];
	const char *bufp = buf;
	va_list va;

	va_start(va,szError);
	vsprintf(buf,szError,va);
	va_end(va);
	if(g_bTestMode)
		printf("%s%s\n",bError?"Error: ":"",buf);
	else
		CServerIo::log(bError?CServerIo::logError:CServerIo::logNotice,buf);
}

DWORD CALLBACK DoCvsThread(LPVOID lpParam)
{
	CSocketIOPtr conn = (CSocketIO*)lpParam;
	CRunFile rf;
	cvs::string line;
	DWORD timestart = GetTickCount(),timeend;

	InterlockedIncrement(&g_dwUsers);
	if(g_dwUsers>g_dwMaxUsers) g_dwMaxUsers=g_dwUsers;

	cvs::sprintf(line,128,"cvs.exe --win32_socket_io=%ld authserver",(long)conn->getsocket());
	rf.setArgs(line.c_str());
	rf.run(NULL);
	if(g_bTestMode)
		printf("%08x: Cvsnt process started\n",GetTickCount());
	do
	{
		int ret;
		if(rf.wait(ret,5000))
			break;
	} while(!g_bStop);

	InterlockedDecrement(&g_dwUsers);
	timeend = GetTickCount();

	InterlockedIncrement(&g_dwSessionCount);

	if(timeend>timestart)
	{
		ClientLock lock;

		// There must be a better way than multiply/add/divide but I can't think of it right now

		DWORD sessiontime = timeend - timestart;
		__int64 tot = g_dwAverageTime;
		tot *= (g_dwSessionCount-1);
		tot += sessiontime;
		tot /= g_dwSessionCount;
		g_dwAverageTime = (DWORD)tot;
	}

	if(g_bStop)
		rf.terminate();

	if(g_bTestMode)
		printf("%08x: Cvsnt process %terminated\n",GetTickCount());

	return 0;
}

DWORD CALLBACK DoUnisonThread(LPVOID lpParam)
{
	CSocketIOPtr conn = (CSocketIO*)lpParam;
	CRunFile rf;
	cvs::string line;

	cvs::sprintf(line,128,"unison.exe -server");
	rf.setArgs(line.c_str());

	rf.setInput(CRunFile::StandardInput,NULL);
	rf.setOutput(CRunFile::StandardOutput,NULL);
	rf.setError(CRunFile::StandardError,NULL);

	SetStdHandle(STD_INPUT_HANDLE,(HANDLE)conn->getsocket());
	SetStdHandle(STD_OUTPUT_HANDLE,(HANDLE)conn->getsocket());
	SetStdHandle(STD_ERROR_HANDLE,(HANDLE)conn->getsocket());

	rf.run(NULL);
	if(g_bTestMode)
		printf("%08x: Unison process started\n",GetTickCount());
	do
	{
		int ret;
		if(rf.wait(ret,5000))
			break;
	} while(!g_bStop);

	if(g_bStop)
		rf.terminate();

	if(g_bTestMode)
		printf("%08x: Unison process %terminated\n",GetTickCount());

	return 0;
}

bool SendStatistics()
{
	CHttpSocket sock;
	char os[256],servicepack[256];
	longint servtime;
	longint uptime;
	int dwMaxUsers;
	int dwAverageTime;
	int dwSessionCount;
	int dwUserCount;
	int major, minor;
	cvs::string registration;
	unsigned  dwCodepage;

#ifdef _WIN32
	HKEY hInstallerKey;
	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,_T("Software\\March Hare Software Ltd\\Keys"),0,KEY_READ,&hInstallerKey))
	{
	   if (RegCreateKeyEx(HKEY_CURRENT_USER,_T("Software\\March Hare Software Ltd\\Keys"),0,NULL,0,KEY_READ,NULL,&hInstallerKey,NULL))
	   {
		if(g_bTestMode)
			printf("Sending Statistics - cannot open Keys\n");
		return false;
	   }
	}

	dwCodepage = GetACP();
#else
	dwCodepage = 0; //  This should be a string, as numeric codepages don't exist outside windows
#endif

	GetOsVersion(os,servicepack, major, minor);
	if(g_bTestMode)
		printf("Sending Statistics - os=%s servicepack=%s major=%d minor=%d\n",
					os,servicepack, major, minor);

	if (!CGlobalSettings::GetGlobalValue("Server",NULL,"StartTime",servtime))
	{
		time_t then = servtime,now;
		time(&now);
		if(then>now) uptime=0;
		else uptime=now-then;
	}
	else uptime=0;

	if(CGlobalSettings::GetGlobalValue("Server",NULL,"MaxUsers",dwMaxUsers))
		dwMaxUsers=0;
		
	if (CGlobalSettings::GetGlobalValue("Server",NULL,"AverageTime",dwAverageTime))
		dwAverageTime=0;
		
	if (CGlobalSettings::GetGlobalValue("Server",NULL,"SessionCount",dwSessionCount))
		dwSessionCount=0;
		
	if (CGlobalSettings::GetGlobalValue("Server",NULL,"UserCount",dwUserCount))
		dwUserCount=0;
	
#ifdef _WIN32	
	DWORD dwLen,dwType;
	registration.resize(256);
	dwLen=(DWORD)registration.size();
	if(!RegQueryValueExA(hInstallerKey,"serversuite",NULL,&dwType,(LPBYTE)registration.data(),&dwLen) || dwType!=REG_SZ)
	{
		dwLen=(DWORD)registration.size();
		if(!RegQueryValueExA(hInstallerKey,"server",NULL,&dwType,(LPBYTE)registration.data(),&dwLen) || dwType!=REG_SZ)
		{
			dwLen=(DWORD)registration.size();
			if(!RegQueryValueExA(hInstallerKey,"servertrial",NULL,&dwType,(LPBYTE)registration.data(),&dwLen) || dwType!=REG_SZ)
				registration="";
			else
				registration.resize(dwLen);
		}
		else
			registration.resize(dwLen);
	}
	else
		registration.resize(dwLen);

	RegCloseKey(hInstallerKey);
#endif

	cvs::string url;
	cvs::sprintf(url,128,"/cvspro/prods-pre.asp?register=stats&os=%s&sp=%s&ver=%s&avgtime=%u&up=%u&users=%u&uusers=%u&susers=%u&id=%s&cp=%u&major=%u&minor=%u",
		os,servicepack,CVSNT_PRODUCTVERSION_SHORT,dwAverageTime,(unsigned)uptime,dwSessionCount,dwUserCount,dwMaxUsers,registration.c_str(),dwCodepage,major,minor);

	// To be 100% certain, replace all ' ' with %20.  Webservers can cope but there may be a proxy
	// that doesn't.

	size_t pos;
	while((pos=url.find_first_of(' '))!=cvs::string::npos)
		url.replace(pos,1,"%20");

	if(g_bTestMode)
		printf("Sending Statistics http://march-hare.com%s\n",url.c_str());
	if(!sock.create("http://www.march-hare.com"))
	{
		if(g_bTestMode)
			printf("Failed to send statistics.  Server not responding\n");
		return false;
	}

	if(!sock.request("GET",url.c_str()))
	{
		// Not sure this can actually fail.. it would normally succeed with an error code
		if(g_bTestMode)
			printf("Failed to send statistics.  Server not responding\n");
		return false;
	}

	if(sock.responseCode()!=200)
	{
		if(g_bTestMode)
			printf("Failed to send statistics.  Server returned error %d\n",sock.responseCode());
		return false;
	}

	return true;
}
