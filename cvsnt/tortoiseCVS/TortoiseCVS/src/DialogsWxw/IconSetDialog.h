// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2003 - Hartmut Honisch
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

#ifndef ICONSET_DLG_H
#define ICONSET_DLG_H

#include <string>
#include <wx/wx.h>
#include <vector>
#include "ExtDialog.h"

class IconSetDialog : ExtDialog
{
public:
   friend bool DoIconSetDialog(wxWindow* parent, std::string& currentIconSet);

private:
   IconSetDialog(wxWindow* parent, std::string& currentIconSet);

   ~IconSetDialog();

   // Add icon sets to listbox
   void InsertIconSets(wxListBox* aListBox, const std::string& currentIconSet);

   // Preview an iconset
   void PreviewIconSet(const std::string& iconSet);

   // icon preview bitmaps
   std::vector<wxStaticBitmap*> myPreviews;

   // Listbox changed
   void ListBoxChanged(wxCommandEvent& event);

   // Listbox with icon sets 
   wxListBox* myListBox;

   DECLARE_EVENT_TABLE()
};

#endif

