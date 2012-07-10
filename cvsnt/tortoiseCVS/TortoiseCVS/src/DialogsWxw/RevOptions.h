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

#ifndef REVOPTIONS_H
#define REVOPTIONS_H

#include <string>

#include <wx/wx.h>
#include <wx/calctrl.h>

#include "../CVSGlue/CVSRoot.h"

// This class implements a gui layout to allow the user to specify a tag
// or date.


class RevOptions : public wxPanel
{
public:
   RevOptions(wxWindow* parent);

   // retrieve results: these can be called at any time,
   // (even if the actual widget layout has been destroyed).
   std::string GetTag() const // "" for no tag
   {
      return myTag;
   }
   
   void SetTag(const std::string& tag);
   
   std::string GetDate() const   // "" for no date
   {
      return myDate;
   }

   void OKClicked();
   void SetCVSRoot(const CVSRoot& cvsroot);
   void SetModule(const std::string& module);
   void SetDir(const std::string& dir);
   void SetForceHead(const bool forceHead);
   bool GetForceHead();

private:
   wxRadioButton* myHeadRadioButton;
   wxRadioButton* myTagRadioButton;
   wxComboBox* myTagCombo;
   wxStaticText* myTagBranchStatic;
   wxCheckBox* myForceHead;

   wxRadioButton* myMostRecentRadioButton;
   wxRadioButton* myDateRadioButton;
   wxComboBox* myDateCombo;
   wxCalendarCtrl* myCalendar;
   wxStaticText *myDateStatic;

   wxButton* myFetchTagBranchList;
   wxCheckBox* myFetchRecursive;

   std::string myDate;
   std::string myTag;

   CVSRoot myCVSRootCache;
   std::string myModuleCache;
   std::string myDir;

   void Update();
   void UpdateSensitivity(wxCommandEvent& event);
   void UpdateSensitivityTagSel(wxCommandEvent& event);
   void UpdateCalendar(wxCalendarEvent& event);
   void FetchTagBranchList(wxCommandEvent& event);
   void FetchCachedTagBranchList();

   DECLARE_EVENT_TABLE()
};

#endif

