// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2003 - Hartmut Honisch
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

#ifndef EXTSTATICTEXT_TEXT_H
#define EXTSTATICTEXT_TEXT_H


#include <wx/wx.h>


class ExtStaticText : public wxStaticText
{
public:
   // Constructor
   ExtStaticText(wxWindow *parent, wxWindowID id, const wxString& label,
      const wxPoint& pos = wxDefaultPosition,
      const wxSize& size = wxDefaultSize,
      long style = 0,
      const wxString& name = wxStaticTextNameStr);


   // Set label
   void SetLabel(const wxString& label);
   // Get label
   wxString GetLabel() const;
   // Set theme support
   void SetThemeSupport(bool enabled);
   // Set autosize
   void SetAutosize(bool enabled);
   // Theme support
   void ApplyParentThemeBackground(const wxColour& bg);

private:
   // Size event
   void OnSize(wxSizeEvent& e);

   // label text
   wxString m_Label;
   // Support themens
   bool m_SupportThemes;
   // Autosize
   bool m_Autosize;

   DECLARE_EVENT_TABLE()
};





#endif
