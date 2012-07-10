/*
	CVSNT Generic API
    Copyright (C) 2006 Tony Hoyle and March-Hare Software Ltd

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
#ifdef _WIN32
// Microsoft braindamage reversal.  
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "win32\resource.h"
#endif
#include <config.h>
#include <cvsapi.h>

#include "MssqlConnectionInformation.h"

const char *CMssqlConnectionInformation::getVariable(const char *name)
{
	if(!strcmp(name,"prefix") || !strcmp(name,"PREFIX")) // Caps is script substitution variable
		return prefix.c_str();
	return CSqlConnectionInformation::getVariable(name);
}

bool CMssqlConnectionInformation::setVariable(const char *name, const char *value)
{
	if(!strcmp(name,"prefix"))
		prefix = value;
	return CSqlConnectionInformation::setVariable(name,value);
}

const char *CMssqlConnectionInformation::enumVariableNames(size_t nVar)
{
	switch(nVar)
	{
	case firstDriverVariable:
		return "prefix";
	default:
		return CSqlConnectionInformation::enumVariableNames(nVar);
	}
}

#ifndef _WIN32
bool CMssqlConnectionInformation::connectionDialog(const void *parentWindow)
{
	return false;
}
#else
HINSTANCE g_hInstance;

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	g_hInstance = hInstance;
	return TRUE;
}

BOOL CALLBACK CMssqlConnectionInformation::ConnectionDialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CMssqlConnectionInformation *pCI = (CMssqlConnectionInformation*)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	switch(uMsg)
	{
		case WM_INITDIALOG:
			pCI = (CMssqlConnectionInformation*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, lParam);
			SetDlgItemText(hWnd,IDC_DATABASE,cvs::wide(pCI->database.c_str()));
			SetDlgItemText(hWnd,IDC_SERVER,cvs::wide(pCI->hostname.c_str()));
			SetDlgItemText(hWnd,IDC_USERNAME,cvs::wide(pCI->username.c_str()));
			SetDlgItemText(hWnd,IDC_PASSWORD,cvs::wide(pCI->password.c_str()));
			SetDlgItemText(hWnd,IDC_PREFIX,cvs::wide(pCI->prefix.c_str()));
			return TRUE;
		case WM_COMMAND:
			switch(wParam)
			{
			case IDOK:
				{
					TCHAR szTmp[1024];
				
					GetDlgItemText(hWnd, IDC_DATABASE, szTmp, sizeof(szTmp)/sizeof(szTmp[0]));
					pCI->database = cvs::narrow(szTmp);
					GetDlgItemText(hWnd, IDC_SERVER, szTmp, sizeof(szTmp)/sizeof(szTmp[0]));
					pCI->hostname = cvs::narrow(szTmp);
					GetDlgItemText(hWnd, IDC_USERNAME, szTmp, sizeof(szTmp)/sizeof(szTmp[0]));
					pCI->username = cvs::narrow(szTmp);
					GetDlgItemText(hWnd, IDC_PASSWORD, szTmp, sizeof(szTmp)/sizeof(szTmp[0]));
					pCI->password = cvs::narrow(szTmp);
					GetDlgItemText(hWnd, IDC_PREFIX, szTmp, sizeof(szTmp)/sizeof(szTmp[0]));
					pCI->prefix = cvs::narrow(szTmp);
				}
				// Fall through
			case IDCANCEL:
				EndDialog(hWnd,wParam);
				return TRUE;
			}
	}
	return FALSE;
}

bool CMssqlConnectionInformation::connectionDialog(const void *parentWindow)
{
	if(DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG1), (HWND)parentWindow, ConnectionDialogProc, (LPARAM)this)!=IDOK)
		return false;
	return true;
}
#endif
