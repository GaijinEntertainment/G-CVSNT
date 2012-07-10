#if !defined(AFX_NEWROOTDIALOG_H__1DDA657E_929C_4496_AF11_A7F908CAD40F__INCLUDED_)
#define AFX_NEWROOTDIALOG_H__1DDA657E_929C_4496_AF11_A7F908CAD40F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// NewRootDialog.h : header file
//
#include "RepositoryPage.h"
#include "afxwin.h"

/////////////////////////////////////////////////////////////////////////////
// CNewRootDialog dialog
class CNewRootDialog : public CDialog
{
// Construction
public:
	CNewRootDialog(CWnd* pParent = NULL);   // standard constructor

	CString m_szRoot;
	CString m_szName;
	CString m_szInstallPath;
	CString m_szDescription;
	bool m_bPublish;
	bool m_bDefault;
	bool m_bOnline;
	bool m_bReadWrite;
	bool m_bSyncName;
	int m_nType;
	CComboBox m_cbType;
	CString m_szRemoteServer;
	CString m_szRemoteRepository;
	CString m_szRemotePassphrase;
	CStatic m_stRemoteGroup;
	CStatic m_stServerText;
	CStatic m_stRepositoryText;
	CStatic m_stPassphraseText;
	CButton m_btReadWrite;
	CEdit m_edLocation;
	CButton m_btSelect;

// Dialog Data
	//{{AFX_DATA(CNewRootDialog)
	enum { IDD = IDD_NEWROOT };
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNewRootDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	void UpdateName();
	BOOL DeepCreateDirectory(LPTSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
	void EnableWindowsForType();

	// Generated message map functions
	//{{AFX_MSG(CNewRootDialog)
	afx_msg void OnSelect();
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	afx_msg void OnEnChangeRoot();
	afx_msg void OnEnChangeName();
	afx_msg void OnEnChangeDescription();
	afx_msg void OnPublish();
	afx_msg void OnBnClickedOnline();
	afx_msg void OnCbnSelendokType();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NEWROOTDIALOG_H__1DDA657E_929C_4496_AF11_A7F908CAD40F__INCLUDED_)
