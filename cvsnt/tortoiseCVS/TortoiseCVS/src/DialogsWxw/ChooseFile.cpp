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
#include <string>
#include "ChooseFile.h"
#include "WxwHelpers.h"
#include <wx/filedlg.h>

std::string DoOpenFileDialog(wxWindow* parent, 
                             const wxString& titleMessage,
                             const std::string& startBrowsingFrom,
                             const wxString& patterns)
{
   wxFileDialog* dlg = new wxFileDialog(parent,
                                        titleMessage,
                                        wxText(startBrowsingFrom),
                                        wxT(""),
                                        patterns,
                                        wxOPEN);
   SetTortoiseDialogPos(dlg, GetRemoteHandle());

   int ret = dlg->ShowModal();
   std::string retString = (ret == wxID_OK) ? wxAscii(dlg->GetPath().c_str()) : "";

   DestroyDialog(dlg);
   return retString;
}

std::string DoSaveFileDialog(wxWindow* parent, 
                             const wxString& titleMessage,
                             const std::string& startBrowsingFrom,
                             const wxString& patterns)
{
   wxFileDialogBase* dlg = new wxFileDialog(parent,
                                            titleMessage,
                                            wxT(""),
                                            wxText(startBrowsingFrom),
                                            patterns,
                                            wxSAVE | wxOVERWRITE_PROMPT);

   SetTortoiseDialogPos(dlg, GetRemoteHandle());
   int ret = dlg->ShowModal();
   std::string retString = (ret == wxID_OK) ? wxAscii(dlg->GetPath().c_str()) : "";

   DestroyDialog(dlg);
   return retString;
}


