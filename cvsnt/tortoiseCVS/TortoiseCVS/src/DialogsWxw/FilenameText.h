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

#ifndef FILENAME_TEXT_H
#define FILENAME_TEXT_H


#include <wx/wx.h>


class FilenameText : public wxStaticText
{
public:
   // Constructor
   FilenameText(wxWindow* parent, wxWindowID id, const wxString& templateText, 
                const wxString& filenameText, const wxPoint& pos = wxDefaultPosition, 
                const wxSize& size = wxDefaultSize);

   // Set filename
   void SetFilenameText(const wxString& filenameText);
   // Get filename
   wxString GetFilenameText();
   // Set template
   void SetTemplateText(const wxString& templateText);
   // Get template
   wxString GetTemplateText();

private:
   // Build label text
   static wxString BuildLabel(const wxString& templateText, const wxString& filenameText);
   // Build filename
   static wxString BuildFilename(const wxString& prefix, const wxString& dir, 
                                 const wxString& filename);
   // Size event
   void OnSize(wxSizeEvent& e);
   // Shorten a filename
   void ShortenFilename(int width);

   // template text
   wxString m_templateText;
   // Filename text
   wxString m_filenameText;

   DECLARE_EVENT_TABLE()
};





#endif
