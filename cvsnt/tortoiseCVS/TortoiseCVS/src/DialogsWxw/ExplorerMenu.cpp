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
#include "ExplorerMenu.h"
#include "../Utils/ShellUtils.h"
#include "../Utils/PathUtils.h"
#include "../Utils/TortoiseDebug.h"
#include "../Utils/SyncUtils.h"
#include "../Utils/StringUtils.h"
#include <map>


std::map<HWND, ExplorerMenu*> explorerMenu_Map;

ExplorerMenu::ExplorerMenu(const wxString& title)
   : wxMenu(title)
{
   m_hwnd = 0;
   m_oldWndProc = 0;
   m_pContextMenu2 = 0;
}


ExplorerMenu::~ExplorerMenu()
{
   // remove instance from map
   explorerMenu_Map.erase(m_hwnd);

   // Install old window proc
   if (m_oldWndProc)
      SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR) m_oldWndProc);

   // Release interface pointer
   if (m_pContextMenu2)
      m_pContextMenu2->Release();
}



// Fill menu
bool ExplorerMenu::FillMenu(HWND hwnd, const std::string& dir, 
                            const std::vector<std::string>& files)
{
   m_hwnd = hwnd;
   // Add instance to map
   explorerMenu_Map[m_hwnd] = this;

   // Add new window proc
   m_oldWndProc = (LPVOID) SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR) ExplorerMenu::WindowProc);

   // Create explorer menu
   return CreateExplorerMenu(dir, files);
}



// Fill menu
bool ExplorerMenu::FillMenu(HWND hwnd, const std::string& file)
{
   m_hwnd = hwnd;
   // Add instance to map
   explorerMenu_Map[m_hwnd] = this;

   // Add new window proc
   m_oldWndProc = (LPVOID) SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR) ExplorerMenu::WindowProc);

   // Create explorer menu
   std::vector<std::string> files;

   return CreateExplorerMenu(file, files);
}



bool ExplorerMenu::CreateExplorerMenu(const std::string& dir,
                                      const std::vector<std::string>& files)
{
   LPMALLOC pMalloc = 0;
   LPSHELLFOLDER psfDesktopFolder = 0;
   LPSHELLFOLDER psfMyFolder = 0;
   LPITEMIDLIST pidlMain = 0;
   LPITEMIDLIST pidlParent = 0;
   LPCITEMIDLIST *pidlFiles = 0;
   LPITEMIDLIST pidlFile = 0;
   LPCONTEXTMENU pContextMenu = 0;
   bool bResult = false;

   // Get pointers to the shell's IMalloc interface and the desktop's
   // IShellFolder interface.
   if (!SUCCEEDED(SHGetMalloc(&pMalloc)))
      return false;
   if (!SUCCEEDED(SHGetDesktopFolder(&psfDesktopFolder)))
   {
      pMalloc->Release();
      return false;
   }

   // Find folder
   std::string myDir = RemoveTrailingDelimiter(dir);
   // Convert to unicode
   std::wstring wsPath = MultibyteToWide(dir);
   // Convert the path name into a pointer to an item ID list (pidl).
   HRESULT hr = psfDesktopFolder->ParseDisplayName(m_hwnd, 0, (LPOLESTR) wsPath.c_str(), 0,
                                                   &pidlMain, 0);
   if (FAILED(hr) || (pidlMain == 0))    
      goto Cleanup;

   if (!files.empty())
   {
      // get dir 
      hr = psfDesktopFolder->BindToObject(pidlMain, 0,
                                          IID_IShellFolder, (LPVOID*) &psfMyFolder);
      if (FAILED(hr) || (psfMyFolder == 0))
         goto Cleanup;

      // Allocate memory for file pidls
      pidlFiles = (LPCITEMIDLIST*) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(LPITEMIDLIST) * files.size());

      // iterate through the filenames
      std::vector<std::string>::const_iterator itFiles = files.begin();
      unsigned int i = 0;
      while (itFiles != files.end())
      {
         // Convert to unicode
         wsPath = MultibyteToWide(*itFiles);
         // get PIDL
         hr = psfMyFolder->ParseDisplayName(m_hwnd, 0, (LPOLESTR) wsPath.c_str(), 0,
                                            &pidlFile, 0);
         if (FAILED(hr) || (pidlFile == 0))
            goto Cleanup;
         pidlFiles[i] = pidlFile;
         pidlFile = 0;

         itFiles++;
         i++;
      }
      
      // Get a pointer to the item's IContextMenu interface and call
      // IContextMenu::QueryContextMenu to initialize a context menu.
      hr = psfMyFolder->GetUIObjectOf(m_hwnd, static_cast<UINT>(files.size()), pidlFiles, 
                                      IID_IContextMenu, 0, (LPVOID *) &pContextMenu);
   }
   else
   {
      // show context menu for the directory only
      pidlParent = StripLastID(pidlMain);
      pidlFile = GetLastID(pidlMain);

      hr = psfDesktopFolder->BindToObject(pidlParent, 0,
                                          IID_IShellFolder, (LPVOID *)&psfMyFolder);
      if (FAILED(hr) || (psfMyFolder == 0))
         goto Cleanup;
      // Get a pointer to the item's IContextMenu interface and call
      // IContextMenu::QueryContextMenu to initialize a context menu.
      hr = psfMyFolder->GetUIObjectOf(m_hwnd, 1, (LPCITEMIDLIST *) &pidlFile, 
                                      IID_IContextMenu, 0, (LPVOID *) &pContextMenu);
   }

   if (FAILED(hr) || (pContextMenu == 0))
      goto Cleanup;

   hr = pContextMenu->QueryInterface(IID_IContextMenu2, 
                                     (LPVOID *) &m_pContextMenu2);
   if (FAILED(hr))
      goto Cleanup;

   hr = m_pContextMenu2->QueryContextMenu((HMENU) GetHMenu(), 0, 1, 0x7FFF, 
                                          CMF_EXPLORE);
   if (FAILED(hr))
      goto Cleanup;

   bResult = true;

Cleanup:
   for (unsigned int i = 0; i < files.size(); i++)
   {
      if (pidlFiles[i] != 0)
         pMalloc->Free((LPVOID) pidlFiles[i]);
   }

   if (pidlParent)
      pMalloc->Free((LPVOID) pidlParent);

   if (pidlFile)
      pMalloc->Free((LPVOID) pidlFile);

   if (pidlMain)
      pMalloc->Free((LPVOID) pidlMain);

   if (pMalloc)
      pMalloc->Release();

   if (psfMyFolder)
      psfMyFolder->Release();

   if (pContextMenu)
      pContextMenu->Release();

   if (psfDesktopFolder)
      psfDesktopFolder->Release();

   if (pidlFiles)
      HeapFree(GetProcessHeap(), 0, pidlFiles);

   return bResult;
}



long ExplorerMenu::WndProc(const MSG &msg)
{
    if (m_pContextMenu2 && (msg.message == WM_INITMENUPOPUP))
    {
        m_pContextMenu2->HandleMenuMsg(msg.message, msg.wParam, msg.lParam);
        return 0;
    }
        
    if (m_pContextMenu2 && (msg.message == WM_DRAWITEM || msg.message == WM_MEASUREITEM))
    {
        m_pContextMenu2->HandleMenuMsg(msg.message, msg.wParam, msg.lParam);
        return TRUE;
    }

    if (m_pContextMenu2 && (msg.message == WM_COMMAND))
    {
        CMINVOKECOMMANDINFO ici;
        ici.cbSize = sizeof(CMINVOKECOMMANDINFO);
        ici.hwnd = m_hwnd;
        ici.fMask = 0;
        ici.lpVerb = MAKEINTRESOURCEA(msg.wParam - 1);
        ici.lpParameters = 0;
        ici.lpDirectory = 0;
        ici.nShow = SW_SHOWNORMAL;
        ici.dwHotKey = 0;
        ici.hIcon = 0;
        SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR) m_oldWndProc);
        HRESULT hr = m_pContextMenu2->InvokeCommand(&ici);
        m_oldWndProc = (LPVOID) SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR) ExplorerMenu::WindowProc);
        if (SUCCEEDED(hr))
        {
            return 0;
        }
    }

    return CallWindowProc((WNDPROC) m_oldWndProc, m_hwnd, msg.message, msg.wParam, msg.lParam);
}


// Window proc
LRESULT CALLBACK ExplorerMenu::WindowProc(HWND hwnd, UINT uMsg,
                                          WPARAM wParam, LPARAM lParam)
{
    // Iterate through instances of ExplorerMenu
    ExplorerMenu* me = 0;
    if (explorerMenu_Map.find(hwnd) != explorerMenu_Map.end())
    {
        me = explorerMenu_Map[hwnd];
    }
    if (me)
    {
        MSG msg;
        msg.message = uMsg;
        msg.wParam = wParam;
        msg.lParam = lParam;
        return me->WndProc(msg);
    }

    DefWindowProc(hwnd, uMsg, wParam, lParam);
    return 0;
}
