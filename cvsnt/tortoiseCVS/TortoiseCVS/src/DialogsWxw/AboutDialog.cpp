// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2002 - Torsten Martinsen
// <torsten@tiscali.dk> - August 2002

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
#include "AboutDialog.h"
#include "CreditsDialog.h"
#include "WxwHelpers.h"
#include "ExtTextCtrl.h"
#include "../Utils/StringUtils.h"
#include <wx/statline.h>
#include <windows.h>
#include <wx/msw/private.h>


#define WM_TCVS_SETFOCUS WM_APP + 1

// static
bool DoAboutDialog(wxWindow* parent, 
                   const std::string& version, 
                   const std::string& cvsclientversion,
                   const std::string& cvsserverversion,
                   const wxString& sshversion)
{
   AboutDialog* dlg = new AboutDialog(parent, version, cvsclientversion, cvsserverversion, sshversion);
   dlg->ShowModal();

   DestroyDialog(dlg);
   return true;
}

enum
{
   ABOUTDLG_ID_CREDITS
};

BEGIN_EVENT_TABLE(AboutDialog, ExtDialog)
   EVT_COMMAND(ABOUTDLG_ID_CREDITS, wxEVT_COMMAND_BUTTON_CLICKED, AboutDialog::ShowCredits)
END_EVENT_TABLE()



AboutDialog::AboutDialog(wxWindow* parent, 
                         const std::string& version,
                         const std::string& cvsclientversion,
                         const std::string& cvsserverversion,
                         const wxString& sshversion)
   : ExtDialog(parent, -1,
               _("TortoiseCVS - About"),
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN)
{
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

   // Charlie image
   wxBitmap charlieMaskBitmap(wxT("BMP_CHARLIE_MASK"), wxBITMAP_TYPE_BMP_RESOURCE);
   wxMask* charlieMask = new wxMask(charlieMaskBitmap);
   wxBitmap charlieBitmap(wxT("BMP_CHARLIE"), wxBITMAP_TYPE_BMP_RESOURCE);
   charlieBitmap.SetMask(charlieMask);
   wxStaticBitmap* charlie = new wxStaticBitmap(this, -1, charlieBitmap, wxDefaultPosition);

   wxString programversion(_("TortoiseCVS"));
   programversion += wxT(" ");
#if defined(_DEBUG) || defined (FORCE_DEBUG)
   programversion += _("DEBUG");
   programversion += wxT(" ");
#endif
   programversion += _("version");
   programversion += wxT("\n");
   programversion += wxText(version);
   ExtTextCtrl* versionLabel = new ExtTextCtrl(this, -1, programversion, 
                                               wxDefaultPosition, wxDefaultSize,
                                               wxTE_READONLY | wxBORDER_NONE | wxTE_MULTILINE |
                                               wxTE_CENTRE | wxTE_NO_VSCROLL);
   wxFont font = versionLabel->GetFont();
   int fontsize = font.GetPointSize();
   versionLabel->SetFont(font);
   versionLabel->SetBackgroundColour(this->GetBackgroundColour());
   versionLabel->SetSize(versionLabel->GetTextSize());
   SetWindowLong(GetHwndOf(versionLabel), GWL_STYLE, 
                 GetWindowLong(GetHwndOf(versionLabel), GWL_STYLE) & ~WS_TABSTOP);

   // OK button
   wxBoxSizer* sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   m_Ok = new wxButton(this, wxID_OK, _("OK"));

   // Hidden Cancel button (only for making Esc close the dialog)
   wxButton* cancel = new wxButton(this, wxID_CANCEL, wxT(""));
   cancel->Hide();
   
   // Credits button
   wxButton* credits = new wxButton(this, ABOUTDLG_ID_CREDITS, _("&Credits..."));
   m_Ok->SetDefault();
   sizerConfirm->Add(m_Ok, 0, wxGROW | wxALL, 5);
   sizerConfirm->Add(credits, 0, wxGROW | wxALL, 5);

   // Main box with everything in it
   wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);
   wxBoxSizer* sizerVersion = new wxBoxSizer(wxHORIZONTAL);
   sizerVersion->Add(charlie, 0, wxALIGN_CENTER | wxALL, 5);
   sizerVersion->Add(versionLabel, 0, wxGROW | wxALL, 5);
   sizerTop->Add(sizerVersion, 0, wxALIGN_CENTER | wxGROW | wxALL, 0);
   wxFlexGridSizer* grid = 0;
   if (!cvsclientversion.empty() || !cvsserverversion.empty() || !sshversion.empty())
   {
      sizerTop->Add(new wxStaticLine(this, -1), 0, wxEXPAND | wxALL, 10);
      grid = new wxFlexGridSizer(2, 5, 10);
      if (!cvsclientversion.empty())
      {
         wxString msg(Printf(_("%s client version"), wxT("CVS")));
         msg += _(":");
         ExtTextCtrl* cvsclientversionLabel = new ExtTextCtrl(this, -1, msg,
                                                              wxDefaultPosition, wxDefaultSize,
                                                              wxTE_READONLY | wxBORDER_NONE);
         cvsclientversionLabel->SetBackgroundColour(this->GetBackgroundColour());
         cvsclientversionLabel->SetSize(cvsclientversionLabel->GetTextSize());
         grid->Add(cvsclientversionLabel);
         wxSize size = cvsclientversionLabel->GetTextSize();
         grid->SetItemMinSize(cvsclientversionLabel, size.GetX(), size.GetY());
         ExtTextCtrl* cvsclientversionValue = new ExtTextCtrl(this, -1, wxText(cvsclientversion),
                                                              wxDefaultPosition, wxDefaultSize,
                                                              wxTE_READONLY | wxBORDER_NONE);
         cvsclientversionValue->SetBackgroundColour(this->GetBackgroundColour());
         cvsclientversionValue->SetSize(cvsclientversionValue->GetTextSize());
         grid->Add(cvsclientversionValue);
         size = cvsclientversionValue->GetTextSize();
         grid->SetItemMinSize(cvsclientversionValue, size.GetX(), size.GetY());
      }
      if (!cvsserverversion.empty())
      {
         wxString msg(Printf(_("%s server version"), wxT("CVS")));
         msg += _(":");
         ExtTextCtrl* cvsserverversionLabel = new ExtTextCtrl(this, -1, msg,
                                                              wxDefaultPosition, wxDefaultSize,
                                                              wxTE_READONLY | wxBORDER_NONE);
         cvsserverversionLabel->SetBackgroundColour(this->GetBackgroundColour());
         cvsserverversionLabel->SetSize(cvsserverversionLabel->GetTextSize());
         grid->Add(cvsserverversionLabel);
         wxSize size = cvsserverversionLabel->GetTextSize();
         grid->SetItemMinSize(cvsserverversionLabel, size.GetX(), size.GetY());
         ExtTextCtrl* cvsserverversionValue = new ExtTextCtrl(this, -1, wxText(cvsserverversion),
                                                              wxDefaultPosition, wxDefaultSize,
                                                              wxTE_READONLY | wxBORDER_NONE);
         cvsserverversionValue->SetBackgroundColour(this->GetBackgroundColour());
         cvsserverversionValue->SetSize(cvsserverversionValue->GetTextSize());
         grid->Add(cvsserverversionValue);
         size = cvsserverversionValue->GetTextSize();
         grid->SetItemMinSize(cvsserverversionValue, size.GetX(), size.GetY());
      }
      if (!sshversion.empty())
      {
         wxString msg(Printf(_("%s client version"), wxT("SSH")));
         msg += _(":");
         ExtTextCtrl* sshversionLabel = new ExtTextCtrl(this, -1, msg,
                                                        wxDefaultPosition, wxDefaultSize,
                                                        wxTE_READONLY | wxBORDER_NONE);
         sshversionLabel->SetBackgroundColour(this->GetBackgroundColour());
         sshversionLabel->SetSize(sshversionLabel->GetTextSize());
         grid->Add(sshversionLabel);
         wxSize size = sshversionLabel->GetTextSize();
         grid->SetItemMinSize(sshversionLabel, size.GetX(), size.GetY());
         ExtTextCtrl* sshversionValue = new ExtTextCtrl(this, -1, sshversion,
                                                        wxDefaultPosition, wxDefaultSize,
                                                        wxTE_READONLY | wxBORDER_NONE);
         sshversionValue->SetBackgroundColour(this->GetBackgroundColour());
         sshversionValue->SetSize(sshversionValue->GetTextSize());
         grid->Add(sshversionValue);
         size = sshversionValue->GetTextSize();
         grid->SetItemMinSize(sshversionValue, size.GetX(), size.GetY());
      }
      sizerTop->Add(grid, 0, wxALIGN_CENTER, 10);
      sizerTop->Add(new wxStaticLine(this, -1), 0, wxEXPAND | wxALL, 10);
      sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxALL, 0);
   }
   if (grid)
   {
       wxSize s = grid->GetMinSize();
       sizerVersion->SetItemMinSize(versionLabel, s.GetX(), 0);
   }
   else
   {
       wxSize size = versionLabel->GetTextSize();
       sizerVersion->SetItemMinSize(versionLabel, size.GetX(), size.GetY());
   }
   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);
   SetTortoiseDialogPos(this, GetRemoteHandle());
   // ok->SetFocus() doesn't work, so use a window message instead to get 
   // the focus off the versionedit field
   PostMessage(GetHwnd(), WM_TCVS_SETFOCUS, 0, 0);
}


void AboutDialog::ShowCredits(wxCommandEvent&)
{
   DoCreditsDialog(this);
}



// Windows callbacks
WXLRESULT AboutDialog::MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam)
{
   if (message == WM_TCVS_SETFOCUS)
   {
      m_Ok->SetFocus();      
      return 1;
   }
   return ExtDialog::MSWWindowProc(message, wParam, lParam);
}
