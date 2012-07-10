#if !defined(AFX_serverPage_H__F52337F0_30FF_11D2_8EED_00A0C94457BF__INCLUDED_)
#define AFX_serverPage_H__F52337F0_30FF_11D2_8EED_00A0C94457BF__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// serverPage.h : header file
//

#include "TooltipPropertyPage.h"
#include "afxwin.h"
/////////////////////////////////////////////////////////////////////////////
// CserverPage dialog

class CserverPage : public CTooltipPropertyPage
{
	DECLARE_DYNCREATE(CserverPage)
	enum { IDD = IDD_PAGE1 };

// Construction
public:
	CserverPage();
	~CserverPage();

protected:
	CStatic m_stVersion;
	CStatic m_stRegistration;
	CStatic m_stHostOs;
	CStatic m_stUptime;
	CStatic m_stUserCount;
	CStatic m_szMaxUsers;
	CStatic m_stAverageTime;
	CButton m_cbSendStatistics;
	CButton m_cbLogo;

	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	afx_msg void OnBnClickedLogo();

	DECLARE_MESSAGE_MAP()
public:
	CStatic m_stSessionCount;
	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedCheck1();
	virtual BOOL OnApply();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_serverPage_H__F52337F0_30FF_11D2_8EED_00A0C94457BF__INCLUDED_)
