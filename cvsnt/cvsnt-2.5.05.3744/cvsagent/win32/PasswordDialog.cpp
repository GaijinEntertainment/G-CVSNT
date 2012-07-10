/*	cvsnt agent
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
// PasswordDialog.cpp : implementation file
//

#include "stdafx.h"
#include "cvsagent.h"
#include "PasswordDialog.h"

void DDX_Text(CDataExchange *pDX, int nIDC, std::string& value)
{
	if(pDX->m_bSaveAndValidate)
	{
		int len = ::GetWindowTextLength(::GetDlgItem(pDX->m_pDlgWnd->GetSafeHwnd(),nIDC));
		value.resize(len);
		::GetDlgItemTextA(pDX->m_pDlgWnd->GetSafeHwnd(),nIDC,(char*)value.data(),len+1);
	}
	else
	{
		::SetDlgItemTextA(pDX->m_pDlgWnd->GetSafeHwnd(),nIDC,value.c_str());
	}
}

// CPasswordDialog dialog

IMPLEMENT_DYNAMIC(CPasswordDialog, CDialog)
CPasswordDialog::CPasswordDialog(bool bTopmost, CWnd* pParent /*=NULL*/)
	: CDialog(CPasswordDialog::IDD, pParent)
{
	m_bTopmost = bTopmost;
}

CPasswordDialog::~CPasswordDialog()
{
}

void CPasswordDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_CVSROOT, m_szCvsRoot);
	DDX_Text(pDX, IDC_PASSWORD, m_szPassword);
}


BEGIN_MESSAGE_MAP(CPasswordDialog, CDialog)
END_MESSAGE_MAP()


// CPasswordDialog message handlers

BOOL CPasswordDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	if(m_bTopmost)
		SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	SetForegroundWindow();
	return TRUE;  
}

