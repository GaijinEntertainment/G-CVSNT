// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2002 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - October 2002

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "StdAfx.h"
#include "ResizeDialog.h"


ResizeDialog::ResizeDialog(wxWindow *parent, wxWindowID id, const wxString& title,
                           const wxPoint& pos, const wxSize& size, long style, const wxString& name)
   : ExtDialog(parent, id, title, pos, size, style, name)
{
   m_OldGripRect.left = 0;
   m_OldGripRect.right = 0;
   m_OldGripRect.top = 0;
   m_OldGripRect.bottom = 0;
}

WXLRESULT ResizeDialog::MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam)
{
   switch (message)
   {
      case WM_NCHITTEST:
      {
         if (!IsMaximized() && !IsIconized())
         {
            POINT pt;
            pt.x = LOWORD(lParam);
            pt.y = HIWORD(lParam);
            ::ScreenToClient(GetHwnd(), &pt);

            RECT rect;
            ::GetClientRect(GetHwnd(), &rect);
            int dx = GetSystemMetrics(SM_CXHSCROLL);
            int dy = GetSystemMetrics(SM_CYVSCROLL);
            rect.left = rect.right - dx;
            rect.top = rect.bottom - dy;

            if (PtInRect(&rect, pt) && (ChildWindowFromPointEx(GetHwnd(), pt,
               CWP_SKIPINVISIBLE | CWP_SKIPTRANSPARENT) == GetHwnd()))
            {
               if (pt.y >= rect.bottom - (dy / dx) * (pt.x - rect.left))
               {
                  return HTBOTTOMRIGHT;
               }
            }
         }
         return wxDialog::MSWWindowProc(message, wParam, lParam);
      }


      case WM_SIZE:
      {
         ::InvalidateRect(GetHwnd(), &m_OldGripRect, false);
         ::GetClientRect(GetHwnd(), &m_OldGripRect);
         m_OldGripRect.left = m_OldGripRect.right - GetSystemMetrics(SM_CXHSCROLL);
         m_OldGripRect.top = m_OldGripRect.bottom - GetSystemMetrics(SM_CYVSCROLL);
         ::InvalidateRect(GetHwnd(), &m_OldGripRect, false);
         return wxDialog::MSWWindowProc(message, wParam, lParam);
      }

      case WM_PAINT:
      {
         if (!IsMaximized() && !IsIconized())
         {
            PAINTSTRUCT ps;
            HDC hdc;
            BeginPaint(GetHwnd(), &ps);
            hdc = ps.hdc;
            HBRUSH hbr = GetSysColorBrush(COLOR_BTNFACE);
            ::GetClientRect(GetHwnd(), &m_OldGripRect);
            ::FillRect(hdc, &m_OldGripRect, hbr);
            m_OldGripRect.left = m_OldGripRect.right - GetSystemMetrics(SM_CXHSCROLL);
            m_OldGripRect.top = m_OldGripRect.bottom - GetSystemMetrics(SM_CYVSCROLL);
            DrawFrameControl(hdc, &m_OldGripRect, DFC_SCROLL, DFCS_SCROLLSIZEGRIP);
            EndPaint(GetHwnd(), &ps);
         }
         else
         {
            ::SetRect(&m_OldGripRect, 0, 0, 0, 0);
         }
         return wxDialog::MSWWindowProc(message, wParam, lParam);
      }
      default:
      {
         return wxDialog::MSWWindowProc(message, wParam, lParam);
      }
   }
}
