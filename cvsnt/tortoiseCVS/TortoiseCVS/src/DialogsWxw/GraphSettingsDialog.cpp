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

#include "StdAfx.h"
#include "GraphSettingsDialog.h"
#include "WxwHelpers.h"
#include <wx/statline.h>
#include "../Utils/Translate.h"
#include "../Utils/TimeUtils.h"

//static
bool DoGraphSettingsDialog(wxWindow* parent, 
                           std::string& startdate,
                           std::string& enddate)
{
   GraphSettingsDialog* dlg = new GraphSettingsDialog(parent);
   dlg->SetStartDate(startdate);
   dlg->SetEndDate(enddate);
   bool ret = (dlg->ShowModal() == wxID_OK);
   if (ret)
   {
      startdate = dlg->GetStartDate();
      enddate = dlg->GetEndDate();
   }

   DestroyDialog(dlg);
   return ret;
}


enum
{
   GRAPHSETTINGSDLG_ID_USESTART = 10001,
   GRAPHSETTINGSDLG_ID_USEEND,
   GRAPHSETTINGSDLG_ID_STARTCAL,
   GRAPHSETTINGSDLG_ID_ENDCAL
};

BEGIN_EVENT_TABLE(GraphSettingsDialog, ExtDialog)
   EVT_COMMAND(GRAPHSETTINGSDLG_ID_USESTART, wxEVT_COMMAND_CHECKBOX_CLICKED, GraphSettingsDialog::RefreshStartDate)
   EVT_COMMAND(GRAPHSETTINGSDLG_ID_USEEND,   wxEVT_COMMAND_CHECKBOX_CLICKED,    GraphSettingsDialog::RefreshEndDate)
   EVT_COMMAND(GRAPHSETTINGSDLG_ID_STARTCAL, wxEVT_CALENDAR_SEL_CHANGED, GraphSettingsDialog::UpdateStartDateCalendar)
   EVT_COMMAND(GRAPHSETTINGSDLG_ID_ENDCAL,   wxEVT_CALENDAR_SEL_CHANGED, GraphSettingsDialog::UpdateEndDateCalendar)
END_EVENT_TABLE()


GraphSettingsDialog::GraphSettingsDialog(wxWindow* parent)
   : ExtDialog(parent, -1,
               _("TortoiseCVS - Revision Graph Settings"),
               wxDefaultPosition, wxDefaultSize, 
               wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN)
{
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));
   SetSize(wxDLG_UNIT(this, wxSize(300, 240)));

   wxFlexGridSizer* gridSizer = new wxFlexGridSizer(2, 0, 0);

   myUseStartDate = new wxCheckBox(this, GRAPHSETTINGSDLG_ID_USESTART,_("&Start date/time:"));
   myUseStartDate->SetToolTip(_("Do not show revisions/tags/branches before a specific date."));
   const wxChar* dateTip = _(
"Choose a date from the calendar, and/or edit the date and time in the box above the calendar.\n\
e.g.: \"1988-09-24\"  \"yesterday\"  \"24 Sep 20:05\"");
   myStartDateCombo = MakeComboList(this, -1, "History\\Revision Date");
   gridSizer->Add(myUseStartDate, 0, wxGROW | wxALL, 5);
   gridSizer->Add(myStartDateCombo, 1, wxGROW | wxALL, 5);
   myStartDateCombo->SetToolTip(dateTip);
    
   myStartDateCalendar = new wxCalendarCtrl(this, GRAPHSETTINGSDLG_ID_STARTCAL, wxDateTime::Now());
   // Hack to fix broken Combobox height on Win2K
//   myStartDateCalendar->GetMonthControl()->SetSize(wxDefaultSize.x, 100);

   wxStaticText* dummy = new wxStaticText(this, -1, wxT(""));
   gridSizer->Add(dummy, 0, wxGROW | wxALL, 5);
   gridSizer->Add(myStartDateCalendar, 1, wxGROW | wxALL, 5);
   myStartDateCalendar->SetToolTip(dateTip);
   dummy->SetToolTip(dateTip);

   myUseStartDate->SetValue(false);

   myUseEndDate = new wxCheckBox(this, GRAPHSETTINGSDLG_ID_USEEND,_("&End date/time:"));
   myUseEndDate->SetToolTip(_("Do not show revisions/tags/branches after a specific date."));
   myEndDateCombo = MakeComboList(this, -1, "History\\Revision Date");
   gridSizer->Add(myUseEndDate, 0, wxGROW | wxALL, 5);
   gridSizer->Add(myEndDateCombo, 1, wxGROW | wxALL, 5);
   myEndDateCombo->SetToolTip(dateTip);
    
   myEndDateCalendar = new wxCalendarCtrl(this, GRAPHSETTINGSDLG_ID_ENDCAL, wxDateTime::Now());
   // Hack to fix broken Combobox height on Win2K
//   myEndDateCalendar->GetMonthControl()->SetSize(wxDefaultSize.x, 100);

   dummy = new wxStaticText(this, -1, wxT(""));
   gridSizer->Add(dummy, 0, wxGROW | wxALL, 5);
   gridSizer->Add(myEndDateCalendar, 1, wxGROW | wxALL, 5);
   myEndDateCalendar->SetToolTip(dateTip);
   dummy->SetToolTip(dateTip);

   myUseEndDate->SetValue(false);
   
   // OK/Cancel button
   wxBoxSizer* sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   wxButton* ok = new wxButton(this, wxID_OK, _("OK"));
   ok->SetDefault();
   wxButton* cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
   sizerConfirm->Add(ok, 0, wxGROW | wxALL, 5);
   sizerConfirm->Add(cancel, 0, wxGROW | wxALL, 5);

   // Main box with everything in it
   wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(gridSizer, 0, wxGROW | wxALL, 5);
   sizerTop->Add(new wxStaticLine( this, -1 ), 0, wxEXPAND | wxALL, 10);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxALL, 0);

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);

   wxCommandEvent event;
   RefreshStartDate(event);
   RefreshEndDate(event);

   SetTortoiseDialogPos(this, GetRemoteHandle());
}


GraphSettingsDialog::~GraphSettingsDialog()
{
}


void GraphSettingsDialog::RefreshStartDate(wxCommandEvent&)
{
   if (myUseStartDate->GetValue())
   {
      myStartDateCombo->Enable(true);
      myStartDateCalendar->Enable(true);
      myStartDateCombo->SetFocus();
   }
   else
   {
      myStartDateCombo->Enable(false);
      myStartDateCalendar->Enable(false);
   }
}

void GraphSettingsDialog::RefreshEndDate(wxCommandEvent&)
{
   if (myUseEndDate->GetValue())
   {
      myEndDateCombo->Enable(true);
      myEndDateCalendar->Enable(true);
      myEndDateCombo->SetFocus();
   }
   else
   {
      myEndDateCombo->Enable(false);
      myEndDateCalendar->Enable(false);
   }
}

void GraphSettingsDialog::UpdateStartDateCalendar(wxCommandEvent&)
{
   if (myUseStartDate->GetValue())
   {
       wxDateTime date = myStartDateCalendar->GetDate();
       wxString formatted = date.Format(wxT("%d %b %Y"));
       myStartDateCombo->SetValue(formatted);
   }
}

void GraphSettingsDialog::UpdateEndDateCalendar(wxCommandEvent&)
{
   if (myUseEndDate->GetValue())
   {
       wxDateTime date = myEndDateCalendar->GetDate();
       wxString formatted = date.Format(wxT("%d %b %Y"));
       myEndDateCombo->SetValue(formatted);
   }
}

void GraphSettingsDialog::OKClicked()
{
   WriteComboList(myStartDateCombo, "History\\Revision Date");
   WriteComboList(myEndDateCombo, "History\\Revision Date");
}

void GraphSettingsDialog::SetStartDate(const std::string& startdate)
{
   if (!startdate.empty())
   {
      myUseStartDate->SetValue(true);
      myStartDateCombo->Enable(true);
      myStartDateCalendar->Enable(true);
      myStartDateCombo->SetValue(wxText(startdate));
      time_t t = get_date(startdate.c_str());
      myStartDateCalendar->SetDate(wxDateTime(t));
   }
}

void GraphSettingsDialog::SetEndDate(const std::string& enddate)
{
   if (!enddate.empty())
   {
      myUseEndDate->SetValue(true);
      myEndDateCombo->Enable(true);
      myEndDateCalendar->Enable(true);
      myEndDateCombo->SetValue(wxText(enddate));
      time_t t = get_date(enddate.c_str());
      myEndDateCalendar->SetDate(wxDateTime(t));
   }
}
