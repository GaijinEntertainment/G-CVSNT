// MainFrameWnd.cpp : implementation file
//

#include "stdafx.h"
#include "MainFrameWnd.h"
#include "LeftPane.h"
#include "RightPane.h"

// CMainFrameWnd

IMPLEMENT_DYNAMIC(CMainFrameWnd, CFrameWnd)

CMainFrameWnd::CMainFrameWnd()
{
}

CMainFrameWnd::~CMainFrameWnd()
{
}


BEGIN_MESSAGE_MAP(CMainFrameWnd, CFrameWnd)
	ON_WM_CREATE()
END_MESSAGE_MAP()


// CMainFrameWnd message handlers

int CMainFrameWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	if (!m_statusBar.CreateEx(this,SBARS_SIZEGRIP))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}

	if(!m_splitterWnd.CreateStatic(this,1,2))
	{
		TRACE0("Failed to create splitter\n");
		return -1;
	}

	CCreateContext cl,cr;
	cl.m_pNewViewClass=RUNTIME_CLASS(CLeftPane);
	cr.m_pNewViewClass=RUNTIME_CLASS(CRightPane);
	m_splitterWnd.CreateView(0,0,RUNTIME_CLASS(CLeftPane),CSize(200,200),&cl);
	m_splitterWnd.CreateView(0,1,RUNTIME_CLASS(CRightPane),CSize(200,200),&cr);

	return 0;
}
