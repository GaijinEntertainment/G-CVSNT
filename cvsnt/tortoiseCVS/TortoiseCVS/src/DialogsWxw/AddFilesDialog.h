// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2002 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - October 2002

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

#ifndef ADD_FILESDIALOG_H
#define ADD_FILESDIALOG_H

#include <string>
#include <vector>
#include <set>
#include "ExtDialog.h"
#include "ExtListCtrl.h"
#include "ExtSplitterWindow.h"
#include <wx/treectrl.h>
#include <wx/imaglist.h>
#include "../CVSGlue/CVSStatus.h"
#include "ExplorerMenu.h"
#include "../Utils/FileTree.h"

class DirectoryGroup;
class DirectoryGroups;
class AddFilesListCtrl;

bool DoAddFilesDialog(wxWindow* parent, 
                      DirectoryGroups& groups,
                      const wxString& caption = wxT(""),
                      const wxString& label = wxT(""),
                      bool makingPatch = false);

class AddFilesDialog : public ExtDialog
{
public:
   friend bool DoAddFilesDialog(wxWindow* parent, 
                                DirectoryGroups& groups,
                                const wxString& caption,
                                const wxString& label,
                                bool makingPatch);
   friend class AddFilesListCtrl;

   // Constructor
   AddFilesDialog(wxWindow* parent,
                  DirectoryGroups& groups,
                  const wxString& caption,
                  const wxString& label,
                  bool makingPatch); 

   // Destructor
   virtual ~AddFilesDialog();

private:
   typedef std::vector<FileTree::DirNode*> DirNodeList;
   typedef std::set<FileTree::DirNode*> DirNodeSet;
   
   // Parse directory groups into tree structures
   void BuildFileTrees();

   // Build image list
   void BuildImageList();

   // Build the GUI tree
   void BuildGuiTree();

   // Add nodes to tree
   bool AddTreeNodes(wxTreeItemId guiNode, FileTree::DirNode* dataNode, 
                     DirectoryGroup* group, bool doShorten = true, 
                     const std::string& namePrefix = "");

   // Rebuild listview and header
   void RebuildListView();

   // Update all list items
   void UpdateListItems();

   // Add items to listview
   void AddListItems(FileTree::DirNode* node, const std::string& prefix, 
                     bool recurse);

   // Double click on list control item
   void DoubleClick(wxListEvent& event);

   // Click on list control column header
   void ColumnClick(wxListEvent& e);

   void ItemChecked(wxListEvent& e);

   void ItemUnchecked(wxListEvent& e);
   
   // Tree selection changed
   void TreeSelectionChanged(wxTreeEvent& event);

   // Tree right click
   void TreeRightClick(wxTreeEvent& event);

   // Tree context menu
   void TreeContextMenu(wxContextMenuEvent& event);

   // Populate tree context menu
   bool PopulateTreeContextMenu(const wxTreeItemId& id, wxMenu& popupMenu);

   // Show tree context menu
   void ShowTreeContextMenu(const wxPoint& position);

   // Checkbox "include subdirs" changed
   void IncludeSubdirsChanged(wxCommandEvent& e);

   // Read column widths from registry
   void ReadColumnWidths();

   // Write column widths to registry
   void WriteColumnWidths();

   // Get column widths from listbox
   void GetColumnWidths();

   // Set column widths in listbox
   void SetColumnWidths();

   void OkClick(wxCommandEvent& e);


   // Find unknown files
   FileTree::Node* FindUnknownFiles(FileTree::DirNode* dirNode);
   // Find unknown files
   FileTree::Node* FindUnknownFiles();
   // Select unknown files in listview
   void ListSelectUnknownFiles();
   
   // Analyze selected items in list
   void AnalyzeListSelection(bool& bAllDirs, CVSStatus::KeywordOptions& keywordOptions,
                             CVSStatus::FileFormat& fileFormat, CVSStatus::FileOptions& fileOptions,
                             const FileTree::Node* &commonParentNode);

   // Menu handler for "Check" in the list
   void MenuListCheck(wxCommandEvent& e);
   // Menu handler for "Uncheck" in the list
   void MenuListUncheck(wxCommandEvent& e);
   // Menu handler for "Autocheck" in the list
   void MenuListAutoCheck(wxCommandEvent& e);
   // Menu handler for "Check recursive" in the list
   void MenuListCheckRecursive(wxCommandEvent& e);
   // Menu handler for "Uncheck recursive" in the list
   void MenuListUncheckRecursive(wxCommandEvent& e);
   // Menu handler for "Autocheck recursive" in the list
   void MenuListAutoCheckRecursive(wxCommandEvent& e);

   // Menu handler for "Keywords->Normal" in the list
   void MenuListKeywordsNormal(wxCommandEvent& e);
   // Menu handler for "Keywords->Names" in the list
   void MenuListKeywordsNames(wxCommandEvent& e);
   // Menu handler for "Keywords->Values" in the list
   void MenuListKeywordsValues(wxCommandEvent& e);
   // Menu handler for "Keywords->Locker" in the list
   void MenuListKeywordsLocker(wxCommandEvent& e);
   // Menu handler for "Keywords->Disabled" in the list
   void MenuListKeywordsDisabled(wxCommandEvent& e);

   // Menu handler for "Format->ASCII" in the list
   void MenuListFormatAscii(wxCommandEvent& e);
   // Menu handler for "Format->Unicode" in the list
   void MenuListFormatUnicode(wxCommandEvent& e);
   // Menu handler for "Format->Binary" in the list
   void MenuListFormatBinary(wxCommandEvent& e);
   // Menu handler for "Format->Auto detect" in the list
   void MenuListFormatAuto(wxCommandEvent& e);

   // Menu handler for "Options->Binary Diff" in the list
   void MenuListOptionsBinDiff(wxCommandEvent& e);
   // Menu handler for "Options->Compressed" in the list
   void MenuListOptionsCompressed(wxCommandEvent& e);
   // Menu handler for "Options->Unix line endings" in the list
   void MenuListOptionsUnix(wxCommandEvent& e);
   void MenuListOptionsStaticFile(wxCommandEvent& e);


   // Menu handler for "Check" in the tree
   void MenuTreeCheck(wxCommandEvent& e);
   // Menu handler for "Uncheck" in the tree
   void MenuTreeUncheck(wxCommandEvent& e);
   // Menu handler for "AutoCheck" in the tree
   void MenuTreeAutoCheck(wxCommandEvent& e);
   // Menu handler for "Check recursive" in the tree
   void MenuTreeCheckRecursive(wxCommandEvent& e);
   // Menu handler for "Uncheck recursive" in the tree
   void MenuTreeUncheckRecursive(wxCommandEvent& e);
   // Menu handler for "Autoheck recursive" in the tree
   void MenuTreeAutoCheckRecursive(wxCommandEvent& e);

   // Set keyword expansion for files
   void SetKeywordOptions(CVSStatus::KeywordOptions keywords);
   // Set format for files
   void SetFileFormat(CVSStatus::FileFormat format);
   // Set file option for files
   void SetFileOption(CVSStatus::FileOption option, bool doSet);
   // One or several list items have been (un)checked
   void CheckedListItem(long index, bool checked, bool recursive, 
                        bool autocheck);
   // A tree item has been checked
   void CheckedTreeItem(wxTreeItemId item, bool checked, bool recursive,
                        bool autocheck);
   // Check items
   void CheckItems(const FileTree::Node* node, 
                   std::map<const FileTree*, DirNodeList>& changedDirs, bool checked, 
                   bool recursive, bool autocheck);
   // One or several items have been (un)checked
   void CheckedItem(std::vector<const FileTree::Node*>& itemsToProcess, 
                    bool checked, bool advanced, bool recursive, bool autocheck);
   // Get relative paths for selected list items
   void GetListSelectionRelativePaths(const FileTree::Node* parentNode, 
                                      std::vector<std::string>& files);
   
   // Calculate which nodes are enabled in the file tree
   void CalcFileTreeEnabledNodes(FileTree::DirNode* node,
                                 bool enabled, 
                                 bool uncheckDisabled);
   // Calculate which nodes are enabled in the gui tree
   void CalcGuiTreeEnabledNodes(wxTreeItemId item);
   // Sort items in listview
   void SortListItems();
   // Compare listitems
   static bool CompareListItems(FileTree::Node* node1,
                                FileTree::Node* node2);
   // Expand tree nodes
   void ExpandTreeNodes(wxTreeItemId item, int levels = -1);

   std::vector<FileTree*>       myFileTrees;
   wxTreeCtrl*                  myTreeCtrl;
   ExtListCtrl*                 myListCtrl;
   wxButton*                    myOK;
   ExtSplitterWindow*           mySplitter;
   DirectoryGroups&             myDirectoryGroups;
   std::vector<int>             myColumnWidths;
   wxCheckBox*                  myIncludeSubdirs;
   wxImageList*                 myImageList;
   wxStatusBar*                 myStatusBar;
   int                          myFolderImageIndex;
   int                          myFileImageIndex;
   int                          myInCVSImageIndex;
   int                          myAddedImageIndex;
   int                          myNotInCVSImageIndex;

   std::string                  myAddedStub;

   bool                         myShowOptions;
   bool                         myInitialized;
   bool                         myMakingPatch;

   FileTree::DirNode*           myCurrentNode;
   std::vector<FileTree::Node*> myCurrentNodes;
   bool                         myCurrentIncludeSubdirs;
   int                          myCurrentSortColumn;
   bool                         myCurrentSortAsc;

   friend class TreeCtrlEventHandler;
   friend class ListCtrlEventHandler;

   DECLARE_EVENT_TABLE()
};

#endif
