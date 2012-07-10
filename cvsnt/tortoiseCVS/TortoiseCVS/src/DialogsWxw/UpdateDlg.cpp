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
#include "UpdateDlg.h"
#include "WxwHelpers.h"
#include "MessageDialog.h"
#include "ExtComboBox.h"
#include "TagValidator.h"
#include <wx/statline.h>
#include <wx/notebook.h>
#include "../Utils/Translate.h"
#include "../CVSGlue/RemoteLists.h"
#include "../CVSGlue/CVSStatus.h"
#include "../Utils/StringUtils.h"

class UpdateBasicsPage : public wxPanel
{
public:
   UpdateBasicsPage(wxWindow* parent,
                    const std::vector<std::string>& dirs,
                    const std::string& initialTag);
   
   void RefreshRevision(wxCommandEvent& event);
   void RefreshDate(wxCommandEvent& event);
   void UpdateCalendar(wxCalendarEvent& event);
   void FetchTagBranchList(wxCommandEvent& event);
   void FetchCachedTagBranchList(std::string initialTag);

   void OKClicked();
   
   wxCheckBox*          myGetRevision;
   wxCheckBox*          myGetDate;
   wxCheckBox*          myCleanCheckbox;
   wxCheckBox*          myCreateDirsCheckbox;
   wxCheckBox*          myForceHeadCheckbox;
   wxComboBox*          myRevisionEntry;
   wxButton*            myFetchTagBranchList;
   wxCheckBox*          myFetchRecursive;
   wxComboBox*          myDateCombo;
   wxCalendarCtrl*      myCalendar;

   const std::vector<std::string>& myDirs;
   
   friend class UpdateDlg;
   
   DECLARE_EVENT_TABLE()
};

class UpdateOptions : public wxPanel
{
public:
   UpdateOptions(wxWindow* parent,
                 bool allowDirectory);
   
   wxCheckBox*  myUpdateDirectory;
   ExtComboBox* myDirectory;
   wxCheckBox*  mySimulate;
   wxCheckBox*  myPruneEmptyDirectories;
   wxCheckBox*  myRecurse;

   friend class UpdateDlg;
};


//static
bool DoUpdateDialog(wxWindow* parent, 
                    std::string& getrevision,
                    std::string& getdate,
                    bool& clean,
                    bool& createdirs,
                    bool& simulate,
                    bool& forceHead,
                    bool& pruneEmptyDirectories,
                    bool& recurse,
                    std::string* directory,
                    const std::vector<std::string>& dirs,
                    const std::string& initialTag)
{
   UpdateDlg* dlg = new UpdateDlg(parent,
                                  dirs,
                                  initialTag,
                                  createdirs,
                                  simulate,
                                  forceHead,
                                  pruneEmptyDirectories,
                                  recurse,
                                  directory ? true : false);
   bool ret = (dlg->ShowModal() == wxID_OK);
   if (ret)
   {
      dlg->myUpdateBasicsPage->OKClicked();
      getrevision = dlg->GetRevision();
      getdate = dlg->GetDate();
      clean = dlg->GetClean();
      createdirs = dlg->GetCreateDirs();
      simulate = dlg->GetSimulate();
      forceHead = dlg->GetForceHead();
      if (directory)
         *directory = dlg->GetDirectory();
      pruneEmptyDirectories = dlg->GetPruneEmptyDirectories();
      recurse = dlg->GetRecurse();
   }

   DestroyDialog(dlg);
   return ret;
}

enum 
{
   UPDATEDLG_ID_BOOK = 10001,
   UPDATEDLG_ID_GETREV,
   UPDATEDLG_ID_GETDATE,
   UPDATEDLG_ID_CALENDAR,
   UPDATEDLG_ID_FETCHTAGLIST,
   UPDATEDLG_ID_UPDATEDIRECTORY
};

BEGIN_EVENT_TABLE(UpdateBasicsPage, wxPanel)
   EVT_CHECKBOX(UPDATEDLG_ID_GETREV,                    UpdateBasicsPage::RefreshRevision)
   EVT_CHECKBOX(UPDATEDLG_ID_GETDATE,                   UpdateBasicsPage::RefreshDate)
   EVT_CALENDAR_SEL_CHANGED(UPDATEDLG_ID_CALENDAR,      UpdateBasicsPage::UpdateCalendar)
   EVT_BUTTON(UPDATEDLG_ID_FETCHTAGLIST,                UpdateBasicsPage::FetchTagBranchList)
END_EVENT_TABLE()
   
UpdateDlg::UpdateDlg(wxWindow* parent, const std::vector<std::string>& dirs, 
                     const std::string& initialTag,
                     bool& createdirs,
                     bool& simulate,
                     bool& forceHead,
                     bool& pruneEmptyDirectories,
                     bool& recurse,
                     bool allowDirectory)
   : ExtDialog(parent, -1,
               _("TortoiseCVS - Update"),
               wxDefaultPosition, wxDefaultSize, 
               wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN)
{
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));
   SetSize(wxDLG_UNIT(this, wxSize(300, 240)));

   // Main notebook
   wxNotebook* book = new wxNotebook(this, UPDATEDLG_ID_BOOK);
   myUpdateBasicsPage = new UpdateBasicsPage(book, dirs, initialTag);
   myUpdateOptions = new UpdateOptions(book, allowDirectory);

   myUpdateBasicsPage->myCreateDirsCheckbox->SetValue(createdirs);
   myUpdateBasicsPage->myForceHeadCheckbox->SetValue(forceHead);

   myUpdateOptions->mySimulate->SetValue(simulate);
   myUpdateOptions->myPruneEmptyDirectories->SetValue(pruneEmptyDirectories);
   myUpdateOptions->myRecurse->SetValue(recurse);

   book->AddPage(myUpdateBasicsPage, _("Basic"));
   book->AddPage(myUpdateOptions, _("Advanced"));
   
   // OK/Cancel button
   wxBoxSizer *sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   wxButton* ok = new wxButton(this, wxID_OK, _("OK"));
   ok->SetDefault();
   wxButton* cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
   sizerConfirm->Add(ok, 0, wxGROW | wxALL, 5);
   sizerConfirm->Add(cancel, 0, wxGROW | wxALL, 5);

   // Main box with text and button in it
   wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(book, 1, wxGROW | wxALL, 3);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxALL, 0);

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);

   wxCommandEvent event;
   myUpdateBasicsPage->RefreshRevision(event);
   myUpdateBasicsPage->RefreshDate(event);

   SetTortoiseDialogPos(this, GetRemoteHandle());
}


UpdateDlg::~UpdateDlg()
{
}


std::string UpdateDlg::GetDate() const
{
   if (myUpdateBasicsPage->myGetDate->GetValue())
   {
      return wxAscii(myUpdateBasicsPage->myDateCombo->GetValue().c_str());
   }
   return "";
}

std::string UpdateDlg::GetRevision() const
{
   std::string revision;
   if (myUpdateBasicsPage->myGetRevision->GetValue())
   {
      revision = wxAscii(myUpdateBasicsPage->myRevisionEntry->GetValue().c_str());
      Trim(revision);
   }
   return revision;
}

bool UpdateDlg::GetClean() const
{
   return myUpdateBasicsPage->myCleanCheckbox->GetValue();
}

bool UpdateDlg::GetCreateDirs() const
{
   return myUpdateBasicsPage->myCreateDirsCheckbox->GetValue();
}

bool UpdateDlg::GetSimulate() const
{
   return myUpdateOptions->mySimulate->GetValue();
}

bool UpdateDlg::GetForceHead() const
{
   return myUpdateBasicsPage->myForceHeadCheckbox->GetValue();
}

bool UpdateDlg::GetPruneEmptyDirectories() const
{
   return myUpdateOptions->myPruneEmptyDirectories->GetValue();
}

bool UpdateDlg::GetRecurse() const
{
   return myUpdateOptions->myRecurse->GetValue();
}

std::string UpdateDlg::GetDirectory() const
{
   std::string directory;
   if (myUpdateOptions->myUpdateDirectory->GetValue())
   {
      directory = wxAscii(myUpdateOptions->myDirectory->GetValue().c_str());
      Trim(directory);
   }
   return directory;
}


void UpdateBasicsPage::RefreshRevision(wxCommandEvent&)
{
   if (myGetRevision->GetValue())
   {
      myRevisionEntry->Enable(true);
      myFetchTagBranchList->Enable(true);
      myFetchRecursive->Enable(true);
      myRevisionEntry->SetFocus();
   }
   else
   {
      myRevisionEntry->Enable(false);
      myFetchTagBranchList->Enable(false);
      myFetchRecursive->Enable(false);
   }
}

void UpdateBasicsPage::RefreshDate(wxCommandEvent&)
{
   if (myGetDate->GetValue())
   {
      myDateCombo->Enable(true);
      myCalendar->Enable(true);
      myDateCombo->SetFocus();
   }
   else
   {
      myDateCombo->Enable(false);
      myCalendar->Enable(false);
   }
}

void UpdateBasicsPage::UpdateCalendar(wxCalendarEvent&)
{
   if (myGetDate->GetValue())
   {
       wxDateTime date = myCalendar->GetDate();
       wxString formatted = date.FormatISODate();
       myDateCombo->SetValue(formatted);
   }
}

void UpdateBasicsPage::OKClicked()
{
   WriteComboList(myDateCombo, "History\\Revision Date");
}

void UpdateBasicsPage::FetchTagBranchList(wxCommandEvent&)
{
   myFetchTagBranchList->Enable(false);

   std::vector<std::string> tagbranch;

   RemoteLists::GetTagBranchList(this, tagbranch, tagbranch, myDirs, myFetchRecursive->GetValue());
   tagbranch.insert(tagbranch.begin(), "HEAD");

   if (!tagbranch.empty())
      FillTagBranchComboBox(myRevisionEntry, tagbranch);
   else   
   {
      wxString msg(_("No tags or branches found."));
      msg += wxT("\n\n");
      msg += wxText(SerializeStringVector(myDirs, "\n"));
      DoMessageDialog(this, msg);
   }
      
   myFetchTagBranchList->Enable(true);
}



void UpdateBasicsPage::FetchCachedTagBranchList(std::string initialTag)
{
   std::vector<std::string> tags, branches, tagbranch;
   RemoteLists::GetCachedTagBranchList(tags, branches, myDirs);
   wxString s = myRevisionEntry->GetValue();

   tagbranch.insert(tagbranch.end(), tags.begin(), tags.end());
   tagbranch.insert(tagbranch.end(), branches.begin(), branches.end());
   if (!initialTag.empty())
   {
      tagbranch.push_back(initialTag);
   }
   // Remove duplicates
   std::sort(tagbranch.begin(), tagbranch.end());
   tagbranch.erase(std::unique(tagbranch.begin(), tagbranch.end()), tagbranch.end());
   tagbranch.insert(tagbranch.begin(), "HEAD");

   myRevisionEntry->Clear();
   FillTagBranchComboBox(myRevisionEntry, tagbranch);

   myRevisionEntry->SetValue(s);
}

UpdateBasicsPage::UpdateBasicsPage(wxWindow* parent,
                                   const std::vector<std::string>& dirs,
                                   const std::string& initialTag)
   : wxPanel(parent),
     myDirs(dirs)
{
   wxFlexGridSizer* gridSizer = new wxFlexGridSizer(2, 0, 0);
   wxStaticText* dummy = new wxStaticText(this, -1, wxT(""));
   myGetRevision = new wxCheckBox(this, UPDATEDLG_ID_GETREV,
                                  _("Get &tag/branch/revision:"));
   myGetRevision->SetToolTip(_(
"Switches your working copy to a specific branch/tag/revision. \
Implies stickiness, so subsequent updates, commits etc. will operate on that version."));
   TagValidator tagValidator(true);
   myRevisionEntry = new ExtComboBox(this, -1, wxT(""), wxDefaultPosition, wxDefaultSize,
                                     0, 0, 0, tagValidator);
   FetchCachedTagBranchList(initialTag);
   if (!initialTag.empty())
       myRevisionEntry->SetSelection(myRevisionEntry->FindString(wxText(initialTag)));
   else
       myRevisionEntry->SetSelection(myRevisionEntry->FindString(wxT("HEAD")));

   myRevisionEntry->SetToolTip(_("Branch/Tag/Revision specifier"));
   gridSizer->Add(myGetRevision, 0, wxGROW | wxALL, 5);
   gridSizer->Add(myRevisionEntry, 1, wxGROW | wxALL, 5);

   wxBoxSizer* fetchListSizer = new wxBoxSizer(wxHORIZONTAL);

   myFetchTagBranchList = new wxButton(this, UPDATEDLG_ID_FETCHTAGLIST, _("&Update list..."));
   const wxChar* fetchTip = _("Retrieves a list of tags and branches from the server, and puts them in the dropdown box");
   myFetchTagBranchList->SetToolTip(fetchTip);
   fetchListSizer->Add(myFetchTagBranchList, 0, wxGROW | wxLEFT, 5);  

   myFetchRecursive = new wxCheckBox(this, -1, _("Scan subdirs"));
   fetchListSizer->Add(myFetchRecursive, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT | wxLEFT, 5);  

   dummy = new wxStaticText(this, -1, wxT(""));
   gridSizer->Add(dummy, 0, wxGROW | wxALL, 5);

   gridSizer->Add(fetchListSizer, 0, wxGROW | wxLEFT, 0);

   myGetDate = new wxCheckBox(this, UPDATEDLG_ID_GETDATE, _("Get &date/time:"));
   myGetDate->SetToolTip(_(
"Switches your working copy to the latest version before a specific date. \
Implies stickiness, so subsequent updates, commits etc. will operate on that version."));
   const wxChar* dateTip = _(
"Choose a date from the calendar, and/or edit the date and time in the box above the calendar.\n\
e.g.: \"1988-09-24\"  \"yesterday\"  \"24 Sep 20:05\"");
   myDateCombo = MakeComboList(this, -1, "History\\Revision Date");
   gridSizer->Add(myGetDate, 0, wxGROW | wxALL, 5);
   gridSizer->Add(myDateCombo, 1, wxGROW | wxALL, 5);
   myDateCombo->SetToolTip(dateTip);
    
   myCalendar = new wxCalendarCtrl(this, UPDATEDLG_ID_CALENDAR, wxDateTime::Now());
   // Hack to fix broken Combobox height on Win2K
//   myCalendar->GetMonthControl()->SetSize(wxDefaultSize.x, 100);

   dummy = new wxStaticText(this, -1, wxT(""));
   gridSizer->Add(dummy, 0, wxGROW | wxALL, 5);
   gridSizer->Add(myCalendar, 1, wxGROW | wxALL, 5);
   myCalendar->SetToolTip(dateTip);
   dummy->SetToolTip(dateTip);    

   myGetRevision->SetValue(false);
   myGetDate->SetValue(false);

   myCleanCheckbox = new wxCheckBox( this, -1, _("&Clean copy - WARNING:  this deletes your changes"));
   myCleanCheckbox->SetToolTip(_(
"Overwrite any locally modified files during the update.\n\
They are backed up to .#file.revision in case you do this by mistake."));

   myCreateDirsCheckbox = new wxCheckBox(this, -1, _("Create missing &subfolders"));
   myCreateDirsCheckbox->SetToolTip(_(
      "Create any folders that exist in the repository if they are missing from the working folder."));
   
   myForceHeadCheckbox = new wxCheckBox(this, -1,
                                        _("If no &matching revision is found, use the most recent one"));
   myForceHeadCheckbox->SetToolTip(_(
"If you've specified a tag / branch / date, and it matches no revision for a file, \
CVS will get you the head revision instead."));
   
   // Main box with everything in it
   wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(gridSizer, 0, wxGROW | wxALL, 5);
   sizerTop->Add(new wxStaticLine( this, -1 ), 0, wxEXPAND | wxALL, 10);
   sizerTop->Add(myCleanCheckbox, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);
   sizerTop->Add(myCreateDirsCheckbox, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);
   sizerTop->Add(myForceHeadCheckbox, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);
   sizerTop->Add(new wxStaticLine( this, -1 ), 0, wxEXPAND | wxALL, 10);

   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
}


UpdateOptions::UpdateOptions(wxWindow* parent,
                             bool allowDirectory)
   : wxPanel(parent)
{
   wxFlexGridSizer* gridSizer = new wxFlexGridSizer(2, 0, 0);

   myUpdateDirectory = new wxCheckBox(this, UPDATEDLG_ID_UPDATEDIRECTORY, _("Get &folder:"));
   myUpdateDirectory->SetToolTip(_(
"Perform the update on the specified folder only. \
Useful for getting a single new folder without getting all other new folders."));
   myDirectory = new ExtComboBox(this, -1);
   gridSizer->Add(myUpdateDirectory, 0, wxGROW | wxALL, 5);
   gridSizer->Add(myDirectory, 1, wxGROW | wxALL, 5);

   if (!allowDirectory)
   {
      myUpdateDirectory->Disable();
      myDirectory->Disable();
   }

   mySimulate = new wxCheckBox(this, -1, _("&Simulate update"));
   mySimulate->SetToolTip(_("Do not execute anything that will change the disk."));

   myPruneEmptyDirectories = new wxCheckBox(this, -1, _("&Prune empty folders"));
   myPruneEmptyDirectories->SetToolTip(_("Delete any folders that do not contain files."));
   
   myRecurse = new wxCheckBox(this, -1, _("&Recurse"));
   myRecurse->SetToolTip(_("Update subfolders."));

   // Main box with everything in it
   wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(gridSizer, 0, wxGROW | wxALL, 5);
   sizerTop->Add(mySimulate, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);
   sizerTop->Add(myPruneEmptyDirectories, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);
   sizerTop->Add(myRecurse, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);

   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
}
