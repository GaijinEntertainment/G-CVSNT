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

#ifndef CHECKOUT_OPTIONS_PAGE_H
#define CHECKOUT_OPTIONS_PAGE_H

#include <string>
#include <list>

#include <wx/wx.h>

class wxCheckBox;
class wxTextCtrl;
class ModuleBasicsPage;

// This class implements the layout for the options page
// of the checkout dialog.
class CheckoutOptionsPage : public wxPanel
{
public:
   CheckoutOptionsPage(wxWindow* parent,
                       ModuleBasicsPage* moduleBasicsPage);

   bool GetExportFlag() const
   {
      return myExportFlag;
   }

   // returns empty string if no alternate directory is selected
   std::string GetAlternateDirectory() const
   {
      return myAlternateDirectory;
   }

   bool GetUnixLineEndingsFlag() const
   {
      return myUnixLinesFlag;
   }

   bool GetReadOnlyFlag() const
   {
      return myReadOnlyFlag;
   }

   void Update();
   
   void OKClicked();
private:
    ModuleBasicsPage*   myModuleBasicsPage;
   
   wxRadioButton*       myCheckoutRadioButton;
   wxRadioButton*       myExportRadioButton;

   wxRadioButton*       myModuleDirRadioButton;
   wxRadioButton*       myModuleTagDirRadioButton;
   wxRadioButton*       myAltDirRadioButton;
   wxStaticText*        myAltDirLabel;

   wxTextCtrl*          myAltDirEntry;

   wxCheckBox*          myUnixLinesCheck;
   wxCheckBox*          myReadOnlyCheck;

   bool                 myExportFlag;
   std::string          myAlternateDirectory;

   bool                 myUnixLinesFlag;
   bool                 myReadOnlyFlag;

   void OnExport(wxCommandEvent& event);
   void Refresh(wxCommandEvent& event);
   void RefreshAltDir(wxCommandEvent& event);

   DECLARE_EVENT_TABLE()
};

#endif // CHECKOUT_OPTIONS_PAGE_H


