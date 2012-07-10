#if !defined(AFX_AboutPage_H__F52337F0_30FF_11D2_8EED_00A0C94457BF__INCLUDED_)
#define AFX_AboutPage_H__F52337F0_30FF_11D2_8EED_00A0C94457BF__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// AboutPage.h : header file
//

#include "TooltipPropertyPage.h"
/////////////////////////////////////////////////////////////////////////////
// CAboutPage dialog

class CAboutPage : public CTooltipPropertyPage
{
	DECLARE_DYNCREATE(CAboutPage)

// Construction
public:
	CAboutPage();
	~CAboutPage();

// Dialog Data
	//{{AFX_DATA(CAboutPage)
	enum { IDD = IDD_PAGE4 };
	CButton	m_btStart;
	CButton	m_btStop;
	CString	m_szVersion;
	CString	m_szStatus;
	CString m_szLockStatus;
	CButton m_btLockStart;
	CButton m_btLockStop;
//	CButton m_btSshStart;
//	CButton m_btSshStop;
//	CString m_szSshStatus;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CAboutPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	SC_HANDLE m_hService;
	SC_HANDLE m_hLockService;
//	SC_HANDLE m_hSshService;
	SC_HANDLE m_hSCManager;

	LPCTSTR GetErrorString();
	void UpdateStatus();

	// Generated message map functions
	//{{AFX_MSG(CAboutPage)
	virtual BOOL OnInitDialog();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnStart();
	afx_msg void OnStop();
	afx_msg void OnBnClickedStart2();
	afx_msg void OnBnClickedStop2();
//	afx_msg void OnBnClickedStart3();
//	afx_msg void OnBnClickedStop3();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnStnClickedLogo();
	CButton m_cbLogo;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_AboutPage_H__F52337F0_30FF_11D2_8EED_00A0C94457BF__INCLUDED_)
