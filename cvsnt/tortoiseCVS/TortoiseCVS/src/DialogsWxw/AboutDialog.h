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

#ifndef ABOUTDLG_H
#define ABOUTDLG_H

#include <string>
#include <wx/wx.h>
#include "ExtDialog.h"

class AboutDialog : ExtDialog
{
public:
   friend bool DoAboutDialog(wxWindow* parent, 
                             const std::string& version,
                             const std::string& cvsclientversion,
                             const std::string& cvsserverversion,
                             const wxString& sshversion);

private:
   AboutDialog(wxWindow* parent, 
               const std::string& version,
               const std::string& cvsclientversion,
               const std::string& cvsserverversion,
               const wxString& sshversion);

   void ShowCredits(wxCommandEvent& event);

   wxButton* m_Ok;

   // Windows callbacks
   WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam);

   DECLARE_EVENT_TABLE()
};

#endif

