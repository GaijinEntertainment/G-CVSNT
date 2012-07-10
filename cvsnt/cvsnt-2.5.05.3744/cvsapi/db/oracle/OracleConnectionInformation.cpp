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

#include "OracleConnectionInformation.h"

static char envstr1[200];
static char envstr2[200];
static char envstr3[200];

const char *COracleConnectionInformation::getVariable(const char *name)
{
	if(!strcmp(name,"home"))
		return home.c_str();
	if(!strcmp(name,"nls_nchar"))
		return nls_nchar.c_str();
	if(!strcmp(name,"nls_lang"))
		return nls_lang.c_str();
	if(!strcmp(name,"schema"))
		return schema.c_str();
	if(!strcmp(name,"SCHEMA"))  // Caps is script substitution variable
	{
		if(schema.size())
			return cvs::cache_static_string((schema+".").c_str());
		return "";
	}
	if(!strcmp(name,"tablespace"))
		return tablespace.c_str();
	if(!strcmp(name,"TABLESPACE"))
	{
		if(tablespace.size())
			return cvs::cache_static_string(("Tablespace "+tablespace).c_str());
		return "";
	}
	return CSqlConnectionInformation::getVariable(name);
}

bool COracleConnectionInformation::setVariable(const char *name, const char *value)
{
	CServerIo::trace(3,"COracleConnectionInformation::setVariable %s=%s",
				name, value);
	if(!strcmp(name,"home"))
	{
		cvs::string orahome;
		home = value;
		orahome = "ORACLE_HOME=" + home;
		if (orahome.length()<200)
		{
			strcpy(envstr1,orahome.c_str());
			putenv(envstr1);
			CServerIo::trace(3,"set environment variable %s", orahome.c_str());
		}
	}
	else if(!strcmp(name,"nls_lang"))
	{
		cvs::string ora_nls_lang;
		nls_lang = value;
		ora_nls_lang = "NLS_LANG=" + nls_lang;
		if (ora_nls_lang.length()<200)
		{
			strcpy(envstr2,ora_nls_lang.c_str());
			putenv(envstr2);
			CServerIo::trace(3,"set environment variable %s", ora_nls_lang.c_str());
		}
	}
	else if(!strcmp(name,"nls_nchar"))
	{
		cvs::string ora_nls_nchar;
		nls_nchar = value;
		ora_nls_nchar = "NLS_NCHAR=" + nls_nchar;
		if (ora_nls_nchar.length()<200)
		{
			strcpy(envstr3,ora_nls_nchar.c_str());
			putenv(envstr3);
			CServerIo::trace(3,"set environment variable %s", ora_nls_nchar.c_str());
		}
	}
	else if(!strcmp(name,"schema"))
		schema = value;
	else if(!strcmp(name,"tablespace"))
		tablespace = value;
	return CSqlConnectionInformation::setVariable(name,value);
}

const char *COracleConnectionInformation::enumVariableNames(size_t nVar)
{
	switch(nVar)
	{
	case firstDriverVariable:
		return "schema";
	case firstDriverVariable+1:
		return "nls_lang";
	case firstDriverVariable+2:
		return "nls_nchar";
	case firstDriverVariable+3:
		return "tablespace";
	case firstDriverVariable+4:
		return "home";
	default:
		return CSqlConnectionInformation::enumVariableNames(nVar);
	}
}

#ifndef _WIN32
bool COracleConnectionInformation::connectionDialog(const void *parentWindow)
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

BOOL CALLBACK COracleConnectionInformation::ConnectionDialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	COracleConnectionInformation *pCI = (COracleConnectionInformation*)GetWindowLongPtr(hWnd,GWLP_USERDATA);
	switch(uMsg)
	{
		case WM_INITDIALOG:
			pCI = (COracleConnectionInformation*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, lParam);
			SetDlgItemText(hWnd,IDC_DATABASE,cvs::wide(pCI->database.c_str()));
			SetDlgItemText(hWnd,IDC_SERVER,cvs::wide(pCI->hostname.c_str()));
			SetDlgItemText(hWnd,IDC_USERNAME,cvs::wide(pCI->username.c_str()));
			SetDlgItemText(hWnd,IDC_PASSWORD,cvs::wide(pCI->password.c_str()));
			SetDlgItemText(hWnd,IDC_SCHEMA,cvs::wide(pCI->schema.c_str()));
			SetDlgItemText(hWnd,IDC_TABLESPACE,cvs::wide(pCI->tablespace.c_str()));
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
					GetDlgItemText(hWnd, IDC_SCHEMA, szTmp, sizeof(szTmp)/sizeof(szTmp[0]));
					pCI->schema = cvs::narrow(szTmp);
					GetDlgItemText(hWnd, IDC_TABLESPACE, szTmp, sizeof(szTmp)/sizeof(szTmp[0]));
					pCI->tablespace = cvs::narrow(szTmp);
				}
				// Fall through
			case IDCANCEL:
				EndDialog(hWnd,wParam);
				return TRUE;
			}
	}
	return FALSE;
}

bool COracleConnectionInformation::connectionDialog(const void *parentWindow)
{
	if(DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG1), (HWND)parentWindow, ConnectionDialogProc, (LPARAM)this)!=IDOK)
		return false;
	return true;
}
#endif
