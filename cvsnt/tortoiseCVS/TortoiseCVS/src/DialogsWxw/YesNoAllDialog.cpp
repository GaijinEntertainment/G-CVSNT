// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2005 - Torsten Martinsen
// <torsten@tiscali.dk> - January 2005

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

#include <wx/wx.h>
#include "YesNoAllDialog.h"
#include "../Utils/StringUtils.h"
#include "../Utils/Translate.h"
#include "ExtStaticText.h"
#include "WxwHelpers.h"


enum
{
   YESNOALLDIALOG_ID_YES_ALL = 10001,
   YESNOALLDIALOG_ID_NO_ALL
};

BEGIN_EVENT_TABLE(YesNoAllDialog, ExtDialog)
   EVT_COMMAND(wxID_YES,                  wxEVT_COMMAND_BUTTON_CLICKED, YesNoAllDialog::OnYes)
   EVT_COMMAND(wxID_NO,                   wxEVT_COMMAND_BUTTON_CLICKED, YesNoAllDialog::OnNo)
   EVT_COMMAND(wxID_CANCEL,               wxEVT_COMMAND_BUTTON_CLICKED, YesNoAllDialog::OnCancel)
   EVT_COMMAND(YESNOALLDIALOG_ID_YES_ALL, wxEVT_COMMAND_BUTTON_CLICKED, YesNoAllDialog::OnYesAll)
   EVT_COMMAND(YESNOALLDIALOG_ID_NO_ALL,  wxEVT_COMMAND_BUTTON_CLICKED, YesNoAllDialog::OnNoAll)
END_EVENT_TABLE()
   
YesNoAllDialog::YesNoAll DoYesNoAllDialog(wxWindow* parent,
                                          const wxString& message,
                                          bool yesdefault,
                                          bool withCancel)
{
   YesNoAllDialog dlg(parent, message, yesdefault, withCancel);
   return static_cast<YesNoAllDialog::YesNoAll>(dlg.ShowModal());
}

YesNoAllDialog::YesNoAllDialog(wxWindow* parent, 
                               const wxString& message,
                               bool yesdefault,
                               bool withCancel)
   : ExtDialog(parent, -1,
               _("TortoiseCVS - Question"),
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN)
{
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

   ExtStaticText* label = new ExtStaticText(this, -1, message.c_str(), wxDefaultPosition,
                                            wxDLG_UNIT(this, wxSize(60, 10)));

   // Buttons
   wxBoxSizer* sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   wxButton* yes = new wxButton(this, wxID_YES, _("&Yes"));
   sizerConfirm->Add(yes, 0, wxGROW | wxALL, 5);
   wxButton* no = new wxButton(this, wxID_NO, _("&No"));
   sizerConfirm->Add(no, 0, wxGROW | wxALL, 5);
   if (withCancel)
   {
       wxButton* cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
       sizerConfirm->Add(no, 0, wxGROW | wxALL, 5);
   }
   wxButton* yesall = new wxButton(this, YESNOALLDIALOG_ID_YES_ALL, _("Yes to &all"));
   sizerConfirm->Add(yesall, 0, wxGROW | wxALL, 5);
   wxButton* noall = new wxButton(this, YESNOALLDIALOG_ID_NO_ALL, _("N&o to all"));
   sizerConfirm->Add(noall, 0, wxGROW | wxALL, 5);
   if (yesdefault)
   {
      yes->SetDefault();
      yes->SetFocus();
   }
   else
   {
      no->SetDefault();
      no->SetFocus();
   }
   // Main box with everything in it
   wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(label, 0, wxGROW | wxALL, 8);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxALL, 0);
   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->SetItemMinSize(label, label->GetMinSize().GetWidth(), 
                            label->GetMinSize().GetHeight());
   sizerTop->Fit(this);
   SetTortoiseDialogPos(this, GetRemoteHandle());
}

void YesNoAllDialog::OnYes(wxCommandEvent&)
{
   EndModal(Yes);
}

void YesNoAllDialog::OnNo(wxCommandEvent&)
{
   EndModal(No);
}

void YesNoAllDialog::OnCancel(wxCommandEvent&)
{
   EndModal(Cancel);
}

void YesNoAllDialog::OnYesAll(wxCommandEvent&)
{
   EndModal(YesAll);
}

void YesNoAllDialog::OnNoAll(wxCommandEvent&)
{
   EndModal(NoAll);
}
