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
#include "CheckQueryDlg.h"
#include "ExtStaticText.h"
#include "WxwHelpers.h"
#include <wx/statline.h>

//static
bool DoCheckQueryDialog(wxWindow* parent, const wxString& message, bool yesdefault, 
                        const wxString& checkText)
{
   CheckQueryDlg* dlg = new CheckQueryDlg(parent, message, yesdefault, checkText);
   dlg->ShowModal();

   bool ret = dlg->myCheckbox->GetValue();
   DestroyDialog(dlg);
   return ret;
}


CheckQueryDlg::CheckQueryDlg(wxWindow* parent, const wxString& message, bool yesdefault, const wxString& checkText)
   : ExtDialog(parent, -1, _("TortoiseCVS Question"),
               wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN)
{
   TDEBUG_ENTER("CheckQueryDlg");
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

   wxStaticText* label = new ExtStaticText(this, -1, message.c_str(), wxDefaultPosition,
                                           wxDLG_UNIT(this, wxSize(60, 10)));

   myCheckbox = new wxCheckBox( this, -1,
                                checkText.c_str(), wxDefaultPosition, wxDefaultSize, 0 );
   myCheckbox->SetValue(yesdefault);

   // OK/Cancel button
   wxBoxSizer* sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   wxButton* ok = new wxButton(this, wxID_OK, _("OK"));
   ok->SetDefault();
   sizerConfirm->Add(ok, 0, wxGROW | wxALL, 5);

   // Main box with everything in it
   wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(label, 0, wxGROW | wxALL, 8);
   sizerTop->Add(new wxStaticLine( this, -1 ), 0, wxEXPAND | wxALL, 10);
   sizerTop->Add(myCheckbox, 0, wxGROW | wxALL, 8);
   sizerTop->Add(new wxStaticLine( this, -1 ), 0, wxEXPAND | wxALL, 10);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxALL, 0);

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->SetItemMinSize(label, label->GetMinSize().GetWidth(), 
                            label->GetMinSize().GetHeight());
   sizerTop->Fit(this);
   
   SetTortoiseDialogPos(this, 0);
}
