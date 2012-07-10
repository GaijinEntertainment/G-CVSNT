/*	cvsnt control panel
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
// cvsnt1.cpp: implementation of the CcvsntCPL class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "cvsnt.h"
#include "cvsnt1.h"
#include "serverPage.h"
#include "RepositoryPage.h"
#include "SettingsPage.h"
#include "CompatibiltyPage.h"
#include "AdvancedPage.h"
#include "ProtocolsPage.h"
#include "AboutPage.h"
#include "AdvertDialog.h"

bool g_bPrivileged;
HKEY g_hServerKey;
HKEY g_hInstallerKey;

/* All this is from the new SDK... remove once it's mainstream */

#ifndef Button_SetElevationRequiredState
#define BCM_SETSHIELD            (BCM_FIRST + 0x000C)
#define Button_SetElevationRequiredState(hwnd, fRequired) \
	(LRESULT)::SendMessage((hwnd), BCM_SETSHIELD, 0, (LPARAM)fRequired)
#endif

class CCplPropertySheet : public CPropertySheet
{
public:
	CCplPropertySheet(LPCTSTR szTitle) : CPropertySheet(szTitle) { }
	virtual ~CCplPropertySheet() { }
protected:
	virtual BOOL OnInitDialog()
	{
		BOOL bRet=CPropertySheet::OnInitDialog();

		OSVERSIONINFO vi = { sizeof(OSVERSIONINFO) };
		if(GetVersionEx(&vi) && (vi.dwMajorVersion>5 || (vi.dwMajorVersion==5 && vi.dwMinorVersion>2)))
		{
			CRect rect;
			GetDlgItem(IDOK)->GetWindowRect(&rect);
			ScreenToClient(&rect);
			btAdmin.Create(_T("Change Settings"),WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,CRect(6,rect.top,6+rect.Width()*2,rect.bottom),this,1111);
			btAdmin.SetFont(CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT)));
			Button_SetElevationRequiredState(btAdmin.m_hWnd,TRUE);
			if(CGlobalSettings::isAdmin())
				btAdmin.EnableWindow(FALSE);
		}

		CAdvertDialog dlg;
		dlg.DoModal();

		return bRet;
	}

	void OnElevate()
	{
		// This isn't documented but seems to work in the beta2.
		TCHAR fn[MAX_PATH],cmd[MAX_PATH*2];
		GetModuleFileName(AfxGetApp()->m_hInstance,fn,MAX_PATH);
		TCHAR *find_underscore;
		find_underscore=_tcsstr(fn,_T("_cvsnt.cpl"));
		if (find_underscore!=NULL)
			_tcscpy(_T("cvsnt.cpl\0"),find_underscore);
		_sntprintf(cmd,sizeof(cmd)/sizeof(cmd[0]),_T("shell32.dll,Control_RunDLL \"%s\""),fn);
		if(ShellExecute(m_hWnd,_T("open"),_T("RunLegacyCPLElevated.exe"),cmd,NULL,SW_SHOWNORMAL)>(HINSTANCE)32)
			PostMessage(WM_COMMAND,IDCANCEL);
	}

	CButton btAdmin;

	DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CCplPropertySheet,CPropertySheet)
	ON_BN_CLICKED(1111,OnElevate)
END_MESSAGE_MAP()

 //////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CcvsntCPL::CcvsntCPL()
{
	CoInitialize(NULL);
	InitCommonControls();
}

CcvsntCPL::~CcvsntCPL()
{
}

BOOL CcvsntCPL::DoubleClick(UINT uAppNum, LONG lData)
{
	WSADATA wsa = {0};
	WSAStartup(MAKEWORD(2,0),&wsa);
	CString tmp;

	if(CGlobalSettings::isAdmin())
	{
		if(RegCreateKeyEx(HKEY_LOCAL_MACHINE,_T("Software\\CVS\\Pserver"),NULL,_T(""),REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&g_hServerKey,NULL))
		{ 
			tmp.Format(_T("Couldn't create HKLM\\Software\\CVS\\Pserver key, error %d\n"),GetLastError());
			AfxMessageBox(tmp,MB_ICONSTOP);
			return FALSE;
		}
		g_bPrivileged=true;
	}
	else
	{
		if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,_T("Software\\CVS\\Pserver"),0,KEY_READ,&g_hServerKey))
		{ 
			tmp.Format(_T("CVSNT Server registry entries cannot be opened.  Quitting.\n"),GetLastError());
			AfxMessageBox(tmp,MB_ICONSTOP);
			return FALSE;
		}
		g_bPrivileged=false;
	}

	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,_T("Software\\March Hare Software Ltd\\Keys"),0,KEY_READ,&g_hInstallerKey))
		g_hInstallerKey=NULL;

	CCplPropertySheet sheet(g_bPrivileged?_T("CVSNT Low Performance Server - Community Edition"):_T("CVSNT Low Performance Server (Read Only) - Community Edition"));

	sheet.m_psh.hIcon = AfxGetApp()->LoadIcon(IDI_ICON1);
	sheet.m_psh.dwFlags |= PSH_USEHICON;

	sheet.AddPage(new CserverPage);
	sheet.AddPage(new CRepositoryPage);
	sheet.AddPage(new CSettingsPage);
	sheet.AddPage(new CCompatibiltyPage);
	sheet.AddPage(new CProtocolsPage);
	sheet.AddPage(new CAdvancedPage);
	sheet.AddPage(new CAboutPage);
	sheet.DoModal();

	RegCloseKey(g_hServerKey);
	return TRUE;
}

BOOL CcvsntCPL::Exit()
{
	return TRUE;
}

LONG CcvsntCPL::GetCount()
{
	return 1;
}

BOOL CcvsntCPL::Init()
{
	return TRUE;
}

BOOL CcvsntCPL::Inquire(UINT uAppNum, LPCPLINFO lpcpli)
{
	lpcpli->idIcon=IDI_ICON1;
	lpcpli->idInfo=IDS_DESCRIPTION;
	lpcpli->idName=IDS_NAME;
	return TRUE;
}

BOOL CcvsntCPL::NewInquire(UINT uAppNum, LPNEWCPLINFO lpcpli)
{
	return FALSE;
}

BOOL CcvsntCPL::Stop(UINT uAppNum, LONG lData)
{
	return TRUE;
}

/* Generic SHBrowseForFolder callback */
int CALLBACK BrowseValid(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	switch(uMsg)
	{
	case BFFM_INITIALIZED:
		return 0;
	case BFFM_SELCHANGED:
		{
			TCHAR fn[MAX_PATH];
			TCHAR shortfn[4];
			LPITEMIDLIST idl = (LPITEMIDLIST)lParam;
			BOOL bOk = SHGetPathFromIDList(idl,fn);
			_tcsncpy(shortfn,fn,3);
			shortfn[3]='\0';
			if(bOk && (!_tcsnicmp(fn,_T("\\\\"),2) || !_tcsnicmp(fn,_T("//"),2)))
			{
				bOk=FALSE;
				SendMessage(hWnd,BFFM_SETSTATUSTEXT,NULL,(LPARAM)"UNC Paths are not allowed");
			}
			if(bOk && GetDriveType(shortfn)==DRIVE_REMOTE)
			{
				bOk=FALSE;
				SendMessage(hWnd,BFFM_SETSTATUSTEXT,NULL,(LPARAM)"Network drives are not allowed");
			}
			SendMessage(hWnd,BFFM_ENABLEOK,NULL,bOk);
		}
		return 0;
	case BFFM_VALIDATEFAILED:
		return -1;
	}
	return 0;
}
