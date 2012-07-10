// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2003 - Torsten Martinsen
// <torsten@image.dk> - August 2003

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
#include "HistoryDialog.h"
#include "HistoryTreeCtrl.h"
#include "../Utils/TortoiseDebug.h"


IMPLEMENT_DYNAMIC_CLASS(HistoryTreeCtrl, wxTreeListCtrl)

   
HistoryTreeCtrl::HistoryTreeCtrl(HistoryDialog* dlg,
                                 wxWindow* parent, const wxWindowID id,
                                 const wxPoint& pos, const wxSize& size,
                                 long style)
   : wxTreeListCtrl(parent, id, pos, size, style),
     myParent(dlg)
{
   mySorter.GetRegistryData();
}

HistoryTreeCtrl::~HistoryTreeCtrl()
{
   mySorter.SaveRegistryData();
}

void HistoryTreeCtrl::SetSortParams(bool ascending, int key)
{
   mySorter.ascending = ascending;
   mySorter.key = key;
}

int HistoryTreeCtrl::OnCompareItems(const wxTreeItemId& item1,
                                    const wxTreeItemId& item2)
{
   CLogNode* node1 = ((HistoryDialog::MyTreeItemData*) myParent->myListCtrl->GetItemData(item1))->m_Node;
   CLogNode* node2 = ((HistoryDialog::MyTreeItemData*) myParent->myListCtrl->GetItemData(item2))->m_Node;
   if ((node1->GetType() != kNodeRev) || (node2->GetType() != kNodeRev))
      return 0;
   
   CLogNodeRev* revnode1 = (CLogNodeRev*) node1;
   CLogNodeRev* revnode2 = (CLogNodeRev*) node2; 
   
   return mySorter.Compare(revnode1->Rev(), revnode2->Rev());
}
