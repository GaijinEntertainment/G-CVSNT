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
// CAdvertDialog dialog
class CAdvertDialog : public CDialog
{
	DECLARE_DYNAMIC(CAdvertDialog)

public:
	CAdvertDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CAdvertDialog();

// Dialog Data
	enum { IDD = IDD_FREEDIALOG };

	CButton m_bFreeOption1;
	CButton m_bFreeOption2;
	CButton m_bFreeOption3;
	CLinkCtrl m_Link1;
	CLinkCtrl m_Link2;
	CLinkCtrl m_Link3;
	CLinkCtrl m_Link4;
	CLinkCtrl m_Link5;
	CLinkCtrl m_Link6;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	virtual void OnOK();
	void gotourl(const TCHAR *urlstr);
	afx_msg void OnBnLink1(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnLink2(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnLink3(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnLink4(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnLink5(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnLink6(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnFreeOption1();
	afx_msg void OnBnFreeOption2();
	afx_msg void OnBnFreeOption3();

	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ADVERTDIALOG_H__1DDA657E_929C_4496_AF11_A7F908CAD40F__INCLUDED_)
