// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - May 2000

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
#include "ShellExt.h"
#include <string>
#include <shlwapi.h>
#include <Utils/PathUtils.h>
#include <Utils/StringUtils.h>
#include <Utils/TortoiseRegistry.h>
#include <Utils/TortoiseDebug.h>
#include <Utils/Keyboard.h>
#include <Utils/TortoiseException.h>
#include <Utils/ShellUtils.h>
#include <Utils/Translate.h>
#include <Utils/LangUtils.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/Preference.h>
#include <ContextMenus/HasMenu.h>
#include <ContextMenus/MenuDescription.h>
#include <ContextMenus/DefaultMenus.h>
#include <TortoiseAct/TortoiseActVerbs.h>

#include "TortoiseShellRes.h"

std::map<std::string, HBITMAP> CShellExt::ourBitmaps;
std::set<HICON>                CShellExt::ourIcons;

void CShellExt::ContextMenuInitialise()
{
   TDEBUG_ENTER("CShellExt::ContextMenuInitialise");
   CSHelper cs(ourShellExtCS, true);
#if !defined(_DEBUG)
   if (!ourContextMenuInitialised)
#endif
   {
      ourMenus.clear();
      FillInDefaultMenus(ourMenus);

      // Index by verb
      for (unsigned int i = 0; i < ourMenus.size(); ++i)
      {
         std::string verb = ourMenus[i].GetVerb();
         myVerbMap[verb] = i;
      }

      ourContextMenuInitialised = true;
   }
}

void CShellExt::ContextMenuFinalise()
{

}

void CShellExt::ClearMenuBitmaps()
{
   for (std::map<std::string, HBITMAP>::iterator it = myMenuBitmapMap.begin();
        it != myMenuBitmapMap.end(); ++it)
      DeleteObject((*it).second);
   myMenuBitmapMap.clear();
}

STDMETHODIMP CShellExt::QueryContextMenu(HMENU hMenu,
                                         UINT indexMenu,
                                         UINT idCmdFirst,
                                         UINT /* idCmdLast */,
                                         UINT uFlags)
{
   TDEBUG_ENTER("CShellExt::QueryContextMenu");

   if (!AreShellExtTargetsAvail())
      return NOERROR;
   CheckLanguage();

   ContextMenuInitialise();

   uFlags &= ~CMF_RESERVED;
   if (!(uFlags & CMF_NOVERBS) 
      && !(uFlags & CMF_VERBSONLY) 
#if defined(CMF_INCLUDESTATIC)
      && !(uFlags & CMF_INCLUDESTATIC)
#endif
      && !(uFlags & CMF_DEFAULTONLY))
   {
      ClearMenuBitmaps();

      wxString submenuName = TortoiseRegistry::ReadWxString("SubMenuName", wxT("C&VS"));
      
      UINT idCmd = idCmdFirst;
      int index = 0;
      MENUITEMINFO mi;
      mi.cbSize = sizeof(mi);

      // Separator before
      mi.fMask = MIIM_TYPE;
      mi.fType = MFT_SEPARATOR;
      InsertMenuItem(hMenu, indexMenu++, TRUE, &mi);

      TDEBUG_TRACE("Delete any existing \"CVS\" submenu");
      // Delete any existing "C&VS" submenu.  This is a workaround for an 
      // Explorer bug - it removes all normal menu items, but not submenu
      // items that we added previously.  This is on the main File menu -
      // for popup context menus, the menu must be generated afresh, so
      // everything is fine.
      wxChar buf[_MAX_PATH];
      int count = GetMenuItemCount(hMenu);
      for (int j = 0; j < count; ++j)
      {
         GetMenuString(hMenu, j, buf, sizeof(buf) - 1, MF_BYPOSITION);
         if (wxString(buf) == submenuName)
            DeleteMenu(hMenu, j, MF_BYPOSITION);
      }
      
      // This is deleted because DestroyMenu is recursive - hopefully
      // Explorer just calls that!
      HMENU subMenu = CreateMenu();
      int indexSubMenu = 0;

      bool hasSubMenu = false;
      // TODO: Better separator mechanism
      bool previousSubMenuWasSeparator = true; // stop two adjacent separators
      std::string previousverb;

      TDEBUG_TRACE("Iterate through ourMenus");
      unsigned int i;
      std::vector<bool> hasMenus(ourMenus.size());
      std::vector<int> menuFlags(ourMenus.size());
      for (i = 0; i < ourMenus.size(); i++)
      {
         menuFlags[i] = ourMenus[i].GetFlags();
      }
      HasMenus(menuFlags, myFiles, hasMenus);

      bool showContextIcons = GetBooleanPreference("ContextIcons");

      for (i = 0; i < ourMenus.size(); ++i)
      {
         MenuDescription& menuDescription = ourMenus[i];
         if (hasMenus[i])
         {
            if (!menuDescription.GetVerb().empty() && (menuDescription.GetVerb() == previousverb))
               continue;
            previousverb = menuDescription.GetVerb();
            // Add item to either submenu or main menu
            if (menuDescription.GetFlags() & ON_SUB_MENU)
            {
               // Submenu
               if (menuDescription.GetVerb().empty())
               {
                  // This is a separator
                  if (!previousSubMenuWasSeparator)
                  {
                     mi.fMask = MIIM_TYPE;
                     mi.fType = MFT_SEPARATOR;
                     InsertMenuItem(subMenu, indexSubMenu++, TRUE, &mi);

                     ++idCmd;
                     previousSubMenuWasSeparator = true;
                  }
               }
               else
               {
                  wxString menuText = GetString(wxTextCStr(menuDescription.GetMenuText()));
                  mi.fMask = MIIM_TYPE | MIIM_ID;
                  mi.fType = MFT_STRING;
                  mi.dwTypeData = const_cast<wxChar*>(menuText.c_str());
                  mi.cch = static_cast<UINT>(menuText.length());
                  mi.wID = idCmd;
                  if (showContextIcons)
                  {
                      std::string iconname = std::string("IDI_") + menuDescription.GetIcon();
                      MakeUpperCase(iconname);
                      if (WindowsVersionIs2K())
                      {
                          mi.fMask = MIIM_STRING | MIIM_ID | MIIM_CHECKMARKS;
                          mi.hbmpChecked = mi.hbmpUnchecked = IconToBitmap(iconname);
                      }
                      else if (WindowsVersionIsVistaOrHigher())
                      {
                          mi.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_BITMAP;
                          mi.hbmpItem = IconToBitmapPARGB32(iconname);
                      }
                      else if (HICON h = (HICON) LoadImageA(g_hInstance, iconname.c_str(), IMAGE_ICON,
                                                            16, 16, LR_DEFAULTCOLOR | LR_SHARED))
                      {
                          mi.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_BITMAP | MIIM_DATA;
                          mi.dwItemData = (ULONG_PTR) h;
                          mi.hbmpItem = HBMMENU_CALLBACK;
                          ourIcons.insert(h);
                      }
                  }
                  
                  InsertMenuItem(subMenu, indexSubMenu++, TRUE, &mi);

                  ++idCmd;
                  hasSubMenu = true;
                  previousSubMenuWasSeparator = false;
                  // We store the relative and absolute diameter
                  // (drawitem callback uses absolute, others relative)
                  myIDMap[idCmd - idCmdFirst - 1] = i;
                  myIDMap[idCmd - 1] = i;
               }
            }
            else
            {
               // Main menu
               if (menuDescription.GetVerb().empty())
               {
                  // This is a separator
                  mi.fMask = MIIM_TYPE;
                  mi.fType = MFT_SEPARATOR;
                  InsertMenuItem(hMenu, indexMenu++, TRUE, &mi);

                  idCmd++;
               }
               else
               {
                  wxString menuText = wxT("CVS ");
                  menuText += GetString(wxTextCStr(menuDescription.GetMenuText()));

                  mi.fMask = MIIM_TYPE | MIIM_ID;
                  mi.fType = MFT_STRING;
                  mi.dwTypeData = (LPTSTR) menuText.c_str();
                  mi.cch = static_cast<UINT>(menuText.length());
                  mi.wID = idCmd;
                  if (showContextIcons)
                  {
                      std::string iconname = std::string("IDI_") + menuDescription.GetIcon();
                      MakeUpperCase(iconname);
                      if (WindowsVersionIs2K())
                      {
                          mi.fMask = MIIM_STRING | MIIM_ID | MIIM_CHECKMARKS;
                          mi.hbmpChecked = mi.hbmpUnchecked = IconToBitmap(iconname);
                      }
                      else if (WindowsVersionIsVistaOrHigher())
                      {
                          mi.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_BITMAP;
                          mi.hbmpItem = IconToBitmapPARGB32(iconname);
                      }
                      else if (HICON h = (HICON) LoadImageA(g_hInstance, iconname.c_str(), IMAGE_ICON,
                                                            16, 16, LR_DEFAULTCOLOR | LR_SHARED))
                      {
                          mi.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_BITMAP | MIIM_DATA;
                          mi.dwItemData = (ULONG_PTR) h;
                          mi.hbmpItem = HBMMENU_CALLBACK;
                          ourIcons.insert(h);
                      }
                  }

                  InsertMenuItem(hMenu, indexMenu++, TRUE, &mi);

                  menuDescription.SetIndex(index);
                  ++idCmd;
                  // We store the relative and absolute diameter
                  // (drawitem callback uses absolute, others relative)
                  myIDMap[idCmd - idCmdFirst - 1] = i;
                  myIDMap[idCmd - 1] = i;
               }
            }
         }
      }

      // 
      TDEBUG_TRACE("Add sub menu to main menu");
      wxString text = submenuName;
      if (hasSubMenu)
      {
         mi.fMask = MIIM_TYPE | MIIM_SUBMENU | MIIM_ID;
         mi.fType = MFT_STRING;
         mi.dwTypeData = const_cast<LPTSTR>(text.c_str());
         mi.cch = static_cast<UINT>(text.size());
         mi.hSubMenu = subMenu;
         if (showContextIcons)
         {
             std::string iconname = std::string("IDI_") + std::string("tortoise");
             MakeUpperCase(iconname);
             if (WindowsVersionIs2K())
             {
                 mi.fMask = MIIM_STRING | MIIM_ID | MIIM_CHECKMARKS | MIIM_SUBMENU;
                 mi.hbmpChecked = mi.hbmpUnchecked = IconToBitmap(iconname);
             }
             else if (WindowsVersionIsVistaOrHigher())
             {
                 mi.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_SUBMENU | MIIM_ID | MIIM_BITMAP;
                 mi.hbmpItem = IconToBitmapPARGB32(iconname);
             }
             else if (HICON h = (HICON) LoadImageA(g_hInstance, iconname.c_str(),
                                                   IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR | LR_SHARED))
             {
                 mi.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_SUBMENU | MIIM_ID | MIIM_BITMAP | MIIM_DATA;
                 mi.wID = idCmd++;
                 mi.dwItemData = (ULONG_PTR) h;
                 mi.hbmpItem = HBMMENU_CALLBACK;
                 ourIcons.insert(h);
             }
         }

         InsertMenuItem(hMenu, indexMenu++, TRUE, &mi);
      }
      else
          DestroyMenu(subMenu);

      // Separator after
      mi.fMask = MIIM_TYPE;
      mi.fType = MFT_SEPARATOR;
      InsertMenuItem(hMenu, indexMenu++, TRUE, &mi);
        
      // Return number of menu items that we added
      return ResultFromScode(MAKE_SCODE(SEVERITY_SUCCESS, 0, (USHORT)(idCmd - idCmdFirst)));
   }

   return NOERROR;
}


// This is called when you invoke a command on the menu:
STDMETHODIMP CShellExt::InvokeCommand(LPCMINVOKECOMMANDINFO lpcmi)
{
    TDEBUG_ENTER("CShellExt::InvokeCommand");
    CheckLanguage();
    ContextMenuInitialise();
    HRESULT hr = E_INVALIDARG;

    // If HIWORD(lpcmi->lpVerb) then we have been called programmatically
    // and lpVerb is a command that should be invoked.  Otherwise, the shell
    // has called us, and LOWORD(lpcmi->lpVerb) is the menu ID the user has
    // selected.  Actually, it's (menu ID - idCmdFirst) from QueryContextMenu().
    MenuDescription* menuDescription = NULL;
    if (!HIWORD(lpcmi->lpVerb))
    {
        if (!AreShellExtTargetsAvail())
            return hr;

        UINT idCmd = LOWORD(lpcmi->lpVerb);

        // See if we have a handler interface for this id
        if (myIDMap.find(idCmd) != myIDMap.end())
            menuDescription = &ourMenus[myIDMap[idCmd]];
    }
    else
    {
        // See if we have a handler interface for this verb
        if (myVerbMap.find(lpcmi->lpVerb) != myVerbMap.end())
            menuDescription = &ourMenus[myVerbMap[lpcmi->lpVerb]];
    }

    // Now we have our handler, we can perform the action
    if (menuDescription)
    {
        hr = NOERROR;
#if defined(_DEBUG)
        TDEBUG_TRACE("Verb: " << menuDescription->GetVerb());
        // Test crash dump
        if (!stricmp(menuDescription->GetVerb().c_str(), CvsAboutVerb.c_str())
            && IsControlDown() && !IsShiftDown())
        {
           MessageBoxA(0, "Testing crash dump in TortoiseShell", "Test", 0);
           int* p = 0;
           *p = 0;
        }
#endif
        menuDescription->Perform(myFiles, lpcmi->hwnd);
    }

    return hr;
}



// This is for the status bar and things like that:
STDMETHODIMP CShellExt::GetCommandString(UINT_PTR idCmd,
                                         UINT uFlags,
                                         UINT FAR * /* reserved */,
                                         LPSTR pszName,
                                         UINT cchMax)
{   
   // See if we have a handler interface for this id
   if (myIDMap.find(idCmd) == myIDMap.end())
      return E_INVALIDARG;

   CheckLanguage();

   MenuDescription& iface = ourMenus[myIDMap[idCmd]];
   HRESULT hr = E_INVALIDARG;
   
   // Return the appropriate help text or verb for the command handler
   switch(uFlags)
   {
   case GCS_HELPTEXTA:
   {
      std::string help = wxAscii(GetString(wxTextCStr(iface.GetHelpText())));
      lstrcpynA(pszName, help.c_str(), cchMax);
      hr = S_OK;
      break; 
   }

   case GCS_HELPTEXTW: 
   {
#if wxUSE_UNICODE
      std::wstring help = GetString(wxTextCStr(iface.GetHelpText()));
#else
      std::wstring help(MultibyteToWide(GetString(iface.GetHelpText().c_str())));
#endif
      lstrcpynW((LPWSTR) pszName, help.c_str(), cchMax);
      hr = S_OK;
      break; 
   }

   case GCS_VERBA:
   {
      // Leave this untranslated
      std::string verb = iface.GetVerb();
      lstrcpynA(pszName, verb.c_str(), cchMax); 
      hr = S_OK;
      break; 
   }

   case GCS_VERBW:
   {
      // Leave this untranslated
      std::wstring verb = MultibyteToWide(iface.GetVerb());
      lstrcpynW((LPWSTR) pszName, verb.c_str(), cchMax); 
      hr = S_OK;
      break; 
   }
   
   default:
      hr = S_OK;
      break; 
   }
   return hr;
}

STDMETHODIMP CShellExt::HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT res;
    return HandleMenuMsg2(uMsg, wParam, lParam, &res);
}

STDMETHODIMP CShellExt::HandleMenuMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
    TDEBUG_ENTER("CShellExt::HandleMenuMsg2");
    // A great tutorial on owner drawn menus in shell extension can be found
    // here: http://www.codeproject.com/shell/shellextguide7.asp

    LRESULT res;
    if (!pResult)
        pResult = &res;
    *pResult = FALSE;

    switch (uMsg)
    {
    case WM_MEASUREITEM:
    {
        MEASUREITEMSTRUCT* lpmis = (MEASUREITEMSTRUCT*)lParam;
        if (lpmis==NULL)
            break;
        lpmis->itemWidth += 2;
        if(lpmis->itemHeight < 16)
            lpmis->itemHeight = 16;
        *pResult = TRUE;
    }
    break;
    case WM_DRAWITEM:
    {
        DRAWITEMSTRUCT* lpdis = (DRAWITEMSTRUCT*)lParam;
        if (!lpdis || (lpdis->CtlType != ODT_MENU) || !lpdis->itemData)
            break; //not for a menu
        DrawIconEx(lpdis->hDC,
                   lpdis->rcItem.left - 16,
                   lpdis->rcItem.top + (lpdis->rcItem.bottom - lpdis->rcItem.top - 16) / 2,
                   (HICON) lpdis->itemData, 16, 16,
                   0, 0, DI_NORMAL);
        *pResult = TRUE;
    }
    break;
    default:
        return NOERROR;
    }

    return NOERROR;
}


HBITMAP CShellExt::IconToBitmap(const std::string& sIcon)
{
    if (ourBitmaps.find(sIcon) != ourBitmaps.end())
        return ourBitmaps[sIcon];

    RECT rect;
    rect.left = rect.top = 0;
    rect.right = ::GetSystemMetrics(SM_CXMENUCHECK);
    rect.bottom = ::GetSystemMetrics(SM_CYMENUCHECK);

    HICON hIcon = (HICON) LoadImageA(g_hInstance, sIcon.c_str(), IMAGE_ICON, rect.right, rect.bottom, LR_DEFAULTCOLOR);
    if (!hIcon)
        return 0;

    HWND desktop = ::GetDesktopWindow();
    if (!desktop)
    {
        DestroyIcon(hIcon);
        return 0;
    }

    HDC screen_dev = ::GetDC(desktop);
    if (!screen_dev)
    {
        DestroyIcon(hIcon);
        return 0;
    }

    // Create a compatible DC
    HDC dst_hdc = ::CreateCompatibleDC(screen_dev);
    if (!dst_hdc)
    {
        DestroyIcon(hIcon);
        ::ReleaseDC(desktop, screen_dev); 
        return 0;
    }

    // Create a new bitmap of icon size
    HBITMAP bmp = ::CreateCompatibleBitmap(screen_dev, rect.right, rect.bottom);
    if (!bmp)
    {
        DestroyIcon(hIcon);
        ::DeleteDC(dst_hdc);
        ::ReleaseDC(desktop, screen_dev); 
        return 0;
    }

    // Select it into the compatible DC
    HBITMAP old_dst_bmp = (HBITMAP) ::SelectObject(dst_hdc, bmp);
    if (!old_dst_bmp)
    {
        DestroyIcon(hIcon);
        return 0;
    }

    // Fill the background of the compatible DC with the given colour
    ::SetBkColor(dst_hdc, RGB(255, 255, 255));
    ::ExtTextOut(dst_hdc, 0, 0, ETO_OPAQUE, &rect, 0, 0, 0);

    // Draw the icon into the compatible DC
    ::DrawIconEx(dst_hdc, 0, 0, hIcon, rect.right, rect.bottom, 0, 0, DI_NORMAL);

    // Restore settings
    ::SelectObject(dst_hdc, old_dst_bmp);
    ::DeleteDC(dst_hdc);
    ::ReleaseDC(desktop, screen_dev); 
    DestroyIcon(hIcon);
    if (bmp)
        ourBitmaps[sIcon] = bmp;
    return bmp;
}


HBITMAP CShellExt::IconToBitmapPARGB32(const std::string& sIcon)
{
    if (ourBitmaps.find(sIcon) != ourBitmaps.end())
        return ourBitmaps[sIcon];
    if (!myGdiplusToken)
        return 0;
    HICON hIcon = (HICON) LoadImageA(g_hInstance, sIcon.c_str(), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    if (!hIcon)
        return 0;
    Gdiplus::Bitmap icon(hIcon);
    Gdiplus::Bitmap bmp(16, 16, PixelFormat32bppPARGB);
    Gdiplus::Graphics g(&bmp);
    g.DrawImage(&icon, 0, 0, 16, 16);

    HBITMAP hBmp = 0;
    bmp.GetHBITMAP(Gdiplus::Color(255, 0, 0, 0), &hBmp);

    if (hBmp)
        ourBitmaps[sIcon] = hBmp;
    return hBmp;
}
