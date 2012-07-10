// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2004 - Torsten Martinsen
// <torsten@tiscali.dk> - October 2004

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
#include "CommandDialog.h"
#include "WxwHelpers.h"
#include "ExtComboBox.h"
#include "ExtStaticText.h"
#include "MessageDialog.h"
#include "../Utils/StringUtils.h"
#include "../Utils/Translate.h"
#include "../Utils/StringUtils.h"
#include "../Utils/TortoiseRegistry.h"
#include "../CVSGlue/RemoteLists.h"

bool DoCommandDialog(wxWindow* parent,
                     const std::vector<std::string>& dirs,
                     std::string& command)
{
   CommandDlg* dlg = new CommandDlg(parent, dirs);
   bool ret = (dlg->ShowModal() == wxID_OK);
        
   if (ret)
   {
      command = wxAscii(dlg->myCommandCombo->GetValue().c_str());
      TortoiseRegistry::InsertFrontVector("History\\Commands", command, 20);
   }
        
   DestroyDialog(dlg);
   return ret;
}


CommandDlg::CommandDlg(wxWindow* parent, const std::vector<std::string>& dirs)
   : ExtDialog(parent, -1,
               wxString(_("TortoiseCVS - Command")),
               wxDefaultPosition, wxSize(400,300), wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN),
     myDirs(dirs)
{
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));
        
   // Top part with command entry field
   wxBoxSizer* sizerCommandEntry= new wxBoxSizer(wxHORIZONTAL);
   wxString msg = _("Command");
   msg += _(":");
   wxStaticText* label = new wxStaticText(this, -1, msg);
   myCommandCombo = new ExtComboBox(this, -1, wxT(""));
   myCommandCombo->SetToolTip(_("CVS command"));
   // Read list from registry
   std::vector<std::string> history;
   TortoiseRegistry::ReadVector("History\\Commands", history);
   for (std::vector<std::string>::iterator it = history.begin(); it != history.end(); ++it)
      myCommandCombo->Append(wxText(*it));

   sizerCommandEntry->Add(label, 0, wxALIGN_CENTER_VERTICAL, 5);
   sizerCommandEntry->Add(myCommandCombo, 1, wxGROW | wxALL, 5);
   myCommandCombo->SetFocus();
        
   // OK/Cancel button
   wxBoxSizer *sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   wxButton* ok = new wxButton(this, wxID_OK, _("OK"));
   ok->SetDefault();
   wxButton* cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
   sizerConfirm->Add(ok, 0, wxGROW | wxALL, 5);
   sizerConfirm->Add(cancel, 0, wxGROW | wxALL, 5);
    
   // Main box with everything in it
   wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(sizerCommandEntry, 0, wxGROW | wxALL, 8);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxALL, 0);
        
   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);

   SetTortoiseDialogPos(this, GetRemoteHandle());
}
