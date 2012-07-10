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

#include "RevOptions.h"

#include <time.h>
#include <wx/statline.h>
#include <algorithm>
#include "WxwHelpers.h"
#include "MessageDialog.h"
#include "ExtComboBox.h"
#include "TagValidator.h"
#include "../Utils/Translate.h"
#include "../Utils/StringUtils.h"
#include "../Utils/TortoiseException.h"
#include "../CVSGlue/RemoteLists.h"

#ifdef __BORLANDC__
   // Types required for time functions
   using std::time_t;
   using std::localtime;
#endif

enum
{
   REVOPTIONS_ID_HEAD = 11000,
   REVOPTIONS_ID_TAG,
   REVOPTIONS_ID_DATE,
   REVOPTIONS_ID_MOSTRECENT,
   REVOPTIONS_ID_CALENDAR,
   REVOPTIONS_ID_FETCHTAGLIST
};

BEGIN_EVENT_TABLE(RevOptions, wxPanel)
   EVT_RADIOBUTTON(REVOPTIONS_ID_HEAD,                  RevOptions::UpdateSensitivity)
   EVT_RADIOBUTTON(REVOPTIONS_ID_TAG,                   RevOptions::UpdateSensitivityTagSel)
   EVT_RADIOBUTTON(REVOPTIONS_ID_DATE,                  RevOptions::UpdateSensitivity)
   EVT_RADIOBUTTON(REVOPTIONS_ID_MOSTRECENT,            RevOptions::UpdateSensitivity)
   EVT_CALENDAR_SEL_CHANGED(REVOPTIONS_ID_CALENDAR,     RevOptions::UpdateCalendar)
   EVT_BUTTON(REVOPTIONS_ID_FETCHTAGLIST,               RevOptions::FetchTagBranchList)
END_EVENT_TABLE()

RevOptions::RevOptions(wxWindow* parent)
    : wxPanel(parent)
{
   wxStaticBox* staticBox = new wxStaticBox(this, -1, _("Branch or tag"));
   wxStaticBoxSizer* sizerStaticBox = new wxStaticBoxSizer(staticBox, wxVERTICAL);

   myHeadRadioButton = new wxRadioButton(this, REVOPTIONS_ID_HEAD, _("Use &HEAD branch"), wxDefaultPosition, 
                                         wxDefaultSize, wxRB_GROUP);
   myHeadRadioButton->SetValue(true);
   myTagRadioButton = new wxRadioButton(this, REVOPTIONS_ID_TAG, _("&Choose branch or tag"));
   wxBoxSizer* tagBranchSizer = new wxBoxSizer(wxHORIZONTAL);
   myTagBranchStatic = new wxStaticText(this, -1, _("Branch or tag name:"));
   tagBranchSizer->Add(myTagBranchStatic, 0, wxALL | wxALIGN_CENTER_VERTICAL, 4);
   TagValidator tagValidator;
   myTagCombo = new ExtComboBox(this, -1, wxT("HEAD"), wxDefaultPosition, wxDefaultSize,
                                0, 0, 0, tagValidator);
   myTagCombo->Append(wxT("HEAD"));
   tagBranchSizer->Add(myTagCombo, 2, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
   myFetchTagBranchList = new wxButton(this, REVOPTIONS_ID_FETCHTAGLIST, _("&Update list..."));
   myFetchTagBranchList->Enable(false);
   tagBranchSizer->Add(myFetchTagBranchList, 0, 0, 5);  
   myFetchRecursive = new wxCheckBox(this, -1, _("&Scan subfolders"));
   myFetchRecursive->Enable(false);
   tagBranchSizer->Add(myFetchRecursive, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);  
   myForceHead = new wxCheckBox(this, -1,
                                _("If no matching revision is found, use the most recent one"));
   myForceHead->SetToolTip(_(
"If you've specified a tag / branch / date, and it matches no revision for a file, \
CVS will get you the head revision instead."));
   const wxChar* tagTip = _("Which tag or branch should be retrieved");
   myTagRadioButton->SetToolTip(tagTip);
   myTagCombo->SetToolTip(tagTip);
   const wxChar* fetchTip = _("Retrieves a list of tags and branches from the server, and puts them in the dropdown box");
   myFetchTagBranchList->SetToolTip(fetchTip);
   sizerStaticBox->Add(myHeadRadioButton, 0, wxGROW | wxALL, 4);
   sizerStaticBox->Add(myTagRadioButton, 0, wxGROW | wxALL, 4);
   sizerStaticBox->Add(tagBranchSizer, 0, wxGROW | wxALL, 2);

   wxStaticBox* staticBox2 = new wxStaticBox(this, -1, _("Date and time"));
   wxStaticBoxSizer* sizerStaticBox2 = new wxStaticBoxSizer(staticBox2, wxVERTICAL);
   wxGridSizer* gridSizer = new wxFlexGridSizer(2);

   const wxChar* dateTip = _(
"Choose a date from the calendar, and/or edit the date and time in the box above the calendar, e.g.\n\
  \"1988-09-24\"\n\
  \"yesterday\"\n\
  \"24 Sep 20:05\"");
   myMostRecentRadioButton = new wxRadioButton(this, REVOPTIONS_ID_MOSTRECENT, _("Most &recent files"),
                                               wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
   myDateRadioButton = new wxRadioButton(this, REVOPTIONS_ID_DATE, _("Files at certain &time"));
   myDateStatic = new wxStaticText(this, -1, _("Date/time of files to get:"));
   myDateCombo = MakeComboList(this, -1, "History\\Revision Date");
   sizerStaticBox2->Add(myMostRecentRadioButton, 0, wxALIGN_LEFT| wxALIGN_CENTER_VERTICAL | wxALL , 4);
   sizerStaticBox2->Add(myDateRadioButton, 0, wxALIGN_LEFT| wxALIGN_CENTER_VERTICAL | wxALL , 4);
   gridSizer->Add(myDateStatic, 0, wxALIGN_LEFT| wxALL | wxALIGN_CENTER_VERTICAL, 5);
   gridSizer->Add(myDateCombo, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, 5);
   myDateRadioButton->SetToolTip(dateTip);
   myDateCombo->SetToolTip(dateTip);
   sizerStaticBox2->Add(gridSizer, 0, wxGROW | wxALL, 0);

   myCalendar = new wxCalendarCtrl(this, REVOPTIONS_ID_CALENDAR, wxDateTime::Now(), wxDefaultPosition, wxDefaultSize);

   // Hack to fix broken Combobox height on Win2K
//   myCalendar->GetMonthControl()->SetSize(wxDefaultSize.x, 100);

   wxStaticText* dummy = new wxStaticText(this, -1, wxT(""));
   gridSizer->Add(dummy, 0, wxALIGN_LEFT| wxALIGN_CENTER_VERTICAL | wxALL, 5);
   gridSizer->Add(myCalendar, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, 5);
   myCalendar->SetToolTip(dateTip);
   dummy->SetToolTip(dateTip);

   // String together
   wxBoxSizer* boxSizer = new wxBoxSizer(wxVERTICAL);
   boxSizer->Add(sizerStaticBox, 0, wxEXPAND | wxALL, 4);
   boxSizer->Add(sizerStaticBox2, 0, wxEXPAND | wxALL, 4);
   boxSizer->Add(myForceHead, 0, wxGROW | wxALL, 4);

   SetAutoLayout(TRUE);
   SetSizer(boxSizer);

   wxCommandEvent event;
   UpdateSensitivity(event);
   return;
}

void RevOptions::UpdateCalendar(wxCalendarEvent&)
{
   wxDateTime date = myCalendar->GetDate();
   wxString formatted = date.Format(wxT("%d %b %Y"));
   myDateCombo->SetValue(formatted);
   Update();
}

void RevOptions::UpdateSensitivity(wxCommandEvent&)
{
   myTagCombo->Enable(myTagRadioButton->GetValue());
   myTagBranchStatic->Enable(myTagRadioButton->GetValue());
   myFetchTagBranchList->Enable(myTagRadioButton->GetValue() 
                                && myCVSRootCache.Valid());
   myFetchRecursive->Enable(myTagRadioButton->GetValue() 
                            && myCVSRootCache.Valid());
   bool dateEnabled = myDateRadioButton->GetValue();
   myDateCombo->Enable(dateEnabled);
   myDateStatic->Enable(dateEnabled);
   myCalendar->Enable(dateEnabled);
   Update();
}

void RevOptions::Update()
{
   if (myTagRadioButton->GetValue())
      myTag = wxAscii(myTagCombo->GetValue().c_str());
   else
      myTag = "";

   if (myDateRadioButton->GetValue())
      myDate = wxAscii(myDateCombo->GetValue().c_str());
   else
      myDate = "";
}

void RevOptions::OKClicked()
{
   Update();
   WriteComboList(myDateCombo, "History\\Revision Date");
}

// Use various heuristics to get a list of modules on the server
void RevOptions::FetchTagBranchList(wxCommandEvent&)
{
   myFetchTagBranchList->Enable(false);

   // Get module list and put it in the dialog
   std::vector<std::string> tagbranch;
   RemoteLists::GetTagBranchListRlog(GetParent(), tagbranch, tagbranch, std::vector<std::string>(1, myDir),
                                     myCVSRootCache.GetCVSROOT(), myModuleCache, myFetchRecursive->GetValue());
   tagbranch.insert(tagbranch.begin(), "HEAD");
   if (!tagbranch.empty())
      FillTagBranchComboBox(myTagCombo, tagbranch);
   else
   {
       wxString msg(_("No tags or branches found."));
       msg += wxT("\n\n");
       msg += wxText(myCVSRootCache.GetCVSROOT());
       DoMessageDialog(GetParent(), msg);
   }
   myFetchTagBranchList->Enable(true);
}

// Use various heuristics to get a list of modules on the server
void RevOptions::FetchCachedTagBranchList()
{
   std::string cvsroot = myCVSRootCache.GetCVSROOT();
   std::string repository = myModuleCache;
   std::string module = CutFirstToken(repository, "/");
   // Get module list and put it in the dialog
   std::vector<std::string> tagbranch;
   RemoteLists::GetCachedTagBranchList(tagbranch, tagbranch, cvsroot, module);
   std::sort(tagbranch.begin(), tagbranch.end());
   tagbranch.insert(tagbranch.begin(), "HEAD");
   wxString s = myTagCombo->GetValue();
   if (!tagbranch.empty())
   {
      FillTagBranchComboBox(myTagCombo, tagbranch);
      myTagCombo->SetValue(s.c_str());
   }
}

void RevOptions::SetCVSRoot(const CVSRoot& cvsroot)
{
   myCVSRootCache = cvsroot;
   wxCommandEvent event;
   UpdateSensitivity(event);
   FetchCachedTagBranchList();
}

void RevOptions::SetTag(const std::string& tag)
{
   myTag = tag;
   myTagRadioButton->SetValue(true);
   myTagCombo->SetValue(wxText(myTag));
}

void RevOptions::SetModule(const std::string& module)
{
   myModuleCache = module;
}


void RevOptions::SetDir(const std::string& dir)
{
   myDir = dir;
}


void RevOptions::SetForceHead(const bool forceHead)
{
   myForceHead->SetValue(forceHead);
}


bool RevOptions::GetForceHead()
{
   return myForceHead->GetValue();
}

void RevOptions::UpdateSensitivityTagSel(wxCommandEvent& event)
{
   UpdateSensitivity(event);
   myTagCombo->SetFocus();
}
