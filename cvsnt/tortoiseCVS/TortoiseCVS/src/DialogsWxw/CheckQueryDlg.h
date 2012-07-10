// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Ben Campbell
// <ben.campbell@creaturelabs.com> - September 2000

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

#ifndef CHECKQUERYDLG_H
#define CHECKQUERYDLG_H

#include <string>
#include <wx/wx.h>
#include "ExtDialog.h"

class CheckQueryDlg : ExtDialog
{
public:
   friend bool DoCheckQueryDialog(wxWindow* parent, const wxString& message, 
                                  bool yesdefault, const wxString& checkText);

private:
   CheckQueryDlg(wxWindow* parent, const wxString& message, bool yesdefault, 
                 const wxString& checkText);

   wxCheckBox* myCheckbox;
};

#endif

