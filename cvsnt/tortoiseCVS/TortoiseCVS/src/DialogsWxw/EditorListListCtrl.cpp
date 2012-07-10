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

#include "StdAfx.h"
#include "EditorListDialog.h"
#include "EditorListListCtrl.h"
#include "ExplorerMenu.h"
#include "WxwHelpers.h"
#include "../Utils/TortoiseRegistry.h"
#include "../Utils/ProcessUtils.h"
#include <Utils/Preference.h>


EditorListListCtrl::EditorListListCtrl(EditorListDialog* parent,
                                       wxWindow* parentwindow,
                                       wxWindowID id,
                                       wxColour& editColour)
   : ExtListCtrl(parentwindow, id, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_ALIGN_LEFT),
     myParent(parent),
     myEditColour(editColour)
{
   InsertColumn(0, _("Filename"), wxLIST_FORMAT_LEFT);
   InsertColumn(1, _("Editors"), wxLIST_FORMAT_LEFT);
   InsertColumn(2, _("Date"), wxLIST_FORMAT_LEFT);
   InsertColumn(3, _("Host"), wxLIST_FORMAT_LEFT);
   InsertColumn(4, _("Local path"), wxLIST_FORMAT_LEFT);
   InsertColumn(5, _("Bug number"), wxLIST_FORMAT_LEFT);
   for (int i = 0; i < 6; ++i)
   {
      std::ostringstream ss;
      ss << "Dialogs\\Editors\\Column Width\\Column " << i;
      int colWidth = 100;
      TortoiseRegistry::ReadInteger(ss.str(), colWidth);
      SetColumnWidth(i, wxDLG_UNIT_X(this, colWidth));
   }
}

EditorListListCtrl::~EditorListListCtrl()
{
   for (int i = 0; i < 5; ++i)
   {
      int colWidth = GetColumnWidth(i);
      colWidth = this->ConvertPixelsToDialog(wxSize(colWidth, i)).GetWidth();
      std::ostringstream ss;
      ss << "Dialogs\\Editors\\Column Width\\Column " << i;
      TortoiseRegistry::WriteInteger(ss.str(), colWidth);
   }
}

int EditorListListCtrl::OnCustomDraw(WXLPARAM lParam)
{
   TDEBUG_ENTER("EditorListListCtrl::OnCustomDraw");
   NMLVCUSTOMDRAW* p = (NMLVCUSTOMDRAW*) lParam;

   switch (p->nmcd.dwDrawStage)
   {
   case CDDS_PREPAINT:
      return CDRF_NOTIFYITEMDRAW;

   case CDDS_ITEMPREPAINT:
      return CDRF_NOTIFYSUBITEMDRAW;
         
   case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
   {
      TDEBUG_TRACE("CDDS_ITEMPREPAINT: " << p->iSubItem);
      if (p->iSubItem == 1)
      {
         wxListItem item;
         item.SetId(p->nmcd.dwItemSpec);
         item.SetColumn(1);
         item.SetMask(wxLIST_MASK_TEXT);
         GetItem(item);
         const std::string& name(wxAscii(item.m_text.c_str()));
         TDEBUG_TRACE("name " << name);
         if (name == GetUserName())
            // Store the color back in the NMLVCUSTOMDRAW struct.
            p->clrText = myEditColour.GetPixel();
      }
      return CDRF_NEWFONT;
   }
         
   default:
      break;
   }
   
   return CDRF_DODEFAULT;
}

void EditorListListCtrl::OnContextMenu(wxContextMenuEvent& event)
{
   wxMenu* popupMenu = 0;
   ExplorerMenu* explorerMenu = 0;
   int i;
   bool bNeedsSeparator = false;
   std::string sDir;

   std::vector<std::string> selectedFiles;
   for (i = 0; i < GetItemCount(); i++)
      if (IsSelected(i))
         selectedFiles.push_back((*(myParent->myEditedFiles))[GetItemData(i)].myFilename);

   if (selectedFiles.empty())
      return;

   // Add common contextmenu entries
   popupMenu = new wxMenu();
   // Add explorer menu
   if (StripCommonPath(selectedFiles, sDir))
   {
      if (bNeedsSeparator)
      {
         popupMenu->AppendSeparator();
         bNeedsSeparator = false;
      }
      explorerMenu = new ExplorerMenu();
      explorerMenu->FillMenu((HWND) GetHandle(), sDir, selectedFiles);
      popupMenu->Append(0, _("Explorer"), explorerMenu);
   }

   wxPoint pos = event.GetPosition();
   // Calc selection
   if (pos == wxPoint(-1, -1))
   {
      pos = GetContextMenuPos(GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED));
      pos = ClientToScreen(pos);
   }
   
   PopupMenu(popupMenu, ScreenToClient(pos));

   delete popupMenu;
}


BEGIN_EVENT_TABLE(EditorListListCtrl, ExtListCtrl)
   EVT_CONTEXT_MENU(EditorListListCtrl::OnContextMenu)
END_EVENT_TABLE()
