#if !defined(AFX_REPOSITORYPAGE_H__D778308C_9BC8_4C67_AC8B_2ACEE694BC44__INCLUDED_)
#define AFX_REPOSITORYPAGE_H__D778308C_9BC8_4C67_AC8B_2ACEE694BC44__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// RepositoryPage.h : header file
//

#include "TooltipPropertyPage.h"
#include "afxwin.h"
/////////////////////////////////////////////////////////////////////////////
// CRepositoryPage dialog

class CRepositoryPage : public CTooltipPropertyPage
{
	DECLARE_DYNCREATE(CRepositoryPage)

// Construction
public:
	struct RootStruct
	{
		std::wstring root;
		std::wstring name;
		std::wstring description;
		std::wstring remote_server;
		std::wstring remote_repository;
		std::wstring remote_passphrase;
		bool publish;
		bool valid;
		bool isdefault;
		bool online;
		bool readwrite;
		int type;
	};

	CRepositoryPage();
	~CRepositoryPage();

	CString m_szInstallPath;

	bool GetRootList();
	void DrawRootList();
	void RebuildRootList();
	int GetListSelection(CListCtrl& list);

	std::vector<RootStruct> m_Roots;

// Dialog Data
	//{{AFX_DATA(CRepositoryPage)
	enum { IDD = IDD_PAGE3 };
	CButton	m_btDelete;
	CButton	m_btAdd;
	CButton m_btEdit;
	CListCtrl m_listRoot;
	//}}AFX_DATA

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CRepositoryPage)
	public:
	virtual BOOL OnApply();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CRepositoryPage)
	virtual BOOL OnInitDialog();
	afx_msg void OnAddroot();
	afx_msg void OnDeleteroot();
	afx_msg void OnEditroot();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnNMDblclkRootlist(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnItemchangedRootlist(NMHDR *pNMHDR, LRESULT *pResult);
	CEdit m_edServerName;
	afx_msg void OnEnChangeServername();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_REPOSITORYPAGE_H__D778308C_9BC8_4C67_AC8B_2ACEE694BC44__INCLUDED_)
