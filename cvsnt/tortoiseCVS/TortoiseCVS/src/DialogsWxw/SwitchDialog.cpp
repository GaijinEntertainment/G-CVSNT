// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2003 - keith d. zimmerman
// <keith@kdz13.net> - August 2003

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

// SwitchDialog - contributed by March Hare Software Ltd 2009


#include "StdAfx.h"

#include "MessageDialog.h"
#include "YesNoDialog.h"
#include "WxwHelpers.h"
#include "../Utils/StringUtils.h"
#include "ExtTextCtrl.h"
#include "wxInitialTipTextCtrl.h"
#include "../CVSGlue/CVSAction.h"
#include "SwitchDialog.h"

enum
{
   MODBASICS_ID_CVSROOTTEXT = 10001,
};

// static
bool DoSwitchDialog(wxWindow* parent, const std::vector<std::string>& files)
{
   bool bResult = false;
   SwitchDialog* dlg = new SwitchDialog(parent);
   if (dlg->ShowModal() == wxID_OK)
   {
      CVSAction glue(dlg);
      wxString sDir;
      if (files.size() == 1) 
         sDir = wxText(files[0]);
      else
         sDir = _("multiple folders");
      glue.SetProgressFinishedCaption(Printf(_("Finished switch in %s"), sDir.c_str()));

      for (unsigned int j = 0; j < files.size(); ++j)
      {
         MakeArgs args;
         args.add_global_option("-Q");
         args.add_option("switch");
         args.add_option("-f");

         std::string newcvsroot = wxAscii(dlg->myCVSROOTText->GetValue().c_str());
		 args.add_option(newcvsroot.c_str());

         CVSRoot cvsRoot;
         cvsRoot.SetCVSROOT(CVSStatus::CVSRootForPath(files[j]));
      
         glue.SetCVSRoot(cvsRoot);
         glue.SetProgressCaption(_("Switching in ") + wxText(files[j]));
         glue.Command(files[j], args);
      }
      bResult = glue.AtLeastOneSuccess() != 0;
   }

   DestroyDialog(dlg);
   return bResult;
}


SwitchDialog::SwitchDialog(wxWindow* parent)
   : ExtDialog(parent, -1,
               _("TortoiseCVS - Switch"),
               wxDefaultPosition, wxDefaultSize, 
               wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN)
{
   // Tooltips
   const wxChar* tip1 = _("Technical details of the CVSROOT which TortoiseCVS is going to use");

   // Create the dialog
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

   wxStaticText* label1 = new wxStaticText(this, -1, wxString(_("CVSROOT")) + _(":"));

   myCVSROOTText = new wxInitialTipTextCtrl(this, MODBASICS_ID_CVSROOTTEXT);

   // OK/Cancel button
   wxBoxSizer* sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   wxButton* ok = new wxButton(this, wxID_OK, _("OK"));
   ok->SetDefault();
   wxButton* cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
   sizerConfirm->Add(ok, 0, wxGROW | wxALL, 5); 
   sizerConfirm->Add(cancel, 0, wxGROW | wxALL, 5);

   // Main box with everything in it
   wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(label1, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
   sizerTop->Add(myCVSROOTText, 1, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxALL, 0);

   myCVSROOTText->SetToolTip(tip1);
   label1->SetToolTip(tip1);
   myCVSROOTText->SetInitialTip(_("Click here and paste in the CVSROOT"));

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);

   SetTortoiseDialogPos(this, GetRemoteHandle());
}

