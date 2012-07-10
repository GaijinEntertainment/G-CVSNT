#pragma once

#include "resource.h"
#include "MainFrameWnd.h"

// CMainFrame dialog

class CMainFrame : public CDialog
{
	DECLARE_DYNAMIC(CMainFrame)

public:
	CMainFrame(CWnd* pParent = NULL);   // standard constructor
	virtual ~CMainFrame();

// Dialog Data
	enum { IDD = IDD_MAINFRAME };

protected:
	CMainFrameWnd *m_pFrame;

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
};
