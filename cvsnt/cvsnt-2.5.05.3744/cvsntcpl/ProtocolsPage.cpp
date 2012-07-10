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
// ProtocolsPage.cpp : implementation file
//

#include "stdafx.h"
#include "ProtocolsPage.h"

// CProtocolsPage dialog

IMPLEMENT_DYNAMIC(CProtocolsPage, CPropertyPage)
CProtocolsPage::CProtocolsPage()
	: CPropertyPage(CProtocolsPage::IDD)
{
}

CProtocolsPage::~CProtocolsPage()
{
}

void CProtocolsPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST1, m_lcProtocols);
	DDX_Control(pDX, IDC_BUTTON1, m_btConfigure);
}


BEGIN_MESSAGE_MAP(CProtocolsPage, CPropertyPage)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST1, OnNMDblclkList1)
//	ON_NOTIFY(LVN_ITEMACTIVATE, IDC_LIST1, OnLvnItemActivateList1)
	ON_BN_CLICKED(IDC_BUTTON1, OnBnClickedButton1)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST1, OnLvnItemchangedList1)
END_MESSAGE_MAP()

void CProtocolsPage::InitSub(HANDLE hFind, LPTSTR buf, LPTSTR bp, WIN32_FIND_DATA& wfd, int& n)
{
	CProtocolLibrary lib;
	do
	{
		plugin_interface *ui = NULL;
		get_plugin_interface_t gpi = NULL;
		_tcscpy(bp,wfd.cFileName);
		HMODULE hLib = LoadLibrary(buf);
		if(hLib)
		{
			if((gpi = (get_plugin_interface_t)GetProcAddress(hLib,"get_plugin_interface"))!=NULL)
				ui = (plugin_interface*)gpi();

			if(ui)
				ui->__cvsnt_reserved = (void*)hLib;
			else
				FreeLibrary(hLib);
		}
		int iItem;
		if(ui && ui->interface_version != PLUGIN_INTERFACE_VERSION)
		{
			iItem = m_lcProtocols.InsertItem(LVIF_TEXT|LVIF_PARAM,n,wfd.cFileName,0,0,0,(LPARAM)ui);
			if(ui->version)
				m_lcProtocols.SetItemText(iItem,1,L"Incorrect version!");
			ui = NULL;
			FreeLibrary(hLib);
		}
		else if(ui && ui->description)
		{
			iItem = m_lcProtocols.InsertItem(LVIF_TEXT|LVIF_PARAM,n,CFileAccess::Win32Wide(ui->description),0,0,0,(LPARAM)ui);
			if(ui->version)
				m_lcProtocols.SetItemText(iItem,1,CFileAccess::Win32Wide(ui->version));
		}
		else if(gpi)
			iItem = m_lcProtocols.InsertItem(LVIF_TEXT|LVIF_PARAM,n,wfd.cFileName,0,0,0,(LPARAM)ui);

		n++;
	} while(FindNextFile(hFind,&wfd));
}

BOOL CProtocolsPage::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	m_lcProtocols.InsertColumn(0,_T("Name"),LVCFMT_LEFT,300);
	m_lcProtocols.InsertColumn(1,_T("Version"),LVCFMT_LEFT,200);
	
	
	TCHAR buf[MAX_PATH];
	int n=0;
	_tcsncpy(buf,cvs::wide(CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDProtocols)),sizeof(buf)/sizeof(buf[0]));

	TCHAR *bp=buf+_tcslen(buf)+1;
	WIN32_FIND_DATA wfd;
	_tcscat(buf,_T("\\*.dll"));
	HANDLE hFind = FindFirstFile(buf,&wfd);
	if(hFind!=INVALID_HANDLE_VALUE)
	{
		InitSub(hFind,buf,bp,wfd,n);
		FindClose(hFind);
	}
	_tcsncpy(buf,cvs::wide(CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDTriggers)),sizeof(buf)/sizeof(buf[0]));
	bp=buf+_tcslen(buf)+1;
	_tcscat(buf,_T("\\*.dll"));
	hFind = FindFirstFile(buf,&wfd);
	if(hFind!=INVALID_HANDLE_VALUE)
	{
		InitSub(hFind,buf,bp,wfd,n);
		FindClose(hFind);
	}
	_tcsncpy(buf,cvs::wide(CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDXdiff)),sizeof(buf)/sizeof(buf[0]));
	bp=buf+_tcslen(buf)+1;
	_tcscat(buf,_T("\\*.dll"));
	hFind = FindFirstFile(buf,&wfd);
	if(hFind!=INVALID_HANDLE_VALUE)
	{
		InitSub(hFind,buf,bp,wfd,n);
		FindClose(hFind);
	}

	m_btConfigure.EnableWindow(FALSE);
	return TRUE;
}


void CProtocolsPage::OnNMDblclkList1(NMHDR *pNMHDR, LRESULT *pResult)
{
	POSITION pos = m_lcProtocols.GetFirstSelectedItemPosition();
	int item;

	if(!pos)
		return;
	item=m_lcProtocols.GetNextSelectedItem(pos);

	plugin_interface *ui = (plugin_interface*)m_lcProtocols.GetItemData(item);
	if(!ui || !ui->configure)
		return;

	ui->configure(ui,(void*)m_hWnd);

	*pResult = 0;
}

void CProtocolsPage::OnLvnItemchangedList1(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMIA = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	POSITION pos = m_lcProtocols.GetFirstSelectedItemPosition();
	int item;

	if(!pos)
		return;
	item=m_lcProtocols.GetNextSelectedItem(pos);

	plugin_interface *ui = (plugin_interface*)m_lcProtocols.GetItemData(item);
	if(!ui || !ui->configure || !g_bPrivileged)
		m_btConfigure.EnableWindow(FALSE);
	else
		m_btConfigure.EnableWindow(TRUE);

	*pResult = 0;
}

void CProtocolsPage::OnBnClickedButton1()
{
	POSITION pos = m_lcProtocols.GetFirstSelectedItemPosition();
	int item;

	if(!pos)
		return;
	item=m_lcProtocols.GetNextSelectedItem(pos);

	plugin_interface *ui = (plugin_interface*)m_lcProtocols.GetItemData(item);
	if(!ui || !ui->configure)
		return;

	ui->configure(ui,(void*)m_hWnd);
}
