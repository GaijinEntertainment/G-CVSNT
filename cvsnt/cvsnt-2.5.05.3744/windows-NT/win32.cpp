/*
 * win32.c
 * - utility functions for cvs under win32
 *
 */

#include "cvs.h"

#define WIN32_LEAN_AND_MEAN
#include "commctrl.h"
#include <windows.h>
#include <shellapi.h>
#include <lm.h>
#include <lmcons.h>
#include <winsock2.h>
#include <dbghelp.h>
#include <objbase.h>
#include <ntdsapi.h>
#include <dsgetdc.h>
#include <wininet.h>
#include <winternl.h>
#define _NTDEF_
#include <ntsecapi.h>
#include <Accctrl.h>
#include <aclapi.h>

#include "resource.h"

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Missing from win32 status, and ntstatus conflicts!
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_NONEXISTENT_EA_ENTRY ((NTSTATUS)0xC0000051L)
#define STATUS_NO_EAS_ON_FILE ((NTSTATUS)0xC0000052L)
#define STATUS_EA_CORRUPT_ERROR ((NTSTATUS)0xC0000053L)

#include "sid.h"
static MAKE_SID1(sidEveryone, 1, 0);

static BOOL g_bSmall; /* Small dump file */
static BOOL g_bContributeHours; /* Will Contribute Hours */

int win32_global_codepage = CP_UTF8;

/* MS BUG:  DNLEN hasn't been maintained so when you're on a legacy-free win2k domain
    you can apparently get a domain that's >DNLEN in size */
#undef DNLEN
#define DNLEN 256

#include "cvs.h"

#include "../version.h"
#include "../version_no.h"

#ifdef SERVER_SUPPORT
void nt_setuid_init();
int nt_setuid(LPCWSTR szMachine, LPCWSTR szUser, HANDLE *phToken);
int nt_s4u(LPCWSTR wszMachine, LPCWSTR wszUser, HANDLE *phToken);
#include "setuid/libsuid/suid.h"
#endif

/* Try to work out if this is a GMT FS.  There is probably
   a way of doing this that works for all FS's, but this
   detects FAT at least */
#define GMT_FS(_s) (!_tcsstr(_s,_T("FAT")))

ITypeLib *myTypeLib;

static const char *current_username=NULL;

#ifdef SERVER_SUPPORT
static void (*thread_exit_handler)(int);
#endif

static int win32_errno;

static struct { DWORD err; int dos;} errors[] =
{
        {  ERROR_INVALID_FUNCTION,       EINVAL    },  /* 1 */
        {  ERROR_FILE_NOT_FOUND,         ENOENT    },  /* 2 */
        {  ERROR_PATH_NOT_FOUND,         ENOENT    },  /* 3 */
        {  ERROR_TOO_MANY_OPEN_FILES,    EMFILE    },  /* 4 */
        {  ERROR_ACCESS_DENIED,          EACCES    },  /* 5 */
        {  ERROR_INVALID_HANDLE,         EBADF     },  /* 6 */
        {  ERROR_ARENA_TRASHED,          ENOMEM    },  /* 7 */
        {  ERROR_NOT_ENOUGH_MEMORY,      ENOMEM    },  /* 8 */
        {  ERROR_INVALID_BLOCK,          ENOMEM    },  /* 9 */
        {  ERROR_BAD_ENVIRONMENT,        E2BIG     },  /* 10 */
        {  ERROR_BAD_FORMAT,             ENOEXEC   },  /* 11 */
        {  ERROR_INVALID_ACCESS,         EINVAL    },  /* 12 */
        {  ERROR_INVALID_DATA,           EINVAL    },  /* 13 */
        {  ERROR_INVALID_DRIVE,          ENOENT    },  /* 15 */
        {  ERROR_CURRENT_DIRECTORY,      EACCES    },  /* 16 */
        {  ERROR_NOT_SAME_DEVICE,        EXDEV     },  /* 17 */
        {  ERROR_NO_MORE_FILES,          ENOENT    },  /* 18 */
        {  ERROR_LOCK_VIOLATION,         EACCES    },  /* 33 */
        {  ERROR_BAD_NETPATH,            ENOENT    },  /* 53 */
        {  ERROR_NETWORK_ACCESS_DENIED,  EACCES    },  /* 65 */
        {  ERROR_BAD_NET_NAME,           ENOENT    },  /* 67 */
        {  ERROR_FILE_EXISTS,            EEXIST    },  /* 80 */
        {  ERROR_CANNOT_MAKE,            EACCES    },  /* 82 */
        {  ERROR_FAIL_I24,               EACCES    },  /* 83 */
        {  ERROR_INVALID_PARAMETER,      EINVAL    },  /* 87 */
        {  ERROR_NO_PROC_SLOTS,          EAGAIN    },  /* 89 */
        {  ERROR_DRIVE_LOCKED,           EACCES    },  /* 108 */
        {  ERROR_BROKEN_PIPE,            EPIPE     },  /* 109 */
        {  ERROR_DISK_FULL,              ENOSPC    },  /* 112 */
        {  ERROR_INVALID_TARGET_HANDLE,  EBADF     },  /* 114 */
        {  ERROR_INVALID_HANDLE,         EINVAL    },  /* 124 */
        {  ERROR_WAIT_NO_CHILDREN,       ECHILD    },  /* 128 */
        {  ERROR_CHILD_NOT_COMPLETE,     ECHILD    },  /* 129 */
        {  ERROR_DIRECT_ACCESS_HANDLE,   EBADF     },  /* 130 */
        {  ERROR_NEGATIVE_SEEK,          EINVAL    },  /* 131 */
        {  ERROR_SEEK_ON_DEVICE,         EACCES    },  /* 132 */
        {  ERROR_DIR_NOT_EMPTY,          ENOTEMPTY },  /* 145 */
        {  ERROR_NOT_LOCKED,             EACCES    },  /* 158 */
        {  ERROR_BAD_PATHNAME,           ENOENT    },  /* 161 */
        {  ERROR_MAX_THRDS_REACHED,      EAGAIN    },  /* 164 */
        {  ERROR_LOCK_FAILED,            EACCES    },  /* 167 */
        {  ERROR_ALREADY_EXISTS,         EEXIST    },  /* 183 */
        {  ERROR_FILENAME_EXCED_RANGE,   ENOENT    },  /* 206 */
        {  ERROR_NESTING_NOT_ALLOWED,    EAGAIN    },  /* 215 */
        {  ERROR_NOT_ENOUGH_QUOTA,       ENOMEM    }    /* 1816 */
};

#if 0
#define IOINFO_L2E          5
#define IOINFO_ARRAY_ELTS   (1 << IOINFO_L2E)

typedef struct {
        long osfhnd;
        char osfile;
        char pipech;
#ifdef _MT
        int lockinitflag;
        CRITICAL_SECTION lock;
#endif  /* _MT */
    }   ioinfo;


#define _pioinfo(i) ( __pioinfo[(i) >> IOINFO_L2E] + ((i) & (IOINFO_ARRAY_ELTS - 1)) )
#define _osfile(i)  ( _pioinfo(i)->osfile )
extern "C" __declspec(dllimport) ioinfo * __pioinfo[];

#define FOPEN           0x01    /* file handle open */
#define FEOFLAG         0x02    /* end of file has been encountered */
#define FCRLF           0x04    /* CR-LF across read buffer (in text mode) */
#define FPIPE           0x08    /* file handle refers to a pipe */
#define FNOINHERIT      0x10    /* file handle opened _O_NOINHERIT */
#define FAPPEND         0x20    /* file handle opened O_APPEND */
#define FDEV            0x40    /* file handle refers to device */
#define FTEXT           0x80    /* file handle is in text mode */
#endif

static const WORD month_len [12] =
{
    31, /* Jan */
    28, /* Feb */
    31, /* Mar */
    30, /* Apr */
    31, /* May */
    30, /* Jun */
    31, /* Jul */
    31, /* Aug */
    30, /* Sep */
    31, /* Oct */
    30, /* Nov */
    31  /* Dec */
};

/* One second = 10,000,000 * 100 nsec */
static const ULONGLONG systemtime_second = 10000000L;

/* Default domain name */
static char g_defaultdomain[DNLEN+1];

// IsDomainMember patch from John Anderson <panic@semiosix.com>
int isDomainMember(char *szDomain);

static LONG WINAPI MiniDumper(PEXCEPTION_POINTERS pExceptionInfo);

static int use_ntsec, use_ntea;

static int tcp_init()
{
    WSADATA data;

    if (WSAStartup (MAKEWORD (1, 1), &data))
    {
		fprintf (stderr, "cvs: unable to initialize winsock\n");
		return -1;
    }
	return 0;
}

static int tcp_close()
{
    WSACleanup ();
	return 0;
}

static BOOL WINAPI FreeDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	TCHAR tmp[64];
	static int nCountdown;
	static UINT_PTR nTimer;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		CheckRadioButton(hWnd,IDC_FREEOPT1,IDC_FREEOPT3,IDC_FREEOPT1);
		SetTimer(hWnd,nTimer,1000,NULL);
		nCountdown=30;
		_sntprintf(tmp,sizeof(tmp),_T("OK - %d"),nCountdown);
		SetDlgItemText(hWnd,IDOK,tmp);
		break;
	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code)
		{
			case NM_CLICK:
			case NM_RETURN:
			switch(LOWORD(wParam))
			{
			// http://www.ohloh.net/p/cvsnt
			case IDC_SYSLINK1:
				ShellExecute(NULL,_T("open"),_T("http://www.ohloh.net/p/cvsnt"),NULL,NULL,SW_SHOWNORMAL);
				break;
			// http://www.gnu.org/philosophy/free-sw.html
			case IDC_SYSLINK2:
				ShellExecute(NULL,_T("open"),_T("http://www.gnu.org/philosophy/free-sw.html"),NULL,NULL,SW_SHOWNORMAL);
				break;
			// http://store.march-hare.com/s.nl/sc.2/category.2/.f
			case IDC_SYSLINK3:
				ShellExecute(NULL,_T("open"),_T("http://store.march-hare.com/s.nl/sc.2/category.2/.f"),NULL,NULL,SW_SHOWNORMAL);
				break;
			// http://march-hare.com/cvsnt/en.asp
			case IDC_SYSLINK4:
				ShellExecute(NULL,_T("open"),_T("http://march-hare.com/cvsnt/en.asp"),NULL,NULL,SW_SHOWNORMAL);
				break;
			// http://march-hare.com/cvspro/?lang=EN#downcmserver
			case IDC_SYSLINK5:
				ShellExecute(NULL,_T("open"),_T("http://march-hare.com/cvspro/?lang=EN#downcmserver"),NULL,NULL,SW_SHOWNORMAL);
				break;
			// http://march-hare.com/cvspro/?lang=EN#downcvsnt
			case IDC_SYSLINK6:
				ShellExecute(NULL,_T("open"),_T("http://march-hare.com/cvspro/?lang=EN#downcvsnt"),NULL,NULL,SW_SHOWNORMAL);
				break;
			}
			default:
				break;
		}
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
			case IDOK:
			case IDCANCEL:
				g_bContributeHours=IsDlgButtonChecked(hWnd,IDC_FREEOPT3)?FALSE:TRUE;
				EndDialog(hWnd,LOWORD(wParam));
		}
		break;
	case WM_TIMER:
		nCountdown--;
		if(!nCountdown)
		{
			KillTimer(hWnd,nTimer);
			PostMessage(hWnd,WM_COMMAND,IDOK,(LPARAM)NULL);
		}
		_sntprintf(tmp,sizeof(tmp),_T("OK - %d"),nCountdown);
		SetDlgItemText(hWnd,IDOK,tmp);
		break;
	case WM_CLOSE:
		g_bContributeHours=IsDlgButtonChecked(hWnd,IDC_FREEOPT3)?FALSE:TRUE;
		break;
	}

	return FALSE;
}

void win32init(int serv_active)
{
	LONGINT lastAdvert=11, advertInterval;
	INT_PTR dlgres;
	time_t nextAdvert,now;
	HWND dlghWnd;
	INITCOMMONCONTROLSEX icex;

	CLibraryAccess::VerifyTrust(NULL,true); // checks the .exe not the .dll
	CLibraryAccess::VerifyTrust("cvsapi",true);
	CLibraryAccess::VerifyTrust("cvstools",true);
	SetThreadLocale(LOCALE_SYSTEM_DEFAULT);
	g_bContributeHours=FALSE;
	icex.dwSize=sizeof(icex);
	icex.dwICC=ICC_STANDARD_CLASSES|ICC_LINK_CLASS;
	if(serv_active<0)
	{
		_tzset(); // Set the timezone from the system, or 'TZ' if specified.

		SetFileApisToANSI();

		CoInitialize(NULL);
		InitCommonControlsEx(&icex);

#if !defined(_DEBUG)
		SetUnhandledExceptionFilter(MiniDumper);
#endif
		tcp_init();

#ifdef SERVER_SUPPORT
		nt_setuid_init();
#endif
	}
	else
	{
		char buffer[1024];
		DWORD dwLen;

		/* By default we run case insensitive.  This can be switched
		dynamically at server startup or if the client starts in
		a case sensitive directory */
		__set_case_sensitive(0);

#ifdef SERVER_SUPPORT

		if(serv_active && !CGlobalSettings::GetGlobalValue("cvsnt","PServer","CaseSensitive",buffer,sizeof(buffer)))
			__set_case_sensitive(atoi(buffer));
#endif

		*g_defaultdomain='\0';
		if(serv_active)
		{
			CGlobalSettings::GetGlobalValue("cvsnt","PServer","DefaultDomain",g_defaultdomain,sizeof(g_defaultdomain));
			if(!*g_defaultdomain)
			{
				if(!isDomainMember(g_defaultdomain))
					*g_defaultdomain='\0';
			}
		}

		CFileAccess::Win32SetUtf8Mode(win32_global_codepage==CP_UTF8);
		SetConsoleOutputCP(win32_global_codepage);

		if(serv_active)
		{
			if(!*g_defaultdomain)
				TRACE(3,"CVS Server is acting as standalone");
			else
				TRACE(3,"CVS Server is acting as member of domain '%s'",g_defaultdomain);
		}

		if(!*g_defaultdomain)
		{
			dwLen=sizeof(g_defaultdomain);
			GetComputerNameA(g_defaultdomain, &dwLen);
		}


		/* Cygwin have deprecated ntea, and ntsec somewat sucks.. also they've removed
		   the CYGWIN variable.. we keep using ntea so we can tunnel the executable
		   bit from Unix, but if there is an alternative found then we'll just drop cygwin
		   compat. altogether. */
		use_ntsec = 0;
		use_ntea = 1;

		if(!serv_active)
		{
			/* Don't read this in server mode */

			if(GetEnvironmentVariableA("CYGWIN",buffer,sizeof(buffer)))
			{
				if(strstr(buffer,"ntsec"))
				{
					use_ntsec = !strstr(buffer,"nontsec");
					if(use_ntsec) use_ntea=0;
				}
				if(strstr(buffer,"ntea"))
				{
					use_ntea = !strstr(buffer,"nontea");
					if(use_ntea) use_ntsec=0;
				}
			}

			if(GetEnvironmentVariableA("CVSNT",buffer,sizeof(buffer)))
			{
				if(strstr(buffer,"ntsec"))
				{
					use_ntsec = !strstr(buffer,"nontsec");
					if(use_ntsec) use_ntea=0;
				}
				if(strstr(buffer,"ntea"))
				{
					use_ntea = !strstr(buffer,"nontea");
					if(use_ntea) use_ntsec=0;
				}
			}

#if (CVSNT_SPECIAL_BUILD_FLAG != 0)
			if (strcasecmp(CVSNT_SPECIAL_BUILD,"Suite")!=0)
#endif
			{
			  if (CGlobalSettings::GetUserValue("cvsnt","cvsadvert","LastAdvert",lastAdvert))
				lastAdvert=0;
			  if (CGlobalSettings::GetUserValue("cvsnt","cvsadvert","AdvertInterval",advertInterval))
				advertInterval=86400; // 1 day
			  if ((advertInterval>115200)||(advertInterval<100)) // 1 week max
				advertInterval=86400; // 1 day

			  time(&now);

			  nextAdvert = lastAdvert + advertInterval;
			  if (nextAdvert > now + 115200) // 1 day + 8 hours
				nextAdvert = now;

			  if((nextAdvert <= now))
			  {
				HMODULE hModule;
				TCHAR InstallPath[1024];
				GetModuleFileName(NULL,InstallPath,sizeof(InstallPath));
				if(!_tcsicmp(InstallPath+_tcslen(InstallPath)-4,_T(".exe")))
					_tcscpy(InstallPath+_tcslen(InstallPath)-4,_T(".dll"));
				else
					_tcscat(InstallPath,_T(".dll"));
				hModule = GetModuleHandle(InstallPath);
				if (hModule==NULL)
					hModule = GetModuleHandle(_T("cvsnt.dll"));
				if (hModule!=NULL)
				{
					dlghWnd=GetTopWindow(NULL);
					if (dlghWnd==NULL)
						dlghWnd=GetForegroundWindow();
					dlgres = DialogBox(hModule,MAKEINTRESOURCE(IDD_FREEDIALOG),dlghWnd,FreeDlgProc);
					if (dlgres <= 0)
					{
						if (dlgres < 0)
						{
							win32_errno=GetLastError();
							LPVOID buf=NULL;
							FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
										NULL,win32_errno,0,(LPTSTR)&buf,0,NULL);
							if (win32_errno!=0)
								MessageBox(NULL,(LPCWSTR)buf,_T("Error displaying advert"),MB_ICONWARNING);
							LocalFree((HLOCAL)buf);
						}
						else
							MessageBox(NULL,_T("hWndParent parameter is invalid"),_T("Error displaying advert"),MB_ICONWARNING);
					}
					else
						CGlobalSettings::SetUserValue("cvsnt","cvsadvert","LastAdvert",now);
				}
			  }
			}
		}
	}
}

void wnt_cleanup (void)
{
	if(!server_active)
		tcp_close();

	if(current_username)
		free((void*)current_username);
}

bool validate_filename(const char *path, bool warn)
{
	static const char *reserved_names[] = { "CON", "AUX", "COM1", "COM2", "COM3", "COM4", "LPT1", "LPT2", "LPT3", "PRN", "NUL", NULL };
	const char *p, *q;
	int l,r;

	for(p=path+strlen(path)-1;p>=path;--p)
		if(ISDIRSEP(*p))
			break;
	p++;
	q=strchr(p,'.');
	if(!q) q=p+strlen(p);
	l=q-p;
	if(l==3 || l==4)
	{
		for(r=0; reserved_names[r]; r++)
		{
			if(l==strlen(reserved_names[r]) && !strnicmp(reserved_names[r],p,l))
			{
				error(0,0,"Cannot use reserved filename '%s' on NT based machines",p);
				errno=EBADF;
				return false;
			}
		}
	}

	if(strpbrk(p,"\"<>|"))
	{
		if(warn && !quiet)
			error(0,0,"Filename '%s' is illegal on this system.",p);
		errno=EBADF;
		return false;
	}
	return true;
}

unsigned sleep(unsigned seconds)
{
	Sleep(1000*seconds);
	return 0;
}

/*
 * Sleep at least useconds microseconds.
 */
int usleep(unsigned long useconds)
{
    /* Not very accurate, but it gets the job done */
	if (useconds > 0)
    {
      unsigned long millis = ((useconds + 999UL) / 1000UL);
      Sleep (millis);
    }
	return 0;
}

DWORD BreakNameIntoParts(LPCTSTR name, LPWSTR w_name, LPWSTR w_domain, LPWSTR w_pdc)
{
	static wchar_t *pw_pdc;
    TCHAR *ptr;
	wchar_t w_computer[DNLEN+1];

	ptr=(TCHAR*)_tcschr(name, '\\');
  	if (ptr)
	{
#ifdef _UNICODE
		_tcscpy(w_name,ptr+1);
		_tcsncpy(w_domain,name,ptr-name);
		w_domain[ptr-name]='\0';
#else
 		w_name[MultiByteToWideChar(win32_global_codepage,0,ptr+1,-1,w_name,UNLEN+1)]='\0';
		w_domain[MultiByteToWideChar(win32_global_codepage,0,name,ptr-name,w_domain,DNLEN)]='\0';
#endif
   	}
	else
	{
#ifdef _UNICODE
		_tcscpy(w_name,name);
#else
 		w_name[MultiByteToWideChar(win32_global_codepage,0,name,-1,w_name,UNLEN+1)]='\0';
#endif
		w_domain[MultiByteToWideChar(win32_global_codepage,0,g_defaultdomain,-1,w_domain,DNLEN+1)]='\0';
	}

	if(w_pdc)
	{
		DWORD dwLen;
		typedef DWORD (WINAPI *DsGetDcNameW_t)(LPCWSTR ComputerName,LPCWSTR DomainName,GUID *DomainGuid,LPCWSTR SiteName,ULONG Flags,PDOMAIN_CONTROLLER_INFOW *DomainControllerInfo);
		DsGetDcNameW_t pDsGetDcNameW;
		TRACE(3,"Find netapi32.dll with the symbol DsGetDcNameW");
		pDsGetDcNameW=(DsGetDcNameW_t)GetProcAddress(GetModuleHandle(_T("netapi32")),"DsGetDcNameW");

		w_pdc[0]='\0';
		dwLen = sizeof(w_computer);
		TRACE(3,"Call GetComputerNameW");
		GetComputerNameW(w_computer, &dwLen);
		if(w_domain[0] && wcscmp(w_domain,w_computer))
		{
			if(pDsGetDcNameW)
			{
				PDOMAIN_CONTROLLER_INFOW pdi;

				TRACE(3,"Call DsGetDcNameW from netapi32.dll");
				if(!pDsGetDcNameW(NULL,w_domain,NULL,NULL,DS_IS_FLAT_NAME,&pdi) || !pDsGetDcNameW(NULL,w_domain,NULL,NULL,DS_IS_DNS_NAME,&pdi))
				{
					wcscpy(w_pdc,pdi->DomainControllerName);
					NetApiBufferFree(pdi);
				}
			}
			else
			{
				TRACE(3,"Call NetGetAnyDCName because we could not find netapi32.dll with the symbol DsGetDcNameW");
				if(!NetGetAnyDCName(NULL,w_domain,(LPBYTE*)&pw_pdc) || !NetGetDCName(NULL,w_domain,(LPBYTE*)&pw_pdc))
				{
					wcscpy(w_pdc,pw_pdc);
					NetApiBufferFree(pw_pdc);
				}
			}
		}

		TRACE(3,"Authenticating server: %S",w_pdc[0]?w_pdc:L"(local)");
	}
	return ERROR_SUCCESS;
}

#ifdef SERVER_SUPPORT
int win32_valid_user(const char *username, const char *password, const char *domain, void **user_token)
{
	HANDLE handle = NULL;
	TCHAR User[UNLEN+1] = {0};
	TCHAR Domain[DNLEN+1] = {0};
	TCHAR Password[UNLEN+1] = {0};
	TCHAR user[UNLEN+DNLEN+2] = {0};

#ifdef _UNICODE
	if(domain)
	{
		MultiByteToWideChar(win32_global_codepage,0,domain,strlen(domain)+1,user,sizeof(user)/sizeof(user[0]));
		_tcscat(user,_T("\\"));
	}
	MultiByteToWideChar(win32_global_codepage,0,username,strlen(username)+1,user+_tcslen(user),sizeof(user)/sizeof(user[0])-_tcslen(user));
#else
	if(domain)
		_snprintf(user,sizeof(user)/sizeof(user[0]),_T("%s\\%s"),domain,username);
	else
		strcpy(user,username);
#endif

	if(BreakNameIntoParts(user, User, Domain, NULL))
		return 0;

	Password[MultiByteToWideChar(win32_global_codepage,0, password, -1, Password, (sizeof(Password)/sizeof(TCHAR))-1)]='\0';

	if(!LogonUserW(User,Domain,Password,LOGON32_LOGON_NETWORK,LOGON32_PROVIDER_DEFAULT,&handle))
	{
		switch(GetLastError())
		{
			case ERROR_SUCCESS:
				break;
			case ERROR_NO_SUCH_DOMAIN:
			case ERROR_NO_SUCH_USER:
			case ERROR_ACCOUNT_DISABLED:
			case ERROR_PASSWORD_EXPIRED:
			case ERROR_ACCOUNT_RESTRICTION:
				/* Do we want to print something here? */
				TRACE(3,"LogonUser() error %08x",GetLastError());
				break;
			case ERROR_PRIVILEGE_NOT_HELD:
			case ERROR_ACCESS_DENIED:
				/* Technically can't happen (due to check for seTcbName above) */
				cvs_outerr("error 0 Cannot login: Server has insufficient rights to validate user account - contact your system administrator\n",0);
				cvs_flusherr();
				TRACE(3,"LogonUser() error %08x",GetLastError());
				break;
			default:
				TRACE(3,"LogonUser() error %08x",GetLastError());
				break;
		}
	}
	else
	{
		if(user_token)
			*user_token = handle;
		else
			CloseHandle(handle);
	}
	return handle?1:0;
}
#endif

struct passwd *win32getpwnam(const char *name)
{
#ifdef WINNT_VERSION // Win95 doesn't support this...  Client only mode...
	static struct passwd pw;
	USER_INFO_1 *pinfo = NULL;
	static wchar_t w_name[UNLEN+1];
	static wchar_t w_domain[DNLEN+1];
	static wchar_t w_pdc[DNLEN+1];
	static char homedir[1024];
	NET_API_STATUS res;

	memset(&pw,0,sizeof(pw));

	TRACE(3,"win32getpwnam(%s)",name);

#ifdef _UNICODE
	{
		wchar_t wn[UNLEN+1];
		MultiByteToWideChar(win32_global_codepage,0,name,strlen(name)+1,wn,sizeof(wn)/sizeof(wn[0]));
		BreakNameIntoParts(wn,w_name,w_domain,w_pdc);
	}
#else
	BreakNameIntoParts(name,w_name,w_domain,w_pdc);
#endif

	// If by sad chance this gets called on Win95, there is
	// no way of verifying user info, and you always get 'Not implemented'.
	// We are stuck in this case...
	res=NetUserGetInfo(w_pdc,w_name,1,(BYTE**)&pinfo);
	if(res==NERR_UserNotFound)
	{
		TRACE(3,"NetUserGetInfo returned NERR_UserNotFound - failing");
		return NULL;
	}

	pw.pw_uid=0;
	pw.pw_gid=0;
	pw.pw_name=name;
	pw.pw_passwd="secret";
	pw.pw_shell="cmd.exe";
	pw.pw_pdc_w=w_pdc;
	pw.pw_domain_w=w_domain;
	pw.pw_name_w = w_name;

	if(res==NERR_Success)
	{
		WideCharToMultiByte(win32_global_codepage,0,pinfo->usri1_home_dir,-1,homedir,sizeof(homedir),NULL,NULL);
		if(*homedir) pw.pw_dir=homedir;
	}
	else
		pw.pw_dir=get_homedir();

	if(pinfo)
		NetApiBufferFree(pinfo);
	return &pw;
#else // Win95 broken version.  Rely on the HOME environment variable...
	static struct passwd pw = {0};
	pw.pw_name=(char*)name;
	pw.pw_passwd="secret";
	pw.pw_shell="cmd.exe";
	pw.pw_dir=get_homedir();
#endif
	return &pw;
}

char *win32getlogin()
{
	static char UserName[UNLEN+1];
	DWORD len=sizeof(UserName);

	if(!GetUserNameA(UserName,&len))
		return NULL;

	/* Patch for cygwin sshd suggested by Markus Kuehni */
	if(!strcmp(UserName,"SYSTEM"))
	{
		/* First try logname, and if that fails try user */
		if(!GetEnvironmentVariableA("LOGNAME",UserName,sizeof(UserName)))
			GetEnvironmentVariableA("USER",UserName,sizeof(UserName));
		if(!*UserName)
			strcpy(UserName,"unknown");
	}
	return UserName;
}

void win32flush(int fd)
{
	FlushFileBuffers((HANDLE)_get_osfhandle(fd));
}

void win32setblock(int fd, int block)
{
	DWORD mode = block?PIPE_WAIT:PIPE_NOWAIT;
	SetNamedPipeHandleState((HANDLE)_get_osfhandle(fd),&mode,NULL,NULL);
}

#if defined(SERVER_SUPPORT)
int win32setuser(const char *username)
{
	/* Store the user for later */
	current_username = xstrdup(username);
	return 0;
}

int trys4u(const struct passwd *pw, HANDLE *user_handle)
{
	DWORD dwErr;
	/* XP actually implements this but drops out early in the processing
		because it's in workstation mode not server */
	/* Also for this to succeed your PDC needs to be a Win2k3 machine */
	TRACE(3,"Trying S4u...");
	dwErr = nt_s4u(pw->pw_domain_w,pw->pw_name_w,user_handle);
	TRACE(3,"S4U login returned %08x",dwErr);
	switch(dwErr)
	{
		case ERROR_SUCCESS:
			TRACE(3,"S4U login returned ERROR_SUCCESS",dwErr);
			return 0;
		case ERROR_NO_SUCH_DOMAIN:
			TRACE(3,"S4U login returned ERROR_NO_SUCH_DOMAIN",dwErr);
			return 4;
		case ERROR_NO_SUCH_USER:
			TRACE(3,"S4U login returned ERROR_NO_SUCH_USER",dwErr);
			return 2;
		case ERROR_ACCOUNT_DISABLED:
		case ERROR_PASSWORD_EXPIRED:
		case ERROR_ACCOUNT_RESTRICTION:
			if (dwErr==ERROR_ACCOUNT_DISABLED)
				TRACE(3,"S4U login returned ERROR_ACCOUNT_DISABLED",dwErr);
			if (dwErr==ERROR_PASSWORD_EXPIRED)
				TRACE(3,"S4U login returned ERROR_PASSWORD_EXPIRED",dwErr);
			if (dwErr==ERROR_ACCOUNT_RESTRICTION)
				TRACE(3,"S4U login returned ERROR_ACCOUNT_RESTRICTION",dwErr);
			return 3;
		case ERROR_PRIVILEGE_NOT_HELD:
		case ERROR_ACCESS_DENIED:
			if (dwErr==ERROR_PRIVILEGE_NOT_HELD)
				TRACE(3,"S4U login returned ERROR_PRIVILEGE_NOT_HELD",dwErr);
			if (dwErr==ERROR_ACCESS_DENIED)
				TRACE(3,"S4U login returned ERROR_ACCESS_DENIED",dwErr);
			return 1;
		default:
			TRACE(3,"S4U login default error.",dwErr);
			return -1;
	}
}

int trysuid(const struct passwd *pw, HANDLE *user_handle)
{
	DWORD dwErr;

	TRACE(3,"Trying Setuid helper... (%S\\%S)",pw->pw_domain_w,pw->pw_name_w);
	dwErr = SuidGetImpersonationTokenW(pw->pw_name_w,pw->pw_domain_w,LOGON32_LOGON_NETWORK,user_handle);
	TRACE(3,"SuidGetImpersonationToken returned %08x",dwErr);
	switch(dwErr)
	{
		case ERROR_SUCCESS:
			return 0;
		case ERROR_NO_SUCH_DOMAIN:
			return 4;
		case ERROR_NO_SUCH_USER:
			return 2;
		case ERROR_ACCOUNT_DISABLED:
		case ERROR_PASSWORD_EXPIRED:
		case ERROR_ACCOUNT_RESTRICTION:
			return 3;
		case ERROR_PRIVILEGE_NOT_HELD:
		case ERROR_ACCESS_DENIED:
			return 1;
		default:
			return -1;
	}
}

int trytoken(const struct passwd *pw, HANDLE *user_handle)
{
	TRACE(3,"Trying NTCreateToken...");
	if(nt_setuid(pw->pw_pdc_w,pw->pw_name_w,user_handle))
	{
		TRACE(3,"NTCreateToken failed");
		return 1;
	}
	TRACE(3,"NTCreateToken succeeded");
	return 0;
}
/*
  Returns:
     0 = ok
	 1 = user found, impersonation failed
	 2 = user not found
	 3 = Account disabled
*/

int win32switchtouser(const char *username, void *preauth_user_handle)
{
	HANDLE user_handle = NULL;
	int ret=-1;

	/* Store the user for later */
	current_username = xstrdup(username);

	TRACE(3,"win32switchtouser(%s)",username);

	if(preauth_user_handle)
	{
		TRACE(3,"Impersonating preauthenticated user credentials\n");
		return ImpersonateLoggedOnUser(preauth_user_handle)?0:1;
	}

	const struct passwd *pw;

	pw = win32getpwnam(username);

	if(!pw)
		return 2;

	if((ret = trys4u(pw,&user_handle))>0)
		return ret;
	if(ret && (ret = trysuid(pw,&user_handle))>0)
		return ret;
	if(ret && (ret = trytoken(pw,&user_handle))>0)
		return ret;
	if(ret<0)
		return 1;

	TRACE(3,"User verified - calling ImpersonateLoggedOnUser");
	/* If we haven't bailed out above, user_handle will be set */
	return ImpersonateLoggedOnUser(user_handle)?0:1;
}

#endif

char *win32getfileowner(const char *file)
{
	static char szName[64];

	/* This is called with a file called '#cvs.[rw]fl.{machine}({user}).pid */
	const char *p=file;
	const char *name=file;

	while(*p)
	{
		if(*p=='\\' || *p=='/')
			name=p+1;
		p++;
	}

	p=strchr(name,'(');
	if(!p)
		return "Unknown User";
	name=p+1;
	p=strchr(name,')');
	if(!p)
		return "Unknown User";
	strncpy(szName,name,p-name);
	szName[p-name]='\0';
	return szName;
}

/* NT won't let you delete a readonly file, even if you have write perms on the directory */
int wnt_unlink(const char *file)
{
	if(!CFileAccess::remove(file))
	{
		_dosmaperr(GetLastError());
		return -1;
	}
	return 0;
}

int wnt_chdir(const char *dir)
{
	if(!CDirectoryAccess::chdir(dir))
	{
		_dosmaperr(GetLastError());
		return -1;
	}
	return 0;
}

/* Based on fix by jmg.  Test whether user is an administrator & can do 'cvs admin' */

int win32_isadmin()
{
#ifndef WINNT_VERSION
	return 1; // Always succeed in the Win95 case...  The server should catch this in client/server, otherwise it's irrelevant.
#else
	/* If Impersonation is enabled, then check the local thread for administrator access,
	   otherwise do the domain checks, as before.  This code is basically Microsoft KB 118626  */
	/* Also use this if we're in client mode */
	if((!current_username || !stricmp(current_username,CVS_Username)))
	{
		BOOL   fReturn         = FALSE;
		DWORD  dwStatus;
		DWORD  dwAccessMask;
		DWORD  dwAccessDesired;
		DWORD  dwACLSize;
		DWORD  dwStructureSize = sizeof(PRIVILEGE_SET);
		PACL   pACL            = NULL;
		PSID   psidAdmin       = NULL;

		HANDLE hToken              = NULL;
		HANDLE hImpersonationToken = NULL;

		PRIVILEGE_SET   ps;
		GENERIC_MAPPING GenericMapping;

		PSECURITY_DESCRIPTOR     psdAdmin           = NULL;
		SID_IDENTIFIER_AUTHORITY SystemSidAuthority = SECURITY_NT_AUTHORITY;


   /* Determine if the current thread is running as a user that is a member of
      the local admins group.  To do this, create a security descriptor that
      has a DACL which has an ACE that allows only local aministrators access.
      Then, call AccessCheck with the current thread's token and the security
      descriptor.  It will say whether the user could access an object if it
      had that security descriptor.  Note: you do not need to actually create
      the object.  Just checking access against the security descriptor alone
      will be sufficient.
   */
		TRACE(2,"Determine if the current thread is running as a user that is a member of the local admins group (win32).");
		__try
		{
		/* AccessCheck() requires an impersonation token.  We first get a primary
			token and then create a duplicate impersonation token.  The
			impersonation token is not actually assigned to the thread, but is
			used in the call to AccessCheck.  Thus, this function itself never
			impersonates, but does use the identity of the thread.  If the thread
			was impersonating already, this function uses that impersonation context.
		*/
			if (!OpenThreadToken(GetCurrentThread(), TOKEN_DUPLICATE|TOKEN_QUERY, TRUE, &hToken))
			{
				if (GetLastError() != ERROR_NO_TOKEN)
					__leave;

				if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE|TOKEN_QUERY, &hToken))
					__leave;
			}

			if (!DuplicateToken (hToken, SecurityImpersonation, &hImpersonationToken))
				__leave;

		/*
			Create the binary representation of the well-known SID that
			represents the local administrators group.  Then create the security
			descriptor and DACL with an ACE that allows only local admins access.
			After that, perform the access check.  This will determine whether
			the current user is a local admin.
		*/
			if (!AllocateAndInitializeSid(&SystemSidAuthority, 2,
										SECURITY_BUILTIN_DOMAIN_RID,
										DOMAIN_ALIAS_RID_ADMINS,
										0, 0, 0, 0, 0, 0, &psidAdmin))
				__leave;

			psdAdmin = LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
			if (psdAdmin == NULL)
				__leave;

			if (!InitializeSecurityDescriptor(psdAdmin, SECURITY_DESCRIPTOR_REVISION))
				__leave;

			// Compute size needed for the ACL.
			dwACLSize = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(psidAdmin) - sizeof(DWORD);

			pACL = (PACL)LocalAlloc(LPTR, dwACLSize);
			if (pACL == NULL)
				__leave;

			if (!InitializeAcl(pACL, dwACLSize, ACL_REVISION2))
				__leave;

			dwAccessMask= ACCESS_READ | ACCESS_WRITE;

			if (!AddAccessAllowedAce(pACL, ACL_REVISION2, dwAccessMask, psidAdmin))
				__leave;

			if (!SetSecurityDescriptorDacl(psdAdmin, TRUE, pACL, FALSE))
				__leave;

		/* AccessCheck validates a security descriptor somewhat; set the group
			and owner so that enough of the security descriptor is filled out to
			make AccessCheck happy.
		*/
			SetSecurityDescriptorGroup(psdAdmin, psidAdmin, FALSE);
			SetSecurityDescriptorOwner(psdAdmin, psidAdmin, FALSE);

			if (!IsValidSecurityDescriptor(psdAdmin))
				__leave;

			dwAccessDesired = ACCESS_READ;

		/*
			Initialize GenericMapping structure even though you
			do not use generic rights.
		*/
			GenericMapping.GenericRead    = ACCESS_READ;
			GenericMapping.GenericWrite   = ACCESS_WRITE;
			GenericMapping.GenericExecute = 0;
			GenericMapping.GenericAll     = ACCESS_READ | ACCESS_WRITE;

			if (!AccessCheck(psdAdmin, hImpersonationToken, dwAccessDesired,
						&GenericMapping, &ps, &dwStructureSize, &dwStatus,
						&fReturn))
			{
				fReturn = FALSE;
				__leave;
			}
		}
		__finally
		{
			// Clean up.
			if (pACL) LocalFree(pACL);
			if (psdAdmin) LocalFree(psdAdmin);
			if (psidAdmin) FreeSid(psidAdmin);
			if (hImpersonationToken) CloseHandle (hImpersonationToken);
			if (hToken) CloseHandle (hToken);
		}
		return fReturn;
	}
	else
	{
		/* Contact the domain controller to decide whether the user is an adminstrator.  This
		   is about 90-95% accurate */
		USER_INFO_1 *info;
		wchar_t w_name[UNLEN+1];
		wchar_t w_domain[DNLEN+1];
		wchar_t w_pdc[DNLEN+1];
		DWORD priv;
		BOOL check_local = FALSE; /* If the domain wasn't specified, try checking for local admin too */

		TRACE(2,"Contact the domain controller to decide whether the user is an adminstrator.");
#ifdef _UNICODE
		{
			wchar_t wn[UNLEN+1];
			MultiByteToWideChar(win32_global_codepage,0,CVS_Username,strlen(CVS_Username)+1,wn,sizeof(wn)/sizeof(wn[0]));
			if(BreakNameIntoParts(wn,w_name,w_domain,w_pdc))
				return 0;
		}
#else
		if(BreakNameIntoParts(CVS_Username,w_name,w_domain,w_pdc))
			return 0;
#endif

		TRACE(3,"Call NetUserGetInfo.");
		if(NetUserGetInfo(w_pdc,w_name,1,(LPBYTE*)&info)!=NERR_Success)
			return 0;
		TRACE(3,"Call NetUserGetInfo complete.");

		priv=info->usri1_priv;
		NetApiBufferFree(info);
		if(priv==USER_PRIV_ADMIN)
			return 1;

		if(check_local)
		{
			/* No domain specified.  Check local admin privs for user.  This assumes the local
			usernames match the domain usernames, which isn't always the best thing to do,
			however people seem to want it that way. */
			TRACE(2,"No domain specified.  Check local admin privs for user.");
			if(NetUserGetInfo(NULL,w_name,1,(LPBYTE*)&info)!=NERR_Success)
				return 0;
			priv=info->usri1_priv;
			NetApiBufferFree(info);
			if(priv==USER_PRIV_ADMIN)
				return 1;
		}
		return 0;
	}
#endif // WINNT_VERSION
}

#ifdef WINNT_VERSION
/*
	 IsDomainMember patch from John Anderson <panic@semiosix.com>

     Find out whether the machine is a member of a domain or not
*/

// build LSA UNICODE strings.
void InitLsaString( PLSA_UNICODE_STRING LsaString, LPWSTR String )
{
    DWORD StringLength;
    if ( String == NULL )
     {
             LsaString->Buffer = NULL;
             LsaString->Length = 0;
             LsaString->MaximumLength = 0;
             return;
     }
     StringLength = wcslen(String);
     LsaString->Buffer = String;
     LsaString->Length = (USHORT) StringLength * sizeof(WCHAR);
     LsaString->MaximumLength=(USHORT)(StringLength+1) * sizeof(WCHAR);
}

NTSTATUS OpenPolicy( LPWSTR ServerName, DWORD DesiredAccess, PLSA_HANDLE PolicyHandle )
{
     LSA_OBJECT_ATTRIBUTES ObjectAttributes;
     LSA_UNICODE_STRING ServerString;
     PLSA_UNICODE_STRING Server = NULL;
     // Always initialize the object attributes to all zeroes.
     ZeroMemory(&ObjectAttributes, sizeof(ObjectAttributes));
     if ( ServerName != NULL )
     {
             // Make a LSA_UNICODE_STRING out of the LPWSTR passed in.
             InitLsaString(&ServerString, ServerName);
             Server = &ServerString;
     }
     // Attempt to open the policy.
     return LsaOpenPolicy( Server, &ObjectAttributes, DesiredAccess, PolicyHandle );
}

int isDomainMember(char *szDomain)
{
     PPOLICY_PRIMARY_DOMAIN_INFO ppdiDomainInfo=NULL;
     PPOLICY_DNS_DOMAIN_INFO pddiDomainInfo=NULL;
     LSA_HANDLE PolicyHandle;
     NTSTATUS status;
     BOOL retval = FALSE;

     // open the policy object for the local system
     status = OpenPolicy(
             NULL
             , GENERIC_READ | POLICY_VIEW_LOCAL_INFORMATION
             , &PolicyHandle
     );
    // You have a handle to the policy object. Now, get the
    // domain information using LsaQueryInformationPolicy.
    if ( !status )
    {
		/* Based on patch by Valdas Sevelis.  Call PolicyDnsDomainInformation first
		   as Win2K Advanced server is broken w/PolicyPrimaryDomainInformation */
        status = LsaQueryInformationPolicy(
                PolicyHandle,
                PolicyDnsDomainInformation,
                (void**)&pddiDomainInfo);
		if(!status)
		{
			retval = pddiDomainInfo->Sid != 0;
			if(szDomain && retval)
			{
				szDomain[WideCharToMultiByte(win32_global_codepage,0,pddiDomainInfo->Name.Buffer,pddiDomainInfo->Name.Length/sizeof(wchar_t),szDomain,DNLEN,0,NULL)]='\0';
			}
		    LsaFreeMemory( (LPVOID)pddiDomainInfo );
		}
		else
		{
             status = LsaQueryInformationPolicy(
                     PolicyHandle,
                     PolicyPrimaryDomainInformation,
                     (void**)&ppdiDomainInfo);
			if(!status)
			{
				retval = ppdiDomainInfo->Sid != 0;
				if(szDomain && retval)
				{
					szDomain[WideCharToMultiByte(win32_global_codepage,0,ppdiDomainInfo->Name.Buffer,ppdiDomainInfo->Name.Length/sizeof(wchar_t),szDomain,DNLEN,0,NULL)]='\0';
				}
			    LsaFreeMemory( (LPVOID)ppdiDomainInfo );
			}
		}
		if(status)
		   TRACE(3,"LsaQueryInformationPolicy returned %08x",status);
    }
    else
       TRACE(3,"OpenPolicy returned %08x",status);
    // Clean up all the memory buffers created by the LSA calls
	LsaClose(PolicyHandle);
    if(retval)
      TRACE(3,"Domain found: %s",szDomain);
    return retval;
}
#endif

int win32_openpipe(const char *pipe)
{
	HANDLE h;

	h = CreateFileA(pipe,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
	if(h==INVALID_HANDLE_VALUE)
	{
		errno=EBADF;
		win32_errno=GetLastError();
		return -1;
	}
	return _open_osfhandle((long)h,_O_RDWR|_O_BINARY);
}

int win32_makepipe(long hPipe)
{
	return _open_osfhandle(hPipe,_O_RDWR|_O_BINARY);
}

void win32_perror(int quit, const char *prefix, ...)
{
	char tmp[128];
	LPVOID buf=NULL;
	va_list va;

	va_start(va,prefix);
	vsprintf(tmp,prefix,va);
	va_end(va);

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,NULL,win32_errno,0,(LPTSTR)&buf,0,NULL);
	error(quit,0,"%s: %s",tmp,buf);
	LocalFree((HLOCAL)buf);
}

void preparse_filename(char *fn)
{
	if(strlen(fn)>12 && !strncmp(fn,"/cygdrive/",10) && fn[11]=='/')
	{
		fn[0]=fn[10];
		fn[1]=':';
		fn[2]='\\';
		strcpy(fn+3,fn+12);
	}
}

#undef fopen
#undef open
#undef fclose
#undef access

FILE *wnt_fopen(const char *filename, const char *mode)
{
	FILE *f;

	if(!validate_filename(filename,*mode=='b'))
		return NULL;

	uc_name fn = filename;
	uc_name wm = mode;

	f = _tfopen(fn,wm);
	return f;

}

int wnt_open(const char *filename, int mode, int umask /* = 0 */)
{
	if(!validate_filename(filename, mode&O_WRONLY))
		return -1;
	uc_name fn = filename;
	int f = _topen(fn,mode,umask);
	return f;
}

int wnt_access(const char *path, int mode)
{
	int ret;
	if(!validate_filename(path,false))
		return -1;
	uc_name fn = path;
	ret = _taccess(fn,mode);
	return ret;
}

int wnt_fclose(FILE *file)
{
	assert(file);
	assert(file->_flag);

//	FlushFileBuffers((HANDLE)_get_osfhandle(fileno(file)));

	return fclose(file);
}

void _dosmaperr(DWORD dwErr)
{
	int n;
	for(n=0; n<sizeof(errors)/sizeof(errors[0]); n++)
	{
		if(errors[n].err==dwErr)
		{
			errno=errors[n].dos;
			return;
		}
	}
	errno=EFAULT;
}

/* Is the year a leap year?
 *
 * Use standard Gregorian: every year divisible by 4
 * except centuries indivisible by 400.
 *
 * INPUTS: WORD year the year (AD)
 *
 * OUTPUTS: TRUE if it's a leap year.
 */
BOOL IsLeapYear ( WORD year )
{
    return ( ((year & 3u) == 0)
            && ( (year % 100u != 0)
                || (year % 400u == 0) ));
}

/* A fairly complicated comparison:
 *
 * Compares a test date against a target date.
 * If the test date is earlier, return a negative number
 * If the test date is later, return a positive number
 * If the two dates are equal, return zero.
 *
 * The comparison is complicated by the way we specify
 * TargetDate.
 *
 * TargetDate is assumed to be the kind of date used in
 * TIME_ZONE_INFORMATION.DaylightDate: If wYear is 0,
 * then it's a kind of code for things like 1st Sunday
 * of the month. As described in the Windows API docs,
 * wDay is an index of week:
 *      1 = first of month
 *      2 = second of month
 *      3 = third of month
 *      4 = fourth of month
 *      5 = last of month (even if there are only four such days).
 *
 * Thus, if wYear = 0, wMonth = 4, wDay = 2, wDayOfWeek = 4
 * it specifies the second Thursday of April.
 *
 * INPUTS: SYSTEMTIME * p_test_date     The date to be tested
 *
 *         SYSTEMTIME * p_target_date   The target date. This should be in
 *                                      the format for a TIME_ZONE_INFORMATION
 *                                      DaylightDate or StandardDate.
 *
 * OUTPUT:  -4/+4 if test month is less/greater than target month
 *          -2/+2 if test day is less/greater than target day
 *          -1/+2 if test hour:minute:seconds.milliseconds less/greater than target
 *          0     if both dates/times are equal.
 *
 */
static int CompareTargetDate (
    const SYSTEMTIME * p_test_date,
    const SYSTEMTIME * p_target_date
    )
{
    WORD first_day_of_month; /* day of week of the first. Sunday = 0 */
    WORD end_of_month;       /* last day of month */
    WORD temp_date;
    int test_milliseconds, target_milliseconds;

    /* Check that p_target_date is in the correct foramt: */
    if (p_target_date->wYear)
    {
        error(0,0,"Internal error: p_target_date is not in TIME_ZONE_INFORMATION format.");
        return 0;
    }
    if (!p_test_date->wYear)
    {
        error(0,0,"Internal error: p_test_date must be an actual date, not TIME_ZONE_INFORMAATION format.");
        return 0;
    }

    /* Don't waste time calculating if we can shortcut the comparison... */
    if (p_test_date->wMonth != p_target_date->wMonth)
    {
        return (p_test_date->wMonth > p_target_date->wMonth) ? 4 : -4;
    }

    /* Months equal. Now we neet to do some calculation.
     * If we know that y is the day of the week for some arbitrary date x,
     * then the day of the week of the first of the month is given by
     * (1 + y - x) mod 7.
     *
     * For instance, if the 19th is a Wednesday (day of week = 3), then
     * the date of the first Wednesday of the month is (19 mod 7) = 5.
     * If the 5th is a Wednesday (3), then the first of the month is
     * four days earlier (it's the first, not the zeroth):
     * (3 - 4) = -1; -1 mod 7 = 6. The first is a Saturday.
     *
     * Check ourselves: The 19th falls on a (6 + 19 - 1) mod 7
     * = 24 mod 7 = 3: Wednesday, as it should be.
     */
    first_day_of_month = (WORD)( (1u + p_test_date->wDayOfWeek - p_test_date->wDay) % 7u);

    /* If the first of the month comes on day y, then the first day of week z falls on
     * (z - y + 1) mod 7.
     *
     * For instance, if the first is a Saturday (6), then the first Tuesday (2) falls on a
     * (2 - 6 + 1) mod 7 = -3 mod 7 = 4: The fourth. This is correct (see above).
     *
     * temp_date gets the first <target day of week> in the month.
     */
    temp_date = (WORD)( (1u + p_target_date->wDayOfWeek - first_day_of_month) % 7u);
    /* If we're looking for the third Tuesday in the month, find the date of the first
     * Tuesday and add (3 - 1) * 7. In the example, it's the 4 + 14 = 18th.
     *
     * temp_date now should hold the date for the wDay'th wDayOfWeek of the month.
     * we only need to handle the special case of the last <DayOfWeek> of the month.
     */
    temp_date = (WORD)( temp_date + 7 * p_target_date->wDay );

    /* what's the last day of the month? */
    end_of_month = month_len [p_target_date->wMonth - 1];
    /* Correct if it's February of a leap year? */
    if ( p_test_date->wMonth == 2 && IsLeapYear(p_test_date->wYear) )
    {
        ++ end_of_month;
    }

    /* if we tried to calculate the fifth Tuesday of the month
     * we may well have overshot. Correct for that case.
     */
    while ( temp_date > end_of_month)
        temp_date -= 7;

    /* At long last, we're ready to do the comparison. */
    if ( p_test_date->wDay != temp_date )
    {
        return (p_test_date->wDay > temp_date) ? 2 : -2;
    }
    else
    {
        test_milliseconds = ((p_test_date->wHour * 60 + p_test_date->wMinute) * 60
                                + p_test_date->wSecond) * 1000 + p_test_date->wMilliseconds;
        target_milliseconds = ((p_target_date->wHour * 60 + p_target_date->wMinute) * 60
                                + p_target_date->wSecond) * 1000 + p_target_date->wMilliseconds;
        test_milliseconds -= target_milliseconds;
        return (test_milliseconds > 0) ? 1 : (test_milliseconds < 0) ? -1 : 0;
    }

}

//
//  Get time zone bias for local time *pst.
//
//  UTC time = *pst + bias.
//
static int GetTimeZoneBias( const SYSTEMTIME * pst )
{
    TIME_ZONE_INFORMATION tz;
    int n, bias;

    GetTimeZoneInformation ( &tz );

    /*  I only deal with cases where we look at
     *  a "last sunday" kind of thing.
     */
    if (tz.DaylightDate.wYear || tz.StandardDate.wYear)
    {
        error(0,0, "Cannont handle year-specific DST clues in TIME_ZONE_INFORMATION");
        return 0;
    }

    bias = tz.Bias;

    n = CompareTargetDate ( pst, & tz.DaylightDate );
    if (n < 0)
        bias += tz.StandardBias;
    else
    {
        n = CompareTargetDate ( pst, & tz.StandardDate );
        if (n < 0)
            bias += tz.DaylightBias;
        else
            bias += tz.StandardBias;
    }
    return bias;
}

//
//  Is the (local) time in DST?
//
static int IsDST( const SYSTEMTIME * pst )
{
    TIME_ZONE_INFORMATION tz;

    GetTimeZoneInformation ( &tz );

    /*  I only deal with cases where we look at
     *  a "last sunday" kind of thing.
     */
    if (tz.DaylightDate.wYear || tz.StandardDate.wYear)
    {
        error(0,0, "Cannont handle year-specific DST clues in TIME_ZONE_INFORMATION");
        return 0;
    }

	return (CompareTargetDate ( pst, & tz.DaylightDate ) >= 0)
			&& (CompareTargetDate ( pst, & tz.StandardDate ) < 0);
}

/* Convert a system time from local time to UTC time, correctly
 * taking DST into account (sort of).
 *
 * INPUTS:
 *      const SYSTEMTIME * p_local:
 *              A file time. It may be in UTC or in local
 *              time (see local_time, below, for details).
 *
 *      SYSTEMTIME * p_utc:
 *              The destination for the converted time.
 *
 * OUTPUTS:
 *      SYSTEMTIME * p_utc:
 *              The destination for the converted time.
 */

void LocalSystemTimeToUtcSystemTime( const SYSTEMTIME * p_local, SYSTEMTIME * p_utc)
{
    TIME_ZONE_INFORMATION tz;
    FILETIME ft;
    ULARGE_INTEGER itime, delta;
    int bias;

    * p_utc = * p_local;

    GetTimeZoneInformation(&tz);
    bias = tz.Bias;
    if ( CompareTargetDate(p_local, & tz.DaylightDate) < 0
            || CompareTargetDate(p_local, & tz.StandardDate) >= 0)
        bias += tz.StandardBias;
    else
        bias += tz.DaylightBias;

    SystemTimeToFileTime(p_local, & ft);
    itime.QuadPart = ((ULARGE_INTEGER *) &ft)->QuadPart;
    delta.QuadPart = systemtime_second;
    delta.QuadPart *= 60;   // minute
    delta.QuadPart *= bias;
    itime.QuadPart += delta.QuadPart;
    ((ULARGE_INTEGER *) & ft)->QuadPart = itime.QuadPart;
    FileTimeToSystemTime(& ft, p_utc);
}


/* Convert a file time to a Unix time_t structure. This function is as
 * complicated as it is because it needs to ask what time system the
 * filetime describes.
 *
 * INPUTS:
 *      const FILETIME * ft: A file time. It may be in UTC or in local
 *                           time (see local_time, below, for details).
 *
 *      time_t * ut:         The destination for the converted time.
 *
 *      BOOL local_time:     TRUE if the time in *ft is in local time
 *                           and I need to convert to a real UTC time.
 *
 * OUTPUTS:
 *      time_t * ut:         Store the result in *ut.
 */
static BOOL FileTimeToUnixTime ( const FILETIME* pft, time_t* put, BOOL local_time )
{
    BOOL success = FALSE;

   /* FILETIME = number of 100-nanosecond ticks since midnight
    * 1 Jan 1601 UTC. time_t = number of 1-second ticks since
    * midnight 1 Jan 1970 UTC. To translate, we subtract a
    * FILETIME representation of midnight, 1 Jan 1970 from the
    * time in question and divide by the number of 100-ns ticks
    * in one second.
    */

    SYSTEMTIME base_st =
    {
        1970,   /* wYear            */
        1,      /* wMonth           */
        0,      /* wDayOfWeek       */
        1,      /* wDay             */
        0,      /* wHour            */
        0,      /* wMinute          */
        0,      /* wSecond          */
        0       /* wMilliseconds    */
    };

    ULARGE_INTEGER itime;
    FILETIME base_ft;
    int bias = 0;

    if (local_time)
    {
        SYSTEMTIME temp_st;
        success = FileTimeToSystemTime(pft, & temp_st);
        bias =  GetTimeZoneBias(& temp_st);
    }

    success = SystemTimeToFileTime ( &base_st, &base_ft );
    if (success)
    {
        itime.QuadPart = ((ULARGE_INTEGER *)pft)->QuadPart;

        itime.QuadPart -= ((ULARGE_INTEGER *)&base_ft)->QuadPart;
        itime.QuadPart /= systemtime_second;	// itime is now in seconds.
        itime.QuadPart += bias * 60;    // bias is in minutes.

        *put = itime.QuadPart;
    }

    if (!success)
    {
        *put = -1;   /* error value used by mktime() */
    }
    return success;
}

/* Create a FileTime from a time_t, taking into account timezone if required */
static BOOL UnixTimeToFileTime ( time_t ut, FILETIME* pft, BOOL local_time )
{
    BOOL success = FALSE;

   /* FILETIME = number of 100-nanosecond ticks since midnight
    * 1 Jan 1601 UTC. time_t = number of 1-second ticks since
    * midnight 1 Jan 1970 UTC. To translate, we subtract a
    * FILETIME representation of midnight, 1 Jan 1970 from the
    * time in question and divide by the number of 100-ns ticks
    * in one second.
    */

    SYSTEMTIME base_st =
    {
        1970,   /* wYear            */
        1,      /* wMonth           */
        0,      /* wDayOfWeek       */
        1,      /* wDay             */
        0,      /* wHour            */
        0,      /* wMinute          */
        0,      /* wSecond          */
        0       /* wMilliseconds    */
    };

    ULARGE_INTEGER itime;
    FILETIME base_ft;
    int bias = 0;

    SystemTimeToFileTime ( &base_st, &base_ft );
	itime.QuadPart = ut;
	itime.QuadPart *= systemtime_second;
	itime.QuadPart += ((ULARGE_INTEGER *)&base_ft)->QuadPart;

    if (local_time)
    {
        SYSTEMTIME temp_st;
        success = FileTimeToSystemTime((FILETIME*)&itime, & temp_st);
        bias =  GetTimeZoneBias(& temp_st);

		itime.QuadPart -= (bias * 60) *systemtime_second;
    }

	*(ULARGE_INTEGER*)pft=itime;

    return success;
}

static BOOL GetFileSec(LPCTSTR strPath, HANDLE hFile, SECURITY_INFORMATION requestedInformation, PSECURITY_DESCRIPTOR *ppSecurityDescriptor)
{
	if(hFile)
	{
		DWORD dwLen = 0;
		GetKernelObjectSecurity(hFile,requestedInformation,NULL,0,&dwLen);
		if(!dwLen)
			return FALSE;
		*ppSecurityDescriptor = (PSECURITY_DESCRIPTOR)xmalloc(dwLen);
		if(!GetKernelObjectSecurity(hFile,requestedInformation,*ppSecurityDescriptor,dwLen,&dwLen))
		{
			xfree(*ppSecurityDescriptor);
			return FALSE;
		}
		return TRUE;
	}
	else
	{
		DWORD dwLen = 0;
		GetFileSecurity(strPath,requestedInformation,NULL,0,&dwLen);
		if(!dwLen)
			return FALSE;
		*ppSecurityDescriptor = (PSECURITY_DESCRIPTOR)xmalloc(dwLen);
		if(!GetFileSecurity(strPath,requestedInformation,*ppSecurityDescriptor,dwLen,&dwLen))
		{
			xfree(*ppSecurityDescriptor);
			return FALSE;
		}
		return TRUE;
	}
}

static mode_t GetUnixFileModeNtSec(LPCTSTR strPath, HANDLE hFile)
{
	PSECURITY_DESCRIPTOR pSec;
	PACL pAcl;
	mode_t mode;
	int index;
	PSID pUserSid,pGroupSid;
	int haveuser,havegroup,haveworld;
	BOOL bPresent,bDefaulted;

	if(!GetFileSec(strPath,hFile,DACL_SECURITY_INFORMATION|OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION,&pSec))
		return 0;

	if(!GetSecurityDescriptorDacl(pSec,&bPresent,&pAcl,&bDefaulted))
	{
		xfree(pSec);
		return 0;
	}

	if(!pAcl)
	{
		xfree(pSec);
		if(bPresent) /* Null DACL */
			return 0755;
		else
			return 0; /* No DACL/No Access */
	}

	if(!GetSecurityDescriptorOwner(pSec,&pUserSid,&bDefaulted))
	{
		xfree(pSec);
		return 0;
	}
	if(!GetSecurityDescriptorGroup(pSec,&pGroupSid,&bDefaulted))
	{
		xfree(pSec);
		return 0;
	}

	mode=0;
	haveuser=havegroup=haveworld=0;
	for (index = 0; index < pAcl->AceCount && haveuser+havegroup+haveworld<3; ++index)
	{
		PACCESS_ALLOWED_ACE pACE;
		PSID pSid;
		mode_t mask,mask2;
		if (!GetAce(pAcl, index, (LPVOID*)&pACE))
			continue;
		pSid = (PSID)&pACE->SidStart;
		if(!haveuser && pUserSid && EqualSid(pSid,pUserSid))
		{
			mask=0700;
			haveuser=1;
		}
		else if(!havegroup && pGroupSid && EqualSid(pSid,pGroupSid))
		{
			mask=0070;
			havegroup=1;
		}
		else if(!haveworld && EqualSid(pSid,&sidEveryone))
		{
			mask=0007;
			haveworld=1;
		}
		else continue;

		mask2=0;
		if((pACE->Mask&FILE_GENERIC_READ)==FILE_GENERIC_READ)
			mask2|=0444;
		if((pACE->Mask&FILE_GENERIC_WRITE)==FILE_GENERIC_WRITE)
			mask2|=0222;
		if((pACE->Mask&FILE_GENERIC_EXECUTE)==FILE_GENERIC_EXECUTE)
			mask2|=0111;

		if(pACE->Header.AceType==ACCESS_ALLOWED_ACE_TYPE)
			mode|=(mask2&mask);
		else if(pACE->Header.AceType==ACCESS_DENIED_ACE_TYPE)
			mode&=~(mask2&mask);
	}

	xfree(pSec);

	/* We could use GetEffectiveRightsFromAcl here but don't want to as it takes
	   into account annoying things like inheritance.  All we want to do is transport
	   the mode bits to/from a unix server relatively unmolested */
	/* If there's no group ownership, make group = world, and if there's no owner ownership,
	   make owner=group.  This should only normally happen on 'legacy' checkins as the
	   files in the sandbox will have both set by SetUnixFileMode. */
	if(!havegroup)
		mode|=(mode&0007)<<3;
	if(!haveuser)
		mode|=(mode&0070)<<3;

	TRACE(3,"GetUnixFileModeNtSec(%s,%p) returns %04o",strPath,hFile,mode);

	return mode;
}

static BOOL SetUnixFileModeNtSec(LPCTSTR strPath, mode_t mode)
{
	PSECURITY_DESCRIPTOR pSec;
	SECURITY_DESCRIPTOR NewSec;
	PACL pAcl,pNewAcl;
	PSID pUserSid,pGroupSid;
	int n;
	BOOL bPresent,bDefaulted;

	TRACE(3,"SetUnixFileModeNtSec(%s,%04o)",strPath,mode);

	mode&=0777;
	mode |= S_IRUSR|S_IWRITE | ((mode & S_IRGRP) ? S_IWGRP : 0)
			   | ((mode & S_IROTH) ? S_IWOTH : 0);

	if(!GetFileSec(strPath,NULL,DACL_SECURITY_INFORMATION|OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION,&pSec))
		return 0;
	if(!GetSecurityDescriptorDacl(pSec,&bPresent,&pAcl,&bDefaulted))
	{
		xfree(pSec);
		return 0;
	}
	if(!GetSecurityDescriptorOwner(pSec,&pUserSid,&bDefaulted))
	{
		xfree(pSec);
		return 0;
	}
	if(!GetSecurityDescriptorGroup(pSec,&pGroupSid,&bDefaulted))
	{
		xfree(pSec);
		return 0;
	}

	if(pAcl)
	{
		pNewAcl=(PACL)xmalloc(pAcl->AclSize+256);
		InitializeAcl(pNewAcl,pAcl->AclSize+256,ACL_REVISION);
		/* Delete any ACEs that equal our data */
		for(n=0; n<(int)pAcl->AceCount; n++)
		{
			ACCESS_ALLOWED_ACE *pAce;
			GetAce(pAcl,n,(LPVOID*)&pAce);
			if(!((pUserSid && EqualSid((PSID)&pAce->SidStart,pUserSid)) ||
				(pGroupSid && EqualSid((PSID)&pAce->SidStart,pGroupSid)) ||
			   (EqualSid((PSID)&pAce->SidStart,&sidEveryone))))
			{
				AddAce(pNewAcl,ACL_REVISION,MAXDWORD,pAce,pAce->Header.AceSize);
			}
		}
	}
	else
	{
		pNewAcl=(PACL)xmalloc(1024);
		InitializeAcl(pNewAcl,1024,ACL_REVISION);
	}

	/* Poor mans exception handling...  better to go C++ really */
	do
	{
		/* Always grant rw + control to the owner, no matter what else happens */
		if(pUserSid && !AddAccessAllowedAce(pNewAcl,ACL_REVISION,STANDARD_RIGHTS_ALL|FILE_GENERIC_READ|FILE_GENERIC_WRITE|((mode&0100)?FILE_GENERIC_EXECUTE:0),pUserSid))
			break;
		if(pGroupSid && !AddAccessAllowedAce(pNewAcl,ACL_REVISION,((mode&0040)?FILE_GENERIC_READ:0)|((mode&0020)?(FILE_GENERIC_WRITE|DELETE):0)|((mode&0010)?FILE_GENERIC_EXECUTE:0),pGroupSid))
			break;
		if(!AddAccessAllowedAce(pNewAcl,ACL_REVISION,((mode&0004)?FILE_GENERIC_READ:0)|((mode&0002)?(FILE_GENERIC_WRITE|DELETE):0)|((mode&0001)?FILE_GENERIC_EXECUTE:0),&sidEveryone))
			break;

		if(!InitializeSecurityDescriptor(&NewSec,SECURITY_DESCRIPTOR_REVISION))
			break;

		if(!SetSecurityDescriptorDacl(&NewSec,TRUE,pNewAcl,FALSE))
			break;

		if(!SetFileSecurity(strPath,DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,&NewSec))
			break;
	} while(0);

	xfree(pSec);
	xfree(pNewAcl);

	return TRUE;
}

typedef NTSTATUS (NTAPI *NtQueryEaFile_t)(HANDLE FileHandle, PIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length, BOOL ReturnSingleEntry, PVOID EaList, ULONG EaListLength,PULONG EaIndex, IN BOOL RestartScan);
typedef NTSTATUS (NTAPI *NtSetEaFile_t)(HANDLE FileHandle, PIO_STATUS_BLOCK IoStatusBlock, PVOID EaBuffer,ULONG EaBufferSize);

static NtQueryEaFile_t pNtQueryEaFile;
static NtSetEaFile_t pNtSetEaFile;

typedef struct _FILE_FULL_EA_INFORMATION
{
	ULONG NextEntryOffset;
	BYTE Flags;
	BYTE EaNameLength;
	USHORT EaValueLength;
	CHAR EaName[1];
} FILE_FULL_EA_INFORMATION, *PFILE_FULL_EA_INFORMATION;

typedef struct _FILE_GET_EA_INFORMATION
{
	ULONG	NextEntryOffset;
	BYTE	EaNameLength;
	CHAR	EaName[1];
} FILE_GET_EA_INFORMATION, *PFILE_GET_EA_INFORMATION;

static mode_t GetUnixFileModeNtEA(LPCTSTR strPath, HANDLE hFile)
{
	mode_t mode = 0;

	if(!pNtQueryEaFile)
		pNtQueryEaFile=(NtQueryEaFile_t)GetProcAddress(GetModuleHandle(_T("NTDLL")),"NtQueryEaFile");
	if(pNtQueryEaFile)
	{
		if(strPath)
			hFile = CreateFile(strPath, FILE_READ_EA, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS, NULL);

		if(hFile != INVALID_HANDLE_VALUE)
		{
			IO_STATUS_BLOCK io = {0};
			BYTE buffer[256];
			NTSTATUS status;
			FILE_GET_EA_INFORMATION *list;
			FILE_FULL_EA_INFORMATION *pea;
			static const char *Attr = ".UNIXATTR";

			list=(FILE_GET_EA_INFORMATION*)buffer;
			list->NextEntryOffset=0;
			list->EaNameLength=strlen(Attr);
			memcpy(list->EaName,Attr,list->EaNameLength+1);

			status = pNtQueryEaFile(hFile, &io, buffer, sizeof(buffer), TRUE, buffer, sizeof(buffer), NULL, TRUE);
			switch(status)
			{
			case STATUS_SUCCESS:
				pea = (FILE_FULL_EA_INFORMATION*)buffer;
				mode=*(mode_t*)&pea->EaName[pea->EaNameLength+1];
				mode&=0777;
				break;
			case STATUS_NONEXISTENT_EA_ENTRY:
				TRACE(3,"GetUnixFileModeNtEA error: STATUS_NONEXISTENT_EA_ENTRY");
				break;
			case STATUS_NO_EAS_ON_FILE:
				TRACE(3,"GetUnixFileModeNtEA error: STATUS_NO_EAS_ON_FILE");
				break;
			case STATUS_EA_CORRUPT_ERROR:
				TRACE(3,"GetUnixFileModeNtEA error: STATUS_EA_CORRUPT_ERROR");
				break;
			default:
				TRACE(3,"GetUnixFileModeNtEA error %08x",status);
				break;
			}

			if(strPath)
				CloseHandle(hFile);
		}
	}

	TRACE(3,"GetUnixFileModeNtEA(%s,%p) returns %04o",strPath,hFile,mode);
	return mode;
}

static BOOL SetUnixFileModeNtEA(LPCTSTR strPath, mode_t mode)
{
	HANDLE hFile = INVALID_HANDLE_VALUE;

	TRACE(3,"SetUnixFileModeNtEA(%s,%04o)",strPath,mode);

	mode&=0777;
	mode |= ((mode & S_IRUSR) ? _S_IWRITE : 0)
		      | ((mode & S_IRGRP) ? S_IWGRP : 0)
			  | ((mode & S_IROTH) ? S_IWOTH : 0);

 /* We set write, which gets removed by the read later */

	if(!pNtSetEaFile)
		pNtSetEaFile=(NtSetEaFile_t)GetProcAddress(GetModuleHandle(_T("NTDLL")),"NtSetEaFile");

	if(pNtSetEaFile)
	{
		if(strPath)
			hFile = CreateFile(strPath, FILE_WRITE_EA, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS, NULL);

		if(hFile != INVALID_HANDLE_VALUE)
		{
			IO_STATUS_BLOCK io = {0};
			BYTE buffer[256];
			NTSTATUS status;
			static const char *Attr = ".UNIXATTR";
			FILE_FULL_EA_INFORMATION *pea;

			pea = (FILE_FULL_EA_INFORMATION*)buffer;

			pea->NextEntryOffset=0;
			pea->Flags=0;
			pea->EaNameLength=strlen(Attr);
			pea->EaValueLength=sizeof(mode);
			memcpy(pea->EaName,Attr,pea->EaNameLength+1);
			*(mode_t*)(pea->EaName+pea->EaNameLength+1)=mode;

			status = pNtSetEaFile(hFile, &io, buffer, sizeof(buffer));

			if(strPath)
				CloseHandle(hFile);
		}
	}

	return FALSE;
}

static int _statcore(HANDLE hFile, const char *filename, struct stat *buf)
{
    BY_HANDLE_FILE_INFORMATION bhfi;
	int isdev;
	TCHAR szFs[32];
	int is_gmt_fs = -1;
	uc_name fn = filename;

	TRACE(3,"_statcore(%08x,%s)",hFile,filename);

	if(!hFile && GetFileAttributes(fn)==0xFFFFFFFF)
	{
		errno=ENOENT;
		return -1;
	}

	if(hFile)
	{
		/* Find out what kind of handle underlies filedes
		*/
		isdev = GetFileType(hFile) & ~FILE_TYPE_REMOTE;

		if ( isdev != FILE_TYPE_DISK )
		{
			/* not a disk file. probably a device or pipe
			 */
			if ( (isdev == FILE_TYPE_CHAR) || (isdev == FILE_TYPE_PIPE) )
			{
				/* treat pipes and devices similarly. no further info is
				 * available from any API, so set the fields as reasonably
				 * as possible and return.
				 */
				if ( isdev == FILE_TYPE_CHAR )
					buf->st_mode = _S_IFCHR;
				else
					buf->st_mode = _S_IFIFO;

				buf->st_rdev = buf->st_dev = (_dev_t)hFile;
				buf->st_nlink = 1;
				buf->st_uid = buf->st_gid = buf->st_ino = 0;
				buf->st_atime = buf->st_mtime = buf->st_ctime = 0;
				if ( isdev == FILE_TYPE_CHAR )
					buf->st_size = 0;
				else
				{
					unsigned long ulAvail;
					int rc;
					rc = PeekNamedPipe(hFile,NULL,0,NULL,&ulAvail,NULL);
					if (rc)
						buf->st_size = (_off_t)ulAvail;
					else
						buf->st_size = (_off_t)0;
				}

				return 0;
			}
			else if ( isdev == FILE_TYPE_UNKNOWN )
			{
				// Network device
				errno = EBADF;
				return -1;
			}
		}
	}

    /* set the common fields
     */
    buf->st_ino = buf->st_uid = buf->st_gid = buf->st_mode = 0;
    buf->st_nlink = 1;


	if(!filename)
	{
		// We have to assume here that the repository doesn't span drives, so the
		// current directory is correct.
		*szFs='\0';
		GetVolumeInformation(NULL,NULL,0,NULL,NULL,NULL,szFs,32);

		is_gmt_fs = GMT_FS(szFs);
	}
	else
	{
		uc_name fn = filename;

		if(fn[1]!=':')
		{
			if((fn[0]=='\\' || fn[0]=='/') && (fn[1]=='\\' || fn[1]=='/'))
			{
				// UNC pathname: Extract server and share and pass it to GVI
				TCHAR szRootPath[MAX_PATH + 1] = _T("\\\\");
				const TCHAR *p = &fn[2];
				TCHAR *q = &szRootPath[2];
				int n;
				for (n = 0; n < 2; n++)
				{
					// Get n-th path element
					while (*p != 0 && *p != '/' && *p != '\\')
					{
						*q = *p;
						p++;
						q++;
					}
					// Add separator
					if (*p != 0)
					{
						*q = '\\';
						p++;
						q++;
					}
				}
				*q = 0;

				*szFs='\0';
				GetVolumeInformation(szRootPath,NULL,0,NULL,NULL,NULL,szFs,sizeof(szFs));
				is_gmt_fs = GMT_FS(szFs);
			}
			else
			{
				// Relative path, treat as local
				GetVolumeInformation(NULL,NULL,0,NULL,NULL,NULL,szFs,sizeof(szFs));
				is_gmt_fs = GMT_FS(szFs);
			}
		}
		else
		{
			// Drive specified...
			TCHAR szRootPath[4] = _T("?:\\");
			szRootPath[0]=fn[0];
			*szFs='\0';
			GetVolumeInformation(szRootPath,NULL,0,NULL,NULL,NULL,szFs,sizeof(szFs));
			is_gmt_fs = GMT_FS(szFs);
		}
	}

	if(hFile)
	{
		TRACE(3,"Using file handle to retrieve information");
		/* use the file handle to get all the info about the file
		*/
		if ( !GetFileInformationByHandle(hFile, &bhfi) )
		{
			_dosmaperr(GetLastError());
			return -1;
		}
	}
	else
	{
		WIN32_FIND_DATA fd;
		WIN32_FILE_ATTRIBUTE_DATA faa;
		HANDLE hFind;

		memset(&bhfi,0,sizeof(bhfi));

		TRACE(3,"Trying GetFileAttributesEx....");
		if(GetFileAttributesEx(fn,GetFileExInfoStandard,&faa))
		{
			bhfi.dwFileAttributes=faa.dwFileAttributes;
			bhfi.ftCreationTime=faa.ftCreationTime;
			bhfi.ftLastAccessTime=faa.ftLastAccessTime;
			bhfi.ftLastWriteTime=faa.ftLastWriteTime;
			bhfi.nFileSizeLow=faa.nFileSizeLow;
			bhfi.nFileSizeHigh=faa.nFileSizeHigh;
			bhfi.nNumberOfLinks=1;
		}
		else
		{
			TRACE(3,"Trying FindFirstFile...");
			hFind=FindFirstFile(fn,&fd);
			if(hFind==INVALID_HANDLE_VALUE)
			{
				TRACE(3,"Trying CreateFile...");
				// If we don't have permissions to view the directory content (required by FindFirstFile,
				// maybe we can open the file directly
				hFile = CreateFile(fn, FILE_READ_ATTRIBUTES, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
					FILE_ATTRIBUTE_NORMAL, 0);

				if(hFile != INVALID_HANDLE_VALUE)
				{
					if(!GetFileInformationByHandle(hFile, &bhfi))
					{
						_dosmaperr(GetLastError());
						return -1;
					}
					CloseHandle(hFile);
				}
				else
				{
					// Can't do anything about the root directory...
					// Win2k and Win98 can return info, but Win95 can't, so
					// it's best to do nothing.
					TRACE(3,"CreateFileFailed.  Falling back to GetFileAttributes");
					bhfi.dwFileAttributes=GetFileAttributes(fn);
					if(bhfi.dwFileAttributes==0xFFFFFFFF)
					{
						_dosmaperr(GetLastError());
						return -1;
					}
					bhfi.nNumberOfLinks=1;
 				}
			}
			else
			{
				FindClose(hFind);
				bhfi.dwFileAttributes=fd.dwFileAttributes;
				bhfi.ftCreationTime=fd.ftCreationTime;
				bhfi.ftLastAccessTime=fd.ftLastAccessTime;
				bhfi.ftLastWriteTime=fd.ftLastWriteTime;
				bhfi.nFileSizeLow=fd.nFileSizeLow;
				bhfi.nFileSizeHigh=fd.nFileSizeHigh;
				bhfi.nNumberOfLinks=1;
			}
		}
	}

	TRACE(3,"File attributes = %08x",bhfi.dwFileAttributes);
	if(bhfi.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
	   TRACE(3," - FILE_ATTRIBUTE_DIRECTORY");
	else if(bhfi.dwFileAttributes&FILE_ATTRIBUTE_READONLY)
	   TRACE(3," - FILE_ATTRIBUTE_READONLY");
	else
		TRACE(3," - read/write file");

	if(!(bhfi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		/* We're assuming a reasonable failure mode on FAT32 here */
		if(use_ntsec)
			buf->st_mode = GetUnixFileModeNtSec(fn,hFile);
		else if(use_ntea)
			buf->st_mode = GetUnixFileModeNtEA(fn,hFile);
		else
			buf->st_mode = 0;
		if(!buf->st_mode)
			buf->st_mode = 0644;
		if ( bhfi.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
			buf->st_mode &= ~(S_IWRITE | S_IWGRP | S_IWOTH);
		else
			buf->st_mode |= _S_IWRITE | ((buf->st_mode & S_IRGRP) ? S_IWGRP : 0)
						  | ((buf->st_mode & S_IROTH) ? S_IWOTH : 0);
	}
	else
	{
		/* We always assume under NT that directories are 0755.  Since Unix permissions don't
		   map properly to NT directories anyway it's probably sensible */
		buf->st_mode = 0755;
	}

    // Potential, but unlikely problem... casting DWORD to short.
    // Reported by Jerzy Kaczorowski, addressed by Jonathan Gilligan
    // if the number of links is greater than 0x7FFF
    // there would be an overflow problem.
    // This is a problem inherent in the struct stat, and hence
    // in the design of the C library.
    if (bhfi.nNumberOfLinks > SHRT_MAX)
	{
        error(0,0,"Internal error: too many links to a file.");
        buf->st_nlink = SHRT_MAX;
    }
    else
	{
	    buf->st_nlink=(short)bhfi.nNumberOfLinks;
    }


	if(is_gmt_fs) // NTFS or similar - everything is in GMT already
	{
		FileTimeToUnixTime ( &bhfi.ftLastAccessTime, &buf->st_atime, FALSE );
		FileTimeToUnixTime ( &bhfi.ftLastWriteTime, &buf->st_mtime, FALSE );
		FileTimeToUnixTime ( &bhfi.ftCreationTime, &buf->st_ctime, FALSE );
	}
	else
	{
        // FAT - timestamps are in incorrectly translated local time.
        // translate them back and let FileTimeToUnixTime() do the
        // job properly.

        FILETIME At,Wt,Ct;

		FileTimeToLocalFileTime ( &bhfi.ftLastAccessTime, &At );
		FileTimeToLocalFileTime ( &bhfi.ftLastWriteTime, &Wt );
		FileTimeToLocalFileTime ( &bhfi.ftCreationTime, &Ct );
		FileTimeToUnixTime ( &At, &buf->st_atime, TRUE );
		FileTimeToUnixTime ( &Wt, &buf->st_mtime, TRUE );
		FileTimeToUnixTime ( &Ct, &buf->st_ctime, TRUE );
	}

	/* off_t is defined as 64bit for win32 builds */
	/* If the off_t is 32bit this just drops the high-bits, which
	   is pretty much what used to happen anyway... for a file that
	   large you're pretty much stuffed without 64bit file access */
    buf->st_size = (((off_t)bhfi.nFileSizeHigh)<<32)+bhfi.nFileSizeLow;
	if(bhfi.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
		buf->st_mode |= _S_IFDIR;
	else
		buf->st_mode |= _S_IFREG;

    /* On DOS, this field contains the drive number, but
     * the drive number is not available on this platform.
     * Also, for UNC network names, there is no drive number.
     */
    buf->st_rdev = buf->st_dev = 0;
	return 0;
}

#undef stat
#undef fstat
#undef lstat
#undef chmod
#undef asctime
#undef ctime
#undef utime

int wnt_stat(const char *name, struct wnt_stat *buf)
{
	TRACE(3,"wnt_stat(%s)",name);
	if(!validate_filename(name,false))
		return -1;
	return _statcore(NULL,name,buf);
}

int wnt_fstat (int fildes, struct wnt_stat *buf)
{
	TRACE(3,"wnt_fstat(%d)",fildes);
	return _statcore((HANDLE)_get_osfhandle(fildes),NULL,buf);
}

int wnt_lstat (const char *name, struct wnt_stat *buf)
{
	TRACE(3,"wnt_lstat(%s)",name);
	if(!validate_filename(name,false))
		return -1;
	return _statcore(NULL,name,buf);
}

int wnt_chmod (const char *name, mode_t mode)
{
	TRACE(3,"wnt_chmod(%s,%04o)",name,mode);
	if(!validate_filename(name,false))
		return -1;

	uc_name fn = name;

	if(_tchmod(fn,mode)<0)
		return -1;

	if(!(GetFileAttributes(fn)&FILE_ATTRIBUTE_DIRECTORY))
	{
		if(use_ntsec)
			SetUnixFileModeNtSec(fn,mode);
		else if(use_ntea)
			SetUnixFileModeNtEA(fn,mode);
	}

	return 0;
}

int wnt_utime(const char *filename, struct utimbuf *uf)
{
	HANDLE h;
	int is_gmt_fs;
	TCHAR szFs[32];
	FILETIME At,Wt;

	if(!validate_filename(filename,false))
		return -1;
	uc_name fn = filename;
	if(fn[1]!=':')
	{
		if((fn[0]=='\\' || fn[0]=='/') && (fn[1]=='\\' || fn[1]=='/'))
		{
			// UNC pathname: Extract server and share and pass it to GVI
			TCHAR szRootPath[MAX_PATH + 1] = _T("\\\\");
			const TCHAR *p = &fn[2];
			TCHAR *q = &szRootPath[2];
			int n;
			for (n = 0; n < 2; n++)
			{
				// Get n-th path element
				while (*p != 0 && *p != '/' && *p != '\\')
				{
					*q = *p;
					p++;
					q++;
				}
				// Add separator
				if (*p != 0)
				{
					*q = '\\';
					p++;
					q++;
				}
			}
			*q = 0;

			*szFs='\0';
			GetVolumeInformation(szRootPath,NULL,0,NULL,NULL,NULL,szFs,sizeof(szFs));
			is_gmt_fs = GMT_FS(szFs);
		}
		else
		{
			// Relative path, treat as local
			GetVolumeInformation(NULL,NULL,0,NULL,NULL,NULL,szFs,sizeof(szFs));
			is_gmt_fs = GMT_FS(szFs);
		}
	}
	else
	{
		// Drive specified...
		TCHAR szRootPath[4] = _T("?:\\");
		szRootPath[0]=fn[0];
		*szFs='\0';
		GetVolumeInformation(szRootPath,NULL,0,NULL,NULL,NULL,szFs,sizeof(szFs));
		is_gmt_fs = GMT_FS(szFs);
	}

	if(is_gmt_fs) // NTFS or similar - everything is in GMT already
	{
		UnixTimeToFileTime ( uf->actime, &At, FALSE);
		UnixTimeToFileTime ( uf->modtime, &Wt, FALSE);
	}
	else
	{
		// FAT or similar.  Convert to local time
		FILETIME fAt,fWt;

		UnixTimeToFileTime ( uf->actime, &fAt, TRUE);
		UnixTimeToFileTime ( uf->modtime, &fWt, TRUE);
		LocalFileTimeToFileTime  ( &fAt, &At );
		LocalFileTimeToFileTime  ( &fWt, &Wt );
	}
	h = CreateFile(fn,FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
	if(h==INVALID_HANDLE_VALUE)
	{
		DWORD dwErr = GetLastError();
		TRACE(3,"wnt_utime: CreateFile failed with error %08x",dwErr);
		_dosmaperr(dwErr);
		return -1;
	}
	SetFileTime(h,NULL,&At,&Wt);
	CloseHandle(h);
	return 0;
}

void wnt_putenv(const char *variable, const char *value)
{
	assert(variable);
	assert(value);
	SetEnvironmentVariableA(variable,value);
}

void wnt_get_temp_dir(char *tempdir, int tempdir_size)
{
	DWORD fa;

	if(!(GetEnvironmentVariableA("TEMP",tempdir,tempdir_size) ||
	     GetEnvironmentVariableA("TMP",tempdir,tempdir_size)))
	{
		// No TEMP or TMP, use default <windir>\TEMP
		GetWindowsDirectoryA(tempdir,tempdir_size);
		strcat(tempdir,"\\TEMP");
	}
	if((fa=GetFileAttributesA(tempdir))==0xFFFFFFFF || !(fa&FILE_ATTRIBUTE_DIRECTORY))
	{
		// Last resort, can't find a valid temp.... use C:\...
		strcpy(tempdir,"C:\\");
	}
}


static BOOL WINAPI MiniDumpDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	TCHAR tmp[64];
	static int nCountdown;
	static UINT_PTR nTimer;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		CheckRadioButton(hWnd,IDC_RADIO1,IDC_RADIO2,IDC_RADIO1);
		SetTimer(hWnd,nTimer,1000,NULL);
		nCountdown=30;
		_sntprintf(tmp,sizeof(tmp),_T("OK - %d"),nCountdown);
		SetDlgItemText(hWnd,IDOK,tmp);
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
			case IDOK:
			case IDCANCEL:
				g_bSmall=IsDlgButtonChecked(hWnd,IDC_RADIO2)?FALSE:TRUE;
				EndDialog(hWnd,LOWORD(wParam));
		}
		break;
	case WM_TIMER:
		nCountdown--;
		if(!nCountdown)
		{
			KillTimer(hWnd,nTimer);
			PostMessage(hWnd,WM_COMMAND,IDOK,(LPARAM)NULL);
		}
		_sntprintf(tmp,sizeof(tmp),_T("OK - %d"),nCountdown);
		SetDlgItemText(hWnd,IDOK,tmp);
		break;
	case WM_CLOSE:
		g_bSmall=IsDlgButtonChecked(hWnd,IDC_RADIO2)?FALSE:TRUE;
		break;
	}

	return FALSE;
}

static BOOL SendFileToCvsnt(LPCSTR szPath, LPCSTR szId)
{
	HINTERNET hNet, hConn, hInet;
	INTERNET_BUFFERS BufferIn = {0};
	LPSTR Func;
	DWORD dwBytesRead;
	DWORD dwBytesWritten;
	HANDLE hFile;
	char Buffer[1024];
	BOOL bRead, bRet = FALSE;
	DWORD sum;

	TRACE(3,"SendFileToCvsnt(%s,%s)",szPath,szId);

	snprintf(Buffer,sizeof(Buffer),"/%s.dmp.gz",szId);
	BufferIn.dwStructSize = sizeof( INTERNET_BUFFERS );

	do
	{
		Func="InternetOpen";
		if((hNet = InternetOpenA("CVSNT "CVSNT_PRODUCTVERSION_SHORT,INTERNET_OPEN_TYPE_PRECONFIG,NULL,NULL,0))==NULL)
			break;
		Func="InternetConnect";
		if((hConn = InternetConnectA(hNet,"crashdump2504.cvsnt.org",INTERNET_DEFAULT_HTTP_PORT,NULL,NULL,INTERNET_SERVICE_HTTP,0,0))==NULL)
			break;
		Func="HttpOpenRequest";
		if((hInet = HttpOpenRequestA(hConn,"PUT",Buffer,"1.1",NULL,NULL,INTERNET_FLAG_KEEP_CONNECTION|INTERNET_FLAG_NO_UI|INTERNET_FLAG_NO_COOKIES|INTERNET_FLAG_PRAGMA_NOCACHE,0))==NULL)
			break;

		Func="CreateFile";
		hFile = CreateFileA (szPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
			break;

		BufferIn.dwBufferTotal = GetFileSize (hFile, NULL);
		TRACE(3,"Crashdump file size is %u", BufferIn.dwBufferTotal);

		Func="HttpSendRequestEx";
		if(!HttpSendRequestEx( hInet, &BufferIn, NULL, HSR_INITIATE, 0))
			break;

		sum = 0;
		do
		{
			Func="ReadFile";
			if(!(bRead = ReadFile (hFile, Buffer, sizeof(Buffer), &dwBytesRead, NULL)))
				break;

			Func="InternetWriteFile";
			if (!(bRet=InternetWriteFile( hInet, Buffer, dwBytesRead, &dwBytesWritten)))
				break;

			sum += dwBytesWritten;
		} while (dwBytesRead == sizeof(Buffer));

		CloseHandle (hFile);
		TRACE(3,"Actual written bytes: %u", sum);

		Func="HttpEndRequest";
		if(!HttpEndRequest(hInet, NULL, 0, 0))
			break;

		bRet = TRUE;
	} while(FALSE);

	if(!bRet)
	{
		DWORD dwErr = GetLastError();
		TRACE(3,"Crash reporting failed - %s returned %08x",Func,dwErr);
		CServerIo::log(CServerIo::logError,"Could not connect to cvsnt.org to report the error (%s returned %08x)",Func,dwErr);
	}

	if(hInet) InternetCloseHandle(hInet);
	if(hConn) InternetCloseHandle(hConn);
	if(hNet) InternetCloseHandle(hNet);
	return bRet;
}

LONG WINAPI MiniDumper(PEXCEPTION_POINTERS pExceptionInfo)
{
	LONG retval = EXCEPTION_CONTINUE_SEARCH;
	char path[MAX_PATH];
	HANDLE hFile;
	char id[MAX_PATH];
	BOOL bSend;

	TRACE(3,"Exception caught - in minidumper");

	g_bSmall = TRUE;
	bSend = DialogBox(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_DIALOG1),NULL,MiniDumpDlgProc)==IDOK;

	TRACE(3,"send=%d",bSend);

	sprintf(id,"cvsnt-%s-",CVSNT_PRODUCTVERSION_SHORT);
	gethostname(id+strlen(id),sizeof(id)-strlen(id));
	sprintf(id+strlen(id),"-%s",global_session_id);
	if(!g_bSmall)
		strcat(id,"-large");
	else
		strcat(id,"-small");
	wnt_get_temp_dir(path,sizeof(path));
	snprintf(path+strlen(path),sizeof(path)-strlen(path),"\\%s.dmp",id);

	TRACE(3,"Dumping to %s",path);

	// create the file
	hFile = CreateFileA( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

	if (hFile!=INVALID_HANDLE_VALUE)
	{
		struct _MINIDUMP_EXCEPTION_INFORMATION ExInfo;

		ExInfo.ThreadId = GetCurrentThreadId();
		ExInfo.ExceptionPointers = pExceptionInfo;
		ExInfo.ClientPointers = FALSE;

		TRACE(3,"Start dump");
		// write the dump
		if(MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, (!g_bSmall)?MiniDumpWithFullMemory:MiniDumpNormal, &ExInfo, NULL, NULL ))
		{
			char path_to[MAX_PATH];

			CloseHandle(hFile);
			sprintf(path_to,"%s.gz",path);
			noexec=0;

			TRACE(3,"Dump succeeded.  Zipping to %s",path_to);
			copy_and_zip_file(path,path_to,1,1);
			CVS_UNLINK(path);

			CServerIo::log(CServerIo::logError,"CVSNT crashed.  A crashdump has been written to %s",path_to);
			if(bSend)
			{
				SendFileToCvsnt(path_to,id);
			}

			retval = EXCEPTION_EXECUTE_HANDLER;
		}
		else
		{
			TRACE(3,"Dump failed - %08x",GetLastError());
			CServerIo::log(CServerIo::logError,"CVSNT crashed.  A crashdump could not be written (disk space?)");
			CloseHandle(hFile);
		}
	}
	else
	{
		TRACE(3,"Creation of dump file failed -%08x",GetLastError());
		CServerIo::log(CServerIo::logError,"CVSNT crashed.  A crashdump could not be written (check permissions on temp directory)");
	}
	return retval;
}

int wnt_link(const char *oldpath, const char *newpath)
{
	static BOOL (WINAPI *pCreateHardLinkA)(LPCSTR lpFileName, LPCSTR lpExistingFileName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
	if(!pCreateHardLinkA)
	{
		HMODULE hLib = GetModuleHandle(_T("Kernel32"));
		pCreateHardLinkA=(BOOL (WINAPI *)(LPCSTR, LPCSTR, LPSECURITY_ATTRIBUTES))GetProcAddress(hLib,"CreateHardLinkA");
	}
	if(!pCreateHardLinkA)
	{
		errno=ENOSYS;
		return -1;
	}
	if(!pCreateHardLinkA(newpath,oldpath,NULL))
	{
		_dosmaperr(GetLastError());
		return -1;
	}
	return 0;
}

int wnt_fseek64(FILE *fp, off_t pos, int whence)
{
	/* Win32 does in fact have a 64bit fseek but it's buried in the CRT and not
	   exported... */
	switch(whence)
	{
	case SEEK_SET:
		if(pos>0x7FFFFFFF)
		{
			if(fseek(fp,0x7FFFFFFF,whence))
				return -1;
			whence=SEEK_CUR;
			pos-=0x7FFFFFFFF;
			while(pos>0x7FFFFFFF && !fseek(fp,0x7FFFFFFF,SEEK_CUR))
				pos-=0x7FFFFFFFF;
			if(pos>0x7FFFFFFF)
				return -1;
		}
		return fseek(fp,(long)pos,whence);
	case SEEK_CUR:
		if(pos>0x7FFFFFFF)
		{
			while(pos>0x7FFFFFFF && !fseek(fp,0x7FFFFFFF,SEEK_CUR))
				pos-=0x7FFFFFFFF;
			if(pos>0x7FFFFFFF)
				return -1;
		}
		return fseek(fp,(long)pos,whence);
	case SEEK_END:
		if(pos>0x7FFFFFFF)
		{
			if(fseek(fp,0x7FFFFFFF,whence))
				return -1;
			whence=SEEK_CUR;
			pos-=0x7FFFFFFFF;
			while(pos>0x7FFFFFFF && !fseek(fp,-0x7FFFFFFF,SEEK_CUR))
				pos-=0x7FFFFFFFF;
			if(pos>0x7FFFFFFF)
				return -1;
			pos=-pos;
		}
		return fseek(fp,(long)pos,whence);
	}
	return -1;
}

off_t wnt_ftell64(FILE *fp)
{
	off_t o=-1;
	fgetpos(fp,&o);
	return o;
}

void wnt_hide_file(const char *fn)
{
	SetFileAttributesA(fn,FILE_ATTRIBUTE_HIDDEN);
}

int w32_is_network_share(const char *directory)
{
	char drive[]="z:\\";
	char dir[MAX_PATH];

	if(strlen(directory)<2)
		return 0;

	if(ISDIRSEP(directory[0]) && ISDIRSEP(directory[1]))
		return 1;

	if(directory[1]==':')
	{
		drive[0]=directory[0];
		if(GetDriveTypeA(drive)==DRIVE_REMOTE)
			return 1;
		return 0;
	}

	GetCurrentDirectoryA(sizeof(dir),dir);
	if(dir[1]==':')
	{
		drive[0]=dir[0];
		if(GetDriveTypeA(drive)==DRIVE_REMOTE)
			return 1;
	}
	return 0;
}

typedef struct
{
	LOCALGROUP_USERS_INFO_0 *local;
	DWORD localcount;
	GROUP_USERS_INFO_0 *global;
	DWORD globalcount;
	TOKEN_GROUPS *token;
} ntgroups;

int win32_getgroups_begin(void **grouplist)
{
#ifndef WINNT_VERSION
	return 0;
#else
	ntgroups *ntg;


	*grouplist = (void*)xmalloc(sizeof(ntgroups));
	memset(*grouplist,0,sizeof(ntgroups));
	ntg=(ntgroups*)*grouplist;

	/* If Impersonation is enabled, then check the local thread token
	   otherwise do the domain checks, as before. */
	/* Also use this if we're in client mode */
	if((!current_username || !stricmp(current_username,CVS_Username)))
	{
		DWORD dwLen=0;
		HANDLE hThread = NULL;

		TRACE(3,"Checking local access token for groups");

		if(!OpenThreadToken(GetCurrentThread(),TOKEN_QUERY,FALSE,&hThread) &&
			!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&hThread))
			TRACE(3,"Failed to open token (%08x)",GetLastError());

		if(!GetTokenInformation(hThread,TokenGroups,NULL,0,&dwLen) && GetLastError()!=ERROR_INSUFFICIENT_BUFFER)
			TRACE(3,"Failed to get token information (%08x)",GetLastError());

		if(hThread && dwLen)
		{
			ntg->token = (TOKEN_GROUPS*)xmalloc(dwLen);
			if(!GetTokenInformation(hThread,TokenGroups,ntg->token,dwLen,&dwLen))
				TRACE(3,"Failed to get token information (%08x)",GetLastError());
		}
		if(hThread)
			CloseHandle(hThread);
		if(ntg->token)
			return ntg->token->GroupCount;
		return 0;
	}
	else
	{
		wchar_t w_name[UNLEN+1];
		wchar_t w_domain[DNLEN+1];
		wchar_t w_pdc[DNLEN+1];
		DWORD totalentries;
		DWORD dwErr1=-1,dwErr2=-1,dwErr3=-1;

		TRACE(3,"Checking authenticating server for groups");

#ifdef _UNICODE
		{
			wchar_t u[UNLEN+1];
			MultiByteToWideChar(win32_global_codepage,0,CVS_Username,-1,u,sizeof(u)/sizeof(u[0]));
			if(BreakNameIntoParts(u,w_name,w_domain,w_pdc))
				return 0;
		}
#else
		if(BreakNameIntoParts(CVS_Username,w_name,w_domain,w_pdc))
			return 0;
#endif
		TRACE(3,"Using Domain Controller %S",w_pdc);

		/* LG_INCLUDE_INDIRECT will fail in certain circumstances */
		if((dwErr1 = NetUserGetLocalGroups(w_pdc,w_name,0,LG_INCLUDE_INDIRECT,(LPBYTE*)&ntg->local,MAX_PREFERRED_LENGTH,&ntg->localcount,&totalentries))==ERROR_ACCESS_DENIED)
			dwErr2 = NetUserGetLocalGroups(w_pdc,w_name,0,0,(LPBYTE*)&ntg->local,MAX_PREFERRED_LENGTH,&ntg->localcount,&totalentries);
		dwErr3 = NetUserGetGroups(w_pdc,w_name,0,(LPBYTE*)&ntg->global,MAX_PREFERRED_LENGTH,&ntg->globalcount,&totalentries);

		if(dwErr1==dwErr2 && dwErr2==dwErr3 && dwErr3==ERROR_ACCESS_DENIED)
			TRACE(3,"Service unable to get domain groups (access denied)");
		if(!(ntg->localcount+ntg->globalcount))
		{
			xfree(*grouplist);
			return 0;
		}
		return ntg->globalcount+ntg->localcount;
	}
#endif
}
static const wchar_t *win32_getgroup_w(void *grouplist, int num)
{
#ifndef WINNT_VERSION
	return NULL;
#else
	ntgroups *ntg = (ntgroups*)grouplist;
	if(ntg->token)
	{
		if(num<ntg->token->GroupCount)
		{
			static wchar_t name[UNLEN+1];
			static wchar_t domain[DNLEN+1];
			SID_NAME_USE Use;
			DWORD DomainLen,NameLen;

			PSID pSid = ntg->token->Groups[num].Sid;
			NameLen=sizeof(name)/sizeof(name[0]);
			DomainLen=sizeof(domain)/sizeof(domain[0]);
			LookupAccountSidW(NULL,pSid,name,&NameLen,domain,&DomainLen,&Use);
			return name;
		}
		else
			return NULL;
	}
	else
	{
		if(num<ntg->globalcount)
			return ntg->global[num].grui0_name;
		num-=ntg->globalcount;
		if(num<ntg->localcount)
			return ntg->local[num].lgrui0_name;
		return NULL;
	}
#endif
}

const char *win32_getgroup(void *grouplist, int num)
{
	const wchar_t *wg = win32_getgroup_w(grouplist,num);
	static char g[UNLEN+1];

	if(!wg)
		return NULL;

	g[WideCharToMultiByte(win32_global_codepage,0,wg,wcslen(wg)+1,g,sizeof(g),NULL,NULL)]='\0';
	return g;
}

void win32_getgroups_end(void **grouplist)
{
#ifdef WINNT_VERSION
	ntgroups *ntg = (ntgroups*)*grouplist;

	if(!ntg)
		return;
	if(ntg->global)
		NetApiBufferFree(ntg->global);
	if(ntg->local)
		NetApiBufferFree(ntg->local);
	if(ntg->token)
		xfree(ntg->token);
	xfree(ntg);
	*grouplist = NULL;
#endif
}

/* Make a username on the local domain into its short form.  Needed for
   SSPI which always uses the long form. */
void win32_sanitize_username(const char **pusername)
{
	char domain[DNLEN+2];
	const char *username = *pusername;

	/* Username doesn't contain domain */
	if(!strchr(username,'\\'))
		return;

	strncpy(domain,g_defaultdomain,sizeof(domain));
	strcat(domain,"\\");
	if(!strncmp(username,domain,strlen(domain)))
	{
		*pusername = xstrdup(username+strlen(domain));
		xfree(username);
	}
}

#ifdef _UNICODE
uc_name::uc_name(const char *file)
{
	wchar_t *p = _fn;

	if(!file)
	{
		_fnp=NULL;
		return;
	}

	if(strlen(file)>=(sizeof(_fn)/sizeof(_fn[0])))
		p=_fnp=new wchar_t[strlen(file)+1];
	else
		_fnp = NULL;
	
	MultiByteToWideChar(win32_global_codepage,0,file,-1,p,(strlen(file)+1)*sizeof(wchar_t));
}

uc_name::~uc_name()
{
	if(_fnp) delete[] _fnp;
}
#endif

/* This might look easier using ADS but:

   ADsSecurity does not fix the ACL ordering problems (so is no better than this code)
   ADsSecurityUtility is only available on XP, and we have Win2k clients to worry about
*/

int win32_set_edit_acl(const char *filename)
{
	PSECURITY_DESCRIPTOR pSec;
	SECURITY_DESCRIPTOR NewSec;
	PACL pAcl,pNewAcl;
	PSID pUserSid;
//	int n;
	BOOL bPresent,bDefaulted;
	uc_name fn = filename;

	TRACE(3,"win32_set_edit_acl(%s)",filename);

	if(!GetFileSec(fn,NULL,DACL_SECURITY_INFORMATION,&pSec))
	{
		TRACE(3,"GetFileSec() failed - error %08x",GetLastError());
		return -1;
	}
	if(!GetSecurityDescriptorDacl(pSec,&bPresent,&pAcl,&bDefaulted))
	{
		TRACE(3,"GetSecurityDesciptorDacl() failed - %08x",GetLastError());
		xfree(pSec);
		return -1;
	}
	HANDLE hThreadToken;
	if(!OpenThreadToken(GetCurrentThread(),TOKEN_QUERY,FALSE,&hThreadToken) && !OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&hThreadToken))
	{
		TRACE(3,"OpenThreadToken() failed - %08x",GetLastError());
		xfree(pSec);
		return -1;
	}
	BYTE tu[512];
	DWORD dwLen;
	if(!GetTokenInformation(hThreadToken,TokenUser,tu,sizeof(tu),&dwLen))
	{
		TRACE(3,"GetTokenInformation() failed - %08x",GetLastError());
		xfree(pSec);
		CloseHandle(hThreadToken);
		return -1;
	}
	pUserSid = ((TOKEN_USER*)tu)->User.Sid;
	CloseHandle(hThreadToken);
	
/*	if(pAcl)
	{
		pNewAcl=(PACL)xmalloc(pAcl->AclSize+256);
		InitializeAcl(pNewAcl,pAcl->AclSize+256,ACL_REVISION);

		// Delete any ACEs that equal our data 
		for(n=0; n<(int)pAcl->AceCount; n++)
		{
			ACCESS_ALLOWED_ACE *pAce;
			GetAce(pAcl,n,(LPVOID*)&pAce);
			if(!(EqualSid((PSID)&pAce->SidStart,pUserSid) ||
			   (EqualSid((PSID)&pAce->SidStart,&sidEveryone))))
			{
				AddAce(pNewAcl,ACL_REVISION,MAXDWORD,pAce,pAce->Header.AceSize);
			}
		}
	}
	else */
	{
		pNewAcl=(PACL)xmalloc(1024);
		InitializeAcl(pNewAcl,1024,ACL_REVISION);
	}

	if(!AddAccessAllowedAce(pNewAcl,ACL_REVISION,STANDARD_RIGHTS_ALL|SPECIFIC_RIGHTS_ALL,pUserSid))
	{
		TRACE(3,"AddAccessAllowedAce() failed - %08x",GetLastError());
	}
	if(!AddAccessAllowedAce(pNewAcl,ACL_REVISION,FILE_GENERIC_READ,&sidEveryone))
	{
		TRACE(3,"AddAccessDeniedAce() failed - %08x",GetLastError());
	}

	do
	{
		if(!InitializeSecurityDescriptor(&NewSec,SECURITY_DESCRIPTOR_REVISION))
		{
			TRACE(3,"InitializeSecurityDescriptor() failed - %08x",GetLastError());
			break;
		}

		if(!SetSecurityDescriptorDacl(&NewSec,TRUE,pNewAcl,FALSE))
		{
			TRACE(3,"SetSecurityDescriptorDacl() failed - %08x",GetLastError());
			break;
		}

		if(!SetFileSecurity(fn,DACL_SECURITY_INFORMATION,&NewSec))
		{
			TRACE(3,"SetFileSecurity() failed - %08x",GetLastError());
			break;
		}
	} while(0);

	xfree(pSec);
	xfree(pNewAcl);

	return 0;
}
