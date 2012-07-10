// RightPane.cpp : implementation file
//

#include "stdafx.h"
#include "RightPane.h"


// CRightPane

IMPLEMENT_DYNCREATE(CRightPane, CView)

CRightPane::CRightPane()
{
}

CRightPane::~CRightPane()
{
}

BEGIN_MESSAGE_MAP(CRightPane, CView)
END_MESSAGE_MAP()


// CRightPane drawing

void CRightPane::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	// TODO: add draw code here
}


// CRightPane diagnostics

#ifdef _DEBUG
void CRightPane::AssertValid() const
{
	CView::AssertValid();
}

void CRightPane::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}
#endif //_DEBUG


// CRightPane message handlers
