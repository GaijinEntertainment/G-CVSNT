// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2002 - Torsten Martinsen
// <torsten@image.dk> - May 2002

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

#ifndef HISTORY_DIALOG_H
#define HISTORY_DIALOG_H

#include <string>
#include <vector>

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/listctrl.h>
#include <wx/imaglist.h>

#include <windows.h>
#include "ExtDialog.h"
#include "ExtSplitterWindow.h"
#include "../Utils/LogUtils.h"
#include "../cvstree/common.h"


class CVSRevisionData;
class CVSRevisionDataSortCriteria;
class CRcsFile;
class HistoryDialogEvtHandler;
class HistoryTreeCtrl;
class ExtTextCtrl;

class HistoryDialog : public ExtDialog
{
public:
   friend bool DoHistoryDialog(wxWindow* parent, 
                               const std::string& filename);
   friend class HistoryDialogEvtHandler;
   
   static void InitializeStatics();
   static void CleanupStatics();

protected:
   static wxColour* myBranchColor;
   static wxColour* mySymbolicColor;
   static int       myRefCount;

private:
   // Container for tree item data
   class MyTreeItemData : public wxTreeItemData
   {
   public:
      MyTreeItemData(CLogNode* node) : m_Node(node)
      {
      }
      
      CLogNode* m_Node;
   };

   HistoryDialog(wxWindow* parent, const std::string& filename);
   ~HistoryDialog();

   void ProcessNode(CLogNode* node, wxTreeItemId rootId);
   void Populate();
   void Refresh();
   void RefreshFileStatus();
   void Update();
   std::string PrepareComment(const std::string& comment);
   void OnTextUrl(wxCommandEvent& event);
   
   CLogNode* GetTree() const;
   const std::string& GetFilename() const   { return myFilename; }

   // Menus
   
   void OnMakeTag(wxCommandEvent& WXUNUSED(event));

   void OnMakeBranch(wxCommandEvent& WXUNUSED(event));

   void OnMenuDiffWorkfile(wxCommandEvent& event);

   void OnMenuDiffSelected(wxCommandEvent& event);

   void OnMenuMerge(wxCommandEvent& event);

   void OnMenuUndo(wxCommandEvent& event);

   void OnMenuView(wxCommandEvent& event);

   void OnMenuGet(wxCommandEvent& event);

   void OnMenuGetClean(wxCommandEvent& event);

   void OnMenuSaveAs(wxCommandEvent& event);

   void OnMenuCopyToClipboard(wxCommandEvent& event);

   void OnMenuAnnotate(wxCommandEvent& event);


   std::string                  myFilename;
   std::string                  myDir;
   std::string                  myFile;
   std::string                  mySandboxRevision;
   bool                         myOfferView;
   wxTreeItemId                 mySandboxItemId;
   // ID of the item whose comment is being edited
   wxTreeItemId                 myCommentId;
   std::string                  mySandboxStickyTag;
   wxString                     myDefaultKWS;
   HistoryTreeCtrl*             myListCtrl;
   wxCheckBox*                  myShowTagsCheckBox;
   ExtTextCtrl*                 myComment;
   wxButton*                    myButtonApply;
   wxStaticText*                myTextTag;
   wxStaticText*                myTextStatus;
   wxStaticText*                myTextKeywordSubst;
   wxStaticText*                myTextOtherInfo;
   ExtSplitterWindow*           mySplitter;
   wxStatusBar*                 myStatusBar;
       
   wxImageList*                 myImageList;
   std::vector<int>             myColumnWidths;
   bool                         myCommentChanged;
   bool                         myCommentChangedEnabled;
   CLogNode*                    myTree;
   bool                         myTreeUpsideDown;
   int                          mySortColumn;

   std::string                  myRevision1;
   std::string                  myRevision2;

   ParserData                   myParserData;
   
   void CommentChanged(wxCommandEvent& e);
   void Apply(wxCommandEvent& e);
   void SymbolicCheckChanged(wxCommandEvent& e);
   
   void SortItems();

   void GetRegistryData();
   void SaveRegistryData();
    
   bool AreItemsSelected();
   void UpdateComment();
   // Get revision that applies to a node
   std::string GetNodeRevision(wxTreeItemId item);

   static int wxCALLBACK CompareFunc(long item1, long item2, long sortData);

   friend class HistoryTreeCtrl;

   DECLARE_EVENT_TABLE()
};

#endif //HISTORY_DIALOG_H
