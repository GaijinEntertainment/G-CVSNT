#pragma once


// CMainFrameWnd frame

class CMainFrameWnd : public CFrameWnd
{
	DECLARE_DYNAMIC(CMainFrameWnd)
public:
	CMainFrameWnd();       
	virtual ~CMainFrameWnd();

protected:
	CStatusBar m_statusBar;
	CSplitterWnd m_splitterWnd;

	DECLARE_MESSAGE_MAP()
public:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
};


