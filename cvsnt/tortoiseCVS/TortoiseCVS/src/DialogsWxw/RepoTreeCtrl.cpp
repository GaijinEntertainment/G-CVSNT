// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2005 - March Hare Software Ltd
// <arthur.barrett@march-hare.com> - July 2005

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
#include "ModuleBasicsPage.h"
#include "RepoTreeCtrl.h"
#include "../Utils/TortoiseDebug.h"


IMPLEMENT_DYNAMIC_CLASS(RepoTreeCtrl, wxTreeListCtrl)

   
RepoTreeCtrl::RepoTreeCtrl(wxWindow* parent, const wxWindowID id,
                           const wxPoint& pos, const wxSize& size,
                           long style)
   : wxTreeListCtrl(parent, id, pos, size, style)
{
}

RepoTreeCtrl::~RepoTreeCtrl()
{
}

void RepoTreeCtrl::SetSortParams(bool ascending, int key)
{
   // TODO?
}

int RepoTreeCtrl::OnCompareItems(const wxTreeItemId& item1,
                                 const wxTreeItemId& item2)
{
   CRepoNode* node1 = static_cast<ModuleBasicsPageTreeItemData*>(GetItemData(item1))->m_Node;
   CRepoNode* node2 = static_cast<ModuleBasicsPageTreeItemData*>(GetItemData(item2))->m_Node;
   if ((node1->GetType() != kNodeModule) || (node2->GetType() != kNodeModule))
      return 0;
   
   CRepoNodeModule* modnode1 = (CRepoNodeModule*) node1;
   CRepoNodeModule* modnode2 = (CRepoNodeModule*) node2; 
   
   return modnode1->c_str() == modnode2->c_str();
}
