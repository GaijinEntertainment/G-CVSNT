/*
	CVSNT Generic API
    Copyright (C) 2004 Tony Hoyle and March-Hare Software Ltd

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
#include <config.h>
#include "lib/api_system.h"

#include <stdarg.h>
#include "cvs_string.h"
#include "ServerIO.h"

int CServerIo::m_loglevel = 0;

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static void AddEventSource(LPCTSTR szService, LPCTSTR szModule);
static void ReportError(BOOL bError, LPCTSTR szError);
#include "ServiceMsg.h"
#else
#include <syslog.h>
#endif

/* Some platforms (eg. solaris) don't have the secure logging facility. */
#ifndef LOG_AUTHPRIV
#define LOG_AUTHPRIV LOG_AUTH
#endif

int (*CServerIo::m_pOutput)(const char *msg, size_t len) = CServerIo::default_output;
int (*CServerIo::m_pWarning)(const char *msg, size_t len) = CServerIo::default_output;
int (*CServerIo::m_pError)(const char *msg, size_t len) = CServerIo::default_output;
int (*CServerIo::m_pTrace)(int level, const char *msg) = CServerIo::default_trace;

void CServerIo::loglevel(int level)
{
	m_loglevel = level;
}

int CServerIo::output(const char *fmt, ...)
{
	va_list va;
	va_start(va,fmt);
	cvs::string str;
	cvs::vsprintf(str,80,fmt,va);
	va_end(va);
	return m_pOutput(str.c_str(),str.length());
}

int CServerIo::output(size_t len, const char *str)
{
	return m_pOutput(str,len);
}

int CServerIo::warning(const char *fmt, ...)
{
	va_list va;
	va_start(va,fmt);
	cvs::string str;
	cvs::vsprintf(str,80,fmt,va);
	va_end(va);
	return m_pWarning(str.c_str(),str.length());
}

int CServerIo::warning(size_t len, const char *str)
{
	return m_pWarning(str,len);
}

int CServerIo::error(const char *fmt, ...)
{
	va_list va;
	va_start(va,fmt);
	cvs::string str;
	cvs::vsprintf(str,80,fmt,va);
	va_end(va);
	return m_pError(str.c_str(),str.length());
}

int CServerIo::error(size_t len, const char *str)
{
	return m_pError(str,len);
}

int CServerIo::trace(int level, const char *fmt, ...)
{
	if(level > m_loglevel)
		return 0;
	va_list va;
	va_start(va,fmt);
	cvs::string str;
	cvs::vsprintf(str,80,fmt,va);
	va_end(va);
	return m_pTrace(level,str.c_str());
}

int CServerIo::default_output(const char *str, size_t len)
{
	return printf("%-*.*s",len,len,str);
}

int CServerIo::default_trace(int level, const char *str)
{
#ifdef _WIN32
	OutputDebugStringA(str);
	OutputDebugString(L"\r\n");
#endif
	return printf("%s\n",str);
}

int CServerIo::init(int (*pOutput)(const char *str, size_t len),int (*pWarning)(const char *str, size_t len),int (*pError)(const char *str, size_t len),int (*pTrace)(int level, const char *msg))
{
	m_pOutput = pOutput;
	m_pWarning = pWarning;
	m_pError = pError;
	m_pTrace = pTrace;
	return 0;
}

void CServerIo::log(logType type, const char *fmt, ...)
{
	va_list va;
	va_start(va,fmt);
	cvs::string str;
	cvs::vsprintf(str,80,fmt,va);
	va_end(va);
#ifdef _WIN32
	ReportError(type==logError?TRUE:FALSE, cvs::wide(str.c_str()));
#else
	int l;

	switch(type)
	{
	case logNotice:
		l = LOG_DAEMON | LOG_NOTICE;
		break;
	case logError:
		l = LOG_DAEMON | LOG_ERR;
		break;
	case logAuth:
		l = LOG_AUTHPRIV | LOG_NOTICE;
		break;
	default:
		l = LOG_DAEMON | LOG_NOTICE;
		break;
	}
	syslog(l | LOG_NOTICE, "%s", str.c_str());
#endif
}

#ifdef _WIN32
HMODULE g_hInstance;

extern "C" BOOL CALLBACK DllMain(HMODULE hModule, DWORD ul, LPVOID lpReserved)
{
	g_hInstance = hModule;
	return TRUE;
}

void ReportError(BOOL bError, LPCTSTR szError)
{
	static BOOL bEventSourceAdded = FALSE;
	if(!bEventSourceAdded)
	{
		TCHAR szModule[MAX_PATH];
		GetModuleFileName(g_hInstance,szModule,MAX_PATH);
		AddEventSource(L"CVSNT",szModule);
		bEventSourceAdded=TRUE;
	}

	HANDLE hEvent = RegisterEventSource(NULL,  L"CVSNT");
	ReportEvent(hEvent,bError?EVENTLOG_ERROR_TYPE:EVENTLOG_INFORMATION_TYPE,0,MSG_STRING,NULL,1,0,&szError,NULL);
	DeregisterEventSource(hEvent);
}

void AddEventSource(LPCTSTR szService, LPCTSTR szModule)
{
	HKEY hk;
	DWORD dwData;
	TCHAR szKey[1024];

	wcscpy(szKey,L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\");
	wcscat(szKey,szService);

    // Add your source name as a subkey under the Application
    // key in the EventLog registry key.
    if (RegCreateKey(HKEY_LOCAL_MACHINE, szKey, &hk))
		return; // Fatal error, no key and no way of reporting the error!!!

    // Add the name to the EventMessageFile subkey.
    if (RegSetValueEx(hk,             // subkey handle
            L"EventMessageFile",       // value name
            0,                        // must be zero
            REG_EXPAND_SZ,            // value type
            (LPBYTE) szModule,           // pointer to value data
            (DWORD)(wcslen(szModule) + 1)*sizeof(szModule[0])))       // length of value data
			return; // Couldn't set key

    // Set the supported event types in the TypesSupported subkey.
    dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE |
        EVENTLOG_INFORMATION_TYPE;
    if (RegSetValueEx(hk,      // subkey handle
            L"TypesSupported",  // value name
            0,                 // must be zero
            REG_DWORD,         // value type
            (LPBYTE) &dwData,  // pointer to value data
            sizeof(DWORD)))    // length of value data
			return; // Couldn't set key
	RegCloseKey(hk);
}
#endif
