// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2005 - Marcel Gosselin
// <marcel.gosselin@polymtl.ca> - March 2005

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

#ifndef FINDDLG_H
#define FINDDLG_H

#include <wx/wx.h>
#include "ExtDialog.h"

class FindDialog : public ExtDialog
{
public:
   FindDialog(wxWindow* parent);

private:
   void OnFindNext(wxCommandEvent& event);
   void OnMarkAll(wxCommandEvent& event);
   void OnClose(wxCommandEvent& event);

   wxTextCtrl* mySearchString;
   wxCheckBox* mySearchUp;
   wxCheckBox* myCaseSensitive;
   wxCheckBox* myWrapSearch;
};

#endif
