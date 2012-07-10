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
#include "CheckoutDialog.h"
#include "CheckoutOptionsPage.h"

#include "WxwHelpers.h"
#include <wx/notebook.h>
#include "../Utils/Translate.h"
#include "../Utils/PathUtils.h"


bool DoCheckoutDialog(wxWindow* parent, CVSRoot& cvsRoot, 
                      std::string& module, std::string& tag, 
                      std::string& date, bool& exportOnly, 
                      std::string& alternateDirectory,
                      bool& unixLineEndings,
#ifdef MARCH_HARE_BUILD
                      bool& fileReadOnly,
#endif
                      const std::string& dir, 
                      bool& forceHead,
                      bool& edit,
                      std::string& bugNumber)
{
   CheckoutDialog* checkoutDialog = new CheckoutDialog(parent, dir);
   checkoutDialog->myRevOptions->SetForceHead(forceHead);
   checkoutDialog->myModuleBasicsPage->SetRepoGraph(parent);
   int modal = checkoutDialog->ShowModal();
   bool ret = (modal == wxID_OK);

   // Allow pages a chance to save settings
   if (ret)
   {
      checkoutDialog->myModuleBasicsPage->OKClicked();
      checkoutDialog->myRevOptions->OKClicked();
      checkoutDialog->myCheckoutOptionsPage->OKClicked();
   }

   cvsRoot = checkoutDialog->myModuleBasicsPage->GetCVSRoot();
   // Remove trailing slash from directory
   cvsRoot.RemoveTrailingDelimiter();
   module = checkoutDialog->myModuleBasicsPage->GetModule();
   tag = checkoutDialog->myRevOptions->GetTag();
   date = checkoutDialog->myRevOptions->GetDate();

   exportOnly = checkoutDialog->myCheckoutOptionsPage->GetExportFlag();
   alternateDirectory = checkoutDialog->myCheckoutOptionsPage->GetAlternateDirectory();
   unixLineEndings = checkoutDialog->myCheckoutOptionsPage->GetUnixLineEndingsFlag();
#ifdef MARCH_HARE_BUILD
   fileReadOnly = checkoutDialog->myCheckoutOptionsPage->GetReadOnlyFlag();
#endif
   forceHead = checkoutDialog->myRevOptions->GetForceHead();
   edit = checkoutDialog->myModuleBasicsPage->GetEdit();
   bugNumber = checkoutDialog->myModuleBasicsPage->GetBugNumber();

   // Delete the dialog
   DestroyDialog(checkoutDialog);

   return ret;
}

bool DoCheckoutDialogPreset(wxWindow* parent, const CVSRoot& cvsRoot,
                            const std::string& module,
                            std::string& tag,
                            const std::string& dir,
                            std::string& date,
                            bool& exportOnly,
                            std::string& alternateDirectory,
                            bool& unixLineEndings,
#ifdef MARCH_HARE_BUILD
                            bool& fileReadOnly,
#endif
                            bool& forceHead,
                            bool& edit,
                            std::string& bugNumber)
{
   CheckoutDialog* checkoutDialog = new CheckoutDialog(parent, dir, true);
   checkoutDialog->myModuleBasicsPage->SetCVSRoot(cvsRoot);
   checkoutDialog->myModuleBasicsPage->SetModule(module);
   checkoutDialog->myRevOptions->SetTag(tag);

   int modal = checkoutDialog->ShowModal();
   bool ret = (modal == wxID_OK);

   // Allow pages a chance to save settings
   if (ret)
   {
      checkoutDialog->myCheckoutOptionsPage->Update();
      checkoutDialog->myModuleBasicsPage->OKClicked();
      checkoutDialog->myRevOptions->OKClicked();

      tag = checkoutDialog->myRevOptions->GetTag();
      date = checkoutDialog->myRevOptions->GetDate();

      exportOnly = checkoutDialog->myCheckoutOptionsPage->GetExportFlag();
      alternateDirectory = checkoutDialog->myCheckoutOptionsPage->GetAlternateDirectory();
      unixLineEndings = checkoutDialog->myCheckoutOptionsPage->GetUnixLineEndingsFlag();
      forceHead = checkoutDialog->myRevOptions->GetForceHead();
      edit = checkoutDialog->myModuleBasicsPage->GetEdit();
      bugNumber = checkoutDialog->myModuleBasicsPage->GetBugNumber();
   }
   
   // Delete the dialog
   DestroyDialog(checkoutDialog);

   return ret;
}

enum
{
   CHECKOUTDLG_ID_BOOK = 10001
};

BEGIN_EVENT_TABLE(CheckoutDialog, ExtDialog)
   EVT_COMMAND(CHECKOUTDLG_ID_BOOK, wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED, CheckoutDialog::PageChanged)
END_EVENT_TABLE()


CheckoutDialog::CheckoutDialog(wxWindow* parent, const std::string& dir, bool fixed)
   : ExtDialog(parent, -1, _("TortoiseCVS - Checkout Module"),
               wxDefaultPosition, wxDefaultSize,
               wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxRESIZE_BORDER |
               wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN)
{
    SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

    // Main notebook
    wxBookCtrl *book = new wxNotebook(this, CHECKOUTDLG_ID_BOOK);
    myModuleBasicsPage = new ModuleBasicsPage(book, MB_CHECKOUT_MODULE, fixed);
    myRevOptions = new RevOptions(book);
    myModuleBasicsPage->SetRevOptions(myRevOptions);
    myRevOptions->SetDir(dir);
    myCheckoutOptionsPage = new CheckoutOptionsPage(book, myModuleBasicsPage);
    book->AddPage(myModuleBasicsPage, _("Module"));
    book->AddPage(myRevOptions, _("Revision"));
    book->AddPage(myCheckoutOptionsPage, _("Options"));

    myStatusBar = new wxStatusBar(this, -1);

   // OK/Cancel button
   wxBoxSizer *sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   wxButton* ok = new wxButton(this, wxID_OK, _("OK"));
   ok->SetDefault();
   wxButton* cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
   sizerConfirm->Add(ok, 0, wxGROW | wxALL, 5);
   sizerConfirm->Add(cancel, 0, wxGROW | wxALL, 5);

   // Main box with text and button in it
   wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(book, 1, wxGROW | wxALL, 3);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxALL, 0);
   sizerTop->Add(myStatusBar, 0, wxGROW | wxALL, 0);

   // Overall dialog layout settings
   myModuleBasicsPage->UpdateSensitivity();
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);

   RestoreTortoiseDialogSize(this, "Checkout", wxDLG_UNIT(this, wxSize(200, 120)));
   SetTortoiseDialogPos(this, GetRemoteHandle());
   RestoreTortoiseDialogState(this, "Checkout");
}

CheckoutDialog::~CheckoutDialog()
{
    StoreTortoiseDialogSize(this, "Checkout");
}

void CheckoutDialog::PageChanged(wxCommandEvent&)
{
   CVSRoot cvsroot = myModuleBasicsPage->GetCVSRoot();
   myRevOptions->SetCVSRoot(cvsroot);
   myCheckoutOptionsPage->Update();
}
