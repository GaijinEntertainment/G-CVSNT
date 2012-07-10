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


#include "StdAfx.h"
#include "BranchTagDlg.h"
#include "WxwHelpers.h"
#include "ExtComboBox.h"
#include "ExtStaticText.h"
#include "MessageDialog.h"
#include "TagValidator.h"
#include <Utils/StringUtils.h>
#include <Utils/Translate.h>
#include <Utils/TortoiseDebug.h>
#include <CVSGlue/RemoteLists.h>

//static
bool DoTagDialog(wxWindow* parent,
                 std::string& tagname,
                 bool& checkunmodified,
                 int& action,
                 const std::vector<std::string>& dirs)
{
   BranchTagDlg* dlg = new BranchTagDlg(parent, BranchTagDlg::modeTag, tagname, dirs);
   bool ret = (dlg->ShowModal() == wxID_OK);
        
   if (ret)
   {
      checkunmodified = dlg->myCheckUnmodifiedButton->GetValue();
      action = dlg->myAction;
      tagname = wxAscii(dlg->myTagEntry->GetValue().c_str());
   }
        
   DestroyDialog(dlg);
   return ret;
}

//static
bool DoBranchDialog(wxWindow* parent, 
                    std::string& branchname, 
                    bool& checkunmodified, 
                    int& action,
                    const std::vector<std::string>& dirs)
{
   BranchTagDlg* dlg = new BranchTagDlg(parent, BranchTagDlg::modeBranch, branchname, dirs);
   bool ret = (dlg->ShowModal() == wxID_OK);
        
   if (ret)
   {
      checkunmodified = dlg->myCheckUnmodifiedButton->GetValue();
      action = dlg->myAction;
      branchname = wxAscii(dlg->myTagEntry->GetValue().c_str());
   }
        
   DestroyDialog(dlg);
   return ret;
}

enum
{
   BRANCHTAGDLG_ID_FETCHTAGLIST = 10001,
   BRANCHTAGDLG_ID_CREATE,
   BRANCHTAGDLG_ID_MOVE,
   BRANCHTAGDLG_ID_DELETE
};

BEGIN_EVENT_TABLE(BranchTagDlg, ExtDialog)
   EVT_COMMAND(BRANCHTAGDLG_ID_FETCHTAGLIST, wxEVT_COMMAND_BUTTON_CLICKED,       BranchTagDlg::FetchTagBranchList)
   EVT_COMMAND(BRANCHTAGDLG_ID_CREATE,       wxEVT_COMMAND_RADIOBUTTON_SELECTED, BranchTagDlg::CreateClicked)
   EVT_COMMAND(BRANCHTAGDLG_ID_MOVE,         wxEVT_COMMAND_RADIOBUTTON_SELECTED, BranchTagDlg::MoveClicked)
   EVT_COMMAND(BRANCHTAGDLG_ID_DELETE,       wxEVT_COMMAND_RADIOBUTTON_SELECTED, BranchTagDlg::DeleteClicked)
END_EVENT_TABLE()


BranchTagDlg::BranchTagDlg(wxWindow* parent, int mode, const std::string& tagname, 
                           const std::vector<std::string>& dirs)
   : ExtDialog(parent, -1,
               wxString(mode == modeTag ? _("TortoiseCVS - Tag") : _("TortoiseCVS - Create Branch")),
               wxDefaultPosition, wxSize(400,300), wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN),
     myMode(mode),
     myDirs(dirs)
{
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

   myAction = TagActionCreate;
   // Top part with tag entry field
   wxBoxSizer* sizerTagEntry = new wxBoxSizer(wxHORIZONTAL);
   wxStaticText* label = new wxStaticText(this, -1, mode == modeTag ? _("&Tag:") : _("New &branch name:"));
   TagValidator tagValidator;
   myTagEntry = new ExtComboBox(this, -1, wxText(tagname), wxDefaultPosition, wxDefaultSize,
                                0, 0, 0, tagValidator);
   FetchCachedTagBranchList();
   myTagEntry->SetToolTip(mode == modeTag ? _("Name of tag") :  _("Name of branch") );
   wxBoxSizer* fetchListSizer = new wxBoxSizer(wxHORIZONTAL);
   myFetchTagBranchList = new wxButton(this, BRANCHTAGDLG_ID_FETCHTAGLIST, _("&Update list..."));
   myFetchRecursive = new wxCheckBox(this, -1, _("&Scan subfolders") );
   fetchListSizer->Add(myTagEntry, 2, wxALIGN_CENTER_VERTICAL, 0);
   fetchListSizer->Add(myFetchTagBranchList, 0, wxLEFT, 5);
   const wxChar* fetchTip = _("Retrieves a list of tags and branches from the server, and puts them in the dropdown box");
   myFetchTagBranchList->SetToolTip(fetchTip);

   sizerTagEntry->Add(label, 0, wxALIGN_CENTER_VERTICAL, 5);
   sizerTagEntry->Add(fetchListSizer, 1, wxGROW | wxALL, 5);
   sizerTagEntry->Add(myFetchRecursive, 0, wxALIGN_CENTER_VERTICAL, 0);
   myTagEntry->SetFocus();
        
   // Check box
   myCheckUnmodifiedButton = new wxCheckBox(this, -1, _("Check that the &files are unmodified"));
   myCheckUnmodifiedButton->SetValue(true);
   myCheckUnmodifiedButton->SetToolTip(_("Ensures there are no uncommitted changes before performing the operation"));
        
   // information text
   wxString inf;
   if (mode == modeTag)
   {
      // Radio buttons
      myRadioCreate = new wxRadioButton(this, BRANCHTAGDLG_ID_CREATE, _("&Create new tag"));
      myRadioMove   = new wxRadioButton(this, BRANCHTAGDLG_ID_MOVE, _("&Move existing tag"));
      myRadioDelete = new wxRadioButton(this, BRANCHTAGDLG_ID_DELETE, _("&Delete existing tag"));
      myRadioCreate->SetValue(true);
      
      inf = _("The tag will be assigned to the repository versions nearest to your working sources. This happens directly to the repository.");
      inf += wxT("\n");
   }
   else
   {
      // Radio buttons
      myRadioCreate = new wxRadioButton(this, BRANCHTAGDLG_ID_CREATE, _("&Create new branch"));
      myRadioMove   = new wxRadioButton(this, BRANCHTAGDLG_ID_MOVE, _("&Move existing branch"));
      myRadioDelete = new wxRadioButton(this, BRANCHTAGDLG_ID_DELETE, _("&Delete existing branch"));
      myRadioCreate->SetValue(true);
      
      inf = _(
"The branch will be applied to the repository versions nearest to your working \
sources. This happens directly to the repository, leaving your working sources on \
the original branch.");
      inf += wxT("\n");
   }
        
   inf += wxT("\n");
   inf += _(
"Tag and branch names can contain letters, digits, \"-\" (dash) and \"_\" (underscore) \
only. In particular, this means no dots, and no spaces.\nThey must also start with a letter.");
        
   wxStaticText* infoText = new ExtStaticText(this, -1, inf,
                                              wxDefaultPosition, 
                                              wxDLG_UNIT(this, wxSize(60, 10)));
        
   // OK/Cancel button
   wxBoxSizer* sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   wxButton* ok = new wxButton(this, wxID_OK, _("OK"));
   ok->SetDefault();
   wxButton* cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
   sizerConfirm->Add(ok, 0, wxGROW | wxALL, 5);
   sizerConfirm->Add(cancel, 0, wxGROW | wxALL, 5);
    
   // Main box with everything in it
   wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(sizerTagEntry, 0, wxGROW | wxALL, 8);
   wxBoxSizer* sizerHorz = new wxBoxSizer(wxHORIZONTAL);
   wxBoxSizer* sizerLeft = new wxBoxSizer(wxVERTICAL);
   sizerLeft->Add(myRadioCreate, 0, wxALL, 4);
   sizerLeft->Add(myRadioMove, 0, wxALL, 4);
   sizerLeft->Add(myRadioDelete, 0, wxALL, 4);
   wxBoxSizer* sizerRight = new wxBoxSizer(wxVERTICAL);
   sizerRight->Add(myCheckUnmodifiedButton, 0, wxALL, 4);
   sizerHorz->Add(sizerLeft, 0, wxGROW | wxLEFT | wxRIGHT, 4);
   sizerHorz->Add(32, 0);
   sizerHorz->Add(sizerRight, 0, wxGROW | wxLEFT | wxRIGHT, 4);
   sizerTop->Add(sizerHorz, 0, wxGROW | wxALL, 0);
   sizerTop->Add(infoText, 0, wxGROW | wxALL, 8);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxALL, 0);
        
   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->SetItemMinSize(infoText, infoText->GetMinSize().GetWidth(), 
                            infoText->GetMinSize().GetHeight());
   sizerTop->Fit(this);

   SetTortoiseDialogPos(this, GetRemoteHandle());
}

void BranchTagDlg::CreateClicked(wxCommandEvent&)
{
   myAction = TagActionCreate;
}

void BranchTagDlg::MoveClicked(wxCommandEvent&)
{
   myAction = TagActionMove;
}

void BranchTagDlg::DeleteClicked(wxCommandEvent&)
{
   myAction = TagActionDelete;
}

// Use various heuristics to get a list of modules on the server
void BranchTagDlg::FetchTagBranchList(wxCommandEvent&)
{
   myFetchTagBranchList->Enable(false);

   // Get module list and put it in the dialog
   std::vector<std::string> tags, branches;

   RemoteLists::GetTagBranchList(this, tags, branches, myDirs, myFetchRecursive->GetValue());
   
   myTagEntry->Clear();

   if (myMode == modeTag)
   {
      if (!tags.empty())
         FillTagBranchComboBox(myTagEntry, tags);
      else
      {
          wxString s(_("No tags found."));
          s += wxT("\n\n");
          s += wxText(SerializeStringVector(myDirs, "\n"));
          DoMessageDialog(this, s);
      }
   }
   else
   {
      FillTagBranchComboBox(myTagEntry, branches);
   }
      
   myFetchTagBranchList->Enable(true);
}


// Use various heuristics to get a list of modules on the server
void BranchTagDlg::FetchCachedTagBranchList()
{
   std::vector<std::string> tags;
   std::vector<std::string> branches;
   RemoteLists::GetCachedTagBranchList(tags, branches, myDirs);
   wxString s = myTagEntry->GetValue();

   myTagEntry->Clear();

   if (myMode == modeTag)
   {
      // Remove duplicates
      std::sort(tags.begin(), tags.end());
      tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
      FillTagBranchComboBox(myTagEntry, tags);
   }
   else
   {
      // Remove duplicates
      std::sort(branches.begin(), branches.end());
      branches.erase(std::unique(branches.begin(), branches.end()), branches.end());
      FillTagBranchComboBox(myTagEntry, branches);
   }
      
   myTagEntry->SetValue(s);
}
