// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2002 - Hartmut Honisch
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
#include "FilenameText.h"
#include <windows.h>
#include <shlwapi.h>

BEGIN_EVENT_TABLE(FilenameText, wxStaticText)
   EVT_SIZE(FilenameText::OnSize)
END_EVENT_TABLE()





// Constructor
FilenameText::FilenameText(wxWindow* parent, wxWindowID id, const wxString& templateText, 
                           const wxString& filenameText, const wxPoint& pos, const wxSize& size)
    : wxStaticText(parent, id, BuildLabel(templateText, wxT("...\\...")), pos, size, wxST_NO_AUTORESIZE)
{
   m_filenameText = filenameText;
   m_templateText = templateText;
}



// Set filename
void FilenameText::SetFilenameText(const wxString& filenameText)
{
   m_filenameText = filenameText;
}



// Get filename
wxString FilenameText::GetFilenameText()
{
   return m_filenameText;
}



// Set template
void FilenameText::SetTemplateText(const wxString& templateText)
{
   m_templateText = templateText;
}



// Get template
wxString FilenameText::GetTemplateText()
{
   return m_templateText;
}



// Build label text
wxString FilenameText::BuildLabel(const wxString& templateText, const wxString& filenameText)
{
   return Printf(templateText.c_str(), filenameText.c_str()).c_str();
}


// Build filename
wxString FilenameText::BuildFilename(const wxString& prefix, const wxString& dir, const wxString& filename)
{
   wxString s;
   if (!prefix.IsEmpty())
   {
      s.Append(prefix);
      s.Append(wxT("\\"));
   }
   if (!dir.IsEmpty())
   {
      s.Append(dir);
      s.Append(wxT("\\"));
   }
   s.Append(filename);
   return s;
}




// Size event
void FilenameText::OnSize(wxSizeEvent& e)
{
   // Shorten filename
   ShortenFilename(e.GetSize().x);

   // Pass event on to next handler
   e.Skip();

}



// Shorten a filename
void FilenameText::ShortenFilename(int width)
{
   // Does the whole filename fit?
   wxString sLabel = BuildLabel(m_templateText, m_filenameText);
   int x, y;
   GetTextExtent(sLabel, &x, &y);
   if (x <= width)
   {
      SetLabel(sLabel);
      return;
   }

   TCHAR buf[_MAX_PATH + 1];
   int newLen = static_cast<int>(m_filenameText.Length());
   int d = static_cast<int>(m_filenameText.Length());
   do
   {
      PathCompactPathEx(buf, m_filenameText.GetData(), newLen, 0);
      sLabel = BuildLabel(m_templateText, buf);
      GetTextExtent(sLabel, &x, &y);
      d = (d + 1) / 2;
      if (x <= width)
      {
         newLen = newLen + d;
      }
      else
      {
         newLen = newLen - d;
      }
   } while ((d > 1) || (x > width));
   SetLabel(sLabel);
}


