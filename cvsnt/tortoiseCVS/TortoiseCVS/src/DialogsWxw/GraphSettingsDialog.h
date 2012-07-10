// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2004 - Torsten Martinsen
// <torsten@image.dk> - December 2004

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

#ifndef GRAPHSETTINGSDLG_H
#define GRAPHSETTINGSDLG_H

#include <string>
#include <vector>
#include <wx/wx.h>
#include <wx/calctrl.h>
#include "ExtDialog.h"

class GraphSettingsDialog : ExtDialog
{
public:
   friend bool DoGraphSettingsDialog(wxWindow* parent, 
                                     std::string& startdate,
                                     std::string& enddate);

private:
   GraphSettingsDialog(wxWindow* parent);
   ~GraphSettingsDialog();
   std::string GetStartDate() const   // "" for no date
   {
      return myUseStartDate->GetValue() ? wxAscii(myStartDateCombo->GetValue().c_str()) : "";
   }

   std::string GetEndDate() const   // "" for no date
   {
      return myUseEndDate->GetValue() ? wxAscii(myEndDateCombo->GetValue().c_str()) : "";
   }

   void SetStartDate(const std::string& startdate);

   void SetEndDate(const std::string& enddate);

   void OKClicked();

   void RefreshStartDate(wxCommandEvent& event);
   void RefreshEndDate(wxCommandEvent& event);
   void UpdateStartDateCalendar(wxCommandEvent& event);
   void UpdateEndDateCalendar(wxCommandEvent& event);

   wxCheckBox*          myUseStartDate;
   wxCheckBox*          myUseEndDate;
   wxComboBox*          myStartDateCombo;
   wxCalendarCtrl*      myStartDateCalendar;
   wxComboBox*          myEndDateCombo;
   wxCalendarCtrl*      myEndDateCalendar;

   DECLARE_EVENT_TABLE()
};

#endif
