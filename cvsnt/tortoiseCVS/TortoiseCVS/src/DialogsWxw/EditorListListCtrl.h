// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2005 - Torsten Martinsen
// <torsten@tiscali.dk> - March 2005

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

#ifndef EDITOR_LIST_LISTCTRL_H
#define EDITOR_LIST_LISTCTRL_H

#include <wx/wx.h>
#include "ExtListCtrl.h"

class EditorListListCtrl : public ExtListCtrl
{
public:
   EditorListListCtrl(EditorListDialog* parent, wxWindow* parentwindow, wxWindowID id, wxColour& editColour);
   
   ~EditorListListCtrl();

   void OnContextMenu(wxContextMenuEvent& event);
   
protected:
   int EditorListListCtrl::OnCustomDraw(WXLPARAM lParam);

private:
   EditorListDialog*    myParent;
   wxColour&            myEditColour;

   DECLARE_EVENT_TABLE()
};

#endif
