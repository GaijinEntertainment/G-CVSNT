/*	cvsnt control service
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
// lockservice.cpp : Defines the entry point for the console application.
//

#ifdef _WIN32
// Microsoft braindamage reversal.  
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#endif

#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#include <shellapi.h>
#include <config.h>

#include "../cvsapi/ServiceMsg.h"
#include "../version_no.h"
#include "../version_fu.h"
#include "resource.h"
#include "../cvsapi/cvsapi.h"

#include "CvsControl.h"

#define SERVICE_NAME "CVSControl"
#define DISPLAY_NAME "CVSNT Control Panel"
#define NTSERVICE_VERSION_STRING "CVSNT Control Panel " CVSNT_PRODUCTVERSION_STRING

static void CALLBACK ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv);
static void CALLBACK ServiceHandler(DWORD fdwControl);
static char* basename(const char* str);
static LPCTSTR GetErrorString();
static void AddEventSource(LPCTSTR szService, LPCTSTR szModule);

void ReportError(BOOL bError, LPCTSTR szError, ...);
BOOL NotifySCM(DWORD dwState, DWORD dwWin32ExitCode, DWORD dwProgress);

static DWORD   g_dwCurrentState;
static SERVICE_STATUS_HANDLE  g_hService;
extern bool g_bStop;
bool g_bTestMode = false;
static int controlserver_port = 12401;
static int local_only = 0;

int main(int argc, char* argv[])
{
    SC_HANDLE  hSCManager = NULL, hService = NULL;
    char szImagePath[MAX_PATH];
	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{ SERVICE_NAME, ServiceMain },
		{ NULL, NULL }
	};

	if(argc==1)
	{
		// Attempt to start service.  If this fails we're probably
		// not running as a service
		if(!StartServiceCtrlDispatcher(ServiceTable))
			return 0;
	}

	if(argc!=2 || (strcmp(argv[1],"-i") && strcmp(argv[1],"-u") && strcmp(argv[1],"-test") && strcmp(argv[1],"-v") && strcmp(argv[1],"-systray") ))
	{
		fprintf(stderr, "NT CVS Service Handler\n\n"
                        "Arguments:\n"
                        "\t%s -i\tInstall\n"
                        "\t%s -u\tUninstall\n"
                        "\t%s -test\tInteractive run"
                        "\t%s -v\tReport version number",
                        basename(argv[0]),basename(argv[0]),
                        basename(argv[0]), basename(argv[0]));
		return -1;
	}

    if (!strcmp(argv[1],"-v"))
	{
        puts(NTSERVICE_VERSION_STRING);
        return 0;
       }

	if(!strcmp(argv[1],"-i"))
	{
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
						STANDARD_RIGHTS_REQUIRED|SERVICE_CHANGE_CONFIG, SERVICE_WIN32_OWN_PROCESS,
						SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
						szImagePath, NULL, NULL, NULL, NULL, NULL)) == NULL)
		{
			fprintf(stderr,"CreateService Failed: %s\n",GetErrorString());
			return -1;
		}
		{
			BOOL (WINAPI *pChangeServiceConfig2)(SC_HANDLE,DWORD,LPVOID);
			pChangeServiceConfig2=(BOOL (WINAPI *)(SC_HANDLE,DWORD,LPVOID))GetProcAddress(GetModuleHandle("advapi32"),"ChangeServiceConfig2A");
			if(pChangeServiceConfig2)
			{
				SERVICE_DESCRIPTION sd = { NTSERVICE_VERSION_STRING };
				pChangeServiceConfig2(hService,SERVICE_CONFIG_DESCRIPTION,&sd);
			}
		}
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		ReportError(FALSE,DISPLAY_NAME " installed successfully");
		printf(DISPLAY_NAME " installed successfully\n");
	}

	if(!strcmp(argv[1],"-u"))
	{
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
		ReportError(FALSE,DISPLAY_NAME " uninstalled successfully");
		printf(DISPLAY_NAME " uninstalled successfully\n");
	}
	else if(!strcmp(argv[1],"-test"))
	{
		ServiceMain(999,NULL);
	}
	return 0;
}

char* basename(const char* str)
{
	char*p = ((char*)str)+strlen(str)-1;
	while(p>str && *p!='\\')
		p--;
	if(p>str) return (p+1);
	else return p;
}

LPCTSTR GetErrorString()
{
	static char ErrBuf[1024];

	FormatMessage(
    FORMAT_MESSAGE_FROM_SYSTEM |
	FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    GetLastError(),
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
    (LPTSTR) ErrBuf,
    sizeof(ErrBuf),
    NULL );
	return ErrBuf;
};

void ReportError(BOOL bError, LPCTSTR szError, ...)
{
	static BOOL bEventSourceAdded = FALSE;
	char buf[512];
	const char *bufp = buf;
	va_list va;

	va_start(va,szError);
	vsprintf(buf,szError,va);
	va_end(va);
	if(g_bTestMode)
	{
		printf("%s%s\n",bError?"Error: ":"",buf);
	}
	else
	{
		if(!bEventSourceAdded)
		{
			char szModule[MAX_PATH];
			GetModuleFileName(NULL,szModule,MAX_PATH);
			AddEventSource(SERVICE_NAME,szModule);
			bEventSourceAdded=TRUE;
		}

		HANDLE hEvent = RegisterEventSource(NULL,  SERVICE_NAME);
		ReportEvent(hEvent,bError?EVENTLOG_ERROR_TYPE:EVENTLOG_INFORMATION_TYPE,0,MSG_STRING,NULL,1,0,&bufp,NULL);
		DeregisterEventSource(hEvent);
	}
}

void AddEventSource(LPCTSTR szService, LPCTSTR szModule)
{
	HKEY hk;
	DWORD dwData;
	char szKey[1024];

	strcpy(szKey,"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\");
	strcat(szKey,szService);

    // Add your source name as a subkey under the Application
    // key in the EventLog registry key.
    if (RegCreateKey(HKEY_LOCAL_MACHINE, szKey, &hk))
		return; // Fatal error, no key and no way of reporting the error!!!

    // Add the name to the EventMessageFile subkey.
    if (RegSetValueEx(hk,             // subkey handle
            "EventMessageFile",       // value name
            0,                        // must be zero
            REG_EXPAND_SZ,            // value type
            (LPBYTE) szModule,           // pointer to value data
            (DWORD)strlen(szModule) + 1))       // length of value data
			return; // Couldn't set key

    // Set the supported event types in the TypesSupported subkey.
    dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE |
        EVENTLOG_INFORMATION_TYPE;
    if (RegSetValueEx(hk,      // subkey handle
            "TypesSupported",  // value name
            0,                 // must be zero
            REG_DWORD,         // value type
            (LPBYTE) &dwData,  // pointer to value data
            sizeof(DWORD)))    // length of value data
			return; // Couldn't set key
	RegCloseKey(hk);
}

void CALLBACK ServiceHandler(DWORD fdwControl)
{
	switch(fdwControl)
	{
	case SERVICE_CONTROL_STOP:
		OutputDebugString(SERVICE_NAME": Stop\n");
		NotifySCM(SERVICE_STOP_PENDING, 0, 0);
		g_bStop=TRUE;
		return;
	case SERVICE_CONTROL_INTERROGATE:
	default:
		break;
	}
	OutputDebugString(SERVICE_NAME": Interrogate\n");
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

void CALLBACK ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
	int seq=1;
	HKEY hk;
	DWORD dwTmp,dwType;
	char szTmp[1024];

	HANDLE hSem = CreateSemaphore(NULL,0,1,"CVSNT_ControlPanel");

	if(dwArgc!=999)
	{
		if (!(g_hService = RegisterServiceCtrlHandler(SERVICE_NAME,ServiceHandler))) { ReportError(TRUE,"Unable to start "SERVICE_NAME" - RegisterServiceCtrlHandler failed"); return; }
		NotifySCM(SERVICE_START_PENDING, 0, seq++);
	}
	else
	{
		g_bTestMode=TRUE;
		printf(SERVICE_NAME" " CVSNT_PRODUCTVERSION_STRING " ("__DATE__") starting in test mode.\n");
	}

// Initialisation
    WSADATA data;

    if(WSAStartup (MAKEWORD (1, 1), &data))
	{
		ReportError(TRUE,"WSAStartup failed... aborting - Error %d\n",WSAGetLastError());
		if(!g_bTestMode)
			NotifySCM(SERVICE_STOPPED,0,0);
		return;
	}

	if(!RegOpenKeyEx(HKEY_LOCAL_MACHINE,"Software\\CVS\\Pserver",NULL,KEY_QUERY_VALUE,&hk))
	{
		dwTmp=sizeof(szTmp);
		if(!RegQueryValueEx(hk,"ControlPanel",NULL,&dwType,(BYTE*)szTmp,&dwTmp))
		{
			char *p = strchr(szTmp,':');
			if(p)
				controlserver_port=atoi(p+1);
		}
		dwTmp=sizeof(szTmp);
		if(!RegQueryValueEx(hk,"ControlPanelLocal",NULL,&dwType,(BYTE*)szTmp,&dwTmp))
		{
			if(dwType==REG_DWORD)
				local_only = *(DWORD*)szTmp;
		}
		RegCloseKey(hk);
	}

    run_server(controlserver_port, seq, local_only);

	CloseHandle(hSem);

	if(!g_bTestMode)
		NotifySCM(SERVICE_STOPPED, 0, 0);
	ReportError(FALSE,SERVICE_NAME" stopped successfully");

}

