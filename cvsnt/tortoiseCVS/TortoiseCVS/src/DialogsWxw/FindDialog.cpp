// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2005 - Marcel Gosselin
// <marcel.gosselin@polymtl.ca> - March 2005

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
#include "FindDialog.h"
#include "AnnotateDialog.h"
#include "WxwHelpers.h"

FindDialog::FindDialog(wxWindow* parent)
   : ExtDialog(parent, -1,
               _("TortoiseCVS - Find"),
               wxDefaultPosition, wxSize(400,300), wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN)
{
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

   wxStaticText* labelFindWhat = new wxStaticText(this, wxID_ANY, _("Find what: "));
   mySearchString = new wxTextCtrl(this, wxID_ANY);

   wxButton* findNextButton = new wxButton(this, wxID_ANY, _("Find Next"));
   wxButton* markAllButton = new wxButton(this, wxID_ANY, _("&Mark All"));
   wxButton* closeButton = new wxButton(this, wxID_CANCEL, _("Cancel"));
   findNextButton->SetDefault();

   // Make buttons the same width
   int findNextWidth = findNextButton->GetSize().GetWidth();
   int markAllWidth = markAllButton->GetSize().GetWidth();
   int closeWidth = closeButton->GetSize().GetWidth();
   int max1 = std::max(findNextWidth, markAllWidth);
   wxSize bestSize(std::max(max1, closeWidth),
                   closeButton->GetSize().GetHeight());
   findNextButton->SetSize(bestSize);
   markAllButton->SetSize(bestSize);
   closeButton->SetSize(bestSize);

   myCaseSensitive = new wxCheckBox(this, wxID_ANY, _("Match &Case"),
                                    wxDefaultPosition, wxDefaultSize, 0);
   mySearchUp = new wxCheckBox(this, wxID_ANY, _("Search &Up"),
                               wxDefaultPosition, wxDefaultSize, 0);
   myWrapSearch = new wxCheckBox(this, wxID_ANY, _("&Wrap Search"),
                                 wxDefaultPosition, wxDefaultSize, 0);

   Connect(findNextButton->GetId(),
           wxEVT_COMMAND_BUTTON_CLICKED,
           reinterpret_cast<wxObjectEventFunction>((&FindDialog::OnFindNext)));
   Connect(markAllButton->GetId(),
           wxEVT_COMMAND_BUTTON_CLICKED,
           reinterpret_cast<wxObjectEventFunction>((&FindDialog::OnMarkAll)));
   Connect(closeButton->GetId(),
           wxEVT_COMMAND_BUTTON_CLICKED,
           reinterpret_cast<wxObjectEventFunction>((&FindDialog::OnClose)));

   // Main box with everything in it
   wxBoxSizer* sizerLeft = new wxBoxSizer(wxVERTICAL);
   wxBoxSizer* sizerFindWhat = new wxBoxSizer(wxHORIZONTAL);
   sizerFindWhat->Add(labelFindWhat, 0, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);
   sizerFindWhat->Add(mySearchString, 1, wxALL|wxEXPAND|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5 );
   sizerLeft->Add(sizerFindWhat);
   sizerLeft->Add(myCaseSensitive, 0, wxLEFT|wxTOP, 5);
   sizerLeft->Add(mySearchUp, 0, wxLEFT, 5);
   sizerLeft->Add(myWrapSearch, 0, wxLEFT, 5);
   wxBoxSizer* sizerRight = new wxBoxSizer(wxVERTICAL);
   sizerRight->Add(findNextButton, 0, wxRIGHT, 5);
   sizerRight->Add(markAllButton, 0, wxRIGHT, 5);
   sizerRight->Add(closeButton, 0, wxRIGHT, 5);
   sizerRight->SetItemMinSize(findNextButton, bestSize.GetX(), bestSize.GetY());
   sizerRight->SetItemMinSize(markAllButton, bestSize.GetX(), bestSize.GetY());
   sizerRight->SetItemMinSize(closeButton, bestSize.GetX(), bestSize.GetY());
   wxBoxSizer *sizerTop = new wxBoxSizer(wxHORIZONTAL);
   sizerTop->Add(sizerLeft, 0, wxGROW | wxALL, 8);
   sizerTop->Add(sizerRight, 0, wxALIGN_CENTER | wxALL, 0);

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);

   SetTortoiseDialogPos(this, GetRemoteHandle());
}

void FindDialog::OnFindNext(wxCommandEvent& event)
{
   AnnotateDialog* parent = reinterpret_cast<AnnotateDialog*>(this->GetParent());
   parent->FindNext(wxAscii(mySearchString->GetValue().c_str()), myCaseSensitive->IsChecked(),
                    mySearchUp->IsChecked(), myWrapSearch->IsChecked());
}

void FindDialog::OnMarkAll(wxCommandEvent& event)
{
   AnnotateDialog* parent = reinterpret_cast<AnnotateDialog*>(this->GetParent());
   parent->FindAndMarkAll(wxAscii(mySearchString->GetValue().c_str()), myCaseSensitive->IsChecked());
}

void FindDialog::OnClose(wxCommandEvent& event)
{
   this->Show(false);
}
