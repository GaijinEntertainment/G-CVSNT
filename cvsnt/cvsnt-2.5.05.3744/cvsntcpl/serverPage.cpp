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
// serverPage.cpp : implementation file
//

#include "stdafx.h"
#include "cvsnt.h"
#include "serverPage.h"
#include "UsageDialog.h"

/////////////////////////////////////////////////////////////////////////////
// CserverPage property page

IMPLEMENT_DYNCREATE(CserverPage, CTooltipPropertyPage)

CserverPage::CserverPage() : CTooltipPropertyPage(CserverPage::IDD)
{
}

CserverPage::~CserverPage()
{
}

void CserverPage::DoDataExchange(CDataExchange* pDX)
{
	CTooltipPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CVSNTVERSION, m_stVersion);
	DDX_Control(pDX, IDC_REGISTRATION, m_stRegistration);
	DDX_Control(pDX, IDC_HOSTOS, m_stHostOs);
	DDX_Control(pDX, IDC_UPTIME, m_stUptime);
	DDX_Control(pDX, IDC_USERS, m_stUserCount);
	DDX_Control(pDX, IDC_SIMULTANEOUSUSERS, m_szMaxUsers);
	DDX_Control(pDX, IDC_TIMEPERUSER, m_stAverageTime);
	DDX_Control(pDX, IDC_CHECK1, m_cbSendStatistics);
	DDX_Control(pDX, IDC_LOGO, m_cbLogo);
	DDX_Control(pDX, IDC_SESSIONCOUNT, m_stSessionCount);
}


BEGIN_MESSAGE_MAP(CserverPage, CTooltipPropertyPage)
	ON_BN_CLICKED(IDC_LOGO, OnBnClickedLogo)
	ON_BN_CLICKED(IDC_BUTTON1, OnBnClickedButton1)
	ON_BN_CLICKED(IDC_CHECK1, OnBnClickedCheck1)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CserverPage message handlers

BOOL CserverPage::OnInitDialog() 
{
	CTooltipPropertyPage::OnInitDialog();

	CBitmap *pBitmap = new CBitmap;
	pBitmap->LoadBitmap(IDB_BITMAP1);
	m_cbLogo.SetBitmap(*pBitmap);
	pBitmap->Detach();
	delete pBitmap;

	m_stVersion.SetWindowText(CVSNT_PRODUCTVERSION_STRING_T);
	char os[256],sp[256];
	int major,minor;
	GetOsVersion(os,sp,major,minor);
	sprintf(os+strlen(os)," %s [%d.%d]",sp,major,minor);
	m_stHostOs.SetWindowText(cvs::wide(os));

	union { __time64_t t; BYTE data[8]; } servtime;
	DWORD dwLen=sizeof(servtime.data),dwType;
	cvs::string uptime,tmp;
	if(!RegQueryValueEx(g_hServerKey,_T("StartTime"),NULL,&dwType,servtime.data,&dwLen) && dwType==REG_BINARY)
	{
		__time64_t then = servtime.t,now;
		_time64(&now);
		if(then>now) uptime="??";
		else
		{
			unsigned elapsed = (unsigned)(now-then);
			if(elapsed>43200)
			{
				cvs::sprintf(tmp,32,"%u days, ",elapsed/43200);
				elapsed-=(elapsed/43200)*43200;
				uptime+=tmp;
			}
			if(elapsed>3600 || uptime.size())
			{
				cvs::sprintf(tmp,32,"%u hours, ",elapsed/3600);
				elapsed-=(elapsed/3600)*3600;
				uptime+=tmp;
			}
			cvs::sprintf(tmp,32,"%u minutes",elapsed/60);
			uptime+=tmp;
		}
	}
	else
		uptime="??";

	m_stUptime.SetWindowText(cvs::wide(uptime.c_str()));

	DWORD dwMaxUsers;
	dwLen=sizeof(dwMaxUsers);
	if(!RegQueryValueEx(g_hServerKey,_T("MaxUsers"),NULL,&dwType,(LPBYTE)&dwMaxUsers,&dwLen) && dwType==REG_DWORD)
		cvs::sprintf(tmp,32,"%u",dwMaxUsers);
	else
		tmp="??";
		
	m_szMaxUsers.SetWindowText(cvs::wide(tmp.c_str()));

	DWORD dwAverageTime;
	dwLen=sizeof(dwAverageTime);
	if(!RegQueryValueEx(g_hServerKey,_T("AverageTime"),NULL,&dwType,(LPBYTE)&dwAverageTime,&dwLen) && dwType==REG_DWORD)
		cvs::sprintf(tmp,32,"%u.%02u seconds",dwAverageTime/1000,(dwAverageTime%1000)/10);
	else
		tmp="??";
		
	m_stAverageTime.SetWindowText(cvs::wide(tmp.c_str()));

	DWORD dwSessionCount;
	dwLen=sizeof(dwSessionCount);
	if(!RegQueryValueEx(g_hServerKey,_T("SessionCount"),NULL,&dwType,(LPBYTE)&dwSessionCount,&dwLen) && dwType==REG_DWORD)
		cvs::sprintf(tmp,32,"%u",dwSessionCount);
	else
		tmp="??";
		
	m_stSessionCount.SetWindowText(cvs::wide(tmp.c_str()));

	DWORD dwUserCount;
	dwLen=sizeof(dwUserCount);
	if(!RegQueryValueEx(g_hServerKey,_T("UserCount"),NULL,&dwType,(LPBYTE)&dwUserCount,&dwLen) && dwType==REG_DWORD)
		cvs::sprintf(tmp,32,"%u",dwUserCount);
	else
		tmp="??";
		
	m_stUserCount.SetWindowText(cvs::wide(tmp.c_str()));

	tmp.resize(256);
	dwLen=tmp.size();
	if(!g_hInstallerKey || RegQueryValueExA(g_hInstallerKey,"serversuite",NULL,&dwType,(LPBYTE)tmp.data(),&dwLen) || dwType!=REG_SZ)
	{
		dwLen=tmp.size();
		if(!g_hInstallerKey || RegQueryValueExA(g_hInstallerKey,"server",NULL,&dwType,(LPBYTE)tmp.data(),&dwLen) || dwType!=REG_SZ)
		{
			dwLen=tmp.size();
			if(!g_hInstallerKey || RegQueryValueExA(g_hInstallerKey,"servertrial",NULL,&dwType,(LPBYTE)tmp.data(),&dwLen) || dwType!=REG_SZ)
				tmp="??";
			else
				tmp.resize(dwLen);
		}
		else
			tmp.resize(dwLen);
	}
	else
		tmp.resize(dwLen);

	m_stRegistration.SetWindowText(cvs::wide(tmp.c_str()));

	DWORD dwSendStatistics;
	dwLen=sizeof(dwSendStatistics);
	if(RegQueryValueEx(g_hServerKey,_T("SendStatistics"),NULL,&dwType,(LPBYTE)&dwSendStatistics,&dwLen))
	{
		PostMessage(WM_COMMAND,IDC_BUTTON1);
		dwSendStatistics=1;
	}

	m_cbSendStatistics.SetCheck(dwSendStatistics?1:0);
	if(!g_bPrivileged) m_cbSendStatistics.EnableWindow(FALSE);

	return TRUE; 
}

void CserverPage::OnBnClickedLogo()
{
	ShellExecute(NULL,_T("open"),_T("http://www.march-hare.com/cvspro"),NULL,NULL,SW_SHOWNORMAL);
}

void CserverPage::OnBnClickedButton1()
{
	CUsageDialog dlg;
	int nRet = dlg.DoModal();
	if(g_bPrivileged)
	{
		if(nRet==IDOK)
			m_cbSendStatistics.SetCheck(1);
		else
			m_cbSendStatistics.SetCheck(0);
		SetModified();
	}
}

void CserverPage::OnBnClickedCheck1()
{
	SetModified();
}

BOOL CserverPage::OnApply()
{
	if(g_bPrivileged)
	{
		DWORD dwSendStatistics = m_cbSendStatistics.GetCheck();
		RegSetValueEx(g_hServerKey,_T("SendStatistics"),NULL,REG_DWORD,(LPBYTE)&dwSendStatistics,sizeof(dwSendStatistics));
	}

	return CTooltipPropertyPage::OnApply();
}
