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


#ifndef MULTI_MON_UTILS_H
#define MULTI_MON_UTILS_H

#include <windows.h>
#include "FixWinDefs.h"


void GetMonitorRect(HWND hwnd, LPRECT prc, BOOL fWork);
void GetMonitorRect(LPCRECT prcP, LPRECT prc, BOOL fWork);
void ClipRectToMonitor(HWND hwnd, RECT *prc, BOOL fWork);
void CenterRectToMonitor(HWND hwnd, RECT *prc, BOOL fWork);
void CenterRectToMonitor(LPCRECT prcP, RECT *prc, BOOL fWork);
void CenterWindowToMonitor(HWND hwndP, HWND hwnd, BOOL fWork);
void CenterWindowToMonitor(LPCRECT prcP, HWND hwnd, BOOL fWork);
void ClipWindowToMonitor(HWND hwndP, HWND hwnd, BOOL fWork);
BOOL IsWindowFullyVisible(HWND hwnd);
BOOL IsWindowFullyHidden(HWND hwnd);
void MakeSureWindowIsVisible(HWND hwnd);


#endif
