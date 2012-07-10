/*
	CVSNT Helper application API
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/* Win32 specific */
#include <cvsapi.h>
#include <windowsx.h>
#include <process.h>
#include <commctrl.h>
#include <shellapi.h>

#include "InfoPanel.h"

CInfoPanel::CInfoPanel()
{
}

CInfoPanel::~CInfoPanel()
{
}

bool CInfoPanel::Create(HWND hParent)
{
	WNDCLASS wc={0};
	if(!GetClassInfo(GetModuleHandle(NULL),_T("InfoPanel"),&wc))
	{
		wc.lpszClassName=_T("InfoPanel");
		wc.cbWndExtra=sizeof(void*);
		wc.lpfnWndProc=_WndProc;
		wc.hInstance=GetModuleHandle(NULL);
		if(!RegisterClass(&wc))
			return false;
	}

	m_hWnd = CreateWindow(_T("InfoPanel"),_T(""),WS_CHILD,0,0,10,10,hParent,NULL,GetModuleHandle(NULL),this);
	if(!m_hWnd)
		return false;

	m_hListWnd = CreateWindow(WC_LISTVIEW,_T(""),WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_NOCOLUMNHEADER|LVS_SINGLESEL,0,0,10,10,m_hWnd,NULL,GetModuleHandle(NULL),NULL);
	if(!m_hListWnd)
		return false;

	ResizeWindowToParent();
	ShowWindow(m_hWnd,SW_SHOW);
	return true;
}

bool CInfoPanel::Resize(int x, int y, int w, int h)
{
	MoveWindow(m_hWnd,x,y,w,h,TRUE);
	ListView_SetColumnWidth(m_hListWnd,0,(w/4)*3);
	ListView_SetColumnWidth(m_hListWnd,1,(w/4)-8);
	MoveWindow(m_hListWnd,0,0,w,h,TRUE);
	return true;
}

bool CInfoPanel::ResizeWindowToParent()
{
	HWND hParent = GetParent(m_hWnd);
	RECT rect;
	GetClientRect(hParent,&rect);
	return Resize(rect.left,rect.top,rect.right-rect.left+1,rect.bottom-rect.top+1);
}

#pragma warning(push)
#pragma warning(disable:4312)
#pragma warning(disable:4244)
LRESULT CInfoPanel::_WndProc(HWND hWnd, UINT uCmd, WPARAM wParam, LPARAM lParam)
{
	CInfoPanel* pthis = (CInfoPanel*)GetWindowLongPtr(hWnd,0);
	switch(uCmd)
	{
		// WM_NCCREATE is actually the second message sent to a window.  The first is WM_GETMINMAXINFO
		// however that isn't useful for passing the this pointer.
		case WM_NCCREATE:
			pthis = (CInfoPanel*)((LPCREATESTRUCT)lParam)->lpCreateParams;
			SetWindowLongPtr(hWnd,0,(LONG_PTR)pthis);
			break;
	}
	if(pthis)
		return pthis->WndProc(hWnd, uCmd, wParam, lParam);
	else
		return DefWindowProc(hWnd,uCmd,wParam,lParam);
}
#pragma warning(pop)

LRESULT CInfoPanel::WndProc(HWND hWnd, UINT uCmd, WPARAM wParam, LPARAM lParam)
{
	switch(uCmd)
	{
		case WM_CREATE: return OnCreate((LPCREATESTRUCT)lParam);
		case WM_SETCURSOR: return OnSetCursor(LOWORD(lParam),HIWORD(lParam));
		case WM_DESTROY: return OnDestroy();

		case WM_NOTIFY:
			{
				NMHDR *pNMHDR = (NMHDR*)lParam;
				switch(pNMHDR->code)
				{
				case NM_CUSTOMDRAW: return OnCustomDraw((NMCUSTOMDRAW*)pNMHDR);
				case NM_CLICK: return OnClick((NMITEMACTIVATE*)pNMHDR);
				}
			}
	}
	return DefWindowProc(hWnd,uCmd,wParam,lParam);
}

LRESULT CInfoPanel::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	m_bLoaded = false;
	m_hItemFont=NULL;
	_beginthread(_LoaderThread,0,this);
	return 0;
}

LRESULT CInfoPanel::OnDestroy()
{
	if(m_hItemFont) DeleteFont(m_hItemFont);
	return 0;
}

LRESULT CInfoPanel::OnCustomDraw(NMCUSTOMDRAW *pDraw)
{
	NMLVCUSTOMDRAW *pLVDraw = (NMLVCUSTOMDRAW*)pDraw;

	if(pDraw->hdr.hwndFrom!=m_hListWnd)
		return 0;

	news_t *iItem = (news_t*)pDraw->lItemlParam;

	switch(pDraw->dwDrawStage)
	{
	case CDDS_PREPAINT:
		return CDRF_NOTIFYITEMDRAW;
	case CDDS_ITEMPREPAINT:
		return CDRF_NOTIFYSUBITEMDRAW;
	case CDDS_ITEMPREPAINT|CDDS_SUBITEM:
		switch(pLVDraw->iSubItem)
		{
		case 0:
			{
			// Docs are wrong, rc member is complete BS.  This fixes it
			ListView_GetSubItemRect(pDraw->hdr.hwndFrom, pDraw->dwItemSpec, pLVDraw->iSubItem, LVIR_BOUNDS, &pDraw->rc); 

			HFONT hOldFont = SelectFont(pDraw->hdc,m_hItemFont);
			COLORREF oldColour = SetTextColor(pDraw->hdc,/*GetSysColor(COLOR_HOTLIGHT)*/RGB(0,0,255));
			pDraw->rc.left+=4; // Small margin, looks better
			DrawText(pDraw->hdc,cvs::wide(iItem->subject.c_str()),-1,&pDraw->rc,DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
			SetTextColor(pDraw->hdc, oldColour);
			SelectFont(pDraw->hdc,hOldFont);
			return CDRF_SKIPDEFAULT;
			}
		case 1:
			return 0;
		}
	}

	return 0;
}

LRESULT CInfoPanel::OnClick(NMITEMACTIVATE* pItem)
{
	if(pItem->hdr.hwndFrom!=m_hListWnd || pItem->iItem<0)
		return 0;

	LVITEM lvi = {0};
	lvi.mask=LVIF_PARAM;
	lvi.iItem=pItem->iItem;
	ListView_GetItem(m_hListWnd,&lvi);
	news_t *pNews = (news_t*)lvi.lParam;

	ShellExecute(NULL,_T("open"),cvs::wide(pNews->url.c_str()),NULL,NULL,SW_SHOWNORMAL);
	return 0;
}


LRESULT CInfoPanel::OnSetCursor(WORD wHitTest, WORD wMouseMsg)
{
	if(wHitTest!=HTCLIENT)
		return 0;

	LVHITTESTINFO ht = {0};
	DWORD dwPos = GetMessagePos();
	ht.pt.x=GET_X_LPARAM(dwPos);
	ht.pt.y=GET_Y_LPARAM(dwPos);
	ScreenToClient(m_hListWnd, &ht.pt);
	ListView_SubItemHitTest(m_hListWnd, &ht);
	if(ht.iItem>=0 && ht.iSubItem==0)
	{
		SetCursor(LoadCursor(NULL,IDC_HAND));
		return TRUE;
	}
	return FALSE;
}

void CInfoPanel::_LoaderThread(void *param)
{
	((CInfoPanel*)param)->LoaderThread();
}

void CInfoPanel::LoaderThread()
{
	m_news.clear();
	CHttpSocket sock;
	cvs::string xml;
	CXmlTree tree;
	size_t len;

	if(!sock.create("http://march-hare.com"))
	{
		xml="<?xml version=\"1.0\" encoding=\"windows-1252\"?>\n<messages>\n<message><subject>Need assistance?  Click here for our professional support options!</subject><author>&quot;March Hare Support&quot; &lt;sales@march-hare.com&gt;</author><url>http://store.march-hare.com/s.nl?sc=2&amp;category=2</url></message>\n</messages>";
		len=xml.length();
	}
	else if(!sock.request("GET","/cvspro/prods-pre.asp?register=advert"))
	{
		xml="<?xml version=\"1.0\" encoding=\"windows-1252\"?>\n<messages>\n<message><subject>Need help NOW?  Click here for our professional support options!</subject><author>&quot;March Hare Support&quot; &lt;sales@march-hare.com&gt;</author><url>http://store.march-hare.com/s.nl?sc=2&amp;category=2</url></message>\n</messages>";
		len=xml.length();
	}
	if(sock.responseCode()!=200)
	{
		xml="<?xml version=\"1.0\" encoding=\"windows-1252\"?>\n<messages>\n<message><subject>Need help? Need integration?  Need training?  Click here for our professional support options!</subject><author>&quot;March Hare Support&quot; &lt;sales@march-hare.com&gt;</author><url>http://store.march-hare.com/s.nl?sc=2&amp;category=2</url></message>\n</messages>";
		len=xml.length();
	}
	else
	{
		cvs::string xml = sock.responseData(len);
	}
	if(!tree.ParseXmlFromMemory(xml.c_str()))
		return;
	CXmlNodePtr node = tree.GetRoot();
	if(strcmp(node->GetName(),"messages"))
		return;
	if(!node->GetChild("message"))
		return;

	do
	{
		news_t n;
		n.subject = node->GetNodeValue("subject");
		n.author = node->GetNodeValue("author");
		n.url = node->GetNodeValue("url");
		m_news.push_back(n);
	} while(node->GetSibling("message"));

	if(!m_hItemFont)
	{
		HFONT hFont = (HFONT)SendMessage(m_hListWnd,WM_GETFONT,0,0);
		if(!hFont) hFont=GetStockFont(DEFAULT_GUI_FONT);

		LOGFONT lf = {0};
		GetObject(hFont,sizeof(lf),&lf);
		lf.lfUnderline=true;
		m_hItemFont = CreateFontIndirect(&lf);
	}

	ListView_DeleteAllItems(m_hListWnd);
	ListView_DeleteColumn(m_hListWnd,1);
	LVCOLUMN lvc={0};
	lvc.mask=LVCF_WIDTH|LVCF_TEXT;
	lvc.cx=500;
	lvc.pszText=_T("Title");
	ListView_InsertColumn(m_hListWnd,0,&lvc);
	lvc.mask=LVCF_WIDTH|LVCF_TEXT;
	lvc.cx=300;
	lvc.pszText=_T("Author");
	ListView_InsertColumn(m_hListWnd,1,&lvc);

	for(size_t n=0; n<m_news.size(); n++)
	{
		LVITEM lvi = {0};
		cvs::wide wnews(m_news[n].subject.c_str());
		cvs::wide wauth(m_news[n].author.c_str());
		lvi.mask=LVIF_TEXT|LVIF_PARAM;
		lvi.iItem=(int)n;
		lvi.pszText=(LPWSTR)(const wchar_t*)wnews;
		lvi.lParam=(LPARAM)&m_news[n];
		int iItem = ListView_InsertItem(m_hListWnd,&lvi);
		ListView_SetItemText(m_hListWnd,iItem,1,(LPWSTR)(const wchar_t*)wauth);
	}
	m_bLoaded = true;
}
