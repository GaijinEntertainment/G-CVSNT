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
// SettingsPage.cpp : implementation file
//

#include "stdafx.h"
#include "cvsnt.h"
#include "SettingsPage.h"

#include <lm.h>
#include <ntsecapi.h>
#include <dsgetdc.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/*
	 IsDomainMember patch from John Anderson <panic@semiosix.com>

     Find out whether the machine is a member of a domain or not
*/

// build LSA UNICODE strings.
void InitLsaString( PLSA_UNICODE_STRING LsaString, LPWSTR String )
{
    DWORD StringLength;
    if ( String == NULL )
     {
             LsaString->Buffer = NULL;
             LsaString->Length = 0;
             LsaString->MaximumLength = 0;
             return;
     }
     StringLength = wcslen(String);
     LsaString->Buffer = String;
     LsaString->Length = (USHORT) StringLength * sizeof(WCHAR);
     LsaString->MaximumLength=(USHORT)(StringLength+1) * sizeof(WCHAR);
}

NTSTATUS OpenPolicy( LPWSTR ServerName, DWORD DesiredAccess, PLSA_HANDLE PolicyHandle )
{
     LSA_OBJECT_ATTRIBUTES ObjectAttributes;
     LSA_UNICODE_STRING ServerString;
     PLSA_UNICODE_STRING Server = NULL;
     // Always initialize the object attributes to all zeroes.
     ZeroMemory(&ObjectAttributes, sizeof(ObjectAttributes));
     if ( ServerName != NULL )
     {
             // Make a LSA_UNICODE_STRING out of the LPWSTR passed in.
             InitLsaString(&ServerString, ServerName);
             Server = &ServerString;
     }
     // Attempt to open the policy.
     return LsaOpenPolicy( Server, &ObjectAttributes, DesiredAccess, PolicyHandle );
}

int isDomainMember(wchar_t *wszDomain)
{
     PPOLICY_PRIMARY_DOMAIN_INFO ppdiDomainInfo=NULL;
     PPOLICY_DNS_DOMAIN_INFO pddiDomainInfo=NULL;
     LSA_HANDLE PolicyHandle;
     NTSTATUS status;
     BOOL retval = FALSE;

	 *wszDomain = '\0';

     // open the policy object for the local system
     status = OpenPolicy(
             NULL
             , GENERIC_READ | POLICY_VIEW_LOCAL_INFORMATION
             , &PolicyHandle
     );
    // You have a handle to the policy object. Now, get the
    // domain information using LsaQueryInformationPolicy.
    if ( !status )
    {
		/* Based on patch by Valdas Sevelis.  Call PolicyDnsDomainInformation first
		   as Win2K Advanced server is broken w/PolicyPrimaryDomainInformation */
        status = LsaQueryInformationPolicy(
                PolicyHandle,
                PolicyDnsDomainInformation,
                (void**)&pddiDomainInfo);
		if(!status)
		{
			retval = pddiDomainInfo->Sid != 0;
			if(wszDomain && retval)
			{
					wcsncpy(wszDomain,pddiDomainInfo->Name.Buffer,pddiDomainInfo->Name.Length/sizeof(wchar_t));
					wszDomain[pddiDomainInfo->Name.Length/sizeof(wchar_t)]='\0';
			}
		    LsaFreeMemory( (LPVOID)pddiDomainInfo );
		}
		else
		{
             status = LsaQueryInformationPolicy(
                     PolicyHandle,
                     PolicyPrimaryDomainInformation,
                     (void**)&ppdiDomainInfo);
			if(!status)
			{
				retval = ppdiDomainInfo->Sid != 0;
				if(wszDomain && retval)
				{
					wcsncpy(wszDomain,ppdiDomainInfo->Name.Buffer,ppdiDomainInfo->Name.Length/sizeof(wchar_t));
					wszDomain[ppdiDomainInfo->Name.Length/sizeof(wchar_t)]='\0';
				}
			    LsaFreeMemory( (LPVOID)ppdiDomainInfo );
			}
		}
    }
    // Clean up all the memory buffers created by the LSA calls
	LsaClose(PolicyHandle);
    return retval;
}

/////////////////////////////////////////////////////////////////////////////
// CSettingsPage property page

IMPLEMENT_DYNCREATE(CSettingsPage, CTooltipPropertyPage)

CSettingsPage::CSettingsPage() : CTooltipPropertyPage(CSettingsPage::IDD)
{
	//{{AFX_DATA_INIT(CSettingsPage)
	//}}AFX_DATA_INIT
}

CSettingsPage::~CSettingsPage()
{
}

void CSettingsPage::DoDataExchange(CDataExchange* pDX)
{
	CTooltipPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSettingsPage)
	DDX_Control(pDX, IDC_EDIT1, m_edTempDir);
	DDX_Control(pDX, IDC_LOCKSERVER, m_edLockServer);
	DDX_Control(pDX, IDC_ENCRYPTION, m_cbEncryption);
	DDX_Control(pDX, IDC_COMPRESSION, m_cbCompression);
	DDX_Control(pDX, IDC_SPIN1, m_sbServerPort);
	DDX_Control(pDX, IDC_SPIN2, m_sbLockPort);
	DDX_Control(pDX, IDC_DEFAULTDOMAIN, m_cbDefaultDomain);
	DDX_Control(pDX, IDC_RUNASUSER, m_cbRunAsUser);
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_ANONUSER, m_edAnonUser);
}


BEGIN_MESSAGE_MAP(CSettingsPage, CTooltipPropertyPage)
	//{{AFX_MSG_MAP(CSettingsPage)
	ON_BN_CLICKED(IDC_CHANGETEMP, OnChangetemp)
	ON_EN_CHANGE(IDC_PSERVERPORT, OnChangePserverport)
	ON_EN_CHANGE(IDC_LOCKSERVERPORT, OnChangeLockserverport)
	ON_CBN_SELENDOK(IDC_ENCRYPTION, OnCbnSelendokEncryption)
	ON_CBN_SELENDOK(IDC_COMPRESSION, OnCbnSelendokCompression)
	ON_CBN_SELENDOK(IDC_DEFAULTDOMAIN, OnCbnSelendokDefaultdomain)
	ON_CBN_SELENDOK(IDC_RUNASUSER, OnCbnSelendokRunasuser)
	ON_CBN_DROPDOWN(IDC_RUNASUSER, OnCbnDropdownRunasuser)
	ON_CBN_EDITCHANGE(IDC_RUNASUSER, OnCbnEditchangeRunasuser)
	//}}AFX_MSG_MAP
	ON_EN_CHANGE(IDC_ANONUSER, OnEnChangeAnonuser)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSettingsPage message handlers

BOOL CSettingsPage::OnInitDialog()
{
	int t;
	BYTE buf[_MAX_PATH*sizeof(TCHAR)];
	DWORD bufLen;
	DWORD dwType;
	CWaitCursor wait;

	CTooltipPropertyPage::OnInitDialog();

	SetDlgItemInt(IDC_PSERVERPORT,(t=QueryDword(_T("PServerPort")))>=0?t:2401,FALSE);
	bufLen=sizeof(buf);
	if(RegQueryValueEx(g_hServerKey,_T("LockServer"),NULL,&dwType,buf,&bufLen))
	{
		SetDlgItemText(IDC_LOCKSERVER,_T("localhost"));
		SetDlgItemInt(IDC_LOCKSERVERPORT,(t=QueryDword(_T("LockServerPort")))>=0?t:2402,FALSE);
	}
	else
	{
		RegDeleteValue(g_hServerKey,_T("LockServerPort"));
		TCHAR *p=_tcschr((TCHAR*)buf,':');
		if(p)
			*p='\0';
		m_edLockServer.SetWindowText((LPCTSTR)buf);
		SetDlgItemInt(IDC_LOCKSERVERPORT,p?_tstoi(p+1):2402,FALSE);
	}

	if(!RegQueryValueEx(g_hServerKey,_T("AnonymousUsername"),NULL,&dwType,buf,&bufLen))
		m_edAnonUser.SetWindowText((TCHAR*)buf);

	SendDlgItemMessage(IDC_PSERVERPORT,EM_LIMITTEXT,4);
	SendDlgItemMessage(IDC_LOCKSERVERPORT,EM_LIMITTEXT,4);

	m_sbServerPort.SetRange32(1,65535);
	m_sbLockPort.SetRange32(1,65535);

	bufLen=sizeof(buf);
	if(RegQueryValueEx(g_hServerKey,_T("TempDir"),NULL,&dwType,buf,&bufLen) &&
	   SHRegGetUSValue(_T("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"),_T("TEMP"),NULL,(LPVOID)buf,&bufLen,TRUE,NULL,0) &&
	   !GetEnvironmentVariable(_T("TEMP"),(LPTSTR)buf,sizeof(buf)) &&
	   !GetEnvironmentVariable(_T("TMP"),(LPTSTR)buf,sizeof(buf)))
		{
			// Not set
			*buf='\0';
		}

	m_edTempDir.SetWindowText((LPCTSTR)buf);

	m_cbEncryption.ResetContent();
	m_cbEncryption.SetItemData(m_cbEncryption.AddString(_T("Optional")),0);
	m_cbEncryption.SetItemData(m_cbEncryption.AddString(_T("Request Authentication")),1);
	m_cbEncryption.SetItemData(m_cbEncryption.AddString(_T("Request Encryption")),2);
	m_cbEncryption.SetItemData(m_cbEncryption.AddString(_T("Require Authentication")),3);
	m_cbEncryption.SetItemData(m_cbEncryption.AddString(_T("Require Encryption")),4);
	m_cbCompression.ResetContent();
	m_cbCompression.SetItemData(m_cbCompression.AddString(_T("Optional")),0);
	m_cbCompression.SetItemData(m_cbCompression.AddString(_T("Request Compression")),1);
	m_cbCompression.SetItemData(m_cbCompression.AddString(_T("Require Compression")),2);

	m_cbEncryption.SetCurSel((t=QueryDword(_T("EncryptionLevel")))>=0?t:0);
	m_cbCompression.SetCurSel((t=QueryDword(_T("CompressionLevel")))>=0?t:0);

	/* Migrate the old setting */
	if((t=QueryDword(_T("DontUseDomain")))>=0)
	{
		if(t)
		{
			/* If dont use domain is set, force domain to computer name */
			/* The server will automatically pick up the domain otherwise */
			bufLen=sizeof(buf);
			GetComputerName((LPTSTR)buf,&bufLen);
			RegSetValueEx(g_hServerKey,_T("DefaultDomain"),0,REG_SZ,(BYTE*)buf,_tcslen((LPCTSTR)buf));
		}
		RegDeleteValue(g_hServerKey,_T("DontUseDomain"));
		if(g_bPrivileged)
			GetParent()->PostMessage(PSM_CHANGED, (WPARAM)m_hWnd); /* SetModified happens too early */
	}

	m_cbDefaultDomain.ResetContent();
	DWORD dwLen = sizeof(mw_computer)/sizeof(mw_computer[0]);

	m_cbDefaultDomain.AddString(_T("(default)"));

	GetComputerName(mw_computer,&dwLen);
	m_cbDefaultDomain.AddString(mw_computer);
	if(isDomainMember(mw_domain))
	{
		LPWSTR pw_pdc;
		m_cbDefaultDomain.AddString(mw_domain);
		if(!NetGetAnyDCName(NULL,mw_domain,(LPBYTE*)&pw_pdc) || !NetGetDCName(NULL,mw_domain,(LPBYTE*)&pw_pdc))
		{
			wcscpy(mw_pdc,pw_pdc);
			NetApiBufferFree(pw_pdc);
		}
	}

	CString szDefaultDomain = QueryString(_T("DefaultDomain"));
	int n = m_cbDefaultDomain.FindStringExact(-1,szDefaultDomain);
	m_cbDefaultDomain.SetCurSel(n>0?n:0);

	m_cbRunAsUser.ResetContent();
	m_cbRunAsUser.AddString(_T("(client user)"));
	CString usr = QueryString(_T("RunAsUser"));
	if(!usr.GetLength())
		m_cbRunAsUser.SetCurSel(0);
	else
		m_cbRunAsUser.SetCurSel(m_cbRunAsUser.AddString(usr));

	if(!g_bPrivileged)
	{
		m_edTempDir.EnableWindow(FALSE);
		m_edLockServer.EnableWindow(FALSE);
		m_cbEncryption.EnableWindow(FALSE);
		m_cbCompression.EnableWindow(FALSE);
		m_sbServerPort.EnableWindow(FALSE);
		m_sbLockPort.EnableWindow(FALSE);
		m_cbDefaultDomain.EnableWindow(FALSE);
		m_cbRunAsUser.EnableWindow(FALSE);
		m_edAnonUser.EnableWindow(FALSE);
		::EnableWindow(*GetDlgItem(IDC_CHANGETEMP),FALSE);
		::EnableWindow(*GetDlgItem(IDC_LOCKSERVERPORT),FALSE);
		::EnableWindow(*GetDlgItem(IDC_PSERVERPORT),FALSE);
	}

	return TRUE;
}

void CSettingsPage::OnChangetemp()
{
	TCHAR fn[MAX_PATH];
	LPITEMIDLIST idl,idlroot;
	IMalloc *mal;

	SHGetSpecialFolderLocation(m_hWnd, CSIDL_DRIVES, &idlroot);
	SHGetMalloc(&mal);
	BROWSEINFO bi = { m_hWnd, idlroot, fn, _T("Select folder for CVS temporary files.  This folder must be writeable by all users that wish to use CVS."), BIF_NEWDIALOGSTYLE|BIF_RETURNONLYFSDIRS|BIF_RETURNFSANCESTORS, BrowseValid };
	idl = SHBrowseForFolder(&bi);

	mal->Free(idlroot);
	if(!idl)
	{
		mal->Release();
		return;
	}

	SHGetPathFromIDList(idl,fn);

	mal->Free(idl);
	mal->Release();

	m_edTempDir.SetWindowText(fn);

	SetModified();
}

BOOL CSettingsPage::OnApply()
{
	DWORD dwVal;
	TCHAR fn[MAX_PATH];

	dwVal = GetDlgItemInt(IDC_PSERVERPORT,NULL,FALSE);
	if(RegSetValueEx(g_hServerKey,_T("PServerPort"),NULL,REG_DWORD,(BYTE*)&dwVal,sizeof(DWORD)))
		AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);

	m_edLockServer.GetWindowText(fn,sizeof(fn)-8);
	dwVal = GetDlgItemInt(IDC_LOCKSERVERPORT,NULL,FALSE);
	_sntprintf(fn+_tcslen(fn),8,_T(":%d"),dwVal);
	if(RegSetValueEx(g_hServerKey,_T("LockServer"),NULL,REG_SZ,(BYTE*)fn,(_tcslen(fn)+1)*sizeof(TCHAR)))
		AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);

	m_edTempDir.GetWindowText(fn,sizeof(fn));
	if(RegSetValueEx(g_hServerKey,_T("TempDir"),NULL,REG_SZ,(BYTE*)fn,(_tcslen(fn)+1)*sizeof(TCHAR)))
		AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);

	dwVal = m_cbCompression.GetItemData(m_cbCompression.GetCurSel());
	if(RegSetValueEx(g_hServerKey,_T("CompressionLevel"),NULL,REG_DWORD,(BYTE*)&dwVal,sizeof(DWORD)))
		AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);

	dwVal = m_cbEncryption.GetItemData(m_cbEncryption.GetCurSel());
	if(RegSetValueEx(g_hServerKey,_T("EncryptionLevel"),NULL,REG_DWORD,(BYTE*)&dwVal,sizeof(DWORD)))
		AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);

	m_cbDefaultDomain.GetWindowText(fn,sizeof(fn));
	if(fn[0]=='(')
		RegDeleteValue(g_hServerKey,_T("DefaultDomain"));
	else
	{
		if(RegSetValueEx(g_hServerKey,_T("DefaultDomain"),NULL,REG_SZ,(BYTE*)fn,(_tcslen(fn)+1)*sizeof(TCHAR)))
			AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);
	}

	m_cbRunAsUser.GetWindowText(fn,sizeof(fn));
	if(fn[0]=='(')
		fn[0]='\0';
	if(RegSetValueEx(g_hServerKey,_T("RunAsUser"),NULL,REG_SZ,(BYTE*)fn,(_tcslen(fn)+1)*sizeof(TCHAR)))
		AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);

	m_edAnonUser.GetWindowText(fn,sizeof(fn));
	if(RegSetValueEx(g_hServerKey,_T("AnonymousUsername"),NULL,REG_SZ,(BYTE*)fn,(_tcslen(fn)+1)*sizeof(TCHAR)))
		AfxMessageBox(_T("RegSetValueEx failed"),MB_ICONSTOP);

	return CTooltipPropertyPage::OnApply();
}

DWORD CSettingsPage::QueryDword(LPCTSTR szKey)
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

CString CSettingsPage::QueryString(LPCTSTR szKey)
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

void CSettingsPage::OnChangePserverport()
{
	SetModified();
}

void CSettingsPage::OnChangeLockserverport()
{
	SetModified();
}

void CSettingsPage::OnCbnSelendokEncryption()
{
	SetModified();
}

void CSettingsPage::OnCbnSelendokCompression()
{
	SetModified();
}

void CSettingsPage::OnCbnSelendokDefaultdomain()
{
	SetModified();
}

void CSettingsPage::OnCbnSelendokRunasuser()
{
	SetModified();
}

void CSettingsPage::OnCbnDropdownRunasuser()
{
	DWORD count;
	NET_DISPLAY_USER *userlist;
	
	if(m_cbRunAsUser.GetCount()<3)
	{
		CString cur;
		CWaitCursor wait;
		m_cbRunAsUser.GetWindowText(cur);
		m_cbRunAsUser.ResetContent();
		m_cbRunAsUser.AddString(_T("(client user)"));
		NetQueryDisplayInformation(NULL,1,0,4096,MAX_PREFERRED_LENGTH,&count,(PVOID*)&userlist);
		for(size_t n=0; n<count; n++)
		{
			if((userlist[n].usri1_flags&(UF_ACCOUNTDISABLE|UF_NORMAL_ACCOUNT|UF_NOT_DELEGATED)) != (UF_NORMAL_ACCOUNT))
				continue;

/*			USER_INFO_1 *ui1;
			if(!NetUserGetInfo(mw_pdc,userlist[n].usri1_name, 1, (LPBYTE*)&ui1))
			{
				if(ui1->usri1_priv == USER_PRIV_ADMIN)
				{
					NetApiBufferFree(ui1);
					continue;
				}
				NetApiBufferFree(ui1);
			}
*/
			CString str;
			str.Format(_T("%s\\%s"),mw_computer,userlist[n].usri1_name);
			m_cbRunAsUser.AddString(str);
		}
		NetApiBufferFree(userlist);
		if(mw_domain[0])
		{
			NetQueryDisplayInformation(mw_pdc,1,0,4096,MAX_PREFERRED_LENGTH,&count,(PVOID*)&userlist);
			for(size_t n=0; n<count; n++)
			{
				if((userlist[n].usri1_flags&(UF_ACCOUNTDISABLE|UF_NORMAL_ACCOUNT|UF_NOT_DELEGATED)) != (UF_NORMAL_ACCOUNT))
					continue;

/*				USER_INFO_1 *ui1;
				if(!NetUserGetInfo(mw_pdc,userlist[n].usri1_name, 1, (LPBYTE*)&ui1))
				{
					if(ui1->usri1_priv == USER_PRIV_ADMIN)
					{
						NetApiBufferFree(ui1);
						continue;
					}
					NetApiBufferFree(ui1);
				}
*/
				CString str;
				str.Format(_T("%s\\%s"),mw_domain,userlist[n].usri1_name);
				m_cbRunAsUser.AddString(str);
			}
			NetApiBufferFree(userlist);
		}
		m_cbRunAsUser.SetCurSel(m_cbRunAsUser.FindStringExact(-1,cur));
	}
}

void CSettingsPage::OnCbnEditchangeRunasuser()
{
	SetModified();
}

void CSettingsPage::OnEnChangeAnonuser()
{
	SetModified();
}
