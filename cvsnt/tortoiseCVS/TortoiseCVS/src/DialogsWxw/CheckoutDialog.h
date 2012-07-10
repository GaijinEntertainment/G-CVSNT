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

#ifndef CHECKOUT_DIALOG_H
#define CHECKOUT_DIALOG_H


#include <string>
#include <list>

#include <wx/wx.h>

class CheckoutOptionsPage;
class wxStatusBar;

#include "RevOptions.h"
#include "ModuleBasicsPage.h"
#include "ExtDialog.h"

#include <string>
#include <list>

class CheckoutDialog : public ExtDialog
{
public:
   friend bool DoCheckoutDialog(wxWindow* parent, CVSRoot& cvsRoot,
                                std::string& module, std::string& tag, std::string& date, bool& exportOnly,
                                std::string& alternateDirectory, bool& unixLineEndings,
#ifdef MARCH_HARE_BUILD
                                bool& fileReadOnly,
#endif
                                const std::string& dir,
                                bool& forceHead,
                                bool& edit,
                                std::string& bugNumber);

   friend bool DoCheckoutDialogPreset(wxWindow* parent,
                                      const CVSRoot& cvsRoot,
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
                                      std::string& bugNumber);
   
private:
   CheckoutDialog(wxWindow* parent, const std::string& dir, bool fixed = false);
   ~CheckoutDialog();

   void PageChanged(wxCommandEvent& event);

   ModuleBasicsPage* myModuleBasicsPage;
   RevOptions* myRevOptions;
   CheckoutOptionsPage* myCheckoutOptionsPage;
   wxStatusBar*         myStatusBar;

   DECLARE_EVENT_TABLE()
};

#endif
