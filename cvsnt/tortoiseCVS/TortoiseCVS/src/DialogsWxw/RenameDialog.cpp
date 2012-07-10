// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2006 - Torsten Martinsen
// <torsten@vip.cybercity.dk> - November 2006

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
#include "RenameDialog.h"
#include "WxwHelpers.h"
#include "ExtTextCtrl.h"

//static
bool DoRenameDialog(wxWindow* parent,
                    const std::string& oldname,
                    std::string& newname)
{
   RenameDialog* dlg = new RenameDialog(parent, oldname, newname);
   bool ret = (dlg->ShowModal() == wxID_OK);
   if (ret)
      newname = wxAscii(dlg->myNewName->GetValue().c_str());
   DestroyDialog(dlg);
   return ret;
}

enum
{
   RENAMEDLG_ID_BROWSE = 10001
};


RenameDialog::RenameDialog(wxWindow* parent,
                           const std::string& oldpath,
                           std::string& newname)
   : ExtDialog(parent, -1,
               _("TortoiseCVS - Rename"),
               wxDefaultPosition, wxDefaultSize,
               wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxRESIZE_BORDER | 
               wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN)
{
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

   myPath = GetDirectoryPart(oldpath);
   std::string oldname = GetLastPart(oldpath);
   wxBoxSizer* sizerRename = new wxBoxSizer(wxVERTICAL);
   wxStaticText* labelTop = new wxStaticText(this, -1, Printf(_("Rename '%s' to"), wxTextCStr(oldname)) + _(":"));
   // Prevent user from leaving current directory
   wxTextValidator validator(wxFILTER_EXCLUDE_CHAR_LIST, &myNewNameString);
   wxArrayString illegalChars;
   illegalChars.Add(wxT("/"));
   illegalChars.Add(wxT("\\"));
   illegalChars.Add(wxT(":"));
   validator.SetExcludes(illegalChars);
   myNewName = new ExtTextCtrl(this, -1, wxText(oldname),
                               wxDefaultPosition, wxDefaultSize, 0,
                               validator);                               
   //myNewName->SetSelection(-1, -1);
   myNewName->SetInsertionPoint(0);
   wxSize size = myNewName->GetTextSize();
   wxSize bestsize = myNewName->GetBestSize();
   size.SetHeight(bestsize.GetHeight());
   size.SetWidth(size.GetWidth()*3/2);
   myNewName->SetSize(size);
   wxBoxSizer* newNameSizer = new wxBoxSizer(wxHORIZONTAL);
   newNameSizer->Add(myNewName, 2, wxALIGN_CENTER_VERTICAL, 0);
   newNameSizer->SetItemMinSize(myNewName, size);

   sizerRename->Add(labelTop, 0, wxALIGN_CENTER_HORIZONTAL, 5);
   sizerRename->Add(newNameSizer, 1, wxGROW | wxALL, 5);

   myNewName->SetFocus();
        
   // OK/Cancel button
   wxBoxSizer *sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   myButton = new wxButton(this, wxID_OK, _("OK"));
   myButton->SetDefault();
   wxButton* cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
   sizerConfirm->Add(myButton, 0, wxGROW | wxALL, 5);
   sizerConfirm->Add(cancel, 0, wxGROW | wxALL, 5);
    
   // Main box with everything in it
   wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(sizerRename, 0, wxGROW | wxALL, 8);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxALL, 0);
        
   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);

   SetTortoiseDialogPos(this, GetRemoteHandle());
}
