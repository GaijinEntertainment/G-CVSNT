// MainFrame.cpp : implementation file
//

#include "stdafx.h"
#include "MainFrame.h"
#include "MainFrameWnd.h"

// CMainFrame dialog

IMPLEMENT_DYNAMIC(CMainFrame, CDialog)
CMainFrame::CMainFrame(CWnd* pParent /*=NULL*/)
	: CDialog(CMainFrame::IDD, pParent)
{
}

CMainFrame::~CMainFrame()
{
}

void CMainFrame::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CMainFrame, CDialog)
	ON_WM_CREATE()
	ON_WM_SIZE()
END_MESSAGE_MAP()


// CMainFrame message handlers

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;

	CString strMyClass = AfxRegisterWndClass(0,
                         ::LoadCursor(NULL, IDC_ARROW),
                         (HBRUSH) ::GetStockObject(WHITE_BRUSH), NULL);
    m_pFrame = new CMainFrameWnd;

	CRect rect;
	GetClientRect(rect);
    m_pFrame->Create(strMyClass,NULL, WS_CHILD|WS_VISIBLE, rect, this);

	return 0;
}

void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	m_pFrame->MoveWindow(0,0,cx,cy);
}
