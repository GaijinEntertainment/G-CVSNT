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
// NewRootDialog.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "NewRootDialog.h"
#include "cvsnt.h"
#include ".\newrootdialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CNewRootDialog dialog


CNewRootDialog::CNewRootDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CNewRootDialog::IDD, pParent)
	, m_szDescription(_T(""))
	, m_bPublish(true)
	, m_bDefault(false)
	, m_bOnline(true)
	, m_bReadWrite(true)
	, m_szRemoteServer(_T(""))
	, m_szRemoteRepository(_T(""))
	, m_szRemotePassphrase(_T(""))
{
	//{{AFX_DATA_INIT(CNewRootDialog)
	//}}AFX_DATA_INIT
	m_nType=1;
}


void DDX_Check(CDataExchange* pDX, int nId, bool& val)
{
	int v = val?1:0;
	DDX_Check(pDX,nId,v);
	val = v?true:false;
}

void CNewRootDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNewRootDialog)
	DDX_Text(pDX, IDC_ROOT, m_szRoot);
	DDX_Text(pDX, IDC_NAME, m_szName);
	DDX_Text(pDX, IDC_DESCRIPTION, m_szDescription);
	DDX_Check(pDX, IDC_PUBLISH, m_bPublish);
	DDX_Check(pDX, IDC_DEFAULT, m_bDefault);
	//}}AFX_DATA_MAP
	DDX_Check(pDX, IDC_ONLINE, m_bOnline);
	DDX_Check(pDX, IDC_READWRITE, m_bReadWrite);
	DDX_Control(pDX, IDC_TYPE, m_cbType);
	DDX_Text(pDX, IDC_SERVER, m_szRemoteServer);
	DDX_Text(pDX, IDC_REPOSITORY, m_szRemoteRepository);
	DDX_Text(pDX, IDC_PASSPHRASE, m_szRemotePassphrase);
	DDX_Control(pDX, IDC_REMOTEGROUP, m_stRemoteGroup);
	DDX_Control(pDX, IDC_SERVERTEXT, m_stServerText);
	DDX_Control(pDX, IDC_REPOSITORYTEXT, m_stRepositoryText);
	DDX_Control(pDX, IDC_PASSPHRASETEXT, m_stPassphraseText);
	DDX_Control(pDX, IDC_READWRITE, m_btReadWrite);
	DDX_Control(pDX, IDC_ROOT, m_edLocation);
	DDX_Control(pDX, IDC_SELECT, m_btSelect);
}


BEGIN_MESSAGE_MAP(CNewRootDialog, CDialog)
	//{{AFX_MSG_MAP(CNewRootDialog)
	ON_BN_CLICKED(IDC_SELECT, OnSelect)
	//}}AFX_MSG_MAP
	ON_EN_CHANGE(IDC_ROOT, OnEnChangeRoot)
	ON_EN_CHANGE(IDC_NAME, OnEnChangeName)
	ON_EN_CHANGE(IDC_DESCRIPTION, OnEnChangeDescription)
	ON_BN_CLICKED(IDC_PUBLISH, OnPublish)
	ON_BN_CLICKED(IDC_ONLINE, OnBnClickedOnline)
	ON_CBN_SELENDOK(IDC_TYPE, OnCbnSelendokType)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNewRootDialog message handlers

void CNewRootDialog::OnSelect() 
{
	TCHAR fn[MAX_PATH];
	LPITEMIDLIST idl,idlroot;
	IMalloc *mal;
	
	SHGetSpecialFolderLocation(m_hWnd, CSIDL_DRIVES, &idlroot);
	SHGetMalloc(&mal);
	BROWSEINFO bi = { m_hWnd, idlroot, fn, _T("Select folder for repository root."), BIF_NEWDIALOGSTYLE|BIF_STATUSTEXT|BIF_UAHINT|BIF_RETURNONLYFSDIRS|BIF_RETURNFSANCESTORS, BrowseValid };
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

	m_szRoot =fn;
	m_szRoot .Replace('\\','/');
	UpdateData(FALSE);
	UpdateName();
}

void CNewRootDialog::OnOK() 
{
	size_t badpos, badlen;
	CString tmp;
	UpdateData(TRUE);
	bool bCreated=false;
	TCHAR shortfn[4];

	m_nType=(int)m_cbType.GetItemData(m_cbType.GetCurSel());

	m_szName.Replace(_T("\\"),_T("/"));
	if(m_szName.GetLength()<2 || (m_szName[0]!='/' && m_szName[1]!=':') || m_szName.Left(2)=="//")
	{
		AfxMessageBox(_T("The name of the repository root must be a unix-style absolute pathname starting with '/'"));
		return;
	}

	if(m_nType!=2) // Proxy all requests doesn't need a local server
	{
		m_szRoot .Replace(_T("\\"),_T("/"));
		_tcsncpy(shortfn,m_szRoot ,4);
		shortfn[3]='\0';
		if(m_szRoot [1]!=':')
		{
			AfxMessageBox(_T("You must specify an absolute root for the pathname"),MB_ICONSTOP|MB_OK);
			return;
		}
		if(GetDriveType(shortfn)==DRIVE_REMOTE)
		{
			AfxMessageBox(_T("You must store the repository on a local drive.  Network drives are not allowed"),MB_ICONSTOP|MB_OK);
			return;
		}

		badlen=m_szRoot.GetLength();
		badpos=m_szRoot.Find(_T("/cvs"));
		if ((badpos!=-1)&&(badpos+4==badlen))
			if(AfxMessageBox(_T("Using reserved words like CVS or CVSROOT in repository paths can create compatibility and reliability problems and is not recommended.  Are you sure you want to continue?"),MB_YESNO|MB_DEFBUTTON2)!=IDYES)
				return;
		badpos=m_szRoot.Find(_T("/CVS"));
		if ((badpos!=-1)&&(badpos+4==badlen))
			if(AfxMessageBox(_T("Using reserved words like CVS or CVSROOT in repository paths can create compatibility and reliability problems and is not recommended.  Are you sure you want to continue?"),MB_YESNO|MB_DEFBUTTON2)!=IDYES)
				return;
		badpos=m_szRoot.Find(_T("/cvsroot"));
		if ((badpos!=-1)&&(badpos+8==badlen))
			if(AfxMessageBox(_T("Using reserved words like CVS or CVSROOT in repository paths can create compatibility and reliability problems and is not recommended.  Are you sure you want to continue?"),MB_YESNO|MB_DEFBUTTON2)!=IDYES)
				return;
		badpos=m_szRoot.Find(_T("/CVSROOT"));
		if ((badpos!=-1)&&(badpos+8==badlen))
			if(AfxMessageBox(_T("Using reserved words like CVS or CVSROOT in repository paths can create compatibility and reliability problems and is not recommended.  Are you sure you want to continue?"),MB_YESNO|MB_DEFBUTTON2)!=IDYES)
				return;
		if ( (m_szRoot.Find(_T("/CVS/"))!=-1) || (m_szRoot.Find(_T("/CVSROOT/"))!=-1) ||
			(m_szRoot.Find(_T("/cvs/"))!=-1) || (m_szRoot.Find(_T("/cvsroot/"))!=-1) )
		{
			if(AfxMessageBox(_T("Using reserved words like CVS or CVSROOT in repository paths can create compatibility and reliability problems and is not recommended.  Are you sure you want to continue?"),MB_YESNO|MB_DEFBUTTON2)!=IDYES)
				return;
		}

	}

	if(m_szName[1]==':')
	{
		if(AfxMessageBox(_T("Using drive letters in repository names can create compatibility problems with Unix clients and is not recommended.  Are you sure you want to continue?"),MB_YESNO|MB_DEFBUTTON2)!=IDYES)
			return;
	}

	badlen=m_szName.GetLength();
	badpos=m_szName.Find(_T("/cvs"));
	if ((badpos!=-1)&&(badpos+4==badlen))
		if(AfxMessageBox(_T("Using reserved words like CVS or CVSROOT in repository names can create compatibility and reliability problems and is not recommended.  Are you sure you want to continue?"),MB_YESNO|MB_DEFBUTTON2)!=IDYES)
			return;
	badpos=m_szName.Find(_T("/CVS"));
	if ((badpos!=-1)&&(badpos+4==badlen))
		if(AfxMessageBox(_T("Using reserved words like CVS or CVSROOT in repository names can create compatibility and reliability problems and is not recommended.  Are you sure you want to continue?"),MB_YESNO|MB_DEFBUTTON2)!=IDYES)
			return;
	badpos=m_szName.Find(_T("/cvsroot"));
	if ((badpos!=-1)&&(badpos+8==badlen))
		if(AfxMessageBox(_T("Using reserved words like CVS or CVSROOT in repository names can create compatibility and reliability problems and is not recommended.  Are you sure you want to continue?"),MB_YESNO|MB_DEFBUTTON2)!=IDYES)
			return;
	badpos=m_szName.Find(_T("/CVSROOT"));
	if ((badpos!=-1)&&(badpos+8==badlen))
		if(AfxMessageBox(_T("Using reserved words like CVS or CVSROOT in repository names can create compatibility and reliability problems and is not recommended.  Are you sure you want to continue?"),MB_YESNO|MB_DEFBUTTON2)!=IDYES)
			return;
	if ( (m_szName.Find(_T("/CVS/"))!=-1) || (m_szName.Find(_T("/CVSROOT/"))!=-1) ||
		(m_szName.Find(_T("/cvs/"))!=-1) || (m_szName.Find(_T("/cvsroot/"))!=-1) )
	{
		if(AfxMessageBox(_T("Using reserved words like CVS or CVSROOT in repository names can create compatibility and reliability problems and is not recommended.  Are you sure you want to continue?"),MB_YESNO|MB_DEFBUTTON2)!=IDYES)
			return;
	}

	for(int n=1; n<m_szName.GetLength(); n++)
	{
		if(strchr("\"+,.;<=>[]|${} '",m_szName[n]) || (n>1 && m_szName[n]==':'))
		{
			if(AfxMessageBox(_T("The repository name contains characters that may create compatibility problems with certain clients.  Using such names is not recommended.  Are you sure you want to continue?"),MB_YESNO|MB_DEFBUTTON2)!=IDYES)
				return;
		}
	}

	if(m_nType!=2) // Proxy all requests doesn't need a local server
	{
		DWORD dwStatus = GetFileAttributes(m_szRoot);
		if(dwStatus==0xFFFFFFFF)
		{
			tmp.Format(_T("%s does not exist.  Create it?"),(LPCTSTR)m_szRoot );
			if(AfxMessageBox(tmp,MB_ICONSTOP|MB_YESNO|MB_DEFBUTTON2)==IDNO)
				return;
			if(!DeepCreateDirectory(m_szRoot.GetBuffer(),NULL))
			{
				m_szRoot.ReleaseBuffer();
				AfxMessageBox(_T("Couldn't create directory"),MB_ICONSTOP|MB_OK);
				return;
			}
			m_szRoot.ReleaseBuffer();
			bCreated=true;
		}
		if(!bCreated && !(dwStatus&FILE_ATTRIBUTE_DIRECTORY))
		{
			tmp.Format(_T("%s is not a directory."),(LPCTSTR)m_szRoot );
			AfxMessageBox(tmp,MB_ICONSTOP|MB_OK);
			return;
		}
		tmp=m_szRoot ;
		tmp+="\\CVSROOT";
		dwStatus = GetFileAttributes(tmp);
		if(dwStatus==0xFFFFFFFF)
		{
			tmp.Format(_T("%s exists, but is not a valid CVS repository.\n\nDo you want to initialise it?"),(LPCTSTR)m_szRoot );
			if(!bCreated && AfxMessageBox(tmp,MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2)==IDNO)
				return;
			tmp.Format(_T("%scvs -d \"%s\" init -n"),(LPCTSTR)m_szInstallPath,(LPCTSTR)m_szRoot );
			{
				CWaitCursor wait;
				STARTUPINFO si = { sizeof(STARTUPINFO) };
				PROCESS_INFORMATION pi = { 0 };
				si.dwFlags = STARTF_USESHOWWINDOW;
				si.wShowWindow = SW_SHOWMINNOACTIVE;
				if(CreateProcess(NULL,(LPTSTR)(LPCTSTR)tmp,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi))
				{
					CloseHandle(pi.hThread);
					WaitForSingleObject(pi.hProcess,INFINITE);
					CloseHandle(pi.hProcess);
				}
			}
			tmp=m_szRoot ;
			tmp+="/CVSROOT";
			dwStatus = GetFileAttributes(tmp);
			if(dwStatus==0xFFFFFFFF)
			{
				tmp.Format(_T("Repository initialisation failed.  To see errors, type the following at the command line:\n\n%scvs -d %s init"),(LPCTSTR)m_szInstallPath,(LPCTSTR)m_szRoot );
				AfxMessageBox(tmp,MB_ICONSTOP|MB_OK);
			}
		}
	}

	CDialog::OnOK();
}

BOOL CNewRootDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();

	if(!m_szName.GetLength())
		m_bSyncName = true;
	else
		m_bSyncName = false;

	m_cbType.SetItemData(m_cbType.AddString(_T("Standard")),1);
	m_cbType.SetItemData(m_cbType.AddString(_T("Proxy all requests")),2);
	m_cbType.SetItemData(m_cbType.AddString(_T("Proxy only write requests")),3);

	if(m_nType)
		m_cbType.SetCurSel(m_nType-1);
	else
	{
		m_nType=1;
		m_cbType.SetCurSel(0);
	}
	
	EnableWindowsForType();

	return TRUE;
}

void CNewRootDialog::OnEnChangeRoot()
{
	UpdateName();
}

void CNewRootDialog::OnEnChangeDescription()
{
}

void CNewRootDialog::OnPublish()
{
}

void CNewRootDialog::UpdateName()
{
	if(m_bSyncName)
	{
		UpdateData();
		if(m_szRoot .GetLength())
		{
			if(m_szRoot .GetLength()>1 && m_szRoot [1]==':')
				m_szName = m_szRoot .Right(m_szRoot .GetLength()-2);
			else if(m_szRoot [0]!='/')
				m_szName = "/" + m_szRoot ;
			else
				m_szName = m_szRoot ;
			m_szName.Replace('\\','/');
			SetDlgItemText(IDC_NAME, m_szName);
			m_bSyncName=true;
		}
		else
			SetDlgItemText(IDC_NAME,_T(""));
	}
}

void CNewRootDialog::OnEnChangeName()
{
	UpdateData();
	if(m_szName.GetLength())
		m_bSyncName=false;
	else
		m_bSyncName=true;
}

BOOL CNewRootDialog::DeepCreateDirectory(LPTSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    LPTSTR cp;

    if (CreateDirectory(lpPathName, lpSecurityAttributes) || GetLastError()==ERROR_ALREADY_EXISTS)
		return TRUE;
    if (GetLastError()!=ERROR_PATH_NOT_FOUND)
		return FALSE;

	cp = _tcsrchr(lpPathName, '/');
	if(cp==lpPathName)
		return FALSE;

    *cp = '\0';
	if(!DeepCreateDirectory(lpPathName, lpSecurityAttributes))
		return FALSE;
    *cp++ = '/';
    if (*cp == '\0')
		return TRUE;
	return CreateDirectory(lpPathName, lpSecurityAttributes);
}

void CNewRootDialog::OnBnClickedOnline()
{
}

void CNewRootDialog::OnCbnSelendokType()
{
	m_nType = m_cbType.GetItemData(m_cbType.GetCurSel());
	EnableWindowsForType();
}

void CNewRootDialog::EnableWindowsForType()
{
	if(m_nType==1)
	{
		m_stRemoteGroup.EnableWindow(FALSE);
		m_stServerText.EnableWindow(FALSE);
		m_stRepositoryText.EnableWindow(FALSE);
		m_stPassphraseText.EnableWindow(FALSE);
		m_btReadWrite.EnableWindow(TRUE);
	}
	else
	{
		m_stRemoteGroup.EnableWindow(TRUE);
		m_stServerText.EnableWindow(TRUE);
		m_stRepositoryText.EnableWindow(TRUE);
		m_stPassphraseText.EnableWindow(TRUE);
		m_btReadWrite.EnableWindow(FALSE);
	}
	if(m_nType==2)
	{
		m_edLocation.EnableWindow(FALSE);
		m_btSelect.EnableWindow(FALSE);
	}
	else
	{
		m_edLocation.EnableWindow(TRUE);
		m_btSelect.EnableWindow(TRUE);
	}
}
