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

#ifndef MAKE_MODULE_DIALOG_H
#define MAKE_MODULE_DIALOG_H

#include <string>
#include <list>
#include <wx/wx.h>

#include "ExtDialog.h"


class MakeModuleBasicsPage;
class CVSRoot;


class MakeModuleDialog : public ExtDialog
{
public:
   friend bool DoMakeModuleDialog(wxWindow* parent, CVSRoot& cvsRoot, 
                                  std::string& module,
                                  bool& watchon, std::string& comment);
   
private:
   MakeModuleDialog(wxWindow* parent, const std::string& module);
   ~MakeModuleDialog();


   MakeModuleBasicsPage* myModuleBasicsPage;
   wxCheckBox*           myCheckWatch;
   wxTextCtrl*           myComment;
};

#endif
