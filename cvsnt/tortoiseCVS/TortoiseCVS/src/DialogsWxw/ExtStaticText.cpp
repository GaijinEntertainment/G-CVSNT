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

#include "StdAfx.h"
#include "ExtStaticText.h"
#include "WxwHelpers.h"

BEGIN_EVENT_TABLE(ExtStaticText, wxStaticText)
   EVT_SIZE(ExtStaticText::OnSize)
END_EVENT_TABLE()





// Constructor
ExtStaticText::ExtStaticText(wxWindow *parent, wxWindowID id, 
                             const wxString& label, const wxPoint& pos, const wxSize& size,
                             long style, const wxString& name)
   : wxStaticText(parent, id, label, pos, size, style |  
                  wxST_NO_AUTORESIZE, name),
     m_Label(label),
     m_SupportThemes(true),
     m_Autosize(true)
{
}



// Set label
void ExtStaticText::SetLabel(const wxString& label)
{
   m_Label = label;
}



// Get label
wxString ExtStaticText::GetLabel() const
{
   return m_Label;
}


// Set theme support
void ExtStaticText::SetThemeSupport(bool enabled)
{
   m_SupportThemes = enabled;
}


// Set autosize
void ExtStaticText::SetAutosize(bool enabled)
{
   m_Autosize = enabled;
}


// Theme support
void ExtStaticText::ApplyParentThemeBackground(const wxColour& bg)
{
   if (m_SupportThemes)
   {
      SetBackgroundColour(bg);
   }
}



// Size event
void ExtStaticText::OnSize(wxSizeEvent& e)
{
   if (m_Autosize)
   {
      // Wrap text
      m_Label = WrapText(e.GetSize().GetWidth(), *this, m_Label);
      wxStaticText::SetLabel(m_Label);
      wxSize size = DoGetBestSize();
      SetMinSize(size);
   }

   // Pass event on to next handler
   e.Skip();
}





