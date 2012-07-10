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
#include "MakeModuleDialog.h"
#include "MakeModuleBasicsPage.h"
#include "WxwHelpers.h"
#include <wx/notebook.h>
#include "../Utils/Translate.h"
#include "../Utils/PathUtils.h"
#include "../Utils/TortoiseRegistry.h"

bool DoMakeModuleDialog(wxWindow* parent, CVSRoot& cvsRoot, 
                        std::string& module,
                        bool& watchon, std::string& comment)
{
   MakeModuleDialog* makeModuleDialog = new MakeModuleDialog(parent, module);
   int modal = makeModuleDialog->ShowModal();
   bool ret = (modal == wxID_OK);

   // Allow page a chance to save settings
   if (ret)
      makeModuleDialog->myModuleBasicsPage->OKClicked();

   cvsRoot = makeModuleDialog->myModuleBasicsPage->GetCVSRoot();
   // Remove trailing slash from directory
   cvsRoot.RemoveTrailingDelimiter();
   module = makeModuleDialog->myModuleBasicsPage->GetModule();
   watchon = makeModuleDialog->myCheckWatch->GetValue();
   comment = wxAscii(makeModuleDialog->myComment->GetValue().c_str());
   TortoiseRegistry::WriteBoolean("Dialogs\\MakeModule\\Watch on", watchon);

   // Delete the dialog
   DestroyDialog(makeModuleDialog);

   return ret;
}

MakeModuleDialog::MakeModuleDialog(wxWindow* parent, const std::string& module)
   : ExtDialog(parent, -1, _("TortoiseCVS - Make New Module"),
               wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
{
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));
   SetSize(wxDLG_UNIT(this, wxSize(240, 260)));

   // Main notebook
   wxBookCtrl *book = new wxNotebook(this, -1);
   myModuleBasicsPage = new MakeModuleBasicsPage(book);
   myModuleBasicsPage->SetModule(module);
   book->AddPage(myModuleBasicsPage, _("Module"));

   wxPanel *options = new wxPanel(book, -1);

   wxString label0txt(_("Comment"));
   label0txt += _(":");
   wxStaticText *label0 = new wxStaticText(options, -1, label0txt);
   myComment = new wxTextCtrl(options, -1, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
   myComment->SetMaxLength(20000);
   
   myCheckWatch = new wxCheckBox(options, -1,
                                 _("Check out files read-only"));
   myCheckWatch->SetValue(TortoiseRegistry::ReadBoolean("Dialogs\\MakeModule\\Watch on", false));
   myCheckWatch->SetToolTip(_("Run 'cvs watch on' on the new module so that files will be checked out read-only"));
   book->AddPage(options, _("Options"));

   wxBoxSizer *sizerOptions = new wxBoxSizer(wxVERTICAL);
   sizerOptions->Add(label0, 0, wxGROW | wxALL, 7);
   sizerOptions->Add(myComment, 1, wxGROW | wxALL, 7);
   sizerOptions->Add(myCheckWatch, 0, wxALL, 7);
   options->SetSizer(sizerOptions);
   sizerOptions->SetSizeHints(options);
   sizerOptions->Fit(options);
   
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
   sizerTop->Add(5, 0);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxALL, 0);

   // Overall dialog layout settings
   myModuleBasicsPage->UpdateSensitivity();
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
 
   sizerTop->SetMinSize(GetSize());
   sizerTop->Fit(this);

   SetTortoiseDialogPos(this, GetRemoteHandle());
}



MakeModuleDialog::~MakeModuleDialog()
{
}
