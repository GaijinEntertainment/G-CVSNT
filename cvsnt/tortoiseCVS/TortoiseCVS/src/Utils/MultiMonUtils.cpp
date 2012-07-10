// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2003 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - October 2003

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
#include "MultiMonUtils.h"
#include "TortoiseDebug.h"
#include "MultiMon.h"
#include <algorithm>

//
//  GetMonitorRect
//
//  gets the "screen" or work area of the monitor that the passed
//  window is on.  this is used for apps that want to clip or
//  center windows.
//
//  the most common problem apps have with multimonitor systems is
//  when they use GetSystemMetrics(SM_C?SCREEN) to center or clip a
//  window to keep it on screen.  If you do this on a multimonitor
//  system the window we be restricted to the primary monitor.
//
//  this is a example of how you used the new Win32 multimonitor APIs
//  to do the same thing.
//
void GetMonitorRect(HWND hwnd, LPRECT prc, BOOL fWork)
{
   MONITORINFO mi;

   mi.cbSize = sizeof(mi);
   GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);

   if (fWork)
      *prc = mi.rcWork;
   else
      *prc = mi.rcMonitor;
}

//
//  GetMonitorRect
//
//  gets the "screen" or work area of the monitor that the passed
//  window is on.  this is used for apps that want to clip or
//  center windows.
//
//  the most common problem apps have with multimonitor systems is
//  when they use GetSystemMetrics(SM_C?SCREEN) to center or clip a
//  window to keep it on screen.  If you do this on a multimonitor
//  system the window we be restricted to the primary monitor.
//
//  this is a example of how you used the new Win32 multimonitor APIs
//  to do the same thing.
//
void GetMonitorRect(LPCRECT prcP, LPRECT prc, BOOL fWork)
{
   MONITORINFO mi;

   mi.cbSize = sizeof(mi);
   GetMonitorInfo(MonitorFromRect(prcP, MONITOR_DEFAULTTONEAREST), &mi);

   if (fWork)
      *prc = mi.rcWork;
   else
      *prc = mi.rcMonitor;
}

//
// ClipRectToMonitor
//
// uses GetMonitorRect to clip a rect to the monitor that
// the passed window is on.
//
void ClipRectToMonitor(HWND hwnd, RECT *prc, BOOL fWork)
{
   RECT rc;
   int  w = prc->right  - prc->left;
   int  h = prc->bottom - prc->top;

   if (hwnd != NULL)
   {
      GetMonitorRect(hwnd, &rc, fWork);
   }
   else
   {
      MONITORINFO mi;

      mi.cbSize = sizeof(mi);
      GetMonitorInfo(MonitorFromRect(prc, MONITOR_DEFAULTTONEAREST), &mi);

      if (fWork)
         rc = mi.rcWork;
      else
         rc = mi.rcMonitor;
   }

   prc->left   = std::max((int) rc.left, std::min((int) rc.right-w, (int) prc->left));
   prc->top    = std::max((int) rc.top,  std::min((int) rc.bottom-h, (int) prc->top));
   prc->right  = prc->left + w;
   prc->bottom = prc->top  + h;
}

//
// CenterRectToMonitor
//
// uses GetMonitorRect to center a rect to the monitor that
// the passed window is on.
//
void CenterRectToMonitor(HWND hwnd, RECT *prc, BOOL fWork)
{
   RECT rc;
   int  w = prc->right  - prc->left;
   int  h = prc->bottom - prc->top;

   GetMonitorRect(hwnd, &rc, fWork);

   prc->left	= rc.left + (rc.right  - rc.left - w) / 2;
   prc->top	= rc.top  + (rc.bottom - rc.top  - h) / 2;
   prc->right	= prc->left + w;
   prc->bottom = prc->top  + h;
}

//
// CenterRectToMonitor
//
// uses GetMonitorRect to center a rect to the monitor that
// the passed window is on.
//
void CenterRectToMonitor(LPCRECT prcP, RECT *prc, BOOL fWork)
{
   RECT rc;
   int  w = prc->right  - prc->left;
   int  h = prc->bottom - prc->top;

   GetMonitorRect(prcP, &rc, fWork);

   prc->left	= rc.left + (rc.right  - rc.left - w) / 2;
   prc->top	= rc.top  + (rc.bottom - rc.top  - h) / 2;
   prc->right	= prc->left + w;
   prc->bottom = prc->top  + h;
}

//
// CenterWindowToMonitor
//
void CenterWindowToMonitor(HWND hwndP, HWND hwnd, BOOL fWork)
{
   RECT rc;
   GetWindowRect(hwnd, &rc);
   CenterRectToMonitor(hwndP, &rc, fWork);
   SetWindowPos(hwnd, NULL, rc.left, rc.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

//
// CenterWindowToMonitor
//
void CenterWindowToMonitor(LPCRECT prcP, HWND hwnd, BOOL fWork)
{
   RECT rc;
   GetWindowRect(hwnd, &rc);
   CenterRectToMonitor(prcP, &rc, fWork);
   SetWindowPos(hwnd, NULL, rc.left, rc.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

//
// ClipWindowToMonitor
//
void ClipWindowToMonitor(HWND hwndP, HWND hwnd, BOOL fWork)
{
   TDEBUG_ENTER("ClipWindowToMonitor");
   RECT rc;
   GetWindowRect(hwnd, &rc);
   TDEBUG_TRACE("Old window rect: l: " << rc.left << ", r: " << rc.right 
      << ", t: " << rc.top << ", b: " << rc.bottom);
   ClipRectToMonitor(hwndP, &rc, fWork);
   TDEBUG_TRACE("New window rect: l: " << rc.left << ", r: " << rc.right 
      << ", t: " << rc.top << ", b: " << rc.bottom);
   ::MoveWindow(hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, false);
}

//
// IsWindowFullyVisible
//
BOOL IsWindowFullyVisible(HWND hwnd)
{
   TDEBUG_ENTER("IsWindowOnScreen");
   RECT rcMon, rcWin;

   GetMonitorRect(hwnd, &rcMon, TRUE);
   TDEBUG_TRACE("Monitor rect: l: " << rcMon.left << ", r: " << rcMon.right 
      << ", t: " << rcMon.top << ", b: " << rcMon.bottom);
   GetWindowRect(hwnd, &rcWin);
   TDEBUG_TRACE("Window rect: l: " << rcWin.left << ", r: " << rcWin.right 
      << ", t: " << rcWin.top << ", b: " << rcWin.bottom);

   if (rcWin.left >= rcMon.left && rcWin.right <= rcMon.right
      && rcWin.top >= rcMon.top && rcWin.bottom <= rcMon.bottom)
   {
      TDEBUG_TRACE("Yes");
      return TRUE;
   }
   else
   {
      TDEBUG_TRACE("No");
      return FALSE;
   }
}

//
// IsWindowFullyHidden
//
BOOL IsWindowFullyHidden(HWND hwnd)
{
   RECT rcMon, rcWin, rc;

   GetMonitorRect(hwnd, &rcMon, TRUE);
   GetWindowRect(hwnd, &rcWin);

   return !IntersectRect(&rc, &rcMon, &rcWin);
}

//
// MakeSureWindowIsVisible
//
void MakeSureWindowIsVisible(HWND hwnd)
{
   if (!IsWindowFullyVisible(hwnd))
   {
      ClipWindowToMonitor(hwnd, hwnd, TRUE);
   }
}
