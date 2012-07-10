#ifndef SETTINGSPAGE__H
#define SETTINGSPAGE__H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// SettingsPage.h : header file
//

#include "TooltipPropertyPage.h"
#include "afxwin.h"
/////////////////////////////////////////////////////////////////////////////
// CSettingsPage dialog

class CSettingsPage : public CTooltipPropertyPage
{
	DECLARE_DYNCREATE(CSettingsPage)

// Construction
public:
	wchar_t mw_domain[256];
	wchar_t mw_computer[256];
	wchar_t mw_pdc[256];

	CSettingsPage();
	~CSettingsPage();

	DWORD QueryDword(LPCTSTR szKey);
	CString QueryString(LPCTSTR szKey);

// Dialog Data
	//{{AFX_DATA(CSettingsPage)
	enum { IDD = IDD_PAGE2 };
	CEdit	m_edTempDir;
	CEdit	m_edLockServer;
	CComboBox m_cbEncryption;
	CComboBox m_cbCompression;
	CComboBox m_cbDefaultDomain;
	CComboBox m_cbRunAsUser;
	CSpinButtonCtrl m_sbServerPort;
	CSpinButtonCtrl m_sbLockPort;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CSettingsPage)
	public:
	virtual BOOL OnApply();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CSettingsPage)
	afx_msg void OnChangetemp();
	virtual BOOL OnInitDialog();
	afx_msg void OnChangePserverport();
	afx_msg void OnChangeLockserverport();
	afx_msg void OnCbnSelendokEncryption();
	afx_msg void OnCbnSelendokCompression();
	afx_msg void OnCbnSelendokDefaultdomain();
	afx_msg void OnCbnSelendokRunasuser();
	afx_msg void OnCbnDropdownRunasuser();
	afx_msg void OnCbnEditchangeRunasuser();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	CEdit m_edAnonUser;
	afx_msg void OnEnChangeAnonuser();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ADVANCEDPAGE_H__D360303C_4B6C_41C1_9261_1D16722BC461__INCLUDED_)
