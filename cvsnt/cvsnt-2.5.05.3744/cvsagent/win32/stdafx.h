// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#define STRICT
#define WINVER 0x0501
#define _WIN32_WINNT 0x0501
#define _CRT_SECURE_NO_DEPRECATE
#pragma warning(disable:4996)

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxcmn.h>			// MFC support for Windows Common Controls
#include <afxdlgs.h>

#include <vector>
#include <map>
#include <string>

#include <winsock2.h>
#include <ws2tcpip.h>

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#include "../../version_no.h"
#include "../../version_fu.h"

#include "resource.h"

#define TRAY_MESSAGE (WM_APP+123)
#define PWD_MESSAGE (WM_APP+124)
#define PWD_MESSAGE_DELETE (WM_APP+125)

extern std::map<std::string,std::string> g_Passwords;
extern bool g_bTopmost;

//Deprecated
//#define HAVE_U3

#ifdef HAVE_U3
#include <u3dapi10.h>
#endif
