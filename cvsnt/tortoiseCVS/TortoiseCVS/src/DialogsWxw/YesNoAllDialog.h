// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2005 - Torsten Martinsen
// <torsten@tiscali.dk> - January 2005

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

#ifndef YESNOALLDIALOG_H
#define YESNOALLDIALOG_H

#include <windows.h>
#include <string>
#include <vector>

#include "ExtDialog.h"

class YesNoAllDialog : private ExtDialog
{
public:
   enum YesNoAll
   {
      Yes,
      No,
      YesAll,
      NoAll,
      Cancel
   };

   friend YesNoAll DoYesNoAllDialog(wxWindow* parent,
                                    const wxString& message,
                                    bool yesdefault,
                                    bool withCancel = false);

private:
   YesNoAllDialog(wxWindow* parent, 
                  const wxString& message,
                  bool yesdefault,
                  bool withCancel);

   void OnYes(wxCommandEvent& event);
   void OnNo(wxCommandEvent& event);
   void OnCancel(wxCommandEvent& event);
   void OnYesAll(wxCommandEvent& event);
   void OnNoAll(wxCommandEvent& event);

   DECLARE_EVENT_TABLE()
};

#endif
