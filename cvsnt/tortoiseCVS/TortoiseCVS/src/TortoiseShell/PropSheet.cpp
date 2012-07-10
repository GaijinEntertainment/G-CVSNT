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
#include <sstream>
#include <CVSGlue/CVSStatus.h>
#include <Utils/PathUtils.h>
#include <Utils/UnicodeStrings.h>
#include <Utils/TimeUtils.h>
#include <Utils/LogUtils.h>
#include <Utils/TortoiseDebug.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/TortoiseRegistry.h>
#include <Utils/LaunchTortoiseAct.h>
#include <ContextMenus/HasMenu.h>
#include "TortoiseShellRes.h"
#include "PropSheet.h"
#include <Utils/Translate.h>
#include <TortoiseAct/TortoiseActVerbs.h>

/////////////////////////////////////////////////////////////////////////////
// List box defines

/////////////////////////////////////////////////////////////////////////////
// Nonmember function prototypes

INT_PTR CALLBACK PageProc (HWND, UINT, WPARAM, LPARAM);
UINT CALLBACK PropPageCallbackProc (HWND hwnd, UINT uMsg, LPPROPSHEETPAGE ppsp);

/////////////////////////////////////////////////////////////////////////////
// CShellExt member functions

const static int HasMenuConst = 
  EXACTLY_ONE | TYPE_INCVS_RO | TYPE_INCVS_RW | TYPE_INCVS_LOCKED | TYPE_ADDED | TYPE_CHANGED |
  TYPE_RENAMED | TYPE_CONFLICT | TYPE_STATIC | TYPE_OUTERDIRECTORY;

STDMETHODIMP CShellExt::AddPages(LPFNADDPROPSHEETPAGE lpfnAddPage,
                                 LPARAM lParam)
{
   // Only show property sheet if there's something worth showing
   CheckLanguage();
   if (!HasMenu(HasMenuConst, myFiles))
   {
      return NOERROR;
   }
   
   PROPSHEETPAGE psp;
   HPROPSHEETPAGE hPage;
   CVSRevisionsPropSheet* sheet = new CVSRevisionsPropSheet(myFiles[0]);

   psp.dwSize = sizeof (psp);
   psp.dwFlags = PSP_USEREFPARENT | PSP_USETITLE | PSP_USEICONID | PSP_USECALLBACK;
   psp.hInstance = g_hInstance;
   psp.pszTemplate = wxT("Revisions");
   psp.pszIcon = wxT("IDI_TORTOISE");
   psp.pszTitle = wxT("CVS");
   psp.pfnDlgProc = PageProc;
   psp.lParam = reinterpret_cast<LPARAM>(sheet);
   psp.pfnCallback = PropPageCallbackProc;
   psp.pcRefParent = &g_cRefThisDll;

   hPage = CreatePropertySheetPage(&psp);

   if (hPage)
      if (!lpfnAddPage(hPage, lParam))
      {
         delete sheet;
         DestroyPropertySheetPage(hPage);
      }

   return NOERROR;
}

STDMETHODIMP CShellExt::ReplacePage(UINT /* uPageID */,
                                    LPFNADDPROPSHEETPAGE /* lpfnReplaceWith */, 
                                    LPARAM /* lParam */)
{
   return E_FAIL;
}

/////////////////////////////////////////////////////////////////////////////
// Dialog procedures and other callback functions

INT_PTR CALLBACK PageProc(HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
   CVSRevisionsPropSheet * sheet;

   if (uMessage == WM_INITDIALOG)
   {
      sheet = (CVSRevisionsPropSheet*) ((LPPROPSHEETPAGE) lParam)->lParam;
      SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG>(sheet));
      sheet->SetHwnd(hwnd);
   }
   else
      sheet = reinterpret_cast<CVSRevisionsPropSheet*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

   if (sheet)
      return sheet->PageProc(uMessage, wParam, lParam);
   else
      return FALSE;
}

UINT CALLBACK PropPageCallbackProc(HWND /* hwnd */, UINT uMsg, LPPROPSHEETPAGE ppsp)
{
    //
    // Delete the internal property sheet storage class before closing.
    //
    if ( uMsg == PSPCB_RELEASE )
    {
        CVSRevisionsPropSheet* sheet = (CVSRevisionsPropSheet*) ppsp->lParam;
        delete sheet;
    }
    return 1;
}

// *********************** CVSRevisionsPropSheet *************************

CVSRevisionsPropSheet::CVSRevisionsPropSheet(const std::string& newFilename)
   : myFilename(newFilename)
{
}

void CVSRevisionsPropSheet::SetHwnd(HWND newHwnd)
{
    myHWND = newHwnd;
}

void CVSRevisionsPropSheet::ButtonClicked(WPARAM ButtonControlId)
{
   switch (ButtonControlId)
   {
   case IDC_HISTORY:
      LaunchTortoiseAct(false, CvsHistoryVerb, myFilename);
      break;

   case IDC_REVISIONGRAPH:
      LaunchTortoiseAct(false, CvsRevisionGraphVerb, myFilename);
      break;

   default:
      break;
      // TODO: Need to assert?
   }
}

INT_PTR CVSRevisionsPropSheet::PageProc (UINT uMessage, WPARAM wParam, LPARAM /* lParam */)
{
   switch (uMessage)
   {
   case WM_INITDIALOG:
      //
      // Initialize all controls on the dialog.
      //
      InitWorkfileView();
      return TRUE;

   case WM_COMMAND:
      if (HIWORD(wParam) != BN_CLICKED)
         return FALSE;

      ButtonClicked(LOWORD(wParam));
      return TRUE;

   case WM_NOTIFY:
      //
      // Respond to notifications.
      //    
      SetWindowLongPtr(myHWND, DWLP_MSGRESULT, FALSE);
      return TRUE;        

   case WM_DESTROY:
      return TRUE;
   }
   return FALSE;
}

// TODO: Need to refactor with other similar code elsewhere

void CVSRevisionsPropSheet::UpdateKeywordExpansionControls()
{
   CVSStatus::FileFormat fmt;
   CVSStatus::KeywordOptions ko;
   CVSStatus::FileOptions fo;
   CVSStatus::GetParsedOptions(myFilename, &fmt, &ko, &fo);
   SetDlgItemText(myHWND, IDC_FILEFORMAT, CVSStatus::FileFormatString(fmt).c_str());
   SetDlgItemText(myHWND, IDC_KEYWORD, CVSStatus::KeywordOptionsString(ko).c_str());
   SetDlgItemText(myHWND, IDC_OPTIONS, CVSStatus::FileOptionsString(fo).c_str());
}

void CVSRevisionsPropSheet::InitWorkfileView()
{
    extern const wxChar* GetString(const wxChar* str);

    // Localisation
    wxString text;
    text = _("Status");
    text += _(":");
    SetDlgItemText(myHWND, IDC_STATUS_H,      text.c_str());
    text = _("CVSROOT");
    text += _(":");
    SetDlgItemText(myHWND, IDC_CVSROOT_H,     text.c_str());
    text = _("Repository");
    text += _(":");
    SetDlgItemText(myHWND, IDC_REPOSITORY_H,  text.c_str());
    text = _("Revision");
    text += _(":");
    SetDlgItemText(myHWND, IDC_REVISION_H,    text.c_str());
    text = _("Sticky tag/date");
    text += _(":");
    SetDlgItemText(myHWND, IDC_TAG_H,         text.c_str());
    text = _("Revision date");
    text += _(":");
    SetDlgItemText(myHWND, IDC_TIMESTAMP_H,   text.c_str());
    text = _("File format");
    text += _(":");
    SetDlgItemText(myHWND, IDC_FILEFORMAT_H,  text.c_str());
    text = _("Keywords");
    text += _(":");
    SetDlgItemText(myHWND, IDC_KEYWORD_H,     text.c_str());
    text = _("File options");
    text += _(":");
    SetDlgItemText(myHWND, IDC_OPTIONS_H,     text.c_str());
    SetDlgItemText(myHWND, IDC_HISTORY,       _("&History..."));
    SetDlgItemText(myHWND, IDC_REVISIONGRAPH, _("Revision &Graph..."));

    // Show CVSROOT
    SetDlgItemText(myHWND, IDC_CVSROOT,      wxTextCStr(CVSStatus::CVSRootForPath(myFilename)));
    SetDlgItemText(myHWND, IDC_REPOSITORY, wxTextCStr(CVSStatus::CVSRepositoryForPath(myFilename)));

    // Show revision number
    if (CVSStatus::HasRevisionNumber(myFilename))
        SetDlgItemText(myHWND, IDC_REVISION, wxTextCStr(CVSStatus::GetRevisionNumber(myFilename)));

    // Show sticky tag or date
    std::string stickyTagOrDate;
    if (CVSStatus::HasStickyTag(myFilename))
        stickyTagOrDate = CVSStatus::GetStickyTag(myFilename);
    else if (CVSStatus::HasStickyDate(myFilename))
    {
        // TODO: Convert from CVS "free-format" date to localized date
        stickyTagOrDate = CVSStatus::GetStickyDate(myFilename);
    }
    wxString wxStickyTagOrDate;
    if (stickyTagOrDate.empty())
        wxStickyTagOrDate = _("Head");
    else
        wxStickyTagOrDate = wxText(stickyTagOrDate);
    SetDlgItemText(myHWND, IDC_TAG, wxStickyTagOrDate.c_str());

    // Show timestamp
    if (CVSStatus::HasTimestamp(myFilename))
    {
        std::string ascTimestamp = CVSStatus::GetTimestamp(myFilename);
        wxChar dateBuf[100];
        
        if (asctime_to_local_DateTimeFormatted(ascTimestamp.c_str(), dateBuf, sizeof(dateBuf)))
            SetDlgItemText(myHWND, IDC_TIMESTAMP, dateBuf);
    }

    // Show file status
    CVSStatus::FileStatus status = CVSStatus::GetFileStatusRecursive(myFilename);
    SetDlgItemText(myHWND, IDC_STATUS, CVSStatus::FileStatusString(status).c_str());

    UpdateKeywordExpansionControls();

    if (IsDirectory(myFilename.c_str()))
    {
        ShowWindow(GetDlgItem(myHWND, IDC_STATIC_FILEONLY), SW_HIDE);
        ShowWindow(GetDlgItem(myHWND, IDC_HISTORY), SW_HIDE);
        ShowWindow(GetDlgItem(myHWND, IDC_REVISIONGRAPH), SW_HIDE);
        ShowWindow(GetDlgItem(myHWND, IDC_REVISION_H), SW_HIDE);
        ShowWindow(GetDlgItem(myHWND, IDC_REVISION), SW_HIDE);
        ShowWindow(GetDlgItem(myHWND, IDC_TIMESTAMP_H), SW_HIDE);
        ShowWindow(GetDlgItem(myHWND, IDC_TIMESTAMP), SW_HIDE);
        ShowWindow(GetDlgItem(myHWND, IDC_FILEFORMAT_H), SW_HIDE);
        ShowWindow(GetDlgItem(myHWND, IDC_FILEFORMAT), SW_HIDE);
        ShowWindow(GetDlgItem(myHWND, IDC_KEYWORD_H), SW_HIDE);
        ShowWindow(GetDlgItem(myHWND, IDC_KEYWORD), SW_HIDE);
        ShowWindow(GetDlgItem(myHWND, IDC_OPTIONS_H), SW_HIDE);
        ShowWindow(GetDlgItem(myHWND, IDC_OPTIONS), SW_HIDE);
    }

    // TODO: Need to move this somewhere else so that
    // focus is indeed on the History button and not
    // on whatever is the first element on the page control
    // (presumably as set by the general property sheet framework)
    SetFocus(GetDlgItem(myHWND, IDC_HISTORY));
}
