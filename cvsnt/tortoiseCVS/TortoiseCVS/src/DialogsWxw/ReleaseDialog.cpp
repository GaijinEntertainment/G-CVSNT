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


#include "StdAfx.h"

#include "ReleaseDialog.h"
#include "MessageDialog.h"
#include "YesNoDialog.h"
#include "WxwHelpers.h"
#include "../Utils/StringUtils.h"
#include "ExtComboBox.h"
#include "../CVSGlue/CVSAction.h"

// static
bool DoReleaseDialog(wxWindow* parent, const std::vector<std::string>& files)
{
   bool bResult = false;
   ReleaseDialog* dlg = new ReleaseDialog(parent);
   if (dlg->ShowModal() == wxID_OK)
   {
      CVSAction glue(dlg);
      wxString sDir;
      if (files.size() == 1) 
         sDir = wxText(files[0]);
      else
         sDir = _("multiple folders");
      glue.SetProgressFinishedCaption(Printf(_("Finished release in %s"), sDir.c_str()));

      for (unsigned int j = 0; j < files.size(); ++j)
      {
         MakeArgs args;
         args.add_global_option("-Q");
         args.add_option("release");

         switch (dlg->myReleaseOptionsChoice->GetSelection())
         {
         case 0:
            args.add_option( "-d" );
            break;
         case 1:
            args.add_option( "-d" );
            args.add_option( "-f" );
            break;
         case 2:
            args.add_option( "-e" );
            break;
         }

         if (dlg->myReleaseOptionsChoice->GetSelection() != 2)
         {
            std::vector<std::string> modified, added, removed, renamed, unmodified, needsAdding;
            std::vector<bool> ignored;

            CVSStatus::RecursiveScan(files[j], &modified, &added, &removed, &renamed, &unmodified, &modified);
            if (dlg->myReleaseOptionsChoice->GetSelection() == 1)
               CVSStatus::RecursiveNeedsAdding(files[j], needsAdding, ignored);
            if (!modified.empty() || !added.empty() || !removed.empty() || !renamed.empty() || !needsAdding.empty())
            {
               wxString s = Printf(_("CVS Release in %s will result in the following potentially destructive changes:"), 
                                   wxText(files[j]).c_str()) + wxT("\n\n");
               if (modified.size())
                  s = s + wxT("\t") + Printf(_("%d modified file(s)"), modified.size()) + wxT("\n");
               
               if (added.size())
                  s = s + wxT("\t") + Printf(_("%d newly added file(s) that have not been committed"), 
                                             added.size()) + wxT("\n");
               
               if (removed.size())
                  s = s + wxT("\t") + Printf(_("%d removed file(s) that have not been committed"), 
                                             removed.size()) + wxT("\n");
               
               if (renamed.size())
                  s = s + wxT("\t") + Printf(_("%d renamed file(s) that have not been committed"), 
                                             renamed.size()) + wxT("\n");
               
               if (needsAdding.size())
                  s = s + wxT("\t") + Printf(_("%d file(s) that are not under CVS control"), 
                                             needsAdding.size()) + wxT("\n");

               s = Printf(_("%s will be deleted.  Are you sure you want to do this?"), s.c_str());
               if (!DoYesNoDialog(dlg, s, false))
                  continue;
            }
         }
         
         args.add_option(ExtractLastPart(files[j]));
      
         CVSRoot cvsRoot;
         cvsRoot.SetCVSROOT(CVSStatus::CVSRootForPath(files[j]));
      
         glue.SetCVSRoot(cvsRoot);
         glue.SetProgressCaption(_("Releasing in ") + wxText(files[j]));
         glue.Command(GetDirectoryAbove(files[j]), args);
      }
      bResult = glue.AtLeastOneSuccess() != 0;
   }

   DestroyDialog(dlg);
   return bResult;
}


ReleaseDialog::ReleaseDialog(wxWindow* parent)
   : ExtDialog(parent, -1,
               _("TortoiseCVS - Release"),
               wxDefaultPosition, wxDefaultSize, 
               wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN)
{
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

   wxStaticText* label1 = new wxStaticText(this, -1, _("Release Options:"));

   const int releaseOptionCount = 3;
   wxString releaseOptions[releaseOptionCount] = 
   {
      _("Delete files and folders that are under CVS control (-d)"),
      _("Delete contents of folders including non-cvs files (-d -f)"),
      _("Delete CVS control files only (-e)")
   };

   myReleaseOptionsChoice = new wxChoice(this, -1, wxDefaultPosition, wxDefaultSize, releaseOptionCount, releaseOptions);
   myReleaseOptionsChoice->SetSelection( 0 );

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
   sizerTop->Add(myReleaseOptionsChoice, 0, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL , 5);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxALL, 0);

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);

   SetTortoiseDialogPos(this, GetRemoteHandle());
}
