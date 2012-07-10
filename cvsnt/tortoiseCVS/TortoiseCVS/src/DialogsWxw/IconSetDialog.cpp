// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2003 - Torsten Martinsen
// <Hartmut_Honisch@web.de> - February 2003

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

#include "IconSetDialog.h"
#include "WxwHelpers.h"
#include "../Utils/TortoiseRegistry.h"
#include "../Utils/PathUtils.h"
#include "../Utils/ShellUtils.h"
#include "../Utils/Translate.h"
#include <wx/statline.h>

static const char* pszIconNames[] =
{
   "TortoiseInCVS",
   "TortoiseInCVSReadOnly",
   "TortoiseChanged",
   "TortoiseAdded",
   "TortoiseConflict",
   "TortoiseNotInCVS",
   "TortoiseIgnored"
};

static const wxChar* pszIconDescriptions[] =
{
   _("In CVS"),
   _("In CVS, read-only"),
   _("Modified"),
   _("Added"),
   _("Needs merge"),
   _("Not in CVS"),
   _("Ignored by CVS")
};


// static
bool DoIconSetDialog(wxWindow* parent, std::string& currentIconSet)
{
   bool bResult = false;
   IconSetDialog* dlg = new IconSetDialog(parent, currentIconSet);
   if (dlg->ShowModal() == wxID_OK)
   {
      std::string* ps = reinterpret_cast<std::string*>(dlg->myListBox->GetClientData(dlg->myListBox->GetSelection()));
      if (ps)
          currentIconSet = *ps;
      else
         currentIconSet.clear();
      bResult = true;
   }

   DestroyDialog(dlg);
   return bResult;
}


enum
{
   ICONSETDLG_ID_LISTBOX = 10001
};

BEGIN_EVENT_TABLE(IconSetDialog, ExtDialog)
   EVT_COMMAND(ICONSETDLG_ID_LISTBOX, wxEVT_COMMAND_LISTBOX_SELECTED, IconSetDialog::ListBoxChanged)
END_EVENT_TABLE()



IconSetDialog::IconSetDialog(wxWindow* parent, std::string& currentIconSet)
   : ExtDialog(parent, -1, _("TortoiseCVS - Icon sets"),
               wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN)
{
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

   // Main box with everything in it
   wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);

   wxString labeltext = _("Please select an icon set to be used as overlay icons:");
   wxStaticText* label = new wxStaticText(this, -1, labeltext);
   sizerTop->Add(label, 0, wxTOP | wxLEFT | wxRIGHT, 10);

   // Listbox that will contain the different icon sets
   myListBox = new wxListBox(this, ICONSETDLG_ID_LISTBOX);

   // Insert iconsets
   InsertIconSets(myListBox, currentIconSet);
   myListBox->SetSize(-1, wxDLG_UNIT_Y(this, 75));

   sizerTop->Add(myListBox, 1, wxGROW | wxALL, 10);

   label = new wxStaticText(this, -1, _("Icon preview:"));
   sizerTop->Add(label, 0, wxLEFT | wxRIGHT, 10);

   wxGridSizer *sizerPreview = new wxGridSizer(4, 3, 0);

   wxStaticBitmap *sbmp;
   wxBitmap *bmp;
   int i;
   int nIcons = sizeof(pszIconNames) / sizeof(*pszIconNames);
   for (i = 0; i < nIcons; i++)
   {
      wxString labelText(wxGetTranslation(pszIconDescriptions[i]));
      labelText += _(":");
      label = new wxStaticText(this, -1, labelText);
      sizerPreview->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT | wxTOP, 5);
      bmp = new wxBitmap(32, 32, -1);
      sbmp = new wxStaticBitmap(this, -1, *bmp);
      delete bmp;
      myPreviews.push_back(sbmp);
      sizerPreview->Add(sbmp, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT | wxTOP, 5);
   }

   sizerTop->Add(sizerPreview, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

   PreviewIconSet(currentIconSet);

   // OK + cancel button
   wxBoxSizer *sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   wxButton* ok = new wxButton(this, wxID_OK, _("OK"));
   wxButton* cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));

   sizerConfirm->Add(ok, 0, wxGROW | wxALL, 5);
   sizerConfirm->Add(cancel, 0, wxGROW | wxALL, 5);

   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxALL, 0);

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);

   SetTortoiseDialogPos(this, GetRemoteHandle());
}


// Destructor
IconSetDialog::~IconSetDialog()
{
   int c = myListBox->GetCount();
   for (int i = 0; i < c; ++i)
      delete reinterpret_cast<std::string*>(myListBox->GetClientData(i));
}



// Add icon sets to listbox
void IconSetDialog::InsertIconSets(wxListBox* aListBox, const std::string& currentIconSet)
{
   aListBox->Append(_("No icons"));
   std::vector<std::string> iconSets;
   TortoiseRegistry::ReadValueNames("Icons", iconSets);
   std::vector<std::string>::const_iterator it = iconSets.begin();
   wxString selectedEntry;
   while (it != iconSets.end())
   {
       std::string key("Icons\\");
       key += *it;
       wxString name(TortoiseRegistry::ReadWxString(key));
       wxString entry(wxGetTranslation(name));
       if (*it == currentIconSet)
           selectedEntry = entry;
       aListBox->Append(entry, new std::string(*it));
       ++it;
   }
   if (selectedEntry.empty())
       aListBox->SetSelection(0);
   else
       aListBox->SetStringSelection(selectedEntry);
}



// Preview an iconset
void IconSetDialog::PreviewIconSet(const std::string& iconSet)
{
   std::string sIconPath;
   std::string sIconFile;
   if (!iconSet.empty())
      sIconPath = GetIconSetPath(iconSet);
   
   // Load icon for standard file
   SHFILEINFO sfi;
   SHGetFileInfo(wxT("*.txt"), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
                 SHGFI_ICON | SHGFI_USEFILEATTRIBUTES);
   wxIcon icoFile;
   icoFile.SetHICON((WXHICON) sfi.hIcon);

   wxPen pen(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW), 1, wxSOLID);
   wxBrush brush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW), wxSOLID);
   
   int i = 0;
   std::vector<wxStaticBitmap*>::const_iterator it = myPreviews.begin();
   while (it != myPreviews.end())
   {
      // Get DC for bitmap 
      wxMemoryDC dc;
      dc.SelectObject((*it)->GetBitmap());

      // Paint background
      dc.SetPen(pen);
      dc.SetBrush(brush);
      dc.DrawRectangle(0, 0, 32, 32);

      // Paint file icon
      dc.DrawIcon(icoFile, 0, 0);

      // Load tortoise icon
      if (!sIconPath.empty())
      {
         sIconFile = EnsureTrailingDelimiter(sIconPath) + pszIconNames[i] + ".ico";

         wxIcon ico(wxText(sIconFile), wxBITMAP_TYPE_ICO, 32, 32);

         // Paint overlay icon
         dc.DrawIcon(ico, 0, 0);
      }

      (*it)->Refresh();
      ++i;
      ++it;
   }
}


// Listbox changed
void IconSetDialog::ListBoxChanged(wxCommandEvent& WXUNUSED(event))
{
   std::string* ps = reinterpret_cast<std::string*>(myListBox->GetClientData(myListBox->GetSelection()));
   std::string s;
   if (ps)
      s = *ps;
   PreviewIconSet(s);
}
