// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__F52337E9_30FF_11D2_8EED_00A0C94457BF__INCLUDED_)
#define AFX_STDAFX_H__F52337E9_30FF_11D2_8EED_00A0C94457BF__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifdef _WIN32
#if _MSC_VER >= 1400
// this is Visual C++ 2005
// Microsoft braindamage reversal.  
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#endif
#endif
#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#define STRICT
#define WINVER 0x0501
#define WIN32_WINNT 0x0501
#define ISOLATION_AWARE_ENABLED 1
#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxcmn.h>			// MFC support for Windows Common Controls

#include <cpl.h>
#include <winsvc.h>

#include <shellapi.h>
#include <Shlwapi.h>
#include <shlobj.h>

#include <vector>

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#include "../version_no.h"
#include "../version_fu.h"
#include <afxdlgs.h>
#include <afxcview.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cvstools.h>
#include "resource.h"

int CALLBACK BrowseValid(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM lpData);

extern bool g_bPrivileged;
extern HKEY g_hServerKey;
extern HKEY g_hInstallerKey;

#if _MSC_VER >= 1400
// this is Visual C++ 2005
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#endif // !defined(AFX_STDAFX_H__F52337E9_30FF_11D2_8EED_00A0C94457BF__INCLUDED_)
