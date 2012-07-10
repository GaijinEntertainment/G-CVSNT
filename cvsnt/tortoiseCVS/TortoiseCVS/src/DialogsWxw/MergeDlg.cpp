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
#include <algorithm>
#include "ExtTextCtrl.h"
#include "MergeDlg.h"
#include "WxwHelpers.h"
#include "../Utils/Translate.h"
#include "../Utils/StringUtils.h"
#include "../CVSGlue/RemoteLists.h"
#include "../CVSGlue/CVSStatus.h"
#include <Utils/TortoiseRegistry.h>
#include "MessageDialog.h"
#include "ExtComboBox.h"
#include "ExtStaticText.h"
#include "../Utils/StringUtils.h"


//static
bool DoMergeDialog(wxWindow* parent, std::string& fromtag1, 
                   std::string& fromtag2, std::string& bugnumber, 
                   const std::vector<std::string>& dirs,
                   bool gotFolder, bool& createdirs,
                   bool& suppressKeywordExpansion)
{
   MergeDlg* dlg = new MergeDlg(parent, dirs, gotFolder);
   if (gotFolder)
      dlg->myCreateDirsCheckbox->SetValue(createdirs);

   bool ret = (dlg->ShowModal() == wxID_OK);
   
   if (ret)
   {
      fromtag1 = wxAscii(dlg->myFromEntry1->GetValue().c_str());
      fromtag2 = wxAscii(dlg->myFromEntry2->GetValue().c_str());
      bugnumber = TrimRight(wxAscii(dlg->myBugNumber->GetValue().c_str()));
      TortoiseRegistry::WriteString("Dialogs\\Commit\\Bug Number", bugnumber);
      if (gotFolder)
         createdirs = dlg->myCreateDirsCheckbox->GetValue();
      suppressKeywordExpansion = dlg->mySuppressKeywordExpansion->GetValue();
   }
   
   DestroyDialog(dlg);
   return ret;
}


enum
{
   MERGEDLG_ID_FETCHTAGLIST = 10001
};

BEGIN_EVENT_TABLE(MergeDlg, ExtDialog)
   EVT_COMMAND(MERGEDLG_ID_FETCHTAGLIST, wxEVT_COMMAND_BUTTON_CLICKED, MergeDlg::FetchTagBranchList)
END_EVENT_TABLE()


MergeDlg::MergeDlg(wxWindow* parent, const std::vector<std::string>& dirs,
                   bool gotFolder)
   : ExtDialog(parent, -1, wxString( _("TortoiseCVS - Merge") ),
               wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | 
               wxCLIP_CHILDREN),
     myDirs(dirs)
{
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));
   SetSize(wxDLG_UNIT(this, wxSize(300, 240)));
   
   // Top part with tag entry field

   wxFlexGridSizer *sizerTags = new wxFlexGridSizer(3, 2, 5, 5);
   sizerTags->AddGrowableCol(1);
   
   wxStaticText *label = new wxStaticText(this, -1, _("Start:"));
   sizerTags->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

   myFromEntry1 = new ExtComboBox(this, -1, wxT(""), wxDefaultPosition, wxDefaultSize,
                                  0, 0, 0);
   sizerTags->Add(myFromEntry1, 2, wxEXPAND | wxLEFT | wxRIGHT, 0);

   wxStaticText *label2 = new wxStaticText(this, -1, _("End:"));
   sizerTags->Add(label2, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

   myFromEntry2 = new ExtComboBox(this, -1, wxT(""), wxDefaultPosition, wxDefaultSize,
                                  0, 0, 0);
   sizerTags->Add(myFromEntry2, 2, wxEXPAND | wxLEFT | wxRIGHT, 0);

   wxStaticText *label4 = new wxStaticText(this, -1, _("Bug:"));
   sizerTags->Add(label4, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
 
   myBugNumber = new ExtTextCtrl(this, -1, wxT(""), wxDefaultPosition,
                                wxDefaultSize, wxTE_RICH2);
   myBugNumber->SetPlainText(true);
   myBugNumber->SetMaxLength(100);
   myBugNumber->SetFocus();
   myBugNumber->SetSelection(-1, -1);
   std::string defaultbugnumber = TortoiseRegistry::ReadString("Dialogs\\Commit\\Bug Number");
   myBugNumber->SetValue(wxText(defaultbugnumber));
   sizerTags->Add(myBugNumber, 2, wxEXPAND | wxLEFT | wxRIGHT, 0);
 
   FetchCachedTagBranchList();

   myFromEntry1->SetToolTip( _("Name of revision/tag/branch to merge into your working folder."));
   myFromEntry2->SetToolTip( _("Name of revision/tag/branch to merge into your working folder."));
   myBugNumber->SetToolTip(_("Restrict merge based on bug number."));

   wxStaticText *dummy = new wxStaticText(this, -1, wxT(""));
   sizerTags->Add(dummy, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

   wxBoxSizer *fetchListSizer = new wxBoxSizer(wxHORIZONTAL);
   myFetchTagBranchList = new wxButton(this, MERGEDLG_ID_FETCHTAGLIST, _("&Update lists..."));
   myFetchRecursive = new wxCheckBox(this, -1, _("&Scan subfolders") );

   mySuppressKeywordExpansion = new wxCheckBox(this, -1, _("Suppress keyword &expansion"),
                                               wxDefaultPosition, wxDefaultSize, 0);
   
   fetchListSizer->Add(myFetchTagBranchList, 0, wxRIGHT, 5);  
   fetchListSizer->Add(myFetchRecursive, 0, wxALIGN_CENTER_VERTICAL, 0);
   sizerTags->Add(fetchListSizer, 0, wxALIGN_LEFT | wxALL, 0);

   wxString fetchTip(_("Retrieves a list of tags and branches from the server, and puts them in the dropdown box"));
   myFetchTagBranchList->SetToolTip(fetchTip);

   wxString suppressTip(_("Suppress expansion of keywords to avoid spurious conflicts"));
   mySuppressKeywordExpansion->SetToolTip(suppressTip);

   myFromEntry1->SetFocus();

   wxBoxSizer* sizerCreateDirs = 0;
   if (gotFolder)
   {
       sizerCreateDirs = new wxBoxSizer(wxHORIZONTAL);
       myCreateDirsCheckbox = new wxCheckBox(this, -1,
                                             _("Create missing subfolders"),
                                             wxDefaultPosition, wxDefaultSize, 0);
       myCreateDirsCheckbox->SetToolTip(_("Create any folders that exist in the repository if they are missing from the working folder."));
       sizerCreateDirs->Add(myCreateDirsCheckbox, 0, wxALL, 5);
   }
   wxBoxSizer* sizerSuppress = new wxBoxSizer(wxHORIZONTAL);
   sizerSuppress->Add(mySuppressKeywordExpansion, 0, wxALL, 5);
   
    // information text
    std::string inf;
    wxStaticText* infoText = new ExtStaticText(this, -1, _(
"The changes between the specified revisions/tags/branches will be merged into \
your current working sources. If you want to merge an entire branch, enter the \
branch name in the first edit field and leave the second one blank.\n\
The repository is not altered until a commit is performed."),
                                               wxDefaultPosition, wxDLG_UNIT(this, wxSize(60, 10)));
    
    // OK/Cancel button
    wxBoxSizer *sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
    wxButton* ok = new wxButton(this, wxID_OK, _("OK"));
    ok->SetDefault();
    wxButton* cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
    sizerConfirm->Add(ok, 0, wxGROW | wxALL, 5);
    sizerConfirm->Add(cancel, 0, wxGROW | wxALL, 5);
   
    // Main box with everything in it
    wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);
    wxStaticText *label3 = new wxStaticText(this, -1, 
                                            _("Merge changes between the following branches/tags/revisions:"));
    sizerTop->Add(label3, 0, wxALIGN_LEFT | wxALL, 8);
    sizerTop->Add(sizerTags, 0, wxGROW | wxALL, 3);
    if (gotFolder)
       sizerTop->Add(sizerCreateDirs, 0, wxGROW | wxALL, 3);
    sizerTop->Add(sizerSuppress, 0, wxGROW | wxALL, 3);
    sizerTop->Add(infoText, 0, wxGROW | wxALL, 5);
    sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxTOP, 5);
   
    // Overall dialog layout settings
    SetAutoLayout(TRUE);
    SetSizer(sizerTop);
    sizerTop->SetSizeHints(this);
    sizerTop->SetItemMinSize(infoText, infoText->GetMinSize().GetWidth(), infoText->GetMinSize().GetHeight());
    sizerTop->Fit(this);

    SetTortoiseDialogPos(this, GetRemoteHandle());
}

void MergeDlg::FetchTagBranchList(wxCommandEvent& WXUNUSED(event))
{
   myFetchTagBranchList->Enable(false);

   std::vector<std::string> tagbranch;
   RemoteLists::GetTagBranchList(this, tagbranch, tagbranch, myDirs, myFetchRecursive->GetValue());
   std::sort(tagbranch.begin(), tagbranch.end());
   tagbranch.insert(tagbranch.begin(), "HEAD");

   if (!tagbranch.empty())
   {
      FillTagBranchComboBox(myFromEntry1, tagbranch);
      FillTagBranchComboBox(myFromEntry2, tagbranch);
   }
   else   
   {
       wxString s(_("No tags or branches found."));
       s += wxT("\n\n");
       s += wxText(SerializeStringVector(myDirs, "\n"));
      DoMessageDialog(this, s);
   }

   myFetchTagBranchList->Enable(true);
}


void MergeDlg::FetchCachedTagBranchList()
{
   std::vector<std::string> tags, branches, tagbranch;
   RemoteLists::GetCachedTagBranchList(tags, branches, myDirs);

   tagbranch.insert(tagbranch.end(), tags.begin(), tags.end());
   tagbranch.insert(tagbranch.end(), branches.begin(), branches.end());

   // Remove duplicates
   std::sort(tagbranch.begin(), tagbranch.end());
   tagbranch.erase(std::unique(tagbranch.begin(), tagbranch.end()), tagbranch.end());
   tagbranch.insert(tagbranch.begin(), "HEAD");

   wxString s1 = myFromEntry1->GetValue();
   wxString s2 = myFromEntry2->GetValue();

   myFromEntry1->Clear();
   myFromEntry2->Clear();

   FillTagBranchComboBox(myFromEntry1, tagbranch);
   FillTagBranchComboBox(myFromEntry2, tagbranch);
      
   myFromEntry1->SetValue(s1.c_str());
   myFromEntry2->SetValue(s2.c_str());
}
