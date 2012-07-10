/*	cvsnt agent
    Copyright (C) 2004-7 Tony Hoyle and March-Hare Software Ltd

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
// cvsagent.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "cvsagent.h"
#include "aboutdialog.h"
#include "PasswordDialog.h"
#include "Scramble.h"
#include <aclapi.h>

struct wide
{
	wide(const char *str) { utf82ucs2(str); }
	operator const wchar_t *() { return w_str.c_str(); }
	std::wstring w_str;
	void utf82ucs2(const char *src)
	{
		unsigned char *p=(unsigned char *)src;
		wchar_t ch;
		w_str.reserve(strlen(src));
		while(*p)
		{
			if(*p<0x80) { ch = p[0]; p++; }
			else if(*p<0xe0) { ch = ((p[0]&0x3f)<<6)+(p[1]&0x3f); p+=2; }
			else if(*p<0xf0) { ch = ((p[0]&0x1f)<<12)+((p[1]&0x3f)<<6)+(p[2]&0x3f); p+=3; }
			else if(*p<0xf8) { ch = ((p[0]&0x0f)<<18)+((p[1]&0x3f)<<12)+((p[2]&0x3f)<<6)+(p[3]&0x3f); p+=4; }
			else if(*p<0xfc) { ch = ((p[0]&0x07)<<24)+((p[1]&0x3f)<<18)+((p[2]&0x3f)<<12)+((p[3]&0x3f)<<6)+(p[4]&0x3f); p+=5; }
			else if(*p<0xfe) { ch = ((p[0]&0x03)<<30)+((p[1]&0x3f)<<24)+((p[2]&0x3f)<<18)+((p[3]&0x3f)<<12)+((p[4]&0x3f)<<6)+(p[5]&0x3f); p+=6; }
			else { ch = '?'; p++; }
			w_str+=(wchar_t)ch;
		}
	}
};

class CAgentWnd : public CWnd
{
public:
	CAgentWnd(bool u3)
	{
#ifdef HAVE_U3
		m_hSession = m_hCallback = m_hDevice = NULL;
		m_dapiAvailable = false;
#endif
		m_bStorePassword = u3?true:false;
		m_bTopmost = false;
#ifdef HAVE_U3
		Register();
#endif
	}

	void ShowContextMenu();

	afx_msg LRESULT OnTrayMessage(WPARAM wParam,LPARAM lParam);
	afx_msg BOOL OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct);
	afx_msg void OnAbout();
	afx_msg void OnQuit();
	afx_msg void OnClearPasswords();
	afx_msg void OnAlwaysOnTop();
	afx_msg void OnStorePasswords();

	DECLARE_MESSAGE_MAP();

protected:
	std::map<std::string,std::string> m_Passwords;

	bool m_bTopmost;
	bool m_bStorePassword;
#ifdef HAVE_U3
	bool m_dapiAvailable;
	HSESSION m_hSession;
	HCALLBACK m_hCallback;
	HDEVICE m_hDevice;

	static void CALLBACK _dapiCallback(HDEVICE hDev, DWORD eventType, void* pEx);
	void dapiCallback(HDEVICE hDev, DWORD eventType);

	bool Register();
	bool Unregister();
#endif
};

BEGIN_MESSAGE_MAP(CAgentWnd,CWnd)
ON_MESSAGE(TRAY_MESSAGE,OnTrayMessage)
ON_COMMAND(ID_ABOUT,OnAbout)
ON_COMMAND(ID_QUIT,OnQuit)
ON_COMMAND(ID_CLEARPASSWORDS,OnClearPasswords)
ON_COMMAND(ID_ALWAYSONTOP,OnAlwaysOnTop)
ON_COMMAND(ID_STOREPASSWORDS,OnStorePasswords)
ON_WM_COPYDATA()
END_MESSAGE_MAP()

class CAgentApp : public CWinApp
{
	virtual BOOL InitInstance();
	virtual BOOL ExitInstance();
};

CAgentApp app;

BOOL CAgentApp::InitInstance()
{
	bool bU3 = false;

	TCHAR *p = _tcsrchr(GetCommandLine(),' ');
	if(p && !_tcscmp(p+1,_T("-stop")))
	{
		CWnd *pWnd = CWnd::FindWindow(_T("CvsAgent"),NULL);
		while(pWnd)
		{
			pWnd->SendMessage(WM_COMMAND,ID_QUIT);
			Sleep(500);
			pWnd = CWnd::FindWindow(_T("CvsAgent"),NULL);
		}
		return FALSE;
	}
	else if(p && !_tcscmp(p+1,_T("-u3")))
		bU3 = true;
	else if(p && !_tcscmp(p+1,_T("-uninstall")))
		return FALSE;

	CWnd *pWnd = CWnd::FindWindow(NULL,_T("CvsAgent"));
	if(pWnd)
		return FALSE;

	WNDCLASS wc = {0};

	wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = AfxWndProc;
    wc.hInstance = AfxGetInstanceHandle();
    wc.hIcon = LoadIcon(IDI_CVSAGENT);
    wc.hCursor = LoadCursor( IDC_ARROW );
    wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = _T("CvsAgent");
	AfxRegisterClass(&wc);

	m_pMainWnd = new CAgentWnd(bU3);
	m_pMainWnd->CreateEx(0,_T("CvsAgent"),_T("Cvs Agent"),WS_OVERLAPPED,CRect(1,1,1,1),NULL,0);

	NOTIFYICONDATA nid = { sizeof(NOTIFYICONDATA) };
	nid.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;
	nid.uCallbackMessage=TRAY_MESSAGE;
	nid.hIcon=LoadIcon(IDI_CVSAGENT);
	nid.uID=IDI_CVSAGENT;
	_tcscpy(nid.szTip,_T("CVS password agent"));
	nid.hWnd=m_pMainWnd->GetSafeHwnd();
	Shell_NotifyIcon(NIM_ADD,&nid);
	return TRUE;
}

BOOL CAgentApp::ExitInstance()
{
	NOTIFYICONDATA nid = { sizeof(NOTIFYICONDATA) };
	nid.hWnd=m_pMainWnd->GetSafeHwnd();
	nid.uID=IDI_CVSAGENT;
	Shell_NotifyIcon(NIM_DELETE,&nid);
	return TRUE;
}

LRESULT CAgentWnd::OnTrayMessage(WPARAM wParam, LPARAM lParam)
{
	switch(lParam)
	{
	case WM_LBUTTONDBLCLK:
		break;
	case WM_RBUTTONDOWN:
	case WM_CONTEXTMENU:
		ShowContextMenu();
		break;
	}
	
	return 0;
}

void CAgentWnd::ShowContextMenu()
{
	CMenu menu;
	CPoint pt;
	menu.LoadMenu(IDR_MENU1);
	SetForegroundWindow();
	GetCursorPos(&pt);
	CMenu *sub = menu.GetSubMenu(0);
	sub->CheckMenuItem(ID_ALWAYSONTOP,m_bTopmost?MF_CHECKED:MF_UNCHECKED);
#ifdef HAVE_U3
	sub->EnableMenuItem(ID_STOREPASSWORDS,m_dapiAvailable?MF_ENABLED:MF_GRAYED);
	sub->CheckMenuItem(ID_STOREPASSWORDS,m_bStorePassword?MF_CHECKED:MF_UNCHECKED);
#else
	sub->EnableMenuItem(ID_STOREPASSWORDS,MF_GRAYED);
#endif
	sub->TrackPopupMenu(TPM_BOTTOMALIGN|TPM_CENTERALIGN|TPM_LEFTBUTTON,pt.x,pt.y,this);
}

void CAgentWnd::OnAbout()
{
	CAboutDialog dlg;
	dlg.DoModal();
}

void CAgentWnd::OnQuit()
{
	PostQuitMessage(0);
}

void CAgentWnd::OnClearPasswords()
{
	m_Passwords.clear();
#ifdef HAVE_U3
	if(m_dapiAvailable && m_bStorePassword)
	{
		dapiDeleteCookie(m_hDevice,L"CvsAgent",NULL);
		BYTE av = m_bStorePassword?1:0;
		HRESULT res = dapiWriteBinaryCookie(m_hDevice,L"CvsAgent",L"_StorePasswords",&av,1);
	}
#endif
}

void CAgentWnd::OnAlwaysOnTop()
{
	m_bTopmost=!m_bTopmost;
}

void CAgentWnd::OnStorePasswords()
{
	m_bStorePassword=!m_bStorePassword;
#ifdef HAVE_U3
	if(m_dapiAvailable)
	{
		BYTE av = m_bStorePassword?1:0;
		HRESULT res = dapiWriteBinaryCookie(m_hDevice,L"CvsAgent",L"_StorePasswords",&av,1);
	}
#endif
}

/*LRESULT CAgentWnd::OnPasswordMessage(WPARAM wParam,LPARAM lParam)
{
	char *buffer = (char*)wParam;
	int *len = (int*)lParam;
	CScramble scramble;

	if(m_Passwords.find(buffer)!=m_Passwords.end())
	{
		*len = (int)m_Passwords[buffer].size();
		strcpy(buffer,m_Passwords[buffer].c_str());
		return 1;
	}

#ifdef HAVE_U3
	if(m_dapiAvailable && m_bStorePassword)
	{
		BYTE buf[BUFSIZ];
		DWORD bufLen = sizeof(buf);
		if(dapiReadBinaryCookie(m_hDevice,L"CvsAgent",wide(buffer),buf,&bufLen)==S_OK)
		{
			*len = (int)bufLen;
			memcpy(buffer,buf,bufLen);
			buffer[bufLen]='\0';
			return 1;
		}
	}
#endif

	CPasswordDialog dlg(m_bTopmost);
	dlg.m_szCvsRoot=buffer;
	if(dlg.DoModal()!=IDOK)
		return 0;
	strcpy(buffer,scramble.Scramble(dlg.m_szPassword.c_str()));
	*len=(int)strlen(buffer);

#ifdef HAVE_U3
	if(m_dapiAvailable && m_bStorePassword)
	{
		dapiWriteBinaryCookie(m_hDevice, L"CvsAgent", wide(dlg.m_szCvsRoot.c_str()), (BYTE*)buffer, *len);
	}
	else
#endif
		m_Passwords[dlg.m_szCvsRoot]=buffer;

	return 1;
}
*/

BOOL CAgentWnd::OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct)
{
	struct _str
	{
		int state; // state=0, return ok, =1, get password, =2, set password, =3, delete password
		char key[256];
		char rslt[256];
	};
	DWORD dwErr;
	CScramble scramble;

	HANDLE hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS,FALSE,(LPCSTR)pCopyDataStruct->lpData);
	if(hMap==INVALID_HANDLE_VALUE)
		return FALSE;

	PSECURITY_DESCRIPTOR ownerSd;
	PSID ownerId;
	if((dwErr=GetSecurityInfo(hMap, SE_KERNEL_OBJECT, OWNER_SECURITY_INFORMATION, &ownerId, NULL, NULL, NULL, &ownerSd))!=ERROR_SUCCESS)
		return FALSE;

	PSECURITY_DESCRIPTOR processSd;
	PSID processId;
	HANDLE hProcess = OpenProcess(READ_CONTROL, FALSE, GetCurrentProcessId());
	if((dwErr=GetSecurityInfo(hProcess, SE_KERNEL_OBJECT, OWNER_SECURITY_INFORMATION, &processId, NULL, NULL, NULL, &processSd))!=ERROR_SUCCESS)
	{
		LocalFree((HLOCAL)ownerSd);
		return FALSE;
	}

	if(!EqualSid(ownerId, processId))
	{
		LocalFree((HLOCAL)ownerSd);
		LocalFree((HLOCAL)processSd);
		return FALSE;
	}

	LocalFree((HLOCAL)ownerSd);
	LocalFree((HLOCAL)processSd);

	_str* pMap = (_str*)MapViewOfFile(hMap,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(_str));
	if(!pMap)
		return FALSE;

	switch(pMap->state)
	{
	case 1: // get password
		if(m_Passwords.find(pMap->key)!=m_Passwords.end())
		{
			strcpy(pMap->rslt,m_Passwords[pMap->key].c_str());
			pMap->state=0;
			break;
		}

#ifdef HAVE_U3
		if(m_dapiAvailable && m_bStorePassword)
		{
			BYTE buf[BUFSIZ];
			DWORD bufLen = sizeof(buf);
			if(dapiReadBinaryCookie(m_hDevice,L"CvsAgent",wide(pMap->key),buf,&bufLen)==S_OK)
			{
				memcpy(pMap->rslt,buf,bufLen);
				pMap->rslt[bufLen]='\0';
				pMap->state=0;
				break;
			}
		}
#endif

		{
		CPasswordDialog dlg(m_bTopmost);
		dlg.m_szCvsRoot=pMap->key;
		if(dlg.DoModal()!=IDOK)
		{
			UnmapViewOfFile(pMap);
			CloseHandle(hMap);
			return 0;
		}
		strcpy(pMap->rslt,scramble.Scramble(dlg.m_szPassword.c_str()));

	#ifdef HAVE_U3
		if(m_dapiAvailable && m_bStorePassword)
			dapiWriteBinaryCookie(m_hDevice, L"CvsAgent", wide(dlg.m_szCvsRoot.c_str()), (BYTE*)pMap->rslt, strlen(pMap->rslt)+1);
	#endif
		m_Passwords[dlg.m_szCvsRoot]=pMap->rslt;
		}
		pMap->state=0;
		break;
	case 2: // set password
	#ifdef HAVE_U3
		if(m_dapiAvailable && m_bStorePassword)
			dapiWriteBinaryCookie(m_hDevice, L"CvsAgent", wide(pMap->key), (BYTE*)pMap->rslt, strlen(pMap->rslt)+1);
	#endif
		m_Passwords[pMap->key]=pMap->rslt;
		pMap->state=0;
		break;
	case 3: // delete password
		if(m_Passwords.find(pMap->key)!=m_Passwords.end())
			m_Passwords.erase(m_Passwords.find(pMap->key));

#ifdef HAVE_U3
		if(m_dapiAvailable && m_bStorePassword)
			dapiDeleteCookie(m_hDevice, L"CvsAgent", wide(pMap->key));
#endif
		pMap->state=0;
		break;
	default:
		UnmapViewOfFile(pMap);
		CloseHandle(hMap);
		return FALSE;
	};
	UnmapViewOfFile(pMap);
	CloseHandle(hMap);
	return TRUE;
}

#ifdef HAVE_U3
bool CAgentWnd::Register()
{
	m_dapiAvailable = false;

	__try
	{
		if(dapiCreateSession(&m_hSession)==S_OK)
		{
			if(dapiRegisterCallback(m_hSession, _T(""), _dapiCallback, this, &m_hCallback)!=S_OK)
				Unregister();
		}
		else
			Unregister();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

    return true;
}

bool CAgentWnd::Unregister()
{
	__try
	{
		if(m_hCallback)
			dapiUnregisterCallback(m_hCallback);
		if(m_hSession)
			dapiDestroySession(m_hSession);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}

	m_hCallback=m_hSession=NULL;
	m_dapiAvailable = false;
	return m_dapiAvailable;
}

void CAgentWnd::_dapiCallback(HDEVICE hDev, DWORD eventType, void* pEx)
{
	((CAgentWnd*)pEx)->dapiCallback(hDev,eventType);
}

void CAgentWnd::dapiCallback(HDEVICE hDev, DWORD eventType)
{
	switch(eventType)
	{
		case DAPI_EVENT_DEVICE_CONNECT:
		case DAPI_EVENT_DEVICE_RECONNECT:
		case DAPI_EVENT_DEVICE_NEW_CONFIG:
			m_dapiAvailable = false;
			if(dapiQueryDeviceCapability(hDev, DAPI_CAP_U3_COOKIE)==S_OK)
			{
				BYTE av;
				DWORD l = 1;

				m_dapiAvailable = true;
				m_bStorePassword = false;
				if(dapiReadBinaryCookie(hDev,L"CvsAgent",L"_StorePasswords",&av,&l)==S_OK)
					m_bStorePassword=av?true:false;
			}
			m_hDevice = hDev;
			break;
		case DAPI_EVENT_DEVICE_DISCONNECT:
			m_dapiAvailable = false;
			m_hDevice = NULL;
			break;
	}
}
#endif
