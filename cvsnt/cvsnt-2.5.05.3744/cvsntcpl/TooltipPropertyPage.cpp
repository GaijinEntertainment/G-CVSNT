/*	cvsnt control panel
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
#include "stdafx.h"
#include "TooltipPropertyPage.h"

IMPLEMENT_DYNAMIC(CTooltipPropertyPage, CPropertyPage)

BEGIN_MESSAGE_MAP(CTooltipPropertyPage, CPropertyPage)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

CTooltipPropertyPage::CTooltipPropertyPage()
{
}

CTooltipPropertyPage::CTooltipPropertyPage(int nID) : CPropertyPage(nID)
{
}

CTooltipPropertyPage::~CTooltipPropertyPage()
{
}


BOOL CTooltipPropertyPage::OnInitDialog()
{
	BOOL bResult = CPropertyPage::OnInitDialog();
	m_wndToolTip.Create(this);
	m_wndToolTip.SetMaxTipWidth(800);
	m_wndToolTip.Activate(TRUE);
	CWnd *pWndChild = GetWindow(GW_CHILD);
	CString strToolTip;
	while (pWndChild)
	{
		int nID = pWndChild->GetDlgCtrlID();
		if (strToolTip.LoadString(nID))
		{
			m_wndToolTip.AddTool(pWndChild, strToolTip);
		}
		pWndChild = pWndChild->GetWindow(GW_HWNDNEXT);
	}
	return bResult;
}

BOOL CTooltipPropertyPage::PreTranslateMessage(MSG *pMsg)
{
	if (pMsg->message >= WM_MOUSEFIRST &&
		pMsg->message <= WM_MOUSELAST)
	{
		MSG msg;
		::CopyMemory(&msg, pMsg, sizeof(MSG));
		HWND hWndParent = ::GetParent(msg.hwnd);
		while (hWndParent && hWndParent != m_hWnd)
		{
			msg.hwnd = hWndParent;
			hWndParent = ::GetParent(hWndParent);
		}
		if (msg.hwnd)
		{
			m_wndToolTip.RelayEvent(&msg);
		}
	}
	return CPropertyPage::PreTranslateMessage(pMsg);
}

void CTooltipPropertyPage::OnDestroy()
{
	m_wndToolTip.DestroyWindow();
}
