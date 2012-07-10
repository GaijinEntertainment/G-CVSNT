// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - May 2000

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
#include "CvsRootListCtrl.h"


enum
{
   MB_REMOVE = 10100,
   MB_REMOVE_ALL
};

BEGIN_EVENT_TABLE(CvsRootListCtrl, ExtListCtrl)
   EVT_CONTEXT_MENU(CvsRootListCtrl::OnContextMenu)
   EVT_KEY_DOWN(CvsRootListCtrl::OnKeyDown)
   EVT_MENU(MB_REMOVE,          CvsRootListCtrl::HistoryRemove)
   EVT_MENU(MB_REMOVE_ALL,      CvsRootListCtrl::HistoryRemoveAll)
END_EVENT_TABLE()


   
CvsRootListCtrl::CvsRootListCtrl(wxWindow* parent, int id)
   : ExtListCtrl(parent,
                 id,
                 wxDefaultPosition, wxDefaultSize,
                 wxLC_REPORT | wxLC_SINGLE_SEL)
{
}
   

void CvsRootListCtrl::OnContextMenu(wxContextMenuEvent& event)
{
   // Check if an item is selected
   bool bItemsSelected = GetSelectedItemCount() > 0;
   wxMenu menu;
   if (bItemsSelected)
   {
      wxString s(_("Remove"));
      s += wxT("\t");
      s += _("Del");
      s.Replace(wxT("\\tab"), wxT("\t"));
      menu.Append(MB_REMOVE, s);
      menu.AppendSeparator();
   }
   menu.Append(MB_REMOVE_ALL, _("Remove All"));
   wxPoint pos = event.GetPosition();
   // Calc selection
   if (pos == wxPoint(-1, -1))
   {
      pos = GetContextMenuPos(GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED));
      pos = ClientToScreen(pos);
   }
   PopupMenu(&menu, ScreenToClient(pos));
}

void CvsRootListCtrl::HistoryRemove(wxCommandEvent&)
{
   long sel = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   if (sel == -1)
      return;
   
   DeleteItem(sel);
   
   // select next item (so folk can hold down delete on the keyboard)
   if (GetItemCount() <= sel)
      sel --;
   if (sel < 0)
      return;
   SetItemState(sel, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
}

void CvsRootListCtrl::HistoryRemoveAll(wxCommandEvent&)
{
   DeleteAllItems();
}



void CvsRootListCtrl::OnKeyDown(wxKeyEvent& event)
{
   if (event.GetKeyCode() == WXK_DELETE)
   {
      wxCommandEvent e;
      HistoryRemove(e);
   }
}
