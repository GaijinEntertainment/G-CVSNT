/*	cvsnt control panel
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
// AboutPage.cpp : implementation file
//

#include "stdafx.h"
#include "cvsnt.h"
#include "AboutPage.h"
#include "NewRootDialog.h"

#define ServiceName _T("CVSNT")
#define ServiceName2 _T("CVSLOCK")

/////////////////////////////////////////////////////////////////////////////
// CAboutPage property page

IMPLEMENT_DYNCREATE(CAboutPage, CTooltipPropertyPage)

CAboutPage::CAboutPage() : CTooltipPropertyPage(CAboutPage::IDD)
//, m_szSshStatus(_T(""))
{
	m_szVersion = "CVSNT " CVSNT_PRODUCTVERSION_STRING;
	//{{AFX_DATA_INIT(CAboutPage)
	m_szStatus = _T("");
	m_szLockStatus =_T("");
	 //}}AFX_DATA_INIT
	m_hService=m_hLockService=m_hSCManager=NULL;
}

CAboutPage::~CAboutPage()
{
}

void CAboutPage::DoDataExchange(CDataExchange* pDX)
{
	CTooltipPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutPage)
	DDX_Control(pDX, IDC_START, m_btStart);
	DDX_Control(pDX, IDC_STOP, m_btStop);
	DDX_Text(pDX, IDC_VERSION, m_szVersion);
	DDX_Text(pDX, IDC_STATUS, m_szStatus);
	DDX_Text(pDX, IDC_STATUS2, m_szLockStatus);
	DDX_Control(pDX, IDC_START2, m_btLockStart);
	DDX_Control(pDX, IDC_STOP2, m_btLockStop);
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_LOGO, m_cbLogo);
}


BEGIN_MESSAGE_MAP(CAboutPage, CTooltipPropertyPage)
	//{{AFX_MSG_MAP(CAboutPage)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_START, OnStart)
	ON_BN_CLICKED(IDC_STOP, OnStop)
	ON_BN_CLICKED(IDC_START2, OnBnClickedStart2)
	ON_BN_CLICKED(IDC_STOP2, OnBnClickedStop2)
	//}}AFX_MSG_MAP
	ON_STN_CLICKED(IDC_LOGO, OnStnClickedLogo)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAboutPage message handlers

BOOL CAboutPage::OnInitDialog() 
{
	CTooltipPropertyPage::OnInitDialog();
	CString tmp;

	m_hSCManager=OpenSCManager(NULL,NULL,g_bPrivileged?GENERIC_EXECUTE:GENERIC_READ);
	if(!m_hSCManager)
	{
		CString tmp;
		DWORD e=GetLastError();

		if(e==5)
		{
			tmp.Format(_T("Couldn't open service control manager - Permission Denied"));
			AfxMessageBox(tmp,MB_ICONSTOP);
			GetParent()->PostMessage(WM_CLOSE);
		}
		else
		{
			tmp.Format(_T("Couldn't open service control manager - error %d"),GetLastError());
			AfxMessageBox(tmp,MB_ICONSTOP);
			GetParent()->PostMessage(WM_CLOSE);
		}
	}
	
	if(g_bPrivileged)
	{
		m_hService=OpenService(m_hSCManager,ServiceName,SERVICE_QUERY_STATUS|SERVICE_START|SERVICE_STOP);
		m_hLockService=OpenService(m_hSCManager,ServiceName2,SERVICE_QUERY_STATUS|SERVICE_START|SERVICE_STOP);
	}
	else
	{
		m_hService=OpenService(m_hSCManager,ServiceName,SERVICE_QUERY_STATUS);
		m_hLockService=OpenService(m_hSCManager,ServiceName2,SERVICE_QUERY_STATUS);
	}

	UpdateStatus();
	SetTimer(0,1000,NULL);

	CBitmap *pBitmap = new CBitmap;
	pBitmap->LoadBitmap(IDB_BITMAP1);
	m_cbLogo.SetBitmap(*pBitmap);
	pBitmap->Detach();
	delete pBitmap;

	return TRUE; 
}

void CAboutPage::UpdateStatus()
{
	SERVICE_STATUS stat = {0};

	if(!m_hService)
	{
		m_szStatus="Service not installed";
		m_btStart.EnableWindow(FALSE);
		m_btStop.EnableWindow(FALSE);
	}
	else
	{
		QueryServiceStatus(m_hService,&stat);
		switch(stat.dwCurrentState)
		{
			case SERVICE_STOPPED:
				m_szStatus="Stopped";
				m_btStart.EnableWindow(g_bPrivileged?TRUE:FALSE);
				m_btStop.EnableWindow(FALSE);
				break;
			case SERVICE_START_PENDING:
				m_szStatus="Starting";
				m_btStart.EnableWindow(FALSE);
				m_btStop.EnableWindow(FALSE);
				break;
			case SERVICE_STOP_PENDING:
				m_szStatus="Stopping";
				m_btStart.EnableWindow(FALSE);
				m_btStop.EnableWindow(FALSE);
				break;
			case SERVICE_RUNNING:
				m_szStatus="Running";
				m_btStart.EnableWindow(FALSE);
				m_btStop.EnableWindow(g_bPrivileged?TRUE:FALSE);
				break;
			default:
				m_szStatus="Unknown state";
				m_btStart.EnableWindow(FALSE);
				m_btStop.EnableWindow(FALSE);
				break;
		}
	}

	if(!m_hLockService)
	{
		m_szLockStatus="Service not installed";
		m_btLockStart.EnableWindow(FALSE);
		m_btLockStop.EnableWindow(FALSE);
	}
	else
	{
		QueryServiceStatus(m_hLockService,&stat);
		switch(stat.dwCurrentState)
		{
			case SERVICE_STOPPED:
				m_szLockStatus="Stopped";
				m_btLockStart.EnableWindow(g_bPrivileged?TRUE:FALSE);
				m_btLockStop.EnableWindow(FALSE);
				break;
			case SERVICE_START_PENDING:
				m_szLockStatus="Starting";
				m_btLockStart.EnableWindow(FALSE);
				m_btLockStop.EnableWindow(FALSE);
				break;
			case SERVICE_STOP_PENDING:
				m_szLockStatus="Stopping";
				m_btLockStart.EnableWindow(FALSE);
				m_btLockStop.EnableWindow(FALSE);
				break;
			case SERVICE_RUNNING:
				m_szLockStatus="Running";
				m_btLockStart.EnableWindow(FALSE);
				m_btLockStop.EnableWindow(g_bPrivileged?TRUE:FALSE);
				break;
			default:
				m_szLockStatus="Unknown state";
				m_btLockStart.EnableWindow(FALSE);
				m_btLockStop.EnableWindow(FALSE);
				break;
		}
	}

	UpdateData(FALSE);
}

void CAboutPage::OnTimer(UINT nIDEvent) 
{
	UpdateStatus();
}

void CAboutPage::OnStart() 
{
	m_btStart.EnableWindow(FALSE);
	if(!StartService(m_hService,0,NULL))
	{
		CString tmp;
		tmp.Format(_T("Couldn't start service: %s"),GetErrorString());
		AfxMessageBox(tmp,MB_ICONSTOP);
	}
	UpdateStatus();
}

void CAboutPage::OnStop() 
{
	SERVICE_STATUS stat = {0};

	m_btStop.EnableWindow(FALSE);
	if(!ControlService(m_hService,SERVICE_CONTROL_STOP,&stat))
	{
		CString tmp;
		tmp.Format(_T("Couldn't stop service: %s"),GetErrorString());
		AfxMessageBox(tmp,MB_ICONSTOP);
	}
	UpdateStatus();
}

LPCTSTR CAboutPage::GetErrorString()
{
	static TCHAR ErrBuf[1024];

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

void CAboutPage::OnBnClickedStart2()
{
	m_btLockStart.EnableWindow(FALSE);
	if(!StartService(m_hLockService,0,NULL))
	{
		CString tmp;
		tmp.Format(_T("Couldn't start service: %s"),GetErrorString());
		AfxMessageBox(tmp,MB_ICONSTOP);
	}
	UpdateStatus();
}

void CAboutPage::OnBnClickedStop2()
{
	SERVICE_STATUS stat = {0};

	m_btLockStop.EnableWindow(FALSE);
	if(!ControlService(m_hLockService,SERVICE_CONTROL_STOP,&stat))
	{
		CString tmp;
		tmp.Format(_T("Couldn't stop service: %s"),GetErrorString());
		AfxMessageBox(tmp,MB_ICONSTOP);
	}
	UpdateStatus();
}

void CAboutPage::OnStnClickedLogo()
{
	ShellExecute(NULL,_T("open"),_T("http://www.march-hare.com/cvspro"),NULL,NULL,SW_SHOWNORMAL);
}
