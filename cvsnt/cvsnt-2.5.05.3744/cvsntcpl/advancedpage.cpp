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
// AdvancedPage.cpp : implementation file
//

#include "stdafx.h"
#include "cvsnt.h"
#include "AdvancedPage.h"

#include <lm.h>
#include <ntsecapi.h>
#include <dsgetdc.h>
#include ".\advancedpage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAdvancedPage property page

IMPLEMENT_DYNCREATE(CAdvancedPage, CTooltipPropertyPage)

CAdvancedPage::CAdvancedPage() : CTooltipPropertyPage(CAdvancedPage::IDD)
{
	//{{AFX_DATA_INIT(CAdvancedPage)
	//}}AFX_DATA_INIT
}

CAdvancedPage::~CAdvancedPage()
{
}

void CAdvancedPage::DoDataExchange(CDataExchange* pDX)
{
	CTooltipPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAdvancedPage)
	DDX_Control(pDX, IDC_NOREVERSEDNS, m_btNoReverseDns);
	DDX_Control(pDX, IDC_ALLOWTRACE, m_btAllowTrace);
	DDX_Control(pDX, IDC_LOCKSERVERLOCAL, m_btLockServerLocal);
	DDX_Control(pDX, IDC_UNICODESERVER, m_btUnicodeServer);
	DDX_Control(pDX, IDC_READONLY, m_btReadOnly);
	DDX_Control(pDX, IDC_REMOTEINIT, m_btRemoteInit);
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_ZEROCONF, m_cbZeroconf);
	DDX_Control(pDX, IDC_ATOMICCHECKOUTS, m_cbAtomicCheckouts);
	DDX_Control(pDX, IDC_GLOBALSCRIPT, m_cbGlobalScript);
	DDX_Control(pDX, IDC_REPLICATION, m_btReplication);
	DDX_Control(pDX, IDC_REPLICATIONPORT, m_edReplicationPort);
	DDX_Control(pDX, IDC_REPLICATIONPORT_TEXT, m_stReplicationPort);
}


BEGIN_MESSAGE_MAP(CAdvancedPage, CTooltipPropertyPage)
	//{{AFX_MSG_MAP(CAdvancedPage)
	ON_BN_CLICKED(IDC_NOREVERSEDNS, OnBnClickedNoreversedns)
	ON_BN_CLICKED(IDC_ALLOWTRACE, OnBnClickedAllowtrace)
	ON_BN_CLICKED(IDC_LOCKSERVERLOCAL, OnBnClickedLockserverlocal)
	ON_BN_CLICKED(IDC_UNICODESERVER, OnBnClickedUnicodeserver)
	ON_BN_CLICKED(IDC_READONLY, OnBnClickedReadonly)
	ON_BN_CLICKED(IDC_REMOTEINIT, OnBnClickedRemoteinit)
	//}}AFX_MSG_MAP
	ON_CBN_SELENDOK(IDC_ZEROCONF, OnCbnSelendokZeroconf)
	ON_BN_CLICKED(IDC_ATOMICCHECKOUTS, OnBnClickedAtomiccheckouts)
	ON_BN_CLICKED(IDC_GLOBALSCRIPT, OnBnClickedGlobalscript)
	ON_CBN_SELCHANGE(IDC_ZEROCONF, OnCbnSelchangeZeroconf)
	ON_BN_CLICKED(IDC_REPLICATION, OnBnClickedReplication)
	ON_EN_CHANGE(IDC_REPLICATIONPORT, OnEnChangeReplicationport)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAdvancedPage message handlers

BOOL CAdvancedPage::OnInitDialog()
{
	int t,up;
	CWaitCursor wait;

	CTooltipPropertyPage::OnInitDialog();

	m_btNoReverseDns.SetCheck((t=QueryDword(_T("NoReverseDns")))>=0?t:0);
	m_btLockServerLocal.SetCheck((t=QueryDword(_T("LockServerLocal")))>=0?t:1);
	m_btAllowTrace.SetCheck((t=QueryDword(_T("AllowTrace")))>=0?t:0);
  	// should the default be UTF8 if no other is specified?
	m_btUnicodeServer.SetCheck(1);
	m_btReadOnly.SetCheck((t=QueryDword(_T("ReadOnlyServer")))>=0?t:0);
	m_btRemoteInit.SetCheck((t=QueryDword(_T("RemoteInit")))>=0?t:0);
	m_cbAtomicCheckouts.SetCheck((t=QueryDword(_T("AtomicCheckouts")))>=0?t:0);
	m_cbGlobalScript.SetCheck((t=QueryDword(_T("GlobalScriptTrigger")))>=0?t:0);

	TCHAR p[32];
	t=QueryDword(_T("UnisonPort"));
	if(t>=0)
	{
		m_edReplicationPort.SetWindowText(_itot(t,p,10));
		up=1;
	}
	else
		up=0;

	CString s = QueryString(_T("InstallPath"));
	if(s.GetLength()) s+=_T("\\");
	s+="unison.exe";
	if(CFileAccess::exists(cvs::narrow(s)))
	{
		m_btReplication.EnableWindow(TRUE);
		if((t=QueryDword(_T("EnableUnison")))>=0)
			; //t=t
		else
			t=up;
	}
	else
	{
		m_btReplication.EnableWindow(FALSE);
		t=0;
	}
	m_btReplication.SetCheck(t);
	if(!t)
	{
		m_stReplicationPort.EnableWindow(FALSE);
		m_edReplicationPort.EnableWindow(FALSE);
	}
	
	m_cbZeroconf.ResetContent();
	m_cbZeroconf.AddString(_T("None"));
	m_cbZeroconf.AddString(_T("Internal"));
	m_cbZeroconf.AddString(_T("Apple"));
	m_cbZeroconf.AddString(_T("Howl"));
	t=QueryDword(_T("ResponderType"));
	if(t>=0)
		m_cbZeroconf.SetCurSel(t);
	else
	{
		t=QueryDword(_T("EnableZeroconf"));
		if(t>=0)
			m_cbZeroconf.SetCurSel(t);
		else
			m_cbZeroconf.SetCurSel(1);
	}

	m_btUnicodeServer.EnableWindow(FALSE);
	if(!g_bPrivileged)
	{
		m_btNoReverseDns.EnableWindow(FALSE);
		m_btAllowTrace.EnableWindow(FALSE);
		m_btLockServerLocal.EnableWindow(FALSE);
		m_btReadOnly.EnableWindow(FALSE);
		m_btRemoteInit.EnableWindow(FALSE);
		m_cbZeroconf.EnableWindow(FALSE);
		m_cbAtomicCheckouts.EnableWindow(FALSE);
		m_cbGlobalScript.EnableWindow(FALSE);
		m_btReplication.EnableWindow(FALSE);
		m_edReplicationPort.EnableWindow(FALSE);
		m_stReplicationPort.EnableWindow(FALSE);
	}

	return TRUE;
}

BOOL CAdvancedPage::OnApply()
{
	DWORD dwVal;

	dwVal=m_btNoReverseDns.GetCheck()?1:0;

	if(RegSetValueEx(g_hServerKey,_T("NoReverseDns"),NULL,REG_DWORD,(BYTE*)&dwVal,sizeof(DWORD)))
		AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);

	dwVal=m_btLockServerLocal.GetCheck()?1:0;

	if(RegSetValueEx(g_hServerKey,_T("LockServerLocal"),NULL,REG_DWORD,(BYTE*)&dwVal,sizeof(DWORD)))
		AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);

	dwVal=m_btAllowTrace.GetCheck()?1:0;

	if(RegSetValueEx(g_hServerKey,_T("AllowTrace"),NULL,REG_DWORD,(BYTE*)&dwVal,sizeof(DWORD)))
		AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);

	dwVal=m_btReadOnly.GetCheck()?1:0;

	if(RegSetValueEx(g_hServerKey,_T("ReadOnlyServer"),NULL,REG_DWORD,(BYTE*)&dwVal,sizeof(DWORD)))
		AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);

	dwVal=m_btRemoteInit.GetCheck()?1:0;

	if(RegSetValueEx(g_hServerKey,_T("RemoteInit"),NULL,REG_DWORD,(BYTE*)&dwVal,sizeof(DWORD)))
		AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);

	dwVal=m_cbAtomicCheckouts.GetCheck()?1:0;

	if(RegSetValueEx(g_hServerKey,_T("AtomicCheckouts"),NULL,REG_DWORD,(BYTE*)&dwVal,sizeof(DWORD)))
		AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);

	dwVal=m_cbGlobalScript.GetCheck()?1:0;

	if(RegSetValueEx(g_hServerKey,_T("GlobalScriptTrigger"),NULL,REG_DWORD,(BYTE*)&dwVal,sizeof(DWORD)))
		AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);

	dwVal=m_cbZeroconf.GetCurSel();

	if(RegSetValueEx(g_hServerKey,_T("ResponderType"),NULL,REG_DWORD,(BYTE*)&dwVal,sizeof(DWORD)))
		AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);

	RegDeleteValue(g_hServerKey,_T("EnableZeroconf"));

	dwVal=m_btReplication.GetCheck();
	if(RegSetValueEx(g_hServerKey,_T("EnableUnison"),NULL,REG_DWORD,(BYTE*)&dwVal,sizeof(DWORD)))
		AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);

	CString s;
	m_edReplicationPort.GetWindowText(s);
	dwVal=_ttoi(s);
	if(dwVal<1 || dwVal>65535)
		dwVal=0;
	if(dwVal)
	{
		if(RegSetValueEx(g_hServerKey,_T("UnisonPort"),NULL,REG_DWORD,(BYTE*)&dwVal,sizeof(DWORD)))
			AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);
	}
	else
		RegDeleteValue(g_hServerKey,_T("UnisonPort"));

	return CTooltipPropertyPage::OnApply();
}

DWORD CAdvancedPage::QueryDword(LPCTSTR szKey)
{
	BYTE buf[64];
	DWORD bufLen=sizeof(buf);
	DWORD dwType;
	if(RegQueryValueEx(g_hServerKey,szKey,NULL,&dwType,buf,&bufLen))
		return -1;
	if(dwType!=REG_DWORD)
		return -1;
	return *(DWORD*)buf;
}

CString CAdvancedPage::QueryString(LPCTSTR szKey)
{
	BYTE buf[16384];
	DWORD bufLen=sizeof(buf);
	DWORD dwType;
	if(RegQueryValueEx(g_hServerKey,szKey,NULL,&dwType,buf,&bufLen))
		return "";
	if(dwType!=REG_SZ && dwType!=REG_EXPAND_SZ)
		return "";
	return (LPTSTR)buf;
}

void CAdvancedPage::OnBnClickedNoreversedns()
{
	SetModified();
}

void CAdvancedPage::OnBnClickedAllowtrace()
{
	SetModified();
}

void CAdvancedPage::OnBnClickedLockserverlocal()
{
	SetModified();
}

void CAdvancedPage::OnBnClickedUnicodeserver()
{
	SetModified();
}

void CAdvancedPage::OnBnClickedReadonly()
{
	SetModified();
}

void CAdvancedPage::OnBnClickedRemoteinit()
{
	SetModified();
}


void CAdvancedPage::OnCbnSelendokZeroconf()
{
	SetModified();
}

void CAdvancedPage::OnBnClickedAtomiccheckouts()
{
	SetModified();
}

void CAdvancedPage::OnBnClickedGlobalscript()
{
	SetModified();
}

void CAdvancedPage::OnCbnSelchangeZeroconf()
{
	SetModified();
}

void CAdvancedPage::OnBnClickedReplication()
{
	SetModified();

	BOOL b = m_btReplication.GetCheck()?TRUE:FALSE;
	m_stReplicationPort.EnableWindow(b);
	m_edReplicationPort.EnableWindow(b);
}

void CAdvancedPage::OnEnChangeReplicationport()
{
	SetModified();
}
