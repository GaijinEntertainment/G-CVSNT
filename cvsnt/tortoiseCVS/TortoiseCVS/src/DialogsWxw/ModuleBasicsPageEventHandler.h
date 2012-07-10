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

#ifndef MODULE_BASICS_PAGE_EVT_HANDLER_H
#define MODULE_BASICS_PAGE_EVT_HANDLER_H

const int MODBASICS_ID_TREECTRL = 10100;

#include <wx/treectrl.h>

class ModuleBasicsPageEvtHandlerParent
{
public:
    virtual class RepoTreeCtrl* GetTreeCtrl() = 0;

    virtual wxComboBox* GetModuleCombo() = 0;

    virtual std::string& GetModule() = 0;

    virtual void UpdateModule() = 0;

    virtual void GoBrowse(const wxTreeItemId& openingItem) = 0;

    virtual int& GetSortColumn() = 0;

    virtual void SortItems() = 0;
};


class ModuleBasicsPageEvtHandler : public wxEvtHandler
{
public:
    ModuleBasicsPageEvtHandler(ModuleBasicsPageEvtHandlerParent* parent)
      : myParent(parent),
        myIgnoreChange(false)
   {
   }

   // TreeList control
   
   void OnListTreeSelChanged(wxTreeEvent& event);
   void OnListTreeExpand(wxTreeEvent& event);

   void OnListTreeRightClick(wxTreeEvent& event);

   void OnListTreeContextMenu(wxContextMenuEvent& event);

   void OnListTreeColClick(class wxListEvent& event);

private:
    ModuleBasicsPageEvtHandlerParent*       myParent;
   bool                 myIgnoreChange;

   DECLARE_EVENT_TABLE()
};


// Container for tree item data
class ModuleBasicsPageTreeItemData : public wxTreeItemData
{
public:
    ModuleBasicsPageTreeItemData(CRepoNode* node) : m_Node(node)
    {
    }
    
    CRepoNode* m_Node;
};


#endif
