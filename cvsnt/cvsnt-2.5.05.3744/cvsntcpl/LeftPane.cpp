// LeftPane.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "LeftPane.h"

// CLeftPane

IMPLEMENT_DYNCREATE(CLeftPane, CTreeView)

CLeftPane::CLeftPane()
{
}

CLeftPane::~CLeftPane()
{
}

BEGIN_MESSAGE_MAP(CLeftPane, CTreeView)
	ON_WM_CREATE()
END_MESSAGE_MAP()


// CLeftPane diagnostics

#ifdef _DEBUG
void CLeftPane::AssertValid() const
{
	CTreeView::AssertValid();
}

void CLeftPane::Dump(CDumpContext& dc) const
{
	CTreeView::Dump(dc);
}
#endif //_DEBUG


// CLeftPane message handlers

int CLeftPane::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CTreeView::OnCreate(lpCreateStruct) == -1)
		return -1;

	CTreeCtrl& tree = GetTreeCtrl();
	tree.ModifyStyle(0, TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_INFOTIP);

	HICON hFolderIcon[2];
	CImageList ilist;
	ExtractIconEx(_T("SHELL32.DLL"),3,NULL,hFolderIcon,2);
	ilist.Create(GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),ILC_COLOR16|ILC_MASK,0,32);
	ilist.Add(hFolderIcon[0]);
	ilist.Add(hFolderIcon[1]);
	tree.SetImageList(&ilist,TVSIL_NORMAL);
	tree.SetImageList(&ilist,TVSIL_STATE);
	ilist.Detach();
	
	HTREEITEM root = tree.InsertItem(TVIF_TEXT|TVIF_PARAM|TVIF_STATE|TVIF_IMAGE|TVIF_SELECTEDIMAGE,_T("CVSNT Control Panel"),0,1,TVIS_EXPANDED,TVIS_EXPANDED,(LPARAM)1,TVI_ROOT,TVI_LAST);
	tree.InsertItem(TVIF_TEXT|TVIF_PARAM|TVIF_IMAGE|TVIF_SELECTEDIMAGE,_T("Server State"),0,1,0,0,(LPARAM)2,root,TVI_LAST);
	tree.InsertItem(TVIF_TEXT|TVIF_PARAM|TVIF_IMAGE|TVIF_SELECTEDIMAGE,_T("Repositories"),0,1,0,0,(LPARAM)2,root,TVI_LAST);
	tree.InsertItem(TVIF_TEXT|TVIF_PARAM|TVIF_IMAGE|TVIF_SELECTEDIMAGE,_T("Server Settings"),0,1,0,0,(LPARAM)2,root,TVI_LAST);
	tree.InsertItem(TVIF_TEXT|TVIF_PARAM|TVIF_IMAGE|TVIF_SELECTEDIMAGE,_T("SSL Settings"),0,1,0,0,(LPARAM)2,root,TVI_LAST);
	HTREEITEM compat = tree.InsertItem(TVIF_TEXT|TVIF_PARAM|TVIF_STATE|TVIF_IMAGE|TVIF_SELECTEDIMAGE,_T("Compatibility"),0,1,TVIS_EXPANDED,TVIS_EXPANDED,(LPARAM)2,root,TVI_LAST);
	tree.InsertItem(TVIF_TEXT|TVIF_PARAM|TVIF_IMAGE|TVIF_SELECTEDIMAGE,_T("Non-CVSNT clients"),0,1,0,0,(LPARAM)2,compat,TVI_LAST);
	tree.InsertItem(TVIF_TEXT|TVIF_PARAM|TVIF_IMAGE|TVIF_SELECTEDIMAGE,_T("CVSNT clients"),0,1,0,0,(LPARAM)2,compat,TVI_LAST);
	tree.InsertItem(TVIF_TEXT|TVIF_PARAM|TVIF_IMAGE|TVIF_SELECTEDIMAGE,_T("Advanced"),0,1,0,0,(LPARAM)2,root,TVI_LAST);
	return 0;
}
