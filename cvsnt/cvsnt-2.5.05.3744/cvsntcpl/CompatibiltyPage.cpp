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
// CompatibiltyPage.cpp : implementation file
//

#include "stdafx.h"
#include "CompatibiltyPage.h"

// CCompatibiltyPage dialog

static const struct { int val; const TCHAR *desc; const TCHAR *reg; } compat_regexp[] =
{
	{ 0, _T("Any CVS/CVSNT"),			  _T("") },
	{ 1, _T("Any CVSNT"),				  _T("^CVSNT.*") },
	{ 2, _T("CVSNT 2.x"),				  _T("^CVSNT 2\\..*") },
	{ 3, _T("CVSNT 2.5.x"),				  _T("^CVSNT 2\\.5\\..*") },
	{ 4, _T("CVSNT 2.6.x"),				  _T("^CVSNT 2\\.6\\..*") },
	{ -1 }
};

IMPLEMENT_DYNAMIC(CCompatibiltyPage, CTooltipPropertyPage)
CCompatibiltyPage::CCompatibiltyPage()
	: CTooltipPropertyPage(CCompatibiltyPage::IDD)
{
}

CCompatibiltyPage::~CCompatibiltyPage()
{
}

void CCompatibiltyPage::DoDataExchange(CDataExchange* pDX)
{
	CTooltipPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_OLDVERSION_CVSNT, m_btOldVersionCvsnt);
	DDX_Control(pDX, IDC_OLDCHECKOUT_CVSNT, m_btOldCheckoutCvsnt);
	DDX_Control(pDX, IDC_HIDESTATUS_CVSNT, m_btHideStatusCvsnt);
	DDX_Control(pDX, IDC_HIDESTATUS_CVSNT, m_btHideStatusCvsnt);
	DDX_Control(pDX, IDC_IGNOREWRAPPERS_CVSNT, m_btIgnoreWrappersCvsnt);
	DDX_Control(pDX, IDC_OLDVERSION_NONCVSNT, m_btOldVersionNonCvsnt);
	DDX_Control(pDX, IDC_OLDCHECKOUT_NONCVSNT, m_btOldCheckoutNonCvsnt);
	DDX_Control(pDX, IDC_HIDESTATUS_NONCVSNT, m_btHideStatusNonCvsnt);
	DDX_Control(pDX, IDC_IGNOREWRAPPERS_NONCVSNT, m_btIgnoreWrappersNonCvsnt);
	DDX_Control(pDX, IDC_ALLOWEDCLIENTS, m_cbAllowedClients);
}


BEGIN_MESSAGE_MAP(CCompatibiltyPage, CTooltipPropertyPage)
	ON_BN_CLICKED(IDC_HIDESTATUS_CVSNT, OnBnClickedHidestatusCvsnt)
	ON_BN_CLICKED(IDC_OLDVERSION_CVSNT, OnBnClickedOldversionCvsnt)
	ON_BN_CLICKED(IDC_OLDCHECKOUT_CVSNT, OnBnClickedOldcheckoutCvsnt)
	ON_BN_CLICKED(IDC_IGNOREWRAPPERS_CVSNT, OnBnClickedIgnorewrappersCvsnt)
	ON_BN_CLICKED(IDC_HIDESTATUS_NONCVSNT, OnBnClickedHidestatusNonCvsnt)
	ON_BN_CLICKED(IDC_OLDVERSION_NONCVSNT, OnBnClickedOldversionNonCvsnt)
	ON_BN_CLICKED(IDC_OLDCHECKOUT_NONCVSNT, OnBnClickedOldcheckoutNonCvsnt)
	ON_BN_CLICKED(IDC_IGNOREWRAPPERS_NONCVSNT, OnBnClickedIgnorewrappersNonCvsnt)
	ON_CBN_SELENDOK(IDC_ALLOWEDCLIENTS, OnCbnSelendokAllowedclients)
END_MESSAGE_MAP()


// CCompatibiltyPage message handlers

BOOL CCompatibiltyPage::OnInitDialog()
{
	BYTE buf[_MAX_PATH*sizeof(TCHAR)];
	DWORD bufLen;
	DWORD dwType;

	CTooltipPropertyPage::OnInitDialog();

	m_cbAllowedClients.ResetContent();
	for(size_t n=0; compat_regexp[n].val>=0; n++)
	{
		m_cbAllowedClients.SetItemData(m_cbAllowedClients.AddString(compat_regexp[n].desc),compat_regexp[n].val);
	}

	bufLen=sizeof(buf);
	if(RegQueryValueEx(g_hServerKey,_T("AllowedClients"),NULL,&dwType,buf,&bufLen))
	{
		m_cbAllowedClients.SetCurSel(0);
	}
	else
	{
		for(size_t n=0; compat_regexp[n].val>=0; n++)
		{
			if(!_tcscmp((const TCHAR*)buf,compat_regexp[n].reg))
				m_cbAllowedClients.SetCurSel(m_cbAllowedClients.FindStringExact(-1,compat_regexp[n].desc));
		}
	}

	CompatibilityData.resize(2);

	if(QueryDword(_T("FakeUnixCvs"),0))
	{
		CompatibilityData[0].OldVersion=1;
		CompatibilityData[0].OldCheckout=1;
		CompatibilityData[0].HideStatus=1;
		CompatibilityData[0].IgnoreWrappers=0;
		RegDeleteValue(g_hServerKey,_T("FakeUnixCvs"));
		if(g_bPrivileged)
			GetParent()->PostMessage(PSM_CHANGED, (WPARAM)m_hWnd); /* SetModified happens too early */
	}
	else
	{
		CompatibilityData[0].OldVersion=QueryDword(_T("Compat0_OldVersion"),0);
		CompatibilityData[0].OldCheckout=QueryDword(_T("Compat0_OldCheckout"),1);
		CompatibilityData[0].HideStatus=QueryDword(_T("Compat0_HideStatus"),0);
		CompatibilityData[0].IgnoreWrappers=QueryDword(_T("Compat0_IgnoreWrappers"),0);
	}

	CompatibilityData[1].OldVersion=QueryDword(_T("Compat1_OldVersion"),0);
	CompatibilityData[1].OldCheckout=QueryDword(_T("Compat1_OldCheckout"),0);
	CompatibilityData[1].HideStatus=QueryDword(_T("Compat1_HideStatus"),0);
	CompatibilityData[1].IgnoreWrappers=QueryDword(_T("Compat1_IgnoreWrappers"),0);

	FillClientType();

	if(!g_bPrivileged)
	{
		m_btOldVersionNonCvsnt.EnableWindow(FALSE);
		m_btOldCheckoutNonCvsnt.EnableWindow(FALSE);
		m_btHideStatusNonCvsnt.EnableWindow(FALSE);
		m_btIgnoreWrappersNonCvsnt.EnableWindow(FALSE);
		m_btOldVersionCvsnt.EnableWindow(FALSE);
		m_btOldCheckoutCvsnt.EnableWindow(FALSE);
		m_btHideStatusCvsnt.EnableWindow(FALSE);
		m_btIgnoreWrappersCvsnt.EnableWindow(FALSE);
		m_cbAllowedClients.EnableWindow(FALSE);
	}

	return TRUE;
}

DWORD CCompatibiltyPage::QueryDword(LPCTSTR szKey, DWORD dwDefault)
{
	BYTE buf[64];
	DWORD bufLen=sizeof(buf);
	DWORD dwType;
	if(RegQueryValueEx(g_hServerKey,szKey,NULL,&dwType,buf,&bufLen))
		return dwDefault;
	if(dwType!=REG_DWORD)
		return dwDefault;
	return *(DWORD*)buf;
}

void CCompatibiltyPage::SetDword(LPCTSTR szKey, DWORD dwVal)
{
	if(RegSetValueEx(g_hServerKey, szKey,NULL,REG_DWORD,(BYTE*)&dwVal,sizeof(DWORD)))
		AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);
}

BOOL CCompatibiltyPage::OnApply()
{
	for(size_t n=0; n<CompatibilityData.size(); n++)
	{
		CString f;

		f.Format(_T("Compat%d_OldVersion"),n);
		SetDword(f,CompatibilityData[n].OldVersion);
		f.Format(_T("Compat%d_OldCheckout"),n);
		SetDword(f,CompatibilityData[n].OldCheckout);
		f.Format(_T("Compat%d_HideStatus"),n);
		SetDword(f,CompatibilityData[n].HideStatus);
		f.Format(_T("Compat%d_IgnoreWrappers"),n);
		SetDword(f,CompatibilityData[n].IgnoreWrappers);
	}

	const TCHAR *allowed = _T("");
	int s = m_cbAllowedClients.GetCurSel();
	if(s!=-1)
	{
		s=m_cbAllowedClients.GetItemData(s);
		for(size_t n=0; compat_regexp[n].val>=0; n++)
		{
			if(compat_regexp[n].val==s)
			{
				allowed=compat_regexp[n].reg;
				break;
			}
		}


		if(RegSetValueEx(g_hServerKey,_T("AllowedClients"),NULL,REG_SZ,(BYTE*)allowed,(_tcslen(allowed)+1)*sizeof(TCHAR)))
			AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);
	}

	return CTooltipPropertyPage::OnApply();
}

void CCompatibiltyPage::OnBnClickedOldversionNonCvsnt()
{
	SetModified();
	CompatibilityData[0].OldVersion=m_btOldVersionNonCvsnt.GetCheck();
}

void CCompatibiltyPage::OnBnClickedOldcheckoutNonCvsnt()
{
	SetModified();
	CompatibilityData[0].OldCheckout=m_btOldCheckoutNonCvsnt.GetCheck();
}

void CCompatibiltyPage::OnBnClickedHidestatusNonCvsnt()
{
	SetModified();
	CompatibilityData[0].HideStatus=m_btHideStatusNonCvsnt.GetCheck();
}

void CCompatibiltyPage::OnBnClickedIgnorewrappersNonCvsnt()
{
	SetModified();
	CompatibilityData[0].IgnoreWrappers=m_btIgnoreWrappersNonCvsnt.GetCheck();
}

void CCompatibiltyPage::OnBnClickedOldversionCvsnt()
{
	SetModified();
	CompatibilityData[1].OldVersion=m_btOldVersionCvsnt.GetCheck();
}

void CCompatibiltyPage::OnBnClickedOldcheckoutCvsnt()
{
	SetModified();
	CompatibilityData[1].OldCheckout=m_btOldCheckoutCvsnt.GetCheck();
}

void CCompatibiltyPage::OnBnClickedHidestatusCvsnt()
{
	SetModified();
	CompatibilityData[1].HideStatus=m_btHideStatusCvsnt.GetCheck();
}

void CCompatibiltyPage::OnBnClickedIgnorewrappersCvsnt()
{
	SetModified();
	CompatibilityData[1].IgnoreWrappers=m_btIgnoreWrappersCvsnt.GetCheck();
}

void CCompatibiltyPage::FillClientType()
{
	m_btOldVersionNonCvsnt.SetCheck(CompatibilityData[0].OldVersion?1:0);
	m_btOldCheckoutNonCvsnt.SetCheck(CompatibilityData[0].OldCheckout?1:0);
	m_btHideStatusNonCvsnt.SetCheck(CompatibilityData[0].HideStatus?1:0);
	m_btIgnoreWrappersNonCvsnt.SetCheck(CompatibilityData[0].IgnoreWrappers?1:0);
	m_btOldVersionCvsnt.SetCheck(CompatibilityData[1].OldVersion?1:0);
	m_btOldCheckoutCvsnt.SetCheck(CompatibilityData[1].OldCheckout?1:0);
	m_btHideStatusCvsnt.SetCheck(CompatibilityData[1].HideStatus?1:0);
	m_btIgnoreWrappersCvsnt.SetCheck(CompatibilityData[1].IgnoreWrappers?1:0);
}


void CCompatibiltyPage::OnCbnSelendokAllowedclients()
{
	SetModified();
}
