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

#ifndef LOG_CONFIG_DIALOG_H
#define LOG_CONFIG_DIALOG_H

#include <string>
#include <wx/wx.h>
#include "ExtDialog.h"

// This class implements a gui layout to allow the user to specify a 
// URL stub for web loggin, or to chose a plain log

class LogConfigDialog : ExtDialog
{
public:
   // urlStub is "plainlog" for plain vanilla log
   // or "scanagain" for scanning again
   friend bool DoLogConfigDialog(wxWindow* parent,
                                 std::string& urlStubPrefix, 
                                 std::string& urlStubSuffix,
                                 const wxString& message,
                                 const std::string& regKey);

private:
   LogConfigDialog(wxWindow* parent,
                   std::string& urlStubPrefix, 
                   std::string& urlStubSuffix,
                   const wxString& message,
                   const std::string& regKey);
   ~LogConfigDialog();

   bool myOK;
   std::string myRegKey;

   wxRadioButton* myRadioURL;
   wxComboBox* myPrefixCombo;
   wxComboBox* mySuffixCombo;
   wxRadioButton* myRadioVanilla;
   wxRadioButton* myRadioScanAgain;

   std::string myURLPrefix;
   std::string myURLSuffix;

   void UpdateText();
   void UpdateSensitivity();

   void cb_RadioChanged(wxCommandEvent& event);
   void cb_TextChanged(wxCommandEvent& event);

   DECLARE_EVENT_TABLE()
};

#endif

