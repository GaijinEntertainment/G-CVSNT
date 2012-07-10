/**
	\file
	Info panel.

	\author
	CVSNT Helper application API
    Copyright (C) 2004-7 Tony Hoyle and March-Hare Software Ltd
*/
/*
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
#ifndef INFOPANEL__H
#define INFOPANEL__H

#include <commctrl.h>

class CInfoPanel
{
public:
	CVSTOOLS_EXPORT CInfoPanel();
	CVSTOOLS_EXPORT virtual ~CInfoPanel();
	CVSTOOLS_EXPORT bool Create(HWND hParent);
	CVSTOOLS_EXPORT bool Resize(int x, int y, int w, int h);
	CVSTOOLS_EXPORT bool ResizeWindowToParent();

protected:
	HWND m_hWnd,m_hListWnd;
	std::vector<RECT> m_activeAreas;
	bool m_bLoaded;
	HFONT m_hItemFont;
	struct news_t
	{
		cvs::string subject;
		cvs::string author;
		cvs::string url;
	};
	std::vector<news_t> m_news;

	static LRESULT CALLBACK _WndProc(HWND hWnd, UINT uCmd, WPARAM wParam, LPARAM lParam);
	LRESULT WndProc(HWND hWnd, UINT uCmd, WPARAM wParam, LPARAM lParam);

	LRESULT OnCreate(LPCREATESTRUCT lpCreateStruct);
	LRESULT OnDestroy();
	LRESULT OnCustomDraw(NMCUSTOMDRAW *pDraw);
	LRESULT OnClick(NMITEMACTIVATE* pItem);
	LRESULT OnSetCursor(WORD wHitTest, WORD wMouseMsg);

	static void _LoaderThread(void *param);
	void LoaderThread();
};

#endif
