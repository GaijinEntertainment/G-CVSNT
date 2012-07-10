/*	cvsnt lockserver
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
#include <tchar.h>
#include <process.h>
#include <config.h>
#include "resource.h"
#include <cvstools.h>

#include <shellapi.h>

#include "LockService.h"

#define SERVICE_NAME "CVSLock"
#define DISPLAY_NAME "CVSNT Locking Service"
#define NTSERVICE_VERSION_STRING "CVSNT Locking Service " CVSNT_PRODUCTVERSION_STRING

#define TRAY_MESSAGE (WM_APP+123)

static void CALLBACK ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv);
static void CALLBACK ServiceHandler(DWORD fdwControl);
static char* basename(const char* str);
static LPCTSTR GetErrorString();
static void systray(void*);

BOOL NotifySCM(DWORD dwState, DWORD dwWin32ExitCode, DWORD dwProgress);

static DWORD   g_dwCurrentState;
static SERVICE_STATUS_HANDLE  g_hService;
extern bool g_bStop;
bool g_bTestMode = false;
static int lockserver_port = 2402;
static int local_only = 1;

CVSNT_EXPORT int main(int argc, char* argv[])
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
                        "\t%s -test\tInteractive run\n"
                        "\t%s -v\tReport version number\n"
						"\t%s -systray\tShow in system tray",
                        basename(argv[0]),basename(argv[0]),
                        basename(argv[0]),basename(argv[0]), 
						basename(argv[0])
                        );
		return -1;
	}

    if (!strcmp(argv[1],"-v"))
	{
        puts(NTSERVICE_VERSION_STRING);
        return 0;
    }

	if(!strcmp(argv[1],"-i"))
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
		CServerIo::log(CServerIo::logNotice,"%s",DISPLAY_NAME " installed successfully");
		printf(DISPLAY_NAME " installed successfully\n");
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
		CServerIo::log(CServerIo::logNotice,"%s",DISPLAY_NAME " uninstalled successfully");
		printf(DISPLAY_NAME " uninstalled successfully\n");
	}	
	else if(!strcmp(argv[1],"-test"))
	{
		ServiceMain(999,NULL);
	}
	else if(!strcmp(argv[1],"-systray"))
	{
		SetCurrentDirectory("C:\\");
		_beginthread(systray,0,NULL);
		ServiceMain(998,NULL);
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

	HANDLE hSem = CreateSemaphore(NULL,0,1,"CVSNT_Lockserver");

	if(dwArgc<997)
	{
		if (!(g_hService = RegisterServiceCtrlHandler(SERVICE_NAME,ServiceHandler))) 
		{
			CServerIo::log(CServerIo::logError,"Unable to start "SERVICE_NAME" - RegisterServiceCtrlHandler failed"); 
			return; 
		}
		NotifySCM(SERVICE_START_PENDING, 0, seq++);
	}
	else
	{
		if(dwArgc==999)
		{
			g_bTestMode=TRUE;
			printf(SERVICE_NAME" " CVSNT_PRODUCTVERSION_STRING " ("__DATE__") starting in test mode.\n");
		}
	}

	{
		CSocketIO s;
		if(!s.init())
		{
			CServerIo::log(CServerIo::logError,"Socket startup failed: %s\n",s.error());
			if(!g_bTestMode)
				NotifySCM(SERVICE_STOPPED,0,0);
			return;
		}
	}

	if(!RegOpenKeyEx(HKEY_LOCAL_MACHINE,"Software\\CVS\\Pserver",NULL,KEY_QUERY_VALUE,&hk))
	{
		dwTmp=sizeof(szTmp);
		if(!RegQueryValueEx(hk,"LockServer",NULL,&dwType,(BYTE*)szTmp,&dwTmp))
		{
			char *p = strchr(szTmp,':');
			if(p)
				lockserver_port=atoi(p+1);
		}
		dwTmp=sizeof(szTmp);
		if(!RegQueryValueEx(hk,"LockServerLocal",NULL,&dwType,(BYTE*)szTmp,&dwTmp))
		{
			if(dwType==REG_DWORD)
				local_only = *(DWORD*)szTmp;
		}
		RegCloseKey(hk);
	}

    run_server(lockserver_port, seq, local_only);

	CloseHandle(hSem);

	if(!g_bTestMode)
		NotifySCM(SERVICE_STOPPED, 0, 0);
	CServerIo::log(CServerIo::logNotice,SERVICE_NAME" stopped successfully");
}

static void ShowContextMenu(HWND hWnd)
{
	HINSTANCE hInst = (HINSTANCE)GetModuleHandle(NULL);
	HMENU hMenu;
	POINT pt;

	hMenu = LoadMenu(hInst,MAKEINTRESOURCE(IDR_MENU1));
	SetForegroundWindow(hWnd);
	GetCursorPos(&pt);
	HMENU hSubMenu = GetSubMenu(hMenu,0);
	TrackPopupMenu(hSubMenu,TPM_BOTTOMALIGN|TPM_CENTERALIGN|TPM_LEFTBUTTON,pt.x,pt.y,NULL,hWnd,NULL);
}

static LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case TRAY_MESSAGE:
		switch(lParam)
		{
		case WM_LBUTTONDBLCLK:
			break;
		case WM_RBUTTONDOWN:
		case WM_CONTEXTMENU:
			ShowContextMenu(hWnd);
			break;
		}
		break;
	case WM_COMMAND:
		switch(wParam)
		{
		case ID_QUIT:
			PostQuitMessage(0);
			break;
		case ID_SHOWDEBUG:
			{
				char newtitle[1024];
				HWND hConsole;

				/* Can't use GetConsoleWindow otherwise we lose Win9x support */
				_snprintf(newtitle,sizeof(newtitle),"foo_%08x",GetCurrentProcessId());
				SetConsoleTitle(newtitle);
				do
				{
					Sleep(40);
					hConsole = FindWindow(NULL,newtitle);
				} while(!hConsole);
				SetConsoleTitle("CVSNT Lockserver debug window");

				/* For some reason this doesn't 'take' the first time around, so
				   we do it twice */
				g_bTestMode = true;
				ShowWindow(hConsole,SW_SHOW);
				Sleep(40);
				ShowWindow(hConsole,SW_SHOW);
				break;
			}
		}
		break;
	}

	return DefWindowProc(hWnd,uMsg,wParam,lParam);
}

void systray(void*)
{
	NOTIFYICONDATA nid = { sizeof(NOTIFYICONDATA) };
	HINSTANCE hInst = (HINSTANCE)GetModuleHandle(NULL);

	WNDCLASS wc = {0};
	wc.lpfnWndProc=TrayWndProc;
	wc.lpszClassName="systray";
	wc.hInstance=hInst;
	if(!RegisterClass(&wc))
		return;

	HWND hWnd = CreateWindowEx(0,"systray",0,WS_OVERLAPPED,1,1,1,1,NULL,NULL,hInst,NULL);
	if(!hWnd)
		return;

	nid.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;
	nid.uCallbackMessage=TRAY_MESSAGE;
	nid.hIcon=LoadIcon(hInst,MAKEINTRESOURCE(IDI_CVSLOCK));
	nid.uID=IDI_CVSLOCK;
	_tcscpy(nid.szTip,_T("CVSNT Lock server"));
	nid.hWnd=hWnd;
	Shell_NotifyIcon(NIM_ADD,&nid);

	MSG msg;
	while(GetMessage(&msg,NULL,0,0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	g_bStop = TRUE;
	Shell_NotifyIcon(NIM_DELETE,&nid);

	/* Forcibly terminate the app.  Something deep inside the Windows API falls over if you return
	   from a GUI thread...  Not worth bothering about for this small app */
	ExitProcess(0);
}

