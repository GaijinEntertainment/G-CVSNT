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

#include "LogConfigDialog.h"
#include "WxwHelpers.h"
#include "ExtStaticText.h"
#include "../Utils/Translate.h"

bool DoLogConfigDialog(wxWindow* parent,
                       std::string& urlStubPrefix,
                       std::string& urlStubSuffix,
                       const wxString& message,
                       const std::string& regKey)
{
   LogConfigDialog* dlg = new LogConfigDialog(parent,
                                              urlStubPrefix, 
                                              urlStubSuffix,
                                              message,
                                              regKey);

   bool ret = dlg->ShowModal() == wxID_OK;

   if (ret)
   {
      WriteComboList(dlg->myPrefixCombo, "History\\LogConfig\\" + dlg->myRegKey + "\\URLPrefix");
      WriteComboList(dlg->mySuffixCombo, "History\\LogConfig\\" + dlg->myRegKey + "\\URLSuffix");

      urlStubPrefix = dlg->myURLPrefix;
      urlStubSuffix = dlg->myURLSuffix;
   }

   DestroyDialog(dlg);
   return ret;
}

enum
{
   LOGCFGDLG_ID_URL = 10001,
   LOGCFGDLG_ID_VANILLA,
   LOGCFGDLG_ID_SCANAGAIN,
   LOGCFGDLG_ID_PREFIX,
   LOGCFGDLG_ID_SUFFIX
};

BEGIN_EVENT_TABLE(LogConfigDialog, ExtDialog)
   EVT_COMMAND(LOGCFGDLG_ID_URL,        wxEVT_COMMAND_RADIOBUTTON_SELECTED,     LogConfigDialog::cb_RadioChanged)
   EVT_COMMAND(LOGCFGDLG_ID_VANILLA,    wxEVT_COMMAND_RADIOBUTTON_SELECTED,     LogConfigDialog::cb_RadioChanged)
   EVT_COMMAND(LOGCFGDLG_ID_SCANAGAIN,  wxEVT_COMMAND_RADIOBUTTON_SELECTED,     LogConfigDialog::cb_RadioChanged)
   EVT_COMMAND(LOGCFGDLG_ID_PREFIX,     wxEVT_COMMAND_COMBOBOX_SELECTED,        LogConfigDialog::cb_TextChanged)
   EVT_COMMAND(LOGCFGDLG_ID_PREFIX,     wxEVT_COMMAND_TEXT_UPDATED,             LogConfigDialog::cb_TextChanged)
   EVT_COMMAND(LOGCFGDLG_ID_SUFFIX,     wxEVT_COMMAND_COMBOBOX_SELECTED,        LogConfigDialog::cb_TextChanged)
   EVT_COMMAND(LOGCFGDLG_ID_SUFFIX,     wxEVT_COMMAND_TEXT_UPDATED,             LogConfigDialog::cb_TextChanged)
END_EVENT_TABLE()


LogConfigDialog::LogConfigDialog(wxWindow* parent,
                                 std::string& initialURLPrefix, 
                                 std::string& initialURLSuffix,
                                 const wxString& message,
                                 const std::string& regKey)
   : ExtDialog(parent, -1, _("TortoiseCVS - Web Log Configuration"),
               wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
{
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

   myRegKey = regKey;
   myOK = false;

   // Description
   wxStaticText* label1 = new ExtStaticText(this, -1, message, wxDefaultPosition,
                                            wxDLG_UNIT(this, wxSize(60, 10)),
                                            wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE | wxTE_NO_VSCROLL);
   label1->SetBackgroundColour(this->GetBackgroundColour());

   // Web log and prefix
   wxBoxSizer* sizerURLPrefix = new wxBoxSizer(wxHORIZONTAL);
   myRadioURL = new wxRadioButton(this, LOGCFGDLG_ID_URL,
                                  _("Web log URL &prefix:"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
   myRadioURL->SetValue(true);
   myPrefixCombo = MakeComboList(this, LOGCFGDLG_ID_PREFIX,
                                 "History\\LogConfig\\" + myRegKey + "\\URLPrefix",
                                 initialURLPrefix);
   sizerURLPrefix->Add(myRadioURL, 0, wxALIGN_CENTER_VERTICAL | wxGROW | wxALL, 0);
   sizerURLPrefix->Add(myPrefixCombo, 2, wxGROW | wxALL, 5);
   myPrefixCombo->SetFocus();

   // Suffix and sizer
   wxStaticText* label2 = new wxStaticText(this, -1, _("&Suffix:"));
   mySuffixCombo = MakeComboList(this, LOGCFGDLG_ID_SUFFIX,
                                 "History\\LogConfig\\" + myRegKey + "\\URLSuffix",
                                 initialURLSuffix);
   sizerURLPrefix->Add(label2, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
   sizerURLPrefix->Add(mySuffixCombo, 1, wxGROW | wxALL, 5);

   // Vanilla
   myRadioVanilla = new wxRadioButton(this, LOGCFGDLG_ID_VANILLA,
                                      _("&Plain text log (if your server doesn't support web log)"));
   
   // Scan again
   myRadioScanAgain= new wxRadioButton(this, LOGCFGDLG_ID_SCANAGAIN,
                                       _("&Automatically scan the server for the URL (again)"));

   // OK/Cancel button
   wxBoxSizer* sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   wxButton* ok = new wxButton(this, wxID_OK, _("OK"));
   ok->SetDefault();
   wxButton* cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
   sizerConfirm->Add(ok, 0, wxGROW | wxALL, 5); 
   sizerConfirm->Add(cancel, 0, wxGROW | wxALL, 5);

   // Main vertical with text and button in it
   wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(label1, 0, wxGROW | wxALL, 10);
   sizerTop->Add(sizerURLPrefix, 0, wxALIGN_LEFT | wxGROW | wxLEFT | wxRIGHT, 5);
   sizerTop->Add(myRadioVanilla, 0, wxALIGN_LEFT | wxGROW | wxLEFT | wxRIGHT, 5);
   sizerTop->Add(myRadioScanAgain, 0, wxALIGN_LEFT | wxGROW | wxLEFT | wxRIGHT, 5);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxALL, 0);

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->SetItemMinSize(label1, label1->GetSize().GetWidth(), label1->GetSize().GetHeight());
   sizerTop->Fit(this);

   // enable/disable appropriate widgets
   UpdateSensitivity();
   UpdateText();

   SetTortoiseDialogPos(this, GetRemoteHandle());
}


LogConfigDialog::~LogConfigDialog()
{
}


void LogConfigDialog::cb_RadioChanged(wxCommandEvent& WXUNUSED(event))
{
    UpdateSensitivity();
    UpdateText();
}

void LogConfigDialog::cb_TextChanged(wxCommandEvent& WXUNUSED(event))
{
    UpdateText();
}

// read out url and store in myURL
void LogConfigDialog::UpdateText()
{
    if (myRadioURL->GetValue())
    {
        myURLPrefix = wxAscii(myPrefixCombo->GetValue().c_str());
        myURLSuffix = wxAscii(mySuffixCombo->GetValue().c_str());
    }
    else
    {
        myURLPrefix = "";
        myURLSuffix = "";
    }

    if (myRadioVanilla->GetValue())
    {
        myURLPrefix = "plainlog";
        myURLSuffix = "";
    }
    if (myRadioScanAgain->GetValue())
    {
        myURLPrefix = "scanagain";
        myURLSuffix = "";
    }
}

void LogConfigDialog::UpdateSensitivity()
{
    bool state = myRadioURL->GetValue();
    myPrefixCombo->Enable(state);
    mySuffixCombo->Enable(state);
}
