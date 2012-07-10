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
// RepositoryPage.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "NewRootDialog.h"
#include "RepositoryPage.h"

#include <winsock2.h>

#include <direct.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define MAX_REPOSITORIES 1024

/////////////////////////////////////////////////////////////////////////////
// CRepositoryPage property page

IMPLEMENT_DYNCREATE(CRepositoryPage, CTooltipPropertyPage)

CRepositoryPage::CRepositoryPage() : CTooltipPropertyPage(CRepositoryPage::IDD)
{
	//{{AFX_DATA_INIT(CRepositoryPage)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

CRepositoryPage::~CRepositoryPage()
{
}

void CRepositoryPage::DoDataExchange(CDataExchange* pDX)
{
	CTooltipPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CRepositoryPage)
	DDX_Control(pDX, IDC_DELETEROOT, m_btDelete);
	DDX_Control(pDX, IDC_ADDROOT, m_btAdd);
	DDX_Control(pDX, IDC_EDITROOT, m_btEdit);
	DDX_Control(pDX, IDC_ROOTLIST, m_listRoot);
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_SERVERNAME, m_edServerName);
}


BEGIN_MESSAGE_MAP(CRepositoryPage, CTooltipPropertyPage)
	//{{AFX_MSG_MAP(CRepositoryPage)
	ON_BN_CLICKED(IDC_ADDROOT, OnAddroot)
	ON_BN_CLICKED(IDC_DELETEROOT, OnDeleteroot)
	ON_BN_CLICKED(IDC_EDITROOT, OnEditroot)
	//}}AFX_MSG_MAP
	ON_NOTIFY(NM_DBLCLK, IDC_ROOTLIST, OnNMDblclkRootlist)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_ROOTLIST, OnLvnItemchangedRootlist)
	ON_EN_CHANGE(IDC_SERVERNAME, OnEnChangeServername)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRepositoryPage message handlers

BOOL CRepositoryPage::OnInitDialog() 
{
	DWORD bufLen,dwType;
	TCHAR buf[_MAX_PATH];

	CTooltipPropertyPage::OnInitDialog();
	
	m_listRoot.InsertColumn(0,_T("Name"),LVCFMT_LEFT,130);
	m_listRoot.InsertColumn(1,_T("Root"),LVCFMT_LEFT,200);
	m_listRoot.InsertColumn(2,_T("Description"),LVCFMT_LEFT,230);

	if(GetRootList() && g_bPrivileged)
		GetParent()->PostMessage(PSM_CHANGED, (WPARAM)m_hWnd); /* SetModified happens too early */
	DrawRootList();
	OnLvnItemchangedRootlist(NULL,NULL);

	bufLen=sizeof(buf);
	if(RegQueryValueEx(g_hServerKey,_T("InstallPath"),NULL,&dwType,(BYTE*)buf,&bufLen))
	{
		// Not set
		*buf='\0';
	}
	m_szInstallPath=buf;

	bufLen=sizeof(buf);
	if(RegQueryValueEx(g_hServerKey,_T("ServerName"),NULL,&dwType,(BYTE*)buf,&bufLen))
	{
		char host[1024];
		if(!gethostname(host,sizeof(host)))
		{
			char *p=strchr(host,'.');
			if(p) *p='\0';
			MultiByteToWideChar(CP_ACP,0,host,-1,buf,sizeof(buf)/sizeof(wchar_t));
		}
		else
		{
			wcscpy(buf,L"localhost?");
		}
	}
	m_edServerName.SetWindowText(buf);

	m_btAdd.EnableWindow(g_bPrivileged);
	m_edServerName.EnableWindow(g_bPrivileged);

	return TRUE;
}

int CRepositoryPage::GetListSelection(CListCtrl& list)
{
    int nItem = -1;
    POSITION nPos = list.GetFirstSelectedItemPosition();
    if (nPos)
        nItem = list.GetNextSelectedItem(nPos);
    return nItem;
}

BOOL CRepositoryPage::OnApply() 
{
	RebuildRootList();
	
	return CTooltipPropertyPage::OnApply();
}

bool CRepositoryPage::GetRootList()
{
	TCHAR buf[MAX_PATH],buf2[MAX_PATH],buf3[512],buf4[512],buf5[512],buf6[512];
	std::wstring prefix;
	DWORD bufLen,dwVal,dwVal2,dwVal3,dwVal4;
	DWORD dwType;
	CString tmp;
	int drive;
	bool bModified = false;

	bufLen=sizeof(buf);
	if(!RegQueryValueEx(g_hServerKey,_T("RepositoryPrefix"),NULL,&dwType,(BYTE*)buf,&bufLen))
	{
		TCHAR *p = buf;
		while((p=_tcschr(p,'\\'))!=NULL)
			*p='/';
		p=buf+_tcslen(buf)-1;
		if(*p=='/')
			*p='\0';
		prefix = buf;
		bModified = true; /* Save will delete this value */
	}

	drive = _getdrive() + 'A' - 1;

	for(int n=0; n<MAX_REPOSITORIES; n++)
	{
		tmp.Format(_T("Repository%d"),n);
		bufLen=sizeof(buf);
		if(RegQueryValueEx(g_hServerKey,tmp,NULL,&dwType,(BYTE*)buf,&bufLen))
			continue;
		if(dwType!=REG_SZ)
			continue;

		TCHAR *p = buf;
		while((p=_tcschr(p,'\\'))!=NULL)
			*p='/';

		tmp.Format(_T("Repository%dName"),n);
		bufLen=sizeof(buf2);
		if(RegQueryValueEx(g_hServerKey,tmp,NULL,&dwType,(BYTE*)buf2,&bufLen))
		{
			_tcscpy(buf2,buf);
			if(prefix.size() && !_tcsnicmp(prefix.c_str(),buf,prefix.size()))
				_tcscpy(buf2,&buf[prefix.size()]);
			else
				_tcscpy(buf2,buf);
			if(buf[1]!=':')
				_sntprintf(buf,sizeof(buf),_T("%c:%s"),drive,buf2);
			p=buf2+_tcslen(buf2)-1;
			if(*p=='/')
				*p='\0';
			bModified = true;
		}
		else if(dwType!=REG_SZ)
			continue;

		tmp.Format(_T("Repository%dDescription"),n);
		bufLen=sizeof(buf3);
		if(RegQueryValueEx(g_hServerKey,tmp,NULL,&dwType,(BYTE*)buf3,&bufLen))
		{
			buf3[0]='\0';
			bModified = true;
		}
		else if(dwType!=REG_SZ)
			continue;

		tmp.Format(_T("Repository%dPublish"),n);
		bufLen=sizeof(dwVal);
		if(RegQueryValueEx(g_hServerKey,tmp,NULL,&dwType,(BYTE*)&dwVal,&bufLen))
		{
			dwVal=1;
			bModified = true;
		}
		else if(dwType!=REG_DWORD)
			continue;

		tmp.Format(_T("Repository%dDefault"),n);
		bufLen=sizeof(dwVal);
		if(RegQueryValueEx(g_hServerKey,tmp,NULL,&dwType,(BYTE*)&dwVal2,&bufLen))
		{
			dwVal2=0;
			bModified = true;
		}
		else if(dwType!=REG_DWORD)
			continue;

		tmp.Format(_T("Repository%dOnline"),n);
		bufLen=sizeof(dwVal);
		if(RegQueryValueEx(g_hServerKey,tmp,NULL,&dwType,(BYTE*)&dwVal2,&bufLen))
		{
			dwVal2=1;
			bModified = true;
		}
		else if(dwType!=REG_DWORD)
			continue;

		tmp.Format(_T("Repository%dReadWrite"),n);
		bufLen=sizeof(dwVal);
		if(RegQueryValueEx(g_hServerKey,tmp,NULL,&dwType,(BYTE*)&dwVal3,&bufLen))
		{
			dwVal3=1;
			bModified = true;
		}
		else if(dwType!=REG_DWORD)
			continue;

		tmp.Format(_T("Repository%dType"),n);
		bufLen=sizeof(dwVal);
		if(RegQueryValueEx(g_hServerKey,tmp,NULL,&dwType,(BYTE*)&dwVal4,&bufLen))
		{
			dwVal4=1;
			bModified = true;
		}
		else if(dwType!=REG_DWORD)
			continue;

		tmp.Format(_T("Repository%dRemoteServer"),n);
		bufLen=sizeof(buf4);
		if(RegQueryValueEx(g_hServerKey,tmp,NULL,&dwType,(BYTE*)buf4,&bufLen))
		{
			buf4[0]='\0';
			bModified = true;
		}
		else if(dwType!=REG_SZ)
			continue;

		tmp.Format(_T("Repository%dRemoteRepository"),n);
		bufLen=sizeof(buf5);
		if(RegQueryValueEx(g_hServerKey,tmp,NULL,&dwType,(BYTE*)buf5,&bufLen))
		{
			buf5[0]='\0';
			bModified = true;
		}
		else if(dwType!=REG_SZ)
			continue;

		tmp.Format(_T("Repository%dRemotePassphrase"),n);
		bufLen=sizeof(buf6);
		if(RegQueryValueEx(g_hServerKey,tmp,NULL,&dwType,(BYTE*)buf6,&bufLen))
		{
			buf6[0]='\0';
			bModified = true;
		}
		else if(dwType!=REG_SZ)
			continue;

		RootStruct r;
		r.root = buf;
		r.name = buf2;
		r.description = buf3;
		r.publish = dwVal?true:false;
		r.isdefault = dwVal2?true:false;
		r.online = dwVal2?true:false;
		r.readwrite = dwVal3?true:false;
		r.type = (int)dwVal4;
		r.valid = true;
		r.remote_server = buf4;
		r.remote_repository = buf5;
		r.remote_passphrase = buf6;

		m_Roots.push_back(r);
	}

	return bModified;
}

void CRepositoryPage::DrawRootList()
{
	m_listRoot.DeleteAllItems();
	for(size_t n=0; n<m_Roots.size(); n++)
	{
		if(!m_Roots[n].valid)
			continue;

		LV_FINDINFO lvf;

		lvf.flags = LVFI_STRING;
		lvf.psz = m_Roots[n].name.c_str();
		if(m_listRoot.FindItem(&lvf)>=0)
		{
			m_Roots[n].valid=false;
			continue;
		}

		int i = m_listRoot.InsertItem(n,m_Roots[n].name.c_str());
		m_listRoot.SetItem(i,0,LVIF_PARAM,0,0,0,0,n,0);
		m_listRoot.SetItem(i,1,LVIF_TEXT,m_Roots[n].root.c_str(),0,0,0,0,0);
		m_listRoot.SetItem(i,2,LVIF_TEXT,m_Roots[n].description.c_str(),0,0,0,0,0);
	}
}

void CRepositoryPage::RebuildRootList()
{
	std::wstring path,alias,desc,name,rem_serv,rem_repos,rem_pass;
	DWORD pub,def,onl,rw,ty;
	TCHAR tmp[64];
	int j;
	size_t n;

	for(n=0; n<MAX_REPOSITORIES; n++)
	{
		_sntprintf(tmp,sizeof(tmp),_T("Repository%d"),n);
		RegDeleteValue(g_hServerKey,tmp);
		_sntprintf(tmp,sizeof(tmp),_T("Repository%dName"),n);
		RegDeleteValue(g_hServerKey,tmp);
		_sntprintf(tmp,sizeof(tmp),_T("Repository%dDescription"),n);
		RegDeleteValue(g_hServerKey,tmp);
		_sntprintf(tmp,sizeof(tmp),_T("Repository%dDefault"),n);
		RegDeleteValue(g_hServerKey,tmp);
		_sntprintf(tmp,sizeof(tmp),_T("Repository%dPublish"),n);
		RegDeleteValue(g_hServerKey,tmp);
		_sntprintf(tmp,sizeof(tmp),_T("Repository%dOnline"),n);
		RegDeleteValue(g_hServerKey,tmp);
	}

	for(n=0,j=0; n<m_Roots.size(); n++)
	{
		path=m_Roots[n].root;
		alias=m_Roots[n].name;
		desc=m_Roots[n].description;
		pub=m_Roots[n].publish?1:0;
		def=m_Roots[n].isdefault?1:0;
		onl=m_Roots[n].online?1:0;
		rw=m_Roots[n].readwrite?1:0;
		ty=m_Roots[n].type;
		rem_serv=m_Roots[n].remote_server;
		rem_repos=m_Roots[n].remote_repository;
		rem_pass=m_Roots[n].remote_passphrase;
		if(m_Roots[n].valid)
		{
			_sntprintf(tmp,sizeof(tmp),_T("Repository%d"),j);
			RegSetValueEx(g_hServerKey,tmp,NULL,REG_SZ,(BYTE*)path.c_str(),(path.length()+1)*sizeof(TCHAR));
			_sntprintf(tmp,sizeof(tmp),_T("Repository%dName"),j);
			RegSetValueEx(g_hServerKey,tmp,NULL,REG_SZ,(BYTE*)alias.c_str(),(alias.length()+1)*sizeof(TCHAR));
			_sntprintf(tmp,sizeof(tmp),_T("Repository%dDescription"),j);
			RegSetValueEx(g_hServerKey,tmp,NULL,REG_SZ,(BYTE*)desc.c_str(),(desc.length()+1)*sizeof(TCHAR));
			_sntprintf(tmp,sizeof(tmp),_T("Repository%dPublish"),j);
			RegSetValueEx(g_hServerKey,tmp,NULL,REG_DWORD,(BYTE*)&pub,sizeof(DWORD));
			_sntprintf(tmp,sizeof(tmp),_T("Repository%dDefault"),j);
			RegSetValueEx(g_hServerKey,tmp,NULL,REG_DWORD,(BYTE*)&def,sizeof(DWORD));
			_sntprintf(tmp,sizeof(tmp),_T("Repository%dOnline"),j);
			RegSetValueEx(g_hServerKey,tmp,NULL,REG_DWORD,(BYTE*)&onl,sizeof(DWORD));
			_sntprintf(tmp,sizeof(tmp),_T("Repository%dReadWrite"),j);
			RegSetValueEx(g_hServerKey,tmp,NULL,REG_DWORD,(BYTE*)&rw,sizeof(DWORD));
			_sntprintf(tmp,sizeof(tmp),_T("Repository%dType"),j);
			RegSetValueEx(g_hServerKey,tmp,NULL,REG_DWORD,(BYTE*)&ty,sizeof(DWORD));
			_sntprintf(tmp,sizeof(tmp),_T("Repository%dRemoteServer"),j);
			RegSetValueEx(g_hServerKey,tmp,NULL,REG_SZ,(BYTE*)rem_serv.c_str(),(rem_serv.length()+1)*sizeof(TCHAR));
			_sntprintf(tmp,sizeof(tmp),_T("Repository%dRemoteRepository"),j);
			RegSetValueEx(g_hServerKey,tmp,NULL,REG_SZ,(BYTE*)rem_repos.c_str(),(rem_repos.length()+1)*sizeof(TCHAR));
			_sntprintf(tmp,sizeof(tmp),_T("Repository%dRemotePassphrase"),j);
			RegSetValueEx(g_hServerKey,tmp,NULL,REG_SZ,(BYTE*)rem_pass.c_str(),(rem_pass.length()+1)*sizeof(TCHAR));
			j++;
		}
	}

	RegDeleteValue(g_hServerKey,_T("RepositoryPrefix"));

	name.resize(256);
	m_edServerName.GetWindowText((LPTSTR)name.data(),name.size());
	name.resize(wcslen(name.c_str()));
	RegSetValueEx(g_hServerKey,_T("ServerName"),NULL,REG_SZ,(BYTE*)name.c_str(),(name.length()+1)*sizeof(TCHAR));
}

void CRepositoryPage::OnAddroot() 
{
	CCrypt cr;
	CNewRootDialog dlg;
	if(m_szInstallPath.GetLength())
		dlg.m_szInstallPath=m_szInstallPath+"\\";
	if(dlg.DoModal()==IDOK)
	{
		RootStruct r;
		r.valid=true;
		r.name=dlg.m_szName;
		r.root=dlg.m_szRoot;
		r.description=dlg.m_szDescription;
		r.publish=dlg.m_bPublish;
		r.online=dlg.m_bOnline;
		r.readwrite=dlg.m_bReadWrite;
		r.type=dlg.m_nType;
		r.remote_server=dlg.m_szRemoteServer;
		r.remote_repository=dlg.m_szRemoteRepository;
		r.remote_passphrase=cvs::wide(cr.crypt(cvs::narrow(dlg.m_szRemotePassphrase)));
		if(dlg.m_bDefault)
		{
			for(size_t n=0; n<m_Roots.size(); n++)
				m_Roots[n].isdefault = false;
		}
		r.isdefault=dlg.m_bDefault;
		m_Roots.push_back(r);

		DrawRootList();
		SetModified();
	}
}

void CRepositoryPage::OnDeleteroot() 
{
	int nSel = GetListSelection(m_listRoot);

	if(nSel<0) return;
	m_Roots[m_listRoot.GetItemData(nSel)].valid=false;
	m_listRoot.DeleteItem(nSel);
	m_btDelete.EnableWindow(false);
	SetModified();
}

void CRepositoryPage::OnEditroot()
{
	CCrypt cr;
	int nSel = GetListSelection(m_listRoot);

	if(nSel<0) return;

	RootStruct& r = m_Roots[m_listRoot.GetItemData(nSel)];
	CNewRootDialog dlg;
	if(m_szInstallPath.GetLength())
		dlg.m_szInstallPath=m_szInstallPath+"\\";
	dlg.m_szName = r.name.c_str();
	dlg.m_szRoot = r.root.c_str();
	dlg.m_szDescription = r.description.c_str();
	dlg.m_bPublish = r.publish;
	dlg.m_bDefault = r.isdefault;
	dlg.m_bOnline = r.online;
	dlg.m_bReadWrite = r.readwrite;
	dlg.m_nType = r.type;
	dlg.m_szRemoteServer = r.remote_server.c_str();
	dlg.m_szRemoteRepository = r.remote_repository.c_str();
	dlg.m_szRemotePassphrase = L"!!passphrase!!";
	if(dlg.DoModal()==IDOK)
	{
		r.name=dlg.m_szName;
		r.root=dlg.m_szRoot;
		r.description=dlg.m_szDescription;
		r.publish=dlg.m_bPublish;
		r.online=dlg.m_bOnline;
		r.readwrite=dlg.m_bReadWrite;
		r.type=dlg.m_nType;
		r.remote_server=dlg.m_szRemoteServer;
		r.remote_repository=dlg.m_szRemoteRepository;
		if(dlg.m_szRemotePassphrase!=L"!!passphrase!!")
			r.remote_passphrase=cvs::wide(cr.crypt(cvs::narrow(dlg.m_szRemotePassphrase)));
		if(dlg.m_bDefault)
		{
			for(size_t n=0; n<m_Roots.size(); n++)
				m_Roots[n].isdefault = false;
		}
		r.isdefault=dlg.m_bDefault;
		DrawRootList();
		SetModified();
	}
}

void CRepositoryPage::OnNMDblclkRootlist(NMHDR *pNMHDR, LRESULT *pResult)
{
	if(g_bPrivileged)
		OnEditroot();
	*pResult = 0;
}

void CRepositoryPage::OnLvnItemchangedRootlist(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMListView = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	if (!pNMListView || (pNMListView->uChanged & LVIF_STATE && ((pNMListView->uNewState & LVIS_SELECTED) != (pNMListView->uOldState & LVIS_SELECTED))))
	{
		m_btAdd.EnableWindow(g_bPrivileged && m_listRoot.GetItemCount()<MAX_REPOSITORIES);
		m_btDelete.EnableWindow(g_bPrivileged && m_listRoot.GetItemCount()>0 && GetListSelection(m_listRoot)>=0);
		m_btEdit.EnableWindow(g_bPrivileged && m_listRoot.GetItemCount()>0 && GetListSelection(m_listRoot)>=0);
	}

	if(pResult)
		*pResult = 0;
}

void CRepositoryPage::OnEnChangeServername()
{
	SetModified();
}
