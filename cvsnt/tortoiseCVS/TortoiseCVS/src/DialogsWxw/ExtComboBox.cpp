// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2002 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - May 2003

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
#include "ExtComboBox.h"
#include "../Utils/SyncUtils.h"
#include <map>
#include <algorithm>
#include <wx/msw/private.h>


#define MAX_DROPDOWN_WIDTH 500


// Constructor
ExtComboBox::ExtComboBox(wxWindow *parent, wxWindowID id, 
                         const wxString& value, const wxPoint& pos,
                         const wxSize& size, int n, const wxString choices[],
                         long style, const wxValidator& validator, 
                         const wxString& name)
              
   : wxComboBox(parent, id, value, pos, size, n, choices, style, validator, name)
{
   long l = GetWindowLong((HWND) GetHWND(), GWL_STYLE);
   l |= WS_TABSTOP;
   SetWindowLong((HWND) GetHWND(), GWL_STYLE, l);
}


// Destructor
ExtComboBox::~ExtComboBox()
{
}


// Notifications
bool ExtComboBox::MSWCommand(WXUINT param, WXWORD WXUNUSED(id))
{
   if (param == CBN_DROPDOWN)
   {
      SetAutoDropDownWidth();
   }
   else if (param == CBN_SELENDOK)
   {
      /* Workaround for bug in wxWidgets */
      int sel = GetSelection();
      if (sel > -1)
      {
         wxCommandEvent event(wxEVT_COMMAND_COMBOBOX_SELECTED, GetId());
         event.SetInt(sel);
         event.SetEventObject(this);
         event.SetString(GetValue());
         ProcessCommand(event);
      }
   }
   return wxComboBox::MSWCommand(param, 0);
}
 


// Set width of dropdown box
void ExtComboBox::SetAutoDropDownWidth()
{
   // iterate through items
   int w = GetSize().GetWidth();
   int n = GetCount();
   int i, x, y, sw;
   wxString s;
   RECT r;
   SendMessage(GetHwnd(), CB_GETDROPPEDCONTROLRECT, 0, (LPARAM) &r);
   int h = SendMessage(GetHwnd(), CB_GETITEMHEIGHT, 0, 0);
   int nmax = (r.bottom - r.top - GetSize().GetHeight() - 2) / h;
   if (n > nmax)
   {
      sw = GetSystemMetrics(SM_CXVSCROLL);
   }
   else
   {
      sw = 0;
   }
   for (i = 0; i < n; i++)
   {
      s = GetString(i);
      GetTextExtent(s, &x, &y);
      x += sw + 8;
      x = std::min(x, ConvertDialogToPixels(wxSize(MAX_DROPDOWN_WIDTH, 0)).GetWidth());
      if (x > w)
         w = x;

   }
   SendMessage(GetHwnd(), CB_SETDROPPEDWIDTH, w, 0);
}
