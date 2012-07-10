// TortoiseCVS - a Windows shell extension for easy version control

// Checkout options page
// Copyright (C) 2002 - Ben Campbell
// <ben.campbell@ntlworld.com> - Jan 2002

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
#include "CheckoutOptionsPage.h"
#include "ModuleBasicsPage.h"
#include "WxwHelpers.h"

#include <wx/textctrl.h>
#include "../Utils/Translate.h"


enum
{
   CHECKOUTOPTIONSPAGE_ID_CHECKOUT = 10001,
   CHECKOUTOPTIONSPAGE_ID_EXPORT,
   CHECKOUTOPTIONSPAGE_ID_MODULEDIR,
   CHECKOUTOPTIONSPAGE_ID_MODULETAGDIR,
   CHECKOUTOPTIONSPAGE_ID_ALTDIR
};


BEGIN_EVENT_TABLE(CheckoutOptionsPage, wxPanel)
   EVT_COMMAND(CHECKOUTOPTIONSPAGE_ID_CHECKOUT, wxEVT_COMMAND_RADIOBUTTON_SELECTED, CheckoutOptionsPage::OnExport)
   EVT_COMMAND(CHECKOUTOPTIONSPAGE_ID_EXPORT, wxEVT_COMMAND_RADIOBUTTON_SELECTED, CheckoutOptionsPage::OnExport)
   EVT_COMMAND(CHECKOUTOPTIONSPAGE_ID_MODULEDIR, wxEVT_COMMAND_RADIOBUTTON_SELECTED, CheckoutOptionsPage::Refresh)
   EVT_COMMAND(CHECKOUTOPTIONSPAGE_ID_MODULETAGDIR, wxEVT_COMMAND_RADIOBUTTON_SELECTED, CheckoutOptionsPage::Refresh)
   EVT_COMMAND(CHECKOUTOPTIONSPAGE_ID_ALTDIR, wxEVT_COMMAND_RADIOBUTTON_SELECTED, CheckoutOptionsPage::RefreshAltDir)
END_EVENT_TABLE()
    

CheckoutOptionsPage::CheckoutOptionsPage(wxWindow* parent,
                                         ModuleBasicsPage* moduleBasicsPage)
   : wxPanel(parent),
     myModuleBasicsPage(moduleBasicsPage)
{
   wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
   SetAutoLayout(TRUE);
   SetSizer(sizer);

   // Input widgets
  
   sizer->Add(0, 4);

   wxStaticBox* staticBox = new wxStaticBox(this, -1, _("Purpose of checkout"));
   wxStaticBoxSizer* sizerStaticBox = new wxStaticBoxSizer(staticBox, wxVERTICAL);

   const wxChar* tip3 = _("Prevents creation of CVS admin folders");
   myCheckoutRadioButton = new wxRadioButton(this, CHECKOUTOPTIONSPAGE_ID_CHECKOUT,
                                             _("&Checkout - for working on the module"),
                                             wxDefaultPosition, wxDefaultSize, wxRB_GROUP );
   myCheckoutRadioButton->SetValue(true);
   myExportRadioButton = new wxRadioButton(this, CHECKOUTOPTIONSPAGE_ID_EXPORT,
                                           _("&Export - for making a software release") );
   sizerStaticBox->Add( myCheckoutRadioButton, 0, wxGROW | wxALL, 4);
   sizerStaticBox->Add( myExportRadioButton, 0, wxGROW | wxALL, 4);
   myExportRadioButton->SetToolTip(tip3);

   sizer->Add(sizerStaticBox, 0, wxGROW | wxALL, 4);

   sizer->Add(0, 10);

#ifdef MARCH_HARE_BUILD
   const wxChar* tip5 = _(
"Normally TortoiseCVS will checkout files so they can be modified. \
If you check this, TortoiseCVS will set the files to read-only and you can use the Edit option to make them changeable.");

   myReadOnlyCheck = new wxCheckBox(this, -1, _("Check out read only"));
   myReadOnlyCheck->SetToolTip(tip5);
   sizer->Add(myReadOnlyCheck, 0, wxGROW | wxALL, 4);
#endif

   wxStaticBox* staticBox2 = new wxStaticBox(this, -1, _("Name of folder to create") );
   wxStaticBoxSizer* sizerStaticBox2 = new wxStaticBoxSizer(staticBox2, wxVERTICAL);

   const wxChar* tip1 = _("Use this name instead of module name for the new folder");

   myModuleDirRadioButton = new wxRadioButton(this, CHECKOUTOPTIONSPAGE_ID_MODULEDIR,
                                              _("Use &default folder name"),
                                              wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
   myModuleDirRadioButton->SetValue(true);
   myModuleTagDirRadioButton = new wxRadioButton(this, CHECKOUTOPTIONSPAGE_ID_MODULETAGDIR,
                                                 _("Use &module name and tag/branch/revision as folder name"));
   myAltDirRadioButton = new wxRadioButton(this, CHECKOUTOPTIONSPAGE_ID_ALTDIR, _("Enter your own &folder name"));
   sizerStaticBox2->Add( myModuleDirRadioButton, 0, wxGROW | wxALL, 4);
   sizerStaticBox2->Add( myModuleTagDirRadioButton, 0, wxGROW | wxALL, 4);
   sizerStaticBox2->Add( myAltDirRadioButton, 0, wxGROW | wxALL, 4);
   myAltDirRadioButton->SetToolTip(tip1);

   wxBoxSizer *horzSizer = new wxBoxSizer(wxHORIZONTAL);
   myAltDirLabel = new wxStaticText(this, -1, _("Custom folder name:"));
   horzSizer->Add( myAltDirLabel, 0, wxALL | wxALIGN_CENTER_VERTICAL, 4);
   myAltDirEntry = new wxTextCtrl(this, -1, wxT(""));
   horzSizer->Add( myAltDirEntry, 1, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 4);
   myAltDirEntry->SetToolTip(tip1);
   myAltDirLabel->SetToolTip(tip1);

   sizerStaticBox2->Add( horzSizer, 0, wxGROW | wxALL, 2);

   sizer->Add( sizerStaticBox2, 0, wxGROW | wxALL, 4);

   sizer->Add(0, 10);

   const wxChar* tip4 = _(
"Normally, text files checked out on Windows will use CRLF as the line ending. \
If you check this, TortoiseCVS will use UNIX line endings (LF only).");

   myUnixLinesCheck = new wxCheckBox(this, -1, _("Use UNIX &line endings"));
   myUnixLinesCheck->SetToolTip(tip4);
   sizer->Add(myUnixLinesCheck, 0, wxGROW | wxALL, 4);

   wxCommandEvent event;
   Refresh(event);
}


void CheckoutOptionsPage::Update()
{
   wxCommandEvent event;
   Refresh(event);
}


void CheckoutOptionsPage::Refresh(wxCommandEvent&)
{
   myExportFlag = myExportRadioButton->GetValue();

   bool enableTagDir = myModuleTagDirRadioButton->GetValue();
   bool enableAltDir = myAltDirRadioButton->GetValue();
   myAltDirEntry->Enable(enableAltDir);
   myAltDirLabel->Enable(enableAltDir);
   if (enableAltDir)
   {
      myAlternateDirectory = wxAscii(myAltDirEntry->GetValue().c_str());
      myAltDirEntry->SetSelection(-1, -1);
   }
   else if (enableTagDir)
   {
      std::string dir = myModuleBasicsPage->GetModule();
      std::string tag = myModuleBasicsPage->GetRevOptions()->GetTag();
      std::string date = myModuleBasicsPage->GetRevOptions()->GetDate();
      if (!tag.empty() || !date.empty())
      {
         dir += '-';
         dir += (date.empty() ? tag : date);
      }
      // Replace characters that cannot be in a filename
      FindAndReplace<std::string>(dir, ":", ".");
      FindAndReplace<std::string>(dir, "/", "-");
      myAltDirEntry->SetValue(wxText(dir));
      myAlternateDirectory = dir;
   }
   else
   {
      myAlternateDirectory = "";
      myAltDirEntry->SetValue(wxText(myAlternateDirectory));
   }
   myUnixLinesFlag = myUnixLinesCheck->GetValue();
#ifdef MARCH_HARE_BUILD
   myReadOnlyFlag = myReadOnlyCheck->GetValue();
#endif
}

void CheckoutOptionsPage::RefreshAltDir(wxCommandEvent& event)
{
   Refresh(event);
   myAltDirEntry->SetFocus();
}


void CheckoutOptionsPage::OnExport(wxCommandEvent& event)
{
    myModuleBasicsPage->ExportEnabled(myExportRadioButton->GetValue());
    Refresh(event);
}


void CheckoutOptionsPage::OKClicked()
{
   wxCommandEvent event;
   Refresh(event);
}
