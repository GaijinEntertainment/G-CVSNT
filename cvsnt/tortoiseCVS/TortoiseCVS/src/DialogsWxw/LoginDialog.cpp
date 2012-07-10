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

#include <wx/wx.h>
#include "LoginDialog.h"
#include "ProgressDialog.h" // for parent
#include "WxwHelpers.h"
#include "../Utils/Translate.h"

bool DoLoginDialog(wxWindow* parent, std::string& password)
{
   wxTextEntryDialog* dlg = new wxTextEntryDialog(parent,
                                                  _("Password:"),
                                                  _("TortoiseCVS"),
                                                  wxT(""),
                                                  wxOK | wxCANCEL | wxTE_PASSWORD);
   
   SetTortoiseDialogPos(dlg, GetRemoteHandle());
   int ret = dlg->ShowModal();
   password = wxAscii(dlg->GetValue().c_str());
   
   DestroyDialog(dlg);
   return ret == wxID_OK;
}
