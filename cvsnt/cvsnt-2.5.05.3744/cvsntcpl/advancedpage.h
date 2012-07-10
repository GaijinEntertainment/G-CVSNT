#if !defined(AFX_ADVANCEDPAGE_H__D360303C_4B6C_41C1_9261_1D16722BC461__INCLUDED_)
#define AFX_ADVANCEDPAGE_H__D360303C_4B6C_41C1_9261_1D16722BC461__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// AdvancedPage.h : header file
//

#include "TooltipPropertyPage.h"
#include "afxwin.h"
/////////////////////////////////////////////////////////////////////////////
// CAdvancedPage dialog

class CAdvancedPage : public CTooltipPropertyPage
{
	DECLARE_DYNCREATE(CAdvancedPage)

// Construction
public:
	CAdvancedPage();
	~CAdvancedPage();

	DWORD QueryDword(LPCTSTR szKey);
	CString QueryString(LPCTSTR szKey);

// Dialog Data
	//{{AFX_DATA(CAdvancedPage)
	enum { IDD = IDD_PAGE6 };
	CButton m_btNoReverseDns;
	CButton m_btLockServerLocal;
	CButton m_btAllowTrace;
	CButton m_btUnicodeServer;
	CButton m_btReadOnly;
	CButton m_btRemoteInit;
	//}}AFX_DATA

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CAdvancedPage)
	public:
	virtual BOOL OnApply();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAdvancedPage)
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedNoreversedns();
	afx_msg void OnBnClickedAllowtrace();
	afx_msg void OnBnClickedLockserverlocal();
	afx_msg void OnBnClickedUnicodeserver();
	afx_msg void OnBnClickedReadonly();
	afx_msg void OnBnClickedRemoteinit();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	CComboBox m_cbZeroconf;
	afx_msg void OnCbnSelendokZeroconf();
	CButton m_cbAtomicCheckouts;
	CButton m_cbGlobalScript;
	afx_msg void OnBnClickedAtomiccheckouts();
	afx_msg void OnBnClickedGlobalscript();
	afx_msg void OnCbnSelchangeZeroconf();
	CButton m_btReplication;
	CEdit m_edReplicationPort;
	CStatic m_stReplicationPort;
	afx_msg void OnBnClickedReplication();
	afx_msg void OnEnChangeReplicationport();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ADVANCEDPAGE_H__D360303C_4B6C_41C1_9261_1D16722BC461__INCLUDED_)
