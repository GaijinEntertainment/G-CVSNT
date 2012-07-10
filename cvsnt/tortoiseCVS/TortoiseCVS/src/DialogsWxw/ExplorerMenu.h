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

#ifndef EXPLORER_MENU_H
#define EXPLORER_MENU_H


#include <wx/wx.h>
#include <wx/menu.h>
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>



class ExplorerMenu : public wxMenu
{
public:
   // Constructor
   ExplorerMenu(const wxString& title = wxT(""));
   // Destructor
   ~ExplorerMenu();
   // Fill menu
   bool FillMenu(HWND hwnd, const std::string& dir, 
      const std::vector<std::string>& files);
   // Fill menu
   bool FillMenu(HWND hwnd, const std::string& file);

private:
   bool CreateExplorerMenu(const std::string& dir, 
      const std::vector<std::string>& files);
   long WndProc(const MSG &msg);

   static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg,
      WPARAM wParam,LPARAM lParam);

   LPVOID m_oldWndProc;
   HWND m_hwnd;
   LPCONTEXTMENU2 m_pContextMenu2;
};








#endif
