// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2003 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - December 2003

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

#ifndef CONFLICT_LIST_DIALOG_H
#define CONFLICT_LIST_DIALOG_H

#include <string>
#include <vector>
#include <wx/wx.h>
#include "ExtDialog.h"
#include "../CVSGlue/CVSStatus.h"


class ExtListCtrl;
class wxListEvent;

class ConflictListDialog : ExtDialog
{
public:
   friend bool DoConflictListDialog(wxWindow* parent, 
                                    const std::vector<std::string>& files);
  
private:
   class ItemData 
   {
   public:
      std::string m_Filename;
      CVSStatus::FileStatus m_Status;
      CVSStatus::FileFormat m_Format;
   };

   class ListCtrlEventHandler : public wxEvtHandler
   {
   public:
      ListCtrlEventHandler(ConflictListDialog* parent)
         : myParent(parent)
      {
      }
   
      void OnContextMenu(wxContextMenuEvent& event);

      void OnDblClick(wxListEvent& event);

      void ListColumnClick(wxListEvent& e);

   private:
      ConflictListDialog* myParent;

      DECLARE_EVENT_TABLE()
   };


   ConflictListDialog(wxWindow* parent, const std::vector<std::string>& modified);
   ~ConflictListDialog();

   void OnContextMenu(wxContextMenuEvent& event);
   void OnDblClick(wxListEvent& event);

   void OnMenuMerge(wxCommandEvent& event);
   void OnMenuFavorLocal(wxCommandEvent& event);
   void OnMenuFavorRepository(wxCommandEvent& event);
   void OnMenuHistory(wxCommandEvent& event);
   void OnMenuGraph(wxCommandEvent& event);
   void OnMenuDiff(wxCommandEvent& e);
   void ListColumnClick(wxListEvent& e);
   void RefreshItem(int i);
   
   // Add files to ExtListCtrl
   void AddFiles(const std::vector<std::string>& filenames,
                 const std::vector<ItemData*>& itemData);

   static int wxCALLBACK CompareFunc(long item1, long item2, long sortData);
   
   ExtListCtrl*                 myFiles;
   wxButton*                    myOK;
   std::string                  myStub;
   wxStatusBar*                 myStatusBar;

   friend class ListCtrlEventHandler;

   DECLARE_EVENT_TABLE()
};

#endif
