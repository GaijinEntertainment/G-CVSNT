// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2007 - Torsten Martinsen
// <torsten@vip.cybercity.dk> - March 2007

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

#include <wx/listctrl.h>

#include "RepoTreeCtrl.h"
#include "ModuleBasicsPageEventHandler.h"


BEGIN_EVENT_TABLE(ModuleBasicsPageEvtHandler, wxEvtHandler)
  EVT_TREE_SEL_CHANGED(MODBASICS_ID_TREECTRL,           ModuleBasicsPageEvtHandler::OnListTreeSelChanged)
  EVT_TREE_ITEM_EXPANDED(MODBASICS_ID_TREECTRL,         ModuleBasicsPageEvtHandler::OnListTreeExpand)
  EVT_TREE_ITEM_RIGHT_CLICK(MODBASICS_ID_TREECTRL,      ModuleBasicsPageEvtHandler::OnListTreeRightClick) 
  EVT_CONTEXT_MENU(                                     ModuleBasicsPageEvtHandler::OnListTreeContextMenu) 
  EVT_LIST_COL_CLICK(MODBASICS_ID_TREECTRL,             ModuleBasicsPageEvtHandler::OnListTreeColClick)
END_EVENT_TABLE()

void ModuleBasicsPageEvtHandler::OnListTreeSelChanged(wxTreeEvent& event)
{
   TDEBUG_ENTER("ModuleBasicsPageEvtHandler::OnListTreeSelChanged");

   if (myIgnoreChange)
   {
      myIgnoreChange = false;
      return;
   }

   wxArrayTreeItemIds sel;
   int selCount = static_cast<int>(myParent->GetTreeCtrl()->GetSelections(sel));
   if (selCount == 1)
   {
      wxTreeItemData* thisdata = myParent->GetTreeCtrl()->GetItemData(sel[0]);
      if (thisdata)
      {
         CRepoNode* thisnode = ((ModuleBasicsPageTreeItemData*) thisdata)->m_Node;
        // If it's a module then just the name goes in the module box.
        // If it's a directory or file then we need to traverse the entire 
        // tree back up to the module collecting all the names on the way...
        // unless the CRepoNode class can do that for us when it creates
        // the node in the first place.... that'd be good...
        switch (thisnode->GetType())
        {
        case kNodeModule:
        {
           CRepoNodeModule* modulenode = (CRepoNodeModule*) (thisnode);

           myParent->GetModuleCombo()->SetValue(wxText(modulenode->c_str()));
           myParent->GetModule() = wxAscii(myParent->GetModuleCombo()->GetValue().c_str());
           myParent->UpdateModule();
        }
        break;
        case kNodeDirectory:
        {
           CRepoNodeDirectory* directorynode = (CRepoNodeDirectory*) (thisnode);

           myParent->GetModuleCombo()->SetValue(wxText(directorynode->c_str()));
           myParent->GetModule() = wxAscii(myParent->GetModuleCombo()->GetValue().c_str());
           myParent->UpdateModule();
        }
        break;
        case kNodeFile:
        {
           CRepoNodeFile* filenode = (CRepoNodeFile*) (thisnode);
           
           myParent->GetModuleCombo()->SetValue(wxText(filenode->c_str()));
           myParent->GetModule() = wxAscii(myParent->GetModuleCombo()->GetValue().c_str());
           myParent->UpdateModule();
        }
        break;
        default:
           break;
        }
      }
   }
}

void ModuleBasicsPageEvtHandler::OnListTreeExpand(wxTreeEvent& event)
{
   TDEBUG_ENTER("ModuleBasicsPageEvtHandler::OnListTreeExpand");
   wxTreeItemId openingitem = event.GetItem();

   long c = 0; // Cookie
   wxTreeItemData* thisdata = myParent->GetTreeCtrl()->GetItemData(openingitem);
   if (thisdata)
   {
      wxTreeItemId childitem = myParent->GetTreeCtrl()->GetFirstChild(openingitem, c);
      wxTreeItemData* childdata = myParent->GetTreeCtrl()->GetItemData(childitem);
      CRepoNode* childnode = ((ModuleBasicsPageTreeItemData*) childdata)->m_Node;
      if (childnode->GetType() == kNodeRubbish)
      {
         myParent->GetTreeCtrl()->DeleteChildren(openingitem);
         myParent->GetTreeCtrl()->SelectItem(openingitem);
         myParent->GoBrowse(openingitem);
      }
   }
}


void ModuleBasicsPageEvtHandler::OnListTreeRightClick(wxTreeEvent& event)
{
   TDEBUG_ENTER("ModuleBasicsPageEvtHandler::OnListTreeRightClick");
   if (!myParent->GetTreeCtrl()->IsSelected(event.GetItem())
       && GetAsyncKeyState(VK_APPS) >= 0)
   {
      myParent->GetTreeCtrl()->SelectItem(event.GetItem());
   }
}


void ModuleBasicsPageEvtHandler::OnListTreeContextMenu(wxContextMenuEvent& event)
{
   TDEBUG_ENTER("ModuleBasicsPageEvtHandler::OnListTreeContextMenu");
}

void ModuleBasicsPageEvtHandler::OnListTreeColClick(wxListEvent& event)
{
    myParent->GetSortColumn() = event.GetColumn();
    myParent->SortItems();
}
