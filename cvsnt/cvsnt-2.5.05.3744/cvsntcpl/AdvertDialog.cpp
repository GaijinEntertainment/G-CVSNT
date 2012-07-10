/*	cvsnt control panel
    Copyright (C) 2010 March Hare Software Ltd

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
// AdvertDialog.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "AdvertDialog.h"
#include "cvsnt.h"
#include ".\AdvertDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAdvertDialog dialog


IMPLEMENT_DYNAMIC(CAdvertDialog, CDialog)
CAdvertDialog::CAdvertDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CAdvertDialog::IDD, pParent)
{
	return;
}

CAdvertDialog::~CAdvertDialog()
{
}

// CAdvertDialog message handlers

BOOL CAdvertDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_bFreeOption1.SetCheck(0);
	m_bFreeOption2.SetCheck(0);
	m_bFreeOption3.SetCheck(0);

	SetIcon(AfxGetApp()->LoadIcon(IDI_ICON1), TRUE);
	m_bFreeOption3.SetCheck(1);

	return TRUE;  
}


void CAdvertDialog::DoDataExchange(CDataExchange* pDX)
{
	DDX_Control(pDX,IDC_FREEOPT1,m_bFreeOption1);
	DDX_Control(pDX,IDC_FREEOPT2,m_bFreeOption2);
	DDX_Control(pDX,IDC_FREEOPT3,m_bFreeOption3);
	DDX_Control(pDX,IDC_SYSLINK1,m_Link1);
	DDX_Control(pDX,IDC_SYSLINK2,m_Link2);
	DDX_Control(pDX,IDC_SYSLINK3,m_Link3);
	DDX_Control(pDX,IDC_SYSLINK4,m_Link4);
	DDX_Control(pDX,IDC_SYSLINK5,m_Link5);
	DDX_Control(pDX,IDC_SYSLINK6,m_Link6);
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CAdvertDialog, CDialog)
	ON_BN_CLICKED(IDC_FREEOPT1, OnBnFreeOption1)
	ON_BN_CLICKED(IDC_FREEOPT2, OnBnFreeOption2)
	ON_BN_CLICKED(IDC_FREEOPT3, OnBnFreeOption3)
	ON_NOTIFY(NM_CLICK,IDC_SYSLINK1, OnBnLink1)
	ON_NOTIFY(NM_CLICK,IDC_SYSLINK2, OnBnLink2)
	ON_NOTIFY(NM_CLICK,IDC_SYSLINK3, OnBnLink3)
	ON_NOTIFY(NM_CLICK,IDC_SYSLINK4, OnBnLink4)
	ON_NOTIFY(NM_CLICK,IDC_SYSLINK5, OnBnLink5)
	ON_NOTIFY(NM_CLICK,IDC_SYSLINK6, OnBnLink6)
END_MESSAGE_MAP()


// CAdvertDialog message handlers

void CAdvertDialog::OnBnFreeOption1()
{
	m_bFreeOption1.SetCheck(1);
	m_bFreeOption2.SetCheck(0);
	m_bFreeOption3.SetCheck(0);
}

void CAdvertDialog::OnBnFreeOption2()
{
	m_bFreeOption1.SetCheck(0);
	m_bFreeOption2.SetCheck(1);
	m_bFreeOption3.SetCheck(0);
}

void CAdvertDialog::OnBnFreeOption3()
{
	m_bFreeOption1.SetCheck(0);
	m_bFreeOption2.SetCheck(0);
	m_bFreeOption3.SetCheck(1);
}

void CAdvertDialog::OnOK()
{
	if (!m_bFreeOption3.GetCheck())
		gotourl(_T("http://store.march-hare.com/s.nl/sc.2/category.2/.f"));

	CDialog::OnOK(); // This will close the dialog and DoModal will return
}

void CAdvertDialog::OnBnLink1(NMHDR *pNMHDR, LRESULT *pResult)
{
	PNMLINK pNMLink = (PNMLINK) pNMHDR;
	gotourl(pNMLink->item.szUrl /*_T("http://www.ohloh.net/p/cvsnt")*/);
}

void CAdvertDialog::OnBnLink2(NMHDR *pNMHDR, LRESULT *pResult)
{
	PNMLINK pNMLink = (PNMLINK) pNMHDR;
	gotourl(pNMLink->item.szUrl /*_T("http://www.ohloh.net/p/cvsnt")*/);
}

void CAdvertDialog::OnBnLink3(NMHDR *pNMHDR, LRESULT *pResult)
{
	PNMLINK pNMLink = (PNMLINK) pNMHDR;
	gotourl(pNMLink->item.szUrl /*_T("http://www.ohloh.net/p/cvsnt")*/);
}

void CAdvertDialog::OnBnLink4(NMHDR *pNMHDR, LRESULT *pResult)
{
	PNMLINK pNMLink = (PNMLINK) pNMHDR;
	gotourl(pNMLink->item.szUrl /*_T("http://www.ohloh.net/p/cvsnt")*/);
}

void CAdvertDialog::OnBnLink5(NMHDR *pNMHDR, LRESULT *pResult)
{
	PNMLINK pNMLink = (PNMLINK) pNMHDR;
	gotourl(pNMLink->item.szUrl /*_T("http://www.ohloh.net/p/cvsnt")*/);
}

void CAdvertDialog::OnBnLink6(NMHDR *pNMHDR, LRESULT *pResult)
{
	PNMLINK pNMLink = (PNMLINK) pNMHDR;
	gotourl(pNMLink->item.szUrl /*_T("http://www.ohloh.net/p/cvsnt")*/);
}

void CAdvertDialog::gotourl(const TCHAR *urlstr)
{
	HINSTANCE shResult;
	shResult = ShellExecute(NULL,_T("open"),urlstr,NULL,NULL,SW_SHOWNORMAL);
	if ((int)shResult<32)
	{
		CString tmp2;
		int rez=(int)shResult;
		switch (rez)
		{
		case 0:
			tmp2.Format(_T("\nShellExecute(%s) failed: The operating system is out of memory or resources."),urlstr);
			break;
		case ERROR_FILE_NOT_FOUND:
			tmp2.Format(_T("\nShellExecute(%s) failed: The specified file was not found."),urlstr);
			break;
		case ERROR_PATH_NOT_FOUND:
			tmp2.Format(_T("\nShellExecute(%s) failed: The specified file was not found."),urlstr);
			break;
		case ERROR_BAD_FORMAT:
			tmp2.Format(_T("\nShellExecute(%s) failed:\nThe .exe file is invalid (non-Microsoft Win32 .exe or error in .exe image)."),urlstr);
			break;
		case SE_ERR_ACCESSDENIED:
			tmp2.Format(_T("\nShellExecute(%s) failed:\nThe operating system denied access to the specified file."),urlstr);
			break;
		case SE_ERR_ASSOCINCOMPLETE:
			tmp2.Format(_T("\nShellExecute(%s) failed:\nThe file name association is incomplete or invalid."),urlstr);
			break;
		case SE_ERR_DDEBUSY:
			tmp2.Format(_T("\nShellExecute(%s) failed:\nThe Dynamic Data Exchange (DDE) transaction could not be completed because other DDE transactions were being processed."),urlstr);
			break;
		case SE_ERR_DDEFAIL:
			tmp2.Format(_T("\nShellExecute(%s) failed:\nThe DDE transaction failed."),urlstr);
			break;
		case SE_ERR_DDETIMEOUT:
			tmp2.Format(_T("\nShellExecute(%s) failed:\nThe DDE transaction could not be completed because the request timed out."),urlstr);
			break;
		case SE_ERR_DLLNOTFOUND:
			tmp2.Format(_T("\nShellExecute(%s) failed:\nThe specified DLL was not found."),urlstr);
			break;
		case SE_ERR_NOASSOC:
			tmp2.Format(_T("\nShellExecute(%s) failed:\nThere is no application associated with the given file name extension. This error will also be returned if you attempt to print a file that is not printable."),urlstr);
			break;
		case SE_ERR_OOM:
			tmp2.Format(_T("\nShellExecute(%s) failed:\nThere was not enough memory to complete the operation."),urlstr);
			break;
		case SE_ERR_SHARE:
			tmp2.Format(_T("\nShellExecute(%s) failed:\nA sharing violation occurred."),urlstr);
			break;
		default:
			tmp2.Format(_T("\nShellExecute(%s) failed: %d"),urlstr,shResult);
		}
		AfxMessageBox(tmp2,MB_ICONSTOP);
	}
	return;
}