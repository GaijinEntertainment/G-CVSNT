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


#include "StdAfx.h"
#include "AddFilesDialog.h"
#include "WxwHelpers.h"
#include <Utils/PathUtils.h>
#include <Utils/TortoiseDebug.h>
#include <Utils/TortoiseRegistry.h>
#include <Utils/StringUtils.h>
#include <Utils/Keyboard.h>
#include <Utils/ShellUtils.h>
#include <Utils/TortoiseException.h>
#include <Utils/FileTree.h>
#include <Utils/Preference.h>
#include <TortoiseAct/DirectoryGroup.h>
#include "FilenameText.h"
#include <set>


enum
{
   ADDFILESDLG_ID_INCSUBDIRS = 10001,
   ADDFILESDLG_ID_TREECTRL,
   ADDFILESDLG_ID_LISTCTRL,

   ADDFILES_CONTEXT_ID_LIST_KEYWORDS_NORMAL,
   ADDFILES_CONTEXT_ID_LIST_KEYWORDS_NAMES,
   ADDFILES_CONTEXT_ID_LIST_KEYWORDS_VALUES,
   ADDFILES_CONTEXT_ID_LIST_KEYWORDS_LOCKER,
   ADDFILES_CONTEXT_ID_LIST_KEYWORDS_DISABLED,
      
   ADDFILES_CONTEXT_ID_LIST_FORMAT_ASCII,
   ADDFILES_CONTEXT_ID_LIST_FORMAT_UNICODE,
   ADDFILES_CONTEXT_ID_LIST_FORMAT_BINARY,
   ADDFILES_CONTEXT_ID_LIST_FORMAT_AUTO,

   ADDFILES_CONTEXT_ID_LIST_OPTIONS_BINDIFF,
   ADDFILES_CONTEXT_ID_LIST_OPTIONS_COMPRESSED,
   ADDFILES_CONTEXT_ID_LIST_OPTIONS_UNIX,
   ADDFILES_CONTEXT_ID_LIST_OPTIONS_STATIC,

   ADDFILES_CONTEXT_ID_LIST_CHECK,
   ADDFILES_CONTEXT_ID_LIST_UNCHECK,
   ADDFILES_CONTEXT_ID_LIST_AUTOCHECK,
   ADDFILES_CONTEXT_ID_LIST_CHECK_REC,
   ADDFILES_CONTEXT_ID_LIST_UNCHECK_REC,
   ADDFILES_CONTEXT_ID_LIST_AUTOCHECK_REC,

   ADDFILES_CONTEXT_ID_TREE_CHECK,
   ADDFILES_CONTEXT_ID_TREE_UNCHECK,
   ADDFILES_CONTEXT_ID_TREE_AUTOCHECK,
   ADDFILES_CONTEXT_ID_TREE_CHECK_REC,
   ADDFILES_CONTEXT_ID_TREE_UNCHECK_REC,
   ADDFILES_CONTEXT_ID_TREE_AUTOCHECK_REC
};

BEGIN_EVENT_TABLE(AddFilesDialog, ExtDialog)
   EVT_MENU(ADDFILES_CONTEXT_ID_LIST_KEYWORDS_NORMAL, AddFilesDialog::MenuListKeywordsNormal)
   EVT_MENU(ADDFILES_CONTEXT_ID_LIST_KEYWORDS_NAMES, AddFilesDialog::MenuListKeywordsNames)
   EVT_MENU(ADDFILES_CONTEXT_ID_LIST_KEYWORDS_VALUES, AddFilesDialog::MenuListKeywordsValues)
   EVT_MENU(ADDFILES_CONTEXT_ID_LIST_KEYWORDS_LOCKER, AddFilesDialog::MenuListKeywordsLocker)
   EVT_MENU(ADDFILES_CONTEXT_ID_LIST_KEYWORDS_DISABLED, AddFilesDialog::MenuListKeywordsDisabled)

   EVT_MENU(ADDFILES_CONTEXT_ID_LIST_FORMAT_ASCII, AddFilesDialog::MenuListFormatAscii)
   EVT_MENU(ADDFILES_CONTEXT_ID_LIST_FORMAT_UNICODE, AddFilesDialog::MenuListFormatUnicode)
   EVT_MENU(ADDFILES_CONTEXT_ID_LIST_FORMAT_BINARY, AddFilesDialog::MenuListFormatBinary)
   EVT_MENU(ADDFILES_CONTEXT_ID_LIST_FORMAT_AUTO, AddFilesDialog::MenuListFormatAuto)

   EVT_MENU(ADDFILES_CONTEXT_ID_LIST_OPTIONS_BINDIFF,   AddFilesDialog::MenuListOptionsBinDiff)
   EVT_MENU(ADDFILES_CONTEXT_ID_LIST_OPTIONS_COMPRESSED, AddFilesDialog::MenuListOptionsCompressed)
   EVT_MENU(ADDFILES_CONTEXT_ID_LIST_OPTIONS_UNIX,      AddFilesDialog::MenuListOptionsUnix)
   EVT_MENU(ADDFILES_CONTEXT_ID_LIST_OPTIONS_STATIC,    AddFilesDialog::MenuListOptionsStaticFile)

   EVT_MENU(ADDFILES_CONTEXT_ID_LIST_CHECK,             AddFilesDialog::MenuListCheck)
   EVT_MENU(ADDFILES_CONTEXT_ID_LIST_UNCHECK,           AddFilesDialog::MenuListUncheck)
   EVT_MENU(ADDFILES_CONTEXT_ID_LIST_AUTOCHECK,         AddFilesDialog::MenuListAutoCheck)
   EVT_MENU(ADDFILES_CONTEXT_ID_LIST_CHECK_REC,         AddFilesDialog::MenuListCheckRecursive)
   EVT_MENU(ADDFILES_CONTEXT_ID_LIST_UNCHECK_REC,       AddFilesDialog::MenuListUncheckRecursive)
   EVT_MENU(ADDFILES_CONTEXT_ID_LIST_UNCHECK_REC,       AddFilesDialog::MenuListAutoCheckRecursive)

   EVT_MENU(ADDFILES_CONTEXT_ID_TREE_CHECK,             AddFilesDialog::MenuTreeCheck)
   EVT_MENU(ADDFILES_CONTEXT_ID_TREE_UNCHECK,           AddFilesDialog::MenuTreeUncheck)
   EVT_MENU(ADDFILES_CONTEXT_ID_TREE_AUTOCHECK,         AddFilesDialog::MenuTreeAutoCheck)
   EVT_MENU(ADDFILES_CONTEXT_ID_TREE_CHECK_REC,         AddFilesDialog::MenuTreeCheckRecursive)
   EVT_MENU(ADDFILES_CONTEXT_ID_TREE_UNCHECK_REC,       AddFilesDialog::MenuTreeUncheckRecursive)
   EVT_MENU(ADDFILES_CONTEXT_ID_TREE_AUTOCHECK_REC,     AddFilesDialog::MenuTreeAutoCheckRecursive)

   EVT_COMMAND(ADDFILESDLG_ID_INCSUBDIRS, wxEVT_COMMAND_CHECKBOX_CLICKED,       AddFilesDialog::IncludeSubdirsChanged)
   EVT_COMMAND(wxID_OK, wxEVT_COMMAND_BUTTON_CLICKED,                           AddFilesDialog::OkClick)

   EVT_LIST_ITEM_ACTIVATED(ADDFILESDLG_ID_LISTCTRL,     AddFilesDialog::DoubleClick)
   EVT_LIST_COL_CLICK(ADDFILESDLG_ID_LISTCTRL,          AddFilesDialog::ColumnClick)
   EVT_LIST_ITEM_CHECKED(ADDFILESDLG_ID_LISTCTRL,       AddFilesDialog::ItemChecked)
   EVT_LIST_ITEM_UNCHECKED(ADDFILESDLG_ID_LISTCTRL,     AddFilesDialog::ItemUnchecked)

   // We also register a right-click handler, since the context menu handler
   // only works when double-clicking in the treeview (Windows bug?)
   EVT_TREE_ITEM_RIGHT_CLICK(ADDFILESDLG_ID_TREECTRL,   AddFilesDialog::TreeRightClick)
   EVT_TREE_SEL_CHANGED(ADDFILESDLG_ID_TREECTRL,        AddFilesDialog::TreeSelectionChanged)
END_EVENT_TABLE()

class TreeCtrlEventHandler : public wxEvtHandler
{
public:
   TreeCtrlEventHandler(AddFilesDialog* parent)
      : myParent(parent)
   {
   }

   void ContextMenu(wxContextMenuEvent& event);

private:
   AddFilesDialog* myParent;

   DECLARE_EVENT_TABLE()
};

void TreeCtrlEventHandler::ContextMenu(wxContextMenuEvent& event)
{
}

BEGIN_EVENT_TABLE(TreeCtrlEventHandler, wxEvtHandler)
   EVT_CONTEXT_MENU(TreeCtrlEventHandler::ContextMenu)
END_EVENT_TABLE()

class ListCtrlEventHandler : public wxEvtHandler
{
public:
   ListCtrlEventHandler(AddFilesDialog* parent)
      : myParent(parent)
   {
   }

   void ContextMenu(wxContextMenuEvent& event);

private:
   AddFilesDialog* myParent;

   DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(ListCtrlEventHandler, wxEvtHandler)
   EVT_CONTEXT_MENU(ListCtrlEventHandler::ContextMenu)
END_EVENT_TABLE()
   
   
static const int DEF_COLUMN_SIZE = 50;
static const int MAX_COLUMN_SIZE = 1000;
static const int MAX_COLUMN_COUNT = 6;

static const int COL_ID_NAME = 0;
static const int COL_ID_TYPE = 1;
static const int COL_ID_ADD = 2;
static const int COL_ID_FORMAT = 3;
static const int COL_ID_KEYWORDS = 4;
static const int COL_ID_OPTIONS = 5;

// User data for file tree

class AddFilesTreeData : public FileTree::UserData
{
public:
   enum NodeType
   {
      TYPE_SIMPLE = 0,
      TYPE_ENTRY
   };

   AddFilesTreeData() { m_Type = TYPE_SIMPLE; };
   wxTreeItemId m_TreeItemId;
   int m_Type;
};

struct AddFilesEntryTreeData : public AddFilesTreeData
{
public:
   AddFilesEntryTreeData(FileTree::Node& node, DirectoryGroup::Entry& entry,
                         void* data);

   // Get format as text
   wxString GetFormatText() const;
   // Get keywords as text
   wxString GetKeywordsText() const;
   // Get file options as text
   wxString GetOptionsText() const;
   // Get file type
   wxString GetFileType() const;


   DirectoryGroup::AddFilesData&        m_AddFilesData;
   DirectoryGroup::Entry&               m_Entry;

   AddFilesDialog*                      m_Dialog;

   bool                                 m_IsDirectory;
   bool                                 m_IsSelected;
   bool                                 m_IsEnabled;
   mutable bool                         m_IsTypeValid;
   bool                                 m_IsIgnored;
   CVSStatus::FileFormat                m_AutoFileFormat;

   mutable wxString                     m_FileType;
};


// User data for gui tree
class AddFilesGuiTreeData : public wxTreeItemData
{
public:
   AddFilesGuiTreeData(const FileTree::Node* node, const DirectoryGroup* group) 
      : m_Node(node), m_DirectoryGroup(group) { }

   const FileTree::Node* m_Node;
   const DirectoryGroup* m_DirectoryGroup;
};


// My virtual listctrl
class AddFilesListCtrl : public ExtListCtrl
{
public:
   // Constructor
   AddFilesListCtrl(AddFilesDialog* dialog, wxWindow* parent, wxWindowID id);
protected:
   // Get item text
   wxString OnGetItemText(long item, long column) const;
   // Get item format
   wxListItemAttr *OnGetItemAttr(long item) const;
   // Get item image
   int OnGetItemImage(long item) const;
   // Get item state
   int OnGetItemState(long item) const; 

private:
   AddFilesDialog* myDialog;
};


//static
bool DoAddFilesDialog(wxWindow* parent, 
                      DirectoryGroups& groups,
                      const wxString& caption,
                      const wxString& label,
                      bool makingPatch)
{
   TDEBUG_ENTER("DoAddFilesDialog");
   AddFilesDialog* dlg = new AddFilesDialog(parent, groups, caption, label, makingPatch);
   bool ret = (dlg->ShowModal() == wxID_OK);
   
   DestroyDialog(dlg);
   return ret;
}


//////////////////////////////////////////////////////////////////////////////
// AddFilesListCtrl

// Constructor
AddFilesListCtrl::AddFilesListCtrl(AddFilesDialog* dialog, wxWindow* parent, 
                                   wxWindowID id)
   : ExtListCtrl(parent, id, wxDefaultPosition, wxDefaultSize,
                 wxLC_REPORT | wxLC_ALIGN_LEFT | wxBORDER_NONE | wxLC_VIRTUAL),
     myDialog(dialog)
{
}


// Get item text
wxString AddFilesListCtrl::OnGetItemText(long item, long column) const
{
   FileTree::Node* node = myDialog->myCurrentNodes[item];
   AddFilesEntryTreeData* treeData = (AddFilesEntryTreeData*) node->GetUserData();
   ASSERT(treeData && treeData->m_Type == AddFilesTreeData::TYPE_ENTRY);

   switch (column)
   {
   case COL_ID_NAME:
      // Column "Name"
      if (myDialog->myIncludeSubdirs)
         return wxText(node->GetPathFrom(myDialog->myCurrentNode));
      return wxText(node->GetName());

   case COL_ID_TYPE:
      // Column "Type"
      return treeData->GetFileType();

   case COL_ID_ADD:
      // Column "Add"
      if (treeData->m_IsSelected && treeData->m_IsEnabled)
         return _("Yes");
      else
         return _("No");

   case COL_ID_FORMAT:
      // Column "Format"
      return treeData->GetFormatText();

   case COL_ID_KEYWORDS:
      // Column "keywords"
      return treeData->GetKeywordsText();

   case COL_ID_OPTIONS:
      // Column "options"
      if (myDialog->myShowOptions)
         return treeData->GetOptionsText();
      // fall through

   default:
      return wxT("");
   }
}


// Get item attributes
wxListItemAttr* AddFilesListCtrl::OnGetItemAttr(long item) const
{
   static wxListItemAttr attr;
   FileTree::Node* node = myDialog->myCurrentNodes[item];
   AddFilesEntryTreeData* treeData = (AddFilesEntryTreeData*) node->GetUserData();
   ASSERT(treeData && treeData->m_Type == AddFilesTreeData::TYPE_ENTRY);
   if (!treeData->m_IsEnabled)
   {
      attr.SetTextColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
      return &attr;
   }
   return 0;
}


// Get item image
int AddFilesListCtrl::OnGetItemImage(long item) const
{
   FileTree::Node* node = myDialog->myCurrentNodes[item];
   AddFilesEntryTreeData* treeData = (AddFilesEntryTreeData*) node->GetUserData();
   ASSERT(treeData && treeData->m_Type == AddFilesTreeData::TYPE_ENTRY);
   if (treeData->m_IsDirectory)
   {
      return myDialog->myFolderImageIndex;
   }
   else
   {
      return myDialog->myFileImageIndex;
   }
}


// Get item state
int AddFilesListCtrl::OnGetItemState(long item) const
{
   int result = 0;
   FileTree::Node* node = myDialog->myCurrentNodes[item];
   AddFilesEntryTreeData* treeData = (AddFilesEntryTreeData*) node->GetUserData();
   ASSERT(treeData && treeData->m_Type == AddFilesTreeData::TYPE_ENTRY);
   // Set checkbox
   if (treeData->m_IsSelected)
   {
      result |= INDEXTOSTATEIMAGEMASK(2);
   }
   else
   {
      result |= INDEXTOSTATEIMAGEMASK(1);
   }

   // Set overlay mask
   if (treeData->m_IsEnabled && treeData->m_IsSelected)
   {
      result |= INDEXTOOVERLAYMASK(myDialog->myAddedImageIndex);
   }
   else
   {
      result |= INDEXTOOVERLAYMASK(myDialog->myNotInCVSImageIndex);
   }

   return result;
}



//////////////////////////////////////////////////////////////////////////////
// AddFilesDialog

   
AddFilesDialog::AddFilesDialog(wxWindow* parent, 
                               DirectoryGroups& groups,
                               const wxString& caption,
                               const wxString& label,
                               bool makingPatch)
   : ExtDialog(parent, -1, (caption.empty() ? _("TortoiseCVS - Add") : caption.c_str()),
               wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | 
               wxRESIZE_BORDER | wxMINIMIZE_BOX | wxMAXIMIZE_BOX),
     mySplitter(0),
     myDirectoryGroups(groups),
     myShowOptions(false),
     myInitialized(false),
     myMakingPatch(makingPatch),
     myCurrentNode(0)
{
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));
   
   // Parse directory groups into tree structures
   BuildFileTrees();

   myStatusBar = new wxStatusBar(this, -1);
   myStatusBar->SetFieldsCount(1);

   mySplitter = new ExtSplitterWindow(this, -1, wxDefaultPosition, 
                                      wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
   wxPanel* leftPanel = new wxPanel(mySplitter, -1, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
   wxBoxSizer* leftPanelSizer = new wxBoxSizer(wxVERTICAL);
   wxPanel* rightPanel = new wxPanel(mySplitter, -1, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
   wxBoxSizer* rightPanelSizer = new wxBoxSizer(wxVERTICAL);

   leftPanel->SetSizer(leftPanelSizer);
   rightPanel->SetSizer(rightPanelSizer);
   mySplitter->SplitVertically(leftPanel, rightPanel, 0);
   mySplitter->SetMinimumPaneSize(wxDLG_UNIT_X(this, 50));
   mySplitter->SetSize(wxDLG_UNIT(this, wxSize(150, 80)));

   wxPanel* headerPanel = new wxPanel(this, -1);

   // Added
   wxStaticText* label2 = new wxStaticText(headerPanel, -1,
                                           label.empty() ? _("Confirm the files to add.") : label.c_str());

   wxStaticText* label3 = new wxStaticText(headerPanel, -1, _("Hint: Right-click to change file format."));
   label3->SetForegroundColour(ColorRefToWxColour(GetIntegerPreference("Colour Tip Text")));

   // Checkbox for showing/hiding symbolics
   myIncludeSubdirs = new wxCheckBox(headerPanel, ADDFILESDLG_ID_INCSUBDIRS, _("Show items in &subfolders"));

   // Image list
   BuildImageList();

   // TreeCtrl
   myTreeCtrl = new wxTreeCtrl(leftPanel, ADDFILESDLG_ID_TREECTRL, wxDefaultPosition, wxDefaultSize, 
                               wxTR_SINGLE | wxTR_HIDE_ROOT | wxTR_LINES_AT_ROOT | wxTR_HAS_BUTTONS | wxBORDER_NONE);
   myTreeCtrl->PushEventHandler(new TreeCtrlEventHandler(this));
   leftPanelSizer->Add(myTreeCtrl, 1, wxGROW | wxALL, 0);

   // Fill tree control
   BuildGuiTree();

   // ListCtrl
   myListCtrl = new AddFilesListCtrl(this, rightPanel, ADDFILESDLG_ID_LISTCTRL);
   myListCtrl->InsertColumn(COL_ID_NAME, _("Filename"), wxLIST_FORMAT_LEFT, 0);
   myListCtrl->InsertColumn(COL_ID_TYPE, _("Type"), wxLIST_FORMAT_LEFT, 0);
   myListCtrl->InsertColumn(COL_ID_ADD, _("Add"), wxLIST_FORMAT_LEFT, 0);
   if (!myMakingPatch)
   {
       myListCtrl->InsertColumn(COL_ID_FORMAT, _("Format"), wxLIST_FORMAT_LEFT, 0);
       myListCtrl->InsertColumn(COL_ID_KEYWORDS, _("Keywords"), wxLIST_FORMAT_LEFT, 0);
   }
   myListCtrl->SetImageList(myImageList, wxIMAGE_LIST_SMALL);
   myListCtrl->PushEventHandler(new ListCtrlEventHandler(this));

   // Read column widths from registry
   ReadColumnWidths();
   // Set column widths
   SetColumnWidths();

   myShowOptions = false;
   myListCtrl->SetCheckboxes(true);

   wxBoxSizer *headerSizer = new wxBoxSizer(wxVERTICAL);
   headerSizer->Add(label2, 0, wxGROW | wxALL, 3);
   headerSizer->Add(label3, 0, wxGROW | wxALL, 3);
   headerSizer->Add(myIncludeSubdirs, 0, wxGROW | wxLEFT | wxRIGHT | wxTOP, 3);
   headerPanel->SetSizer(headerSizer);

   // Add checkboxes to ListCtrl
   wxBoxSizer *sizer2 = new wxBoxSizer(wxVERTICAL);
   sizer2->Add(headerPanel, 0, wxGROW | wxALL, 0);
   rightPanelSizer->Add(myListCtrl, 1, wxGROW | wxALL, 0);

   leftPanelSizer->SetSizeHints(leftPanel);
   leftPanelSizer->Fit(leftPanel);
   rightPanelSizer->SetSizeHints(rightPanel);

   // OK/Cancel button
   wxBoxSizer *sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   myOK = new wxButton(this, wxID_OK, _("OK"));
   myOK->SetDefault();
   wxButton* cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
   sizerConfirm->Add(myOK, 0, wxGROW | wxALL, 5);
   sizerConfirm->Add(cancel, 0, wxGROW | wxALL, 5);
   
   // Main box with everything in it
   wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(sizer2, 0, wxGROW | wxALL, 5);
   sizerTop->Add(mySplitter, 1, wxGROW | wxALL, 5);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, 10);
   sizerTop->Add(myStatusBar, 0, wxGROW |wxALL, 0);


   myIncludeSubdirs->SetValue(TortoiseRegistry::ReadBoolean("Dialogs\\AddFiles\\Include Subdirs", false));
   myCurrentIncludeSubdirs = myIncludeSubdirs->GetValue();
   myCurrentSortColumn = COL_ID_NAME;
   TortoiseRegistry::ReadInteger("Dialogs\\AddFiles\\Sort Column", myCurrentSortColumn);
   myCurrentSortAsc = TortoiseRegistry::ReadBoolean("Dialogs\\AddFiles\\Sort Ascending", true);

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);

   RestoreTortoiseDialogSize(this, "AddFiles", wxDLG_UNIT(this, wxSize(200, 120)));
   SetTortoiseDialogPos(this, GetRemoteHandle());
   RestoreTortoiseDialogState(this, "AddFiles");
   int pos = 80;
   TortoiseRegistry::ReadInteger("Dialogs\\AddFiles\\Sash Position", pos);
   mySplitter->SetSashPosition(wxDLG_UNIT_X(this, pos));
   RebuildListView();

   myInitialized = true;
}


AddFilesDialog::~AddFilesDialog()
{
   myTreeCtrl->PopEventHandler(true);
   myListCtrl->PopEventHandler(true);

   StoreTortoiseDialogSize(this, "AddFiles");
   if (mySplitter->IsSplit())
   {
      int w = mySplitter->GetSashPosition();
      w = this->ConvertPixelsToDialog(wxSize(w, 0)).GetWidth();
      TortoiseRegistry::WriteInteger("Dialogs\\AddFiles\\Sash Position", w);
   }
   GetColumnWidths();
   WriteColumnWidths();
   TortoiseRegistry::WriteBoolean("Dialogs\\AddFiles\\Include Subdirs",
                                  myIncludeSubdirs->GetValue());
   TortoiseRegistry::WriteInteger("Dialogs\\AddFiles\\Sort Column",
                                  myCurrentSortColumn);
   TortoiseRegistry::WriteBoolean("Dialogs\\AddFiles\\Sort Ascending",
                                  myCurrentSortAsc);
   std::vector<FileTree*>::iterator it = myFileTrees.begin();
   while (it != myFileTrees.end())
   {
      delete *it;
      it++;
   }

   delete myImageList;
}



// Parse directory groups into tree structures
void AddFilesDialog::BuildFileTrees()
{
   TDEBUG_ENTER("AddFilesDialog::BuildFileTrees");
   for (size_t i = 0; i < myDirectoryGroups.size(); i++)
   {
      DirectoryGroup& group = myDirectoryGroups[i];

      // Get server features
      if (!group.myCVSServerFeatures.Initialize())
         TortoiseFatalError(_("Could not connect to server"));

      // Create new tree
      FileTree* fileTree = group.BuildFileTree<AddFilesEntryTreeData>(this);

      myFileTrees.push_back(fileTree);

      // Calculate which nodes are enabled and which aren't
      CalcFileTreeEnabledNodes(fileTree->GetRoot(), true, false);
   }
}



// Build image list
void AddFilesDialog::BuildImageList()
{
   TDEBUG_ENTER("AddFilesDialog::BuildImageList");
   // Determine size of shell small icon
   int w, h;
   DWORD dw;
   ShellGetIconSize(SHGFI_SMALLICON, &w, &h);
   TDEBUG_TRACE("Image list size: " << w << " x " << h);
   wxIcon ico;
   HANDLE hIcon;

   myImageList = new wxImageList(w, h);
   myImageList->GetSize(0, w, h);
   
   // Get folder icon
   TDEBUG_TRACE("Add folder icon");
   SHFILEINFO sfi;
   dw = SHGetFileInfo(wxT("*"), FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_DIRECTORY, &sfi, 
                      sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
   TDEBUG_TRACE("ShGetFileInfo returned " << dw);
   ico.SetHICON((WXHICON) sfi.hIcon);
   TDEBUG_TRACE("Add to image list");
   myFolderImageIndex = myImageList->Add(ico);

   // Add file icon
   TDEBUG_TRACE("Add file icon");
   dw = SHGetFileInfo(wxT("*.txt"), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
                      SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
   TDEBUG_TRACE("ShGetFileInfo returned " << dw);
   ico.SetHICON((WXHICON) sfi.hIcon);
   TDEBUG_TRACE("Add to image list");
   myFileImageIndex = myImageList->Add(ico);


   // Add overlay icons
   myAddedImageIndex = 0;
   myNotInCVSImageIndex = 0;
   myInCVSImageIndex = 0;
   std::string icons = GetStringPreference("Icons");
   if (!icons.empty())
   {
      myAddedImageIndex = 1;
      myNotInCVSImageIndex = 2;
      myInCVSImageIndex = 3;

      std::string sIconPath = GetIconSetPath(icons);
      int idx;

      // Load "added" overlay
      TDEBUG_TRACE("Add file " << sIconPath << "AddedIcon.ico");
      hIcon = LoadImageA(0, (sIconPath + "AddedIcon.ico").c_str(), IMAGE_ICON, w, h, LR_LOADFROMFILE);
      ico.SetHICON((WXHICON) hIcon);
      idx = myImageList->Add(ico);
      TDEBUG_TRACE("idx: " << idx);
      ImageList_SetOverlayImage((HIMAGELIST) myImageList->GetHIMAGELIST(), 
                                idx, myAddedImageIndex); 

      // Load "not in CVS" overlay
      TDEBUG_TRACE("Add file " << sIconPath << "UnversionedIcon.ico");
      hIcon = LoadImageA(0, (sIconPath + "UnversionedIcon.ico").c_str(), IMAGE_ICON, w, h, LR_LOADFROMFILE);
      ico.SetHICON((WXHICON) hIcon);
      idx = myImageList->Add(ico);
      TDEBUG_TRACE("idx: " << idx);
      ImageList_SetOverlayImage((HIMAGELIST) myImageList->GetHIMAGELIST(), 
                                idx, myNotInCVSImageIndex); 

      // Load "In CVS" overlay
      TDEBUG_TRACE("Add file " << sIconPath << "NormalIcon.ico");
      hIcon = LoadImageA(0, (sIconPath + "NormalIcon.ico").c_str(), IMAGE_ICON, w, h, LR_LOADFROMFILE);
      ico.SetHICON((WXHICON) hIcon);
      idx = myImageList->Add(ico);
      TDEBUG_TRACE("idx: " << idx);
      ImageList_SetOverlayImage((HIMAGELIST) myImageList->GetHIMAGELIST(), idx, myInCVSImageIndex); 
   }
}



// Build the GUI tree
void AddFilesDialog::BuildGuiTree()
{
   // Set image list
   myTreeCtrl->SetImageList(myImageList);

   // Add the root node
   wxTreeItemId rootNodeId = myTreeCtrl->AddRoot(wxT("Root"));

   // iterate through all directory groups
   for (size_t i = 0; i < myDirectoryGroups.size(); i++)
   {
      DirectoryGroup& group = myDirectoryGroups[i];

      // Add tree items
      AddTreeNodes(rootNodeId, myFileTrees[i]->GetRoot(), &group, true, 
                   group.myDirectory);
   }

   // Calc enabled nodes
   CalcGuiTreeEnabledNodes(rootNodeId);

   // Expand nodes
   ExpandTreeNodes(rootNodeId, 2);

   // Set focus on first node
   wxTreeItemIdValue cookie;
   wxTreeItemId id = myTreeCtrl->GetFirstChild(rootNodeId, cookie);
   if (id.IsOk())
      myTreeCtrl->SelectItem(id);
}


// Add nodes to tree
bool AddFilesDialog::AddTreeNodes(wxTreeItemId guiNode, FileTree::DirNode* dataNode,
                                  DirectoryGroup* group, bool doShorten, 
                                  const std::string& namePrefix)
{
   // Get all dir nodes from current node
   std::vector<std::string> nodeNames;
   const FileTree::Node* node;
   FileTree::NodeIterator it = dataNode->Begin();
   bool hasAddableItems = false;
   while (it != dataNode->End())
   {
      node = it->second;
      if (node->GetNodeType() == FileTree::TYPE_DIR)
      {
         nodeNames.push_back(node->GetName());
      }
      if (node->GetUserData())
      {
         hasAddableItems = true;
      }
      it++;
   }

   // iterate through nodes
   std::vector<std::string>::iterator it1 = nodeNames.begin();
   wxTreeItemId wxnode;

   // should we shorten the paths
   if (doShorten && nodeNames.size() == 1 && !hasAddableItems)
   {
      bool added;
      std::string newPrefix = namePrefix;
      if (!newPrefix.empty())
      {
         newPrefix = EnsureTrailingDelimiter(newPrefix);
      }
      newPrefix += nodeNames[0];
      added = AddTreeNodes(guiNode, (FileTree::DirNode*) dataNode->GetNode(it1->c_str()),
                           group, true, newPrefix);
      if (!added)
      {
         wxnode = myTreeCtrl->AppendItem(guiNode, wxText(newPrefix), myFolderImageIndex, -1, 
                                         new AddFilesGuiTreeData(dataNode, group));
         AddFilesTreeData* treeData = (AddFilesTreeData *) dataNode->GetUserData();
         if (!treeData)
         {
            treeData = new AddFilesTreeData();
            dataNode->SetUserData(treeData);
         }
         treeData->m_TreeItemId = wxnode;

      }
      return true;
   }
   else
   {
      bool added = false;
      FileTree::DirNode* subNode;
      if (!namePrefix.empty())
      {
         wxTreeItemId newNode = myTreeCtrl->AppendItem(guiNode, wxText(namePrefix), 
                                                       myFolderImageIndex, -1, new AddFilesGuiTreeData(dataNode, group));
         AddFilesTreeData* treeData = (AddFilesTreeData*) dataNode->GetUserData();
         if (!treeData)
         {
            treeData = new AddFilesTreeData();
            dataNode->SetUserData(treeData);
         }
         treeData->m_TreeItemId = newNode;

         guiNode = newNode;
         added = true;
      }

      while (it1 != nodeNames.end())
      {
         added = true;
         std::string name = namePrefix;
         subNode = (FileTree::DirNode*) dataNode->GetNode(it1->c_str());
         // Don't add empty folders to the tree that are in CVS
         if (subNode->HasSubNodes() || subNode->GetUserData())
         {
            wxnode = myTreeCtrl->AppendItem(guiNode, wxText(*it1), myFolderImageIndex, -1, 
                                            new AddFilesGuiTreeData(subNode, group));
            AddFilesTreeData* treeData = (AddFilesTreeData *) subNode->GetUserData();
            if (!treeData)
            {
               treeData = new AddFilesTreeData();
               subNode->SetUserData(treeData);
            }
            treeData->m_TreeItemId = wxnode;
            added |= AddTreeNodes(wxnode, (FileTree::DirNode*) subNode, group, false);
         }
         it1++;
      }
      return added;
   }
}



// Rebuild listview and header
void AddFilesDialog::RebuildListView()
{
   TDEBUG_ENTER("AddFilesDialog::RebuildListView");
   wxBusyCursor wait;

   // Get active tree node
   wxTreeItemId id = myTreeCtrl->GetSelection();

   AddFilesGuiTreeData *guiTreeData = 
      (AddFilesGuiTreeData *) myTreeCtrl->GetItemData(id);
   ASSERT(guiTreeData != 0);

   FileTree::DirNode* node = (FileTree::DirNode *) guiTreeData->m_Node;
   ASSERT(node->GetNodeType() == FileTree::TYPE_DIR);

   // If node hasn't changed, do nothing
   if (node == myCurrentNode && 
      myCurrentIncludeSubdirs == myIncludeSubdirs->GetValue())
      return;

   // Remove old items
   myListCtrl->DeleteAllItems();

   // Adjust columns
   GetColumnWidths();
   if (!myMakingPatch && guiTreeData->m_DirectoryGroup->myCVSServerFeatures.SupportsFileOptions() &&
       !myShowOptions)
   {
      myListCtrl->InsertColumn(COL_ID_OPTIONS, _("Options"), wxLIST_FORMAT_LEFT, 0);
      myShowOptions = true;
   }
   else if (!guiTreeData->m_DirectoryGroup->myCVSServerFeatures.SupportsFileOptions() &&
            myShowOptions)
   {
      myColumnWidths[COL_ID_OPTIONS] = myListCtrl->GetColumnWidth(COL_ID_OPTIONS);
      myShowOptions = false;
   }
   SetColumnWidths();
  
   // Add new items
   myCurrentNode = node;
   myCurrentIncludeSubdirs = myIncludeSubdirs->GetValue();

   // Get item list
   myCurrentNodes.clear();
   myCurrentNodes.reserve(myCurrentNode->GetLastChildId() 
                          - myCurrentNode->GetId() + 1);
   AddListItems(myCurrentNode, "", myCurrentIncludeSubdirs);
   myListCtrl->SetItemCount(static_cast<long>(myCurrentNodes.size()));
   SortListItems();

   // Adjust status bar
   myStatusBar->SetStatusText(Printf(_("%d file(s)"), myListCtrl->GetItemCount()).c_str());
}


// Update all list items
void AddFilesDialog::UpdateListItems()
{
   myListCtrl->Refresh(false);
}


// Add items to listview
void AddFilesDialog::AddListItems(FileTree::DirNode* node, 
                                  const std::string& prefix,
                                  bool recurse)
{
   TDEBUG_ENTER("AddFilesDialog::AddListItems");
   std::string name;
   FileTree::NodeIterator it = node->Begin();
   while (it != node->End())
   {
      // Add this node
      AddFilesTreeData* treeData = 
         (AddFilesTreeData*) it->second->GetUserData();
      if (treeData && treeData->m_Type == AddFilesTreeData::TYPE_ENTRY)
         myCurrentNodes.push_back(it->second);

      if (recurse)
      {
         if (it->second->GetNodeType() == FileTree::TYPE_DIR)
         {
            AddListItems((FileTree::DirNode *) it->second, 
                         prefix + it->second->GetName() + "\\", true);
         }
      }

      it++;
   }
}





// Populate tree context menu
bool AddFilesDialog::PopulateTreeContextMenu(const wxTreeItemId& id, wxMenu& popupMenu)
{
   bool bResult = false;
   if (id.IsOk())
   {
      bResult = true;
      // Can the current node be added?
      AddFilesGuiTreeData *guiTreeData = (AddFilesGuiTreeData *) myTreeCtrl->GetItemData(id);
      ASSERT(guiTreeData != 0);

      FileTree::DirNode* node = (FileTree::DirNode *) guiTreeData->m_Node;
      ASSERT(node->GetNodeType() == FileTree::TYPE_DIR);

      AddFilesTreeData* treeData = (AddFilesTreeData *) node->GetUserData();

      // if this is a directory that can be added, show "Check" and "Uncheck"
      if (treeData->m_Type == AddFilesTreeData::TYPE_ENTRY)
      {
         popupMenu.Append(ADDFILES_CONTEXT_ID_TREE_CHECK,
                          _("Check"), _("Check item(s)"));
         popupMenu.Append(ADDFILES_CONTEXT_ID_TREE_UNCHECK,
                          _("Uncheck"), _("Uncheck item(s)"));
         popupMenu.Append(ADDFILES_CONTEXT_ID_TREE_AUTOCHECK,
                          _("Autoignore"), _("Ignore item(s) according to CVS ignore rules"));
         popupMenu.AppendSeparator();
      }

      // Always show recursive (un)check
      popupMenu.Append(ADDFILES_CONTEXT_ID_TREE_CHECK_REC,
                       _("Check recursively"), 
                       _("Check item(s) recursively"));
      popupMenu.Append(ADDFILES_CONTEXT_ID_TREE_UNCHECK_REC,
                       _("Uncheck recursively"), 
                       _("Uncheck item(s) recursively"));
      popupMenu.Append(ADDFILES_CONTEXT_ID_TREE_AUTOCHECK_REC,
                       _("Autoignore recursively"), 
                       _("Ignore item(s) recursively according to CVS ignore rules"));
      popupMenu.AppendSeparator();

      // Add Explorer menu
      ExplorerMenu* explorerMenu = new ExplorerMenu();
        
      explorerMenu->FillMenu((HWND) GetHandle(), node->GetFullName());
      popupMenu.Append(0, _("Explorer"), explorerMenu);
 
   }

   return bResult;
}



// Show tree context menu
void AddFilesDialog::ShowTreeContextMenu(const wxPoint& position)
{
   // Get item id
   wxTreeItemId id = myTreeCtrl->GetSelection();
   if (id.IsOk())
   {
      // Context menu
      wxMenu popupMenu;

      // Populate the tree context menu
      if (PopulateTreeContextMenu(id, popupMenu))
      {
         // Calculate correct position
         wxPoint pos = position;
         if (pos == wxPoint(-1, -1))
         {
            wxRect rect;
            if (myTreeCtrl->GetBoundingRect(id, rect, true))
            {
               int d = rect.GetHeight() / 2;
               pos.x = rect.GetLeft() + d;
               pos.y = rect.GetTop() + d;
            }
         }

         PopupMenu(&popupMenu, ScreenToClient(ClientToScreen(pos)));
      }
   }
}



// Checkbox "include subdirs" changed
void AddFilesDialog::IncludeSubdirsChanged(wxCommandEvent&)
{
   RebuildListView();
}



// Read column widths from registry
void AddFilesDialog::ReadColumnWidths()
{
   myColumnWidths.clear();
   TortoiseRegistry::ReadVector("Dialogs\\AddFiles\\Column Widths", myColumnWidths);
   myColumnWidths.resize(MAX_COLUMN_COUNT);
   for (size_t i = 0; i < myColumnWidths.size(); i++)
   {
      int w = myColumnWidths[i];
      if (w <= 0 || w >= MAX_COLUMN_SIZE)
         w = DEF_COLUMN_SIZE;
      w = wxDLG_UNIT_X(this, w);
      myColumnWidths[i] = w;
   }
}


// Write column widths to registry
void AddFilesDialog::WriteColumnWidths()
{
   for (size_t i = 0; i < myColumnWidths.size(); i++)
   {
      int w = myColumnWidths[i];
      w = this->ConvertPixelsToDialog(wxSize(w, 0)).GetWidth();
      myColumnWidths[i] = w;
   }
   TortoiseRegistry::WriteVector("Dialogs\\AddFiles\\Column Widths", myColumnWidths);   
}


// Get column widths from listbox
void AddFilesDialog::GetColumnWidths()
{
   myColumnWidths[COL_ID_NAME] = myListCtrl->GetColumnWidth(COL_ID_NAME);
   myColumnWidths[COL_ID_TYPE] = myListCtrl->GetColumnWidth(COL_ID_TYPE);
   myColumnWidths[COL_ID_ADD] = myListCtrl->GetColumnWidth(COL_ID_ADD);
   myColumnWidths[COL_ID_FORMAT] = myListCtrl->GetColumnWidth(COL_ID_FORMAT);
   myColumnWidths[COL_ID_KEYWORDS] = myListCtrl->GetColumnWidth(COL_ID_KEYWORDS);
   if (myShowOptions)
      myColumnWidths[COL_ID_OPTIONS] = myListCtrl->GetColumnWidth(COL_ID_OPTIONS);
}


// Set column widths in listbox
void AddFilesDialog::SetColumnWidths()
{
   myListCtrl->SetColumnWidth(COL_ID_NAME, myColumnWidths[COL_ID_NAME]);
   myListCtrl->SetColumnWidth(COL_ID_TYPE, myColumnWidths[COL_ID_TYPE]);
   myListCtrl->SetColumnWidth(COL_ID_ADD, myColumnWidths[COL_ID_ADD]);
   myListCtrl->SetColumnWidth(COL_ID_FORMAT, myColumnWidths[COL_ID_FORMAT]);
   myListCtrl->SetColumnWidth(COL_ID_KEYWORDS, myColumnWidths[COL_ID_KEYWORDS]);
   if (myShowOptions)
      myListCtrl->SetColumnWidth(COL_ID_OPTIONS, myColumnWidths[COL_ID_OPTIONS]);
}


// Get relative paths for selected list items
void AddFilesDialog::GetListSelectionRelativePaths(const FileTree::Node* parentNode, 
                                                   std::vector<std::string>& files)
{
   long item = -1;
   FileTree::Node* node = 0;
   do
   {
      item = myListCtrl->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
      if (item != -1)
      {
         // Get associated tree node
         node = myCurrentNodes[item];
         files.push_back(node->GetPathFrom(parentNode));
      }
   } while (item != -1);
}


void AddFilesDialog::OkClick(wxCommandEvent&)
{
    TDEBUG_ENTER("AddFilesDialog::OkClick");

    if (!myMakingPatch)
    {
        // Walk the tree to find any files of type "unknown"
        FileTree::Node* node = FindUnknownFiles();

        if (node != 0)
        {
            // Get parent
            const FileTree::Node* parentNode = node->GetParent();
            ASSERT(parentNode != 0);

            // Select node in the tree
            AddFilesTreeData* treeData = (AddFilesTreeData*) parentNode->GetUserData();
            ASSERT(treeData && treeData->m_TreeItemId.IsOk());
            myTreeCtrl->SelectItem(treeData->m_TreeItemId);

            // Select unknown files in listview
            ListSelectUnknownFiles();

            // Show error message
            wxString errMsg = Printf(_("Invalid file format: %s"), 
                                     CVSStatus::FileFormatString(CVSStatus::fmtUnknown).c_str());
            MessageBox(GetHwnd(), _("Please specify a valid file format for all files you want to add."), 
                       errMsg, MB_ICONERROR);
            return;
        }
    }
    
    EndModal(wxID_OK);
}



// Find unknown files
FileTree::Node* AddFilesDialog::FindUnknownFiles(FileTree::DirNode* dirNode)
{
   ASSERT(dirNode->GetNodeType() == FileTree::TYPE_DIR);
   FileTree::Node* result = 0;
   AddFilesEntryTreeData* treeData;
   FileTree::NodeIterator it = dirNode->Begin();
   while (result == 0 && it != dirNode->End())
   {
      FileTree::Node* node = it->second;

      // Node is a file node
      if (node->GetNodeType() == FileTree::TYPE_FILE)
      {
         treeData = (AddFilesEntryTreeData*) node->GetUserData();
         ASSERT(treeData && treeData->m_Type == AddFilesTreeData::TYPE_ENTRY);
         if (treeData->m_IsEnabled && treeData->m_IsSelected
            && treeData->m_AddFilesData.myFileFormat == CVSStatus::fmtUnknown)
         {
            result = node;
         }
      }

      // Node is a directory node
      else if (node->GetNodeType() == FileTree::TYPE_DIR)
      {
         // Check if the node is enabled
         bool bDoProcess = true;
         treeData = (AddFilesEntryTreeData*) node->GetUserData();
         if (treeData && treeData->m_Type == AddFilesTreeData::TYPE_ENTRY)
         {
            if (!treeData->m_IsEnabled && treeData->m_IsSelected)
               bDoProcess = false;
         }

         // Process subnodes
         if (bDoProcess)
            result = FindUnknownFiles((FileTree::DirNode *) node);
      }
      it++;
   }

   return result;
}



// Find unknown files
FileTree::Node* AddFilesDialog::FindUnknownFiles()
{
   FileTree::Node* result = 0;

   // Process all my filetrees
   std::vector<FileTree*>::iterator it = myFileTrees.begin();
   while (result == 0 && it != myFileTrees.end())
   {
      FileTree* tree = *it;
      // Start with the root
      FileTree::DirNode* rootNode = tree->GetRoot();
      result = FindUnknownFiles(rootNode);
      it++;
   }

   return result;
}



// Select unknown files in listview
void AddFilesDialog::ListSelectUnknownFiles()
{
   long item = -1;
   FileTree::Node* node = 0;
   AddFilesEntryTreeData* treeData; 
   do
   {
      item = myListCtrl->GetNextItem(item, wxLIST_NEXT_ALL);
      if (item != -1)
      {
         // Get associated tree node
         node = myCurrentNodes[item];

         // Should we select this node
         if (node->GetNodeType() == FileTree::TYPE_FILE)
         {
            treeData = (AddFilesEntryTreeData*) node->GetUserData();
            ASSERT(treeData);
            if (treeData->m_Type == AddFilesTreeData::TYPE_ENTRY)
            {
               if (treeData->m_IsEnabled && treeData->m_IsSelected 
                  && treeData->m_AddFilesData.myFileFormat == CVSStatus::fmtUnknown)
               {
                  myListCtrl->SetItemState(item, wxLIST_STATE_SELECTED, 
                                           wxLIST_STATE_SELECTED);
               }
            }

         }
      }
   } while (item != -1);
}



// Analyze selected items in list
void AddFilesDialog::AnalyzeListSelection(bool& bAllDirs, 
                                          CVSStatus::KeywordOptions& keywordOptions,
                                          CVSStatus::FileFormat& fileFormat, 
                                          CVSStatus::FileOptions& fileOptions,
                                          const FileTree::Node*& commonParentNode)
{
   long item = -1;
   bAllDirs = true;
   FileTree::Node* node = 0;
   const FileTree::Node* parentNode = 0;
   keywordOptions = 0;
   fileFormat = CVSStatus::fmtUnknown;
   fileOptions = 0;
   bool firstItem = true;
   bool firstFile = true;
   commonParentNode = 0;

   // Loop through all selected items
   do
   {
      item = myListCtrl->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
      if (item != -1)
      {
         // Get associated tree node
         node = myCurrentNodes[item];
         if (firstItem)
         {
            bAllDirs = node->GetNodeType() == FileTree::TYPE_DIR;
            firstItem = false;
            commonParentNode = node->GetParent();
         }
         else
         {
            bAllDirs = bAllDirs && (node->GetNodeType() == FileTree::TYPE_DIR);
            if (commonParentNode != 0)
            {
               parentNode = node->GetParent();
               if (parentNode != commonParentNode)
               {
                  commonParentNode = 0;
               }
            }
         }

         if (node->GetNodeType() == FileTree::TYPE_FILE)
         {
            AddFilesEntryTreeData* treeData = (AddFilesEntryTreeData*) node->GetUserData();
            ASSERT(treeData && treeData->m_Type == AddFilesTreeData::TYPE_ENTRY);

            if (firstFile) 
            {
               keywordOptions = treeData->m_AddFilesData.myKeywordOptions;
               fileFormat = treeData->m_AddFilesData.myFileFormat;
               fileOptions = treeData->m_AddFilesData.myFileOptions;
               firstFile = false;
            }
            else
            {
               if (keywordOptions != 0 && 
                   keywordOptions != treeData->m_AddFilesData.myKeywordOptions)
               {
                  keywordOptions = 0;
               }

               if (fileFormat != CVSStatus::fmtUnknown && 
                   fileFormat != treeData->m_AddFilesData.myFileFormat)
               {
                  fileFormat = CVSStatus::fmtUnknown;
               }

               if ((fileOptions & CVSStatus::foBinaryDiff) != 0 &&
                   (treeData->m_AddFilesData.myFileOptions & CVSStatus::foBinaryDiff) == 0)
               {
                  fileOptions &= ~CVSStatus::foBinaryDiff; 
               }

               if ((fileOptions & CVSStatus::foCompressed) != 0 &&
                   (treeData->m_AddFilesData.myFileOptions & CVSStatus::foCompressed) == 0)
               {
                  fileOptions &= ~CVSStatus::foCompressed; 
               }

               if ((fileOptions & CVSStatus::foUnixLines) != 0 &&
                   (treeData->m_AddFilesData.myFileOptions & CVSStatus::foUnixLines) == 0)
               {
                   fileOptions &= ~CVSStatus::foUnixLines;
               }

               if ((fileOptions & CVSStatus::foStaticFile) != 0 &&
                   (treeData->m_AddFilesData.myFileOptions & CVSStatus::foStaticFile) == 0)
               {
                   fileOptions &= ~CVSStatus::foStaticFile;
               }
            }
         }
      }
   } while (item != -1);
}



// Menu handler for "Check" in the list
void AddFilesDialog::MenuListCheck(wxCommandEvent&)
{
   CheckedListItem(-1, true, false, false);
}



// Menu handler for "Uncheck" in the list
void AddFilesDialog::MenuListUncheck(wxCommandEvent&)
{
   CheckedListItem(-1, false, false, false);
}



// Menu handler for "Autocheck" in the list
void AddFilesDialog::MenuListAutoCheck(wxCommandEvent&)
{
   CheckedListItem(-1, false, false, true);
}



// Menu handler for "Check recursive" in the list
void AddFilesDialog::MenuListCheckRecursive(wxCommandEvent&)
{
   CheckedListItem(-1, true, true, false);
}



// Menu handler for "Uncheck recursive" in the list
void AddFilesDialog::MenuListUncheckRecursive(wxCommandEvent&)
{
   CheckedListItem(-1, false, true, false);
}


// Menu handler for "AutoCheck recursive" in the list
void AddFilesDialog::MenuListAutoCheckRecursive(wxCommandEvent&)
{
   CheckedListItem(-1, false, true, true);
}


// Menu handler for "Keywords->Normal" in the list
void AddFilesDialog::MenuListKeywordsNormal(wxCommandEvent&)
{
   SetKeywordOptions(CVSStatus::keyNames | CVSStatus::keyValues);
}


// Menu handler for "Keywords->Names" in the list
void AddFilesDialog::MenuListKeywordsNames(wxCommandEvent&)
{
   SetKeywordOptions(CVSStatus::keyNames);
}


// Menu handler for "Keywords->Values" in the list
void AddFilesDialog::MenuListKeywordsValues(wxCommandEvent&)
{
   SetKeywordOptions(CVSStatus::keyValues);
}


// Menu handler for "Keywords->Locker" in the list
void AddFilesDialog::MenuListKeywordsLocker(wxCommandEvent&)
{
   SetKeywordOptions(CVSStatus::keyNames | CVSStatus::keyValues 
                     | CVSStatus::keyLocker);
}


// Menu handler for "Keywords->Disabled" in the list
void AddFilesDialog::MenuListKeywordsDisabled(wxCommandEvent&)
{
   SetKeywordOptions(CVSStatus::keyDisabled);
}


// Menu handler for "Format->ASCII" in the list
void AddFilesDialog::MenuListFormatAscii(wxCommandEvent&)
{
   SetFileFormat(CVSStatus::fmtASCII);
}


// Menu handler for "Format->Unicode" in the list
void AddFilesDialog::MenuListFormatUnicode(wxCommandEvent&)
{
   SetFileFormat(CVSStatus::fmtUnicode);
}


// Menu handler for "Format->Binary" in the list
void AddFilesDialog::MenuListFormatBinary(wxCommandEvent&)
{
   SetFileFormat(CVSStatus::fmtBinary);
}


// Menu handler for "Format->Auto detect" in the list
void AddFilesDialog::MenuListFormatAuto(wxCommandEvent&)
{
   SetFileFormat(CVSStatus::fmtUnknown);
}


// Menu handler for "Options->Binary Diff" in the list
void AddFilesDialog::MenuListOptionsBinDiff(wxCommandEvent& e)
{
   SetFileFormat(CVSStatus::fmtBinary);
   SetFileOption(CVSStatus::foBinaryDiff, e.IsChecked());
}


// Menu handler for "Options->Compressed" in the list
void AddFilesDialog::MenuListOptionsCompressed(wxCommandEvent& e)
{
   SetFileOption(CVSStatus::foCompressed, e.IsChecked());
}


// Menu handler for "Options->Unix line endings" in the list
void AddFilesDialog::MenuListOptionsUnix(wxCommandEvent& e)
{
   SetFileOption(CVSStatus::foUnixLines, e.IsChecked());
}

// Menu handler for "Options->Static file" in the list
void AddFilesDialog::MenuListOptionsStaticFile(wxCommandEvent& e)
{
   SetFileOption(CVSStatus::foStaticFile, e.IsChecked());
}


// Menu handler for "Check" in the tree
void AddFilesDialog::MenuTreeCheck(wxCommandEvent&)
{
   // Get active tree node
   wxTreeItemId id = myTreeCtrl->GetSelection();

   if (id.IsOk())
   {
      CheckedTreeItem(id, true, false, false);
   }
}


// Menu handler for "Uncheck" in the tree
void AddFilesDialog::MenuTreeUncheck(wxCommandEvent&)
{
   // Get active tree node
   wxTreeItemId id = myTreeCtrl->GetSelection();

   if (id.IsOk())
   {
      CheckedTreeItem(id, false, false, false);
   }
}


// Menu handler for "AutoCheck" in the tree
void AddFilesDialog::MenuTreeAutoCheck(wxCommandEvent&)
{
   // Get active tree node
   wxTreeItemId id = myTreeCtrl->GetSelection();

   if (id.IsOk())
   {
      CheckedTreeItem(id, false, false, true);
   }
}

// Menu handler for "Check recursive" in the tree
void AddFilesDialog::MenuTreeCheckRecursive(wxCommandEvent&)
{
   // Get active tree node
   wxTreeItemId id = myTreeCtrl->GetSelection();

   if (id.IsOk())
   {
      CheckedTreeItem(id, true, true, false);
   }
}


// Menu handler for "Uncheck recursive" in the tree
void AddFilesDialog::MenuTreeUncheckRecursive(wxCommandEvent&)
{
   // Get active tree node
   wxTreeItemId id = myTreeCtrl->GetSelection();

   if (id.IsOk())
   {
      CheckedTreeItem(id, false, true, false);
   }
}


// Menu handler for "Autocheck recursive" in the tree
void AddFilesDialog::MenuTreeAutoCheckRecursive(wxCommandEvent&)
{
   // Get active tree node
   wxTreeItemId id = myTreeCtrl->GetSelection();

   if (id.IsOk())
   {
      CheckedTreeItem(id, false, true, true);
   }
}


// Set keyword expansion for files
void AddFilesDialog::SetKeywordOptions(CVSStatus::KeywordOptions keywords)
{
   long item = -1;
   FileTree::Node* node = 0;
   do
   {
      item = myListCtrl->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
      if (item != -1)
      {
         // Get associated tree node
         node = myCurrentNodes[item];
         if (node->GetNodeType() == FileTree::TYPE_FILE)
         {
            AddFilesEntryTreeData* treeData = (AddFilesEntryTreeData*) node->GetUserData();
            ASSERT(treeData && treeData->m_Type == AddFilesTreeData::TYPE_ENTRY);

            // Set keywords 
            treeData->m_AddFilesData.myKeywordOptions = keywords;

            // File format must be either ASCII or Unicode
            if (treeData->m_AddFilesData.myFileFormat != CVSStatus::fmtASCII &&
                treeData->m_AddFilesData.myFileFormat != CVSStatus::fmtUnicode)
            {
               treeData->m_AddFilesData.myFileFormat = CVSStatus::fmtASCII;
            }

            // Storage options must not include "Binary diff"
            if (treeData->m_AddFilesData.myFileOptions & CVSStatus::foBinaryDiff)
            {
               treeData->m_AddFilesData.myFileOptions &= ~CVSStatus::foBinaryDiff;
            }
         }
      }
   } while (item != -1);
   UpdateListItems();
}



// Set format for files
void AddFilesDialog::SetFileFormat(CVSStatus::FileFormat format)
{
   long item = -1;
   FileTree::Node* node = 0;
   CVSStatus::FileFormat fmt = format;
   do
   {
      item = myListCtrl->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
      if (item != -1)
      {
         // Get associated tree node
         node = myCurrentNodes[item];
         if (node->GetNodeType() == FileTree::TYPE_FILE)
         {
            AddFilesEntryTreeData* treeData = (AddFilesEntryTreeData*) node->GetUserData();
            ASSERT(treeData && treeData->m_Type == AddFilesTreeData::TYPE_ENTRY);

            // Set file format 
            if (fmt == CVSStatus::fmtUnknown)
            {
               format = treeData->m_AutoFileFormat;
            }
            treeData->m_AddFilesData.myFileFormat = format;

            if (format == CVSStatus::fmtBinary)
            {
               // Binary files can't have keyword expansion
               treeData->m_AddFilesData.myKeywordOptions = 0;
               // Binary files can't have Unix line endings
               treeData->m_AddFilesData.myFileOptions &= ~CVSStatus::foUnixLines;
            }
            else 
            {
               // Keyword expansion must be specified
               if (treeData->m_AddFilesData.myKeywordOptions == 0)
               {
                  treeData->m_AddFilesData.myKeywordOptions = CVSStatus::keyNormal;
               }
                  
               // Storage options must not include "Binary diff"
               if (treeData->m_AddFilesData.myFileOptions & CVSStatus::foBinaryDiff)
               {
                  treeData->m_AddFilesData.myFileOptions &= ~CVSStatus::foBinaryDiff;
               }
            }
         }
      }
   } while (item != -1);
   UpdateListItems();
}


// Set file option for files
void AddFilesDialog::SetFileOption(CVSStatus::FileOption option, bool doSet)
{
   long item = -1;
   FileTree::Node* node = 0;
   do
   {
      item = myListCtrl->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
      if (item != -1)
      {
         // Get associated tree node
         node = myCurrentNodes[item];
         if (node->GetNodeType() == FileTree::TYPE_FILE)
         {
            AddFilesEntryTreeData* treeData = (AddFilesEntryTreeData*) node->GetUserData();
            ASSERT(treeData && treeData->m_Type == AddFilesTreeData::TYPE_ENTRY);

            // Set file option 
            if (doSet)
            {
               treeData->m_AddFilesData.myFileOptions |= option;
               // If option "Binary diff", set:
               // - format to binary 
               // - remove foUnixLines from options 
               // - keyword options to disabled
               if (option == CVSStatus::foBinaryDiff)
               {
                  treeData->m_AddFilesData.myFileFormat = CVSStatus::fmtBinary;
                  treeData->m_AddFilesData.myFileOptions &= ~CVSStatus::foUnixLines;
                  treeData->m_AddFilesData.myKeywordOptions = 0;
               }
               // If option "Unix line endings", set 
               // - format to ascii 
               // - remove soBinaryDiff from options
               // - keyword options to normal
               if (option == CVSStatus::foUnixLines 
                   && treeData->m_AddFilesData.myFileFormat == CVSStatus::fmtBinary)
               {
                  treeData->m_AddFilesData.myFileFormat = CVSStatus::fmtASCII;
                  treeData->m_AddFilesData.myFileOptions &= ~CVSStatus::foBinaryDiff;
                  treeData->m_AddFilesData.myKeywordOptions = CVSStatus::keyNormal;
               }
            }
            // Unset file option
            else
            {
               treeData->m_AddFilesData.myFileOptions &= ~option;
            }
         }
      }
   } while (item != -1);
   UpdateListItems();
}



// One or several list items have been (un)checked
void AddFilesDialog::CheckedListItem(long index, bool checked, bool recursive, 
                                     bool autocheck)
{
   bool bCtrlIsDown = IsControlDown();
   AddFilesEntryTreeData* treeData = 0;
   const FileTree::Node* node = 0;
   // Get list of all items that need to be processed
   std::vector<const FileTree::Node*> itemsToProcess;
   if (index >= 0)
   {
      node = myCurrentNodes[index];
      treeData = (AddFilesEntryTreeData *) node->GetUserData();
      ASSERT(treeData && treeData->m_Type == AddFilesTreeData::TYPE_ENTRY);
      if (autocheck)
         checked = !treeData->m_IsIgnored;
      if (!treeData->m_IsSelected == checked)
         itemsToProcess.push_back(node);
   }
   else
   {
      int nItems = myListCtrl->GetItemCount();
      itemsToProcess.reserve(nItems);
      for (int i = 0; i < nItems; i++)
      {
         if (myListCtrl->GetItemState(i, wxLIST_STATE_SELECTED) & wxLIST_STATE_SELECTED)
         {
            node = myCurrentNodes[i];
            treeData = (AddFilesEntryTreeData *) node->GetUserData();
            ASSERT(treeData && treeData->m_Type == AddFilesTreeData::TYPE_ENTRY);
            if (autocheck)
               checked = !treeData->m_IsIgnored;
            if (!treeData->m_IsSelected == checked)
               itemsToProcess.push_back(node);
         }
      }
   }

   CheckedItem(itemsToProcess, checked, bCtrlIsDown, recursive, autocheck);
}
   


// A tree item has been checked
void AddFilesDialog::CheckedTreeItem(wxTreeItemId item, bool checked, bool recursive,
                                     bool autocheck)
{
   bool bCtrlIsDown = IsControlDown();
   AddFilesEntryTreeData* treeData = 0;
   const FileTree::Node* node = 0;
   // Get list of all items that need to be processed
   std::vector<const FileTree::Node*> itemsToProcess;

   AddFilesGuiTreeData* guiTreeData = 
      (AddFilesGuiTreeData *) myTreeCtrl->GetItemData(item);
   ASSERT(guiTreeData);
   node = guiTreeData->m_Node;
   treeData = (AddFilesEntryTreeData*) node->GetUserData();
   if (autocheck)
      checked = !treeData->m_IsIgnored;
//   if (treeData->m_Type != AddFilesTreeData::TYPE_ENTRY || 
//      !treeData->m_IsSelected == checked || recursive)
   itemsToProcess.push_back(node);

   CheckedItem(itemsToProcess, checked, bCtrlIsDown, recursive, autocheck);
}



// Check items
void AddFilesDialog::CheckItems(const FileTree::Node* node,
                                std::map<const FileTree*, DirNodeList>& changedDirs,
                                bool checked, bool recursive, bool autocheck)
{
   AddFilesEntryTreeData* treeData = (AddFilesEntryTreeData *) node->GetUserData();
   if (treeData != 0)
   {
      if (treeData->m_Type == AddFilesTreeData::TYPE_ENTRY)
      {
//         if (!checked && !treeData->m_IsSelected && !recursive)
//            return;
//         if (checked && treeData->m_IsSelected && treeData->m_IsEnabled && !recursive)
//            return;
         if (autocheck)
            checked = !treeData->m_IsIgnored;
         treeData->m_IsSelected = checked;
         treeData->m_AddFilesData.mySelected = checked && treeData->m_IsEnabled;
      }

      if (node->GetNodeType() == FileTree::TYPE_DIR)
      {
         FileTree::DirNode* dirNode = (FileTree::DirNode*) node;
         changedDirs[node->GetFileTree()].push_back(dirNode);

         // Recurse into dir
         if (recursive)
         {
            FileTree::NodeIterator it = dirNode->Begin();
            while (it != dirNode->End()) 
            {
               CheckItems(it->second, changedDirs, checked, recursive, autocheck);
               it++;
            }
         }
      }
   }
}



// One or several items have been (un)checked
void AddFilesDialog::CheckedItem(std::vector<const FileTree::Node*>& itemsToProcess, 
                                 bool checked, bool advanced, bool recursive, 
                                 bool autocheck)
{
   // Include all items and parent items and
   // get a list of all dirs that were checked
   const FileTree::Node* node = 0;
   bool uncheckDisabled = false;
   std::map<const FileTree*, DirNodeList> changedDirs, changedDirsRecursive;
   std::map<const FileTree*, DirNodeSet> changedParents;
   std::vector<const FileTree::Node*>::iterator it = itemsToProcess.begin();
   while (it != itemsToProcess.end())
   {
      node = *it;
      // If recursive, store dirs in different list for preprocessing later
      if (recursive)
      {
         CheckItems(node, changedDirsRecursive, checked, false, autocheck);
      }
      else 
      {
         CheckItems(node, changedDirs, checked, false, autocheck);
      }

      // If checked, include parent dir
      if (checked && !advanced && !autocheck)
      {
         node = node->GetParent();
         if (node != 0)
         {
            ASSERT(node->GetNodeType() == FileTree::TYPE_DIR);
            DirNodeSet& dirsParent = changedParents[node->GetFileTree()];
            FileTree::DirNode* dirNode = (FileTree::DirNode*) node;
            dirsParent.insert(dirNode);
         }
      }
      it++;
   }
   itemsToProcess.clear();

   // if unchecked and Ctrl pressed, include all children on deselect
   if (!checked && advanced)
   {
      uncheckDisabled = true;
   }

   // Efficiently check parent dirs
   std::map<const FileTree*, DirNodeSet>::iterator itChangedParents 
      = changedParents.begin();
   while (itChangedParents != changedParents.end())
   {
      const FileTree* tree = itChangedParents->first;
      DirNodeSet& dirsParent = changedParents[tree];

      // Add dirs to list
      DirNodeList nodeList;
      nodeList.reserve(dirsParent.size());
      DirNodeSet::iterator itSet = dirsParent.begin();
      while (itSet != dirsParent.end())
      {
         nodeList.push_back(*itSet);
         itSet++;
      }
      dirsParent.clear();

      if (nodeList.size() > 1)
      {
         // reverse sort nodes by id 
         std::sort(nodeList.begin(), nodeList.end(), FileTree::SortNodesByIdReverse);

         // remove redundant parent nodes
         DirNodeList::iterator it2 = nodeList.begin();
         DirNodeList::iterator it2a = it2;
         it2a++;
         while (it2a != nodeList.end())
         {
            if ((*it2)->GetId() <= (*it2a)->GetLastChildId())
            {
               nodeList.erase(it2a);
            }
            else
            {
               it2++;
            }
            it2a = it2;
            it2a++;
         }
      }

      // Check all nodes and their parents
      DirNodeList::iterator itList = nodeList.begin();
      while (itList != nodeList.end())
      {
         node = *itList;
         while (node != 0)
         {
            ASSERT(node->GetNodeType() == FileTree::TYPE_DIR);
            FileTree::DirNode* dirNode = (FileTree::DirNode*) node;
            if (dirsParent.find(dirNode) != dirsParent.end())
            {
               break;
            }
            else
            {
               CheckItems(node, changedDirs, checked, false, autocheck);
               dirsParent.insert(dirNode);
               node = dirNode->GetParent();
            }
         }
         itList++;
      }



      itChangedParents++;
   }

   // Sort dirs that should be checked recursively by filetrees and nodeid
   std::map<const FileTree*, DirNodeList>::iterator it1 = changedDirsRecursive.begin();
   while (it1 != changedDirsRecursive.end())
   {
      DirNodeList& nodeList = it1->second;

      if (nodeList.size() > 1)
      {
         // sort nodes by id
         std::sort(nodeList.begin(), nodeList.end(), FileTree::SortNodesById);

         // remove child nodes
         DirNodeList::iterator it2 = nodeList.begin();
         DirNodeList::iterator it2a = it2;
         it2a++;
         while (it2a != nodeList.end())
         {
            if ((*it2a)->GetId() <= (*it2)->GetLastChildId())
            {
               nodeList.erase(it2a);
            }
            else
            {
               it2++;
            }
            it2a = it2;
            it2a++;
         }
      }

      // recursively check all remaining nodes
      DirNodeList::iterator it2 = nodeList.begin();
      while (it2 != nodeList.end())
      {
         CheckItems(*it2, changedDirs, checked, recursive, autocheck);
         it2++;
      }
      it1++;
   }

   // Sort nodes by filetrees and nodeid
   it1 = changedDirs.begin();
   while (it1 != changedDirs.end())
   {
      DirNodeList& nodeList = it1->second;

      if (nodeList.size() > 1)
      {
         // sort nodes by id
         std::sort(nodeList.begin(), nodeList.end(), FileTree::SortNodesById);

         // remove child nodes
         DirNodeList::iterator it2 = nodeList.begin();
         DirNodeList::iterator it2a = it2;
         it2a++;
         while (it2a != nodeList.end())
         {
            if ((*it2a)->GetId() <= (*it2)->GetLastChildId())
            {
               nodeList.erase(it2a);
            }
            else
            {
               it2++;
            }
            it2a = it2;
            it2a++;
         }
      }

      // recalculate tree for all remaining nodes
      DirNodeList::iterator it2 = nodeList.begin();
      while (it2 != nodeList.end())
      {
         CalcFileTreeEnabledNodes(*it2, true, uncheckDisabled);
         AddFilesTreeData* treeData = (AddFilesTreeData *) (*it2)->GetUserData();
         ASSERT(treeData);
         CalcGuiTreeEnabledNodes(treeData->m_TreeItemId);
         it2++;
      }
      it1++;
   }

   changedDirs.clear();
   // Update the list items
   UpdateListItems();
}




// Calculate which nodes are enabled in the file tree
void AddFilesDialog::CalcFileTreeEnabledNodes(FileTree::DirNode* node, 
                                              bool enabled,
                                              bool uncheckDisabled)
{
   // Get enabled state
   AddFilesEntryTreeData* treeData = (AddFilesEntryTreeData*) node->GetUserData();
   if (treeData && treeData->m_Type == AddFilesTreeData::TYPE_ENTRY)
   {
      enabled = treeData->m_IsEnabled && treeData->m_IsSelected;
   }

   // Set enabled status for all subnodes
   FileTree::NodeIterator it = node->Begin();
   while (it != node->End())
   {
      FileTree::Node* subnode = it->second;
      AddFilesEntryTreeData* treeData = (AddFilesEntryTreeData*) subnode->GetUserData();
      if (treeData && treeData->m_Type == AddFilesTreeData::TYPE_ENTRY)
      {
         bool itemEnabled = enabled;
         if (myMakingPatch && (treeData->m_AddFilesData.myFileFormat == CVSStatus::fmtBinary))
         {
            itemEnabled = false;
            treeData->m_IsSelected = false;
         }
         treeData->m_IsEnabled = itemEnabled;
         if (!itemEnabled && uncheckDisabled)
         {
            treeData->m_IsSelected = false;
         }
         treeData->m_AddFilesData.mySelected = itemEnabled && treeData->m_IsSelected;
      }
      if (subnode->GetNodeType() == FileTree::TYPE_DIR)
      {
         CalcFileTreeEnabledNodes((FileTree::DirNode*) subnode, enabled, 
                                  uncheckDisabled);
      }
      it++;
   }
}


// Calculate which nodes are enabled in the gui tree
void AddFilesDialog::CalcGuiTreeEnabledNodes(wxTreeItemId item)
{
   // Get enabled state
   AddFilesGuiTreeData* guiTreeData = (AddFilesGuiTreeData*) 
      myTreeCtrl->GetItemData(item);
   bool enabled = false;
   bool hasData = false;
   if (guiTreeData)
   {
      wxString s = myTreeCtrl->GetItemText(item);
      AddFilesEntryTreeData* treeData = 
         (AddFilesEntryTreeData*) guiTreeData->m_Node->GetUserData();
      if (treeData && treeData->m_Type == AddFilesTreeData::TYPE_ENTRY)
      {
         enabled = treeData->m_IsSelected && treeData->m_IsEnabled;
         hasData = true;
      }
   }

   wxColour colDisabled = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
   wxColour colEnabled = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);

   if (hasData)
   {
      if (enabled)
      {
         myTreeCtrl->SetItemTextColour(item, colEnabled);
         myTreeCtrl->SetItemBold(item, true);
         TreeView_SetItemState(GetHwndOf(myTreeCtrl), (HTREEITEM)item.m_pItem,
                               INDEXTOOVERLAYMASK(myAddedImageIndex), TVIS_OVERLAYMASK);
      }
      else
      {
         myTreeCtrl->SetItemTextColour(item, colDisabled);
         myTreeCtrl->SetItemBold(item, false);
         TreeView_SetItemState(GetHwndOf(myTreeCtrl), (HTREEITEM)item.m_pItem,
                               INDEXTOOVERLAYMASK(myNotInCVSImageIndex), TVIS_OVERLAYMASK);
      }
   }
   else
   {
      TreeView_SetItemState(GetHwndOf(myTreeCtrl), (HTREEITEM)item.m_pItem,
                            INDEXTOOVERLAYMASK(myInCVSImageIndex), TVIS_OVERLAYMASK);
   }


   wxTreeItemIdValue cookie;
   wxTreeItemId subitem;
   subitem = myTreeCtrl->GetFirstChild(item, cookie);
   while (subitem.IsOk())
   {
      CalcGuiTreeEnabledNodes(subitem);
      subitem = myTreeCtrl->GetNextChild(item, cookie);
   }
}



// Compare node paths
bool AddFilesDialog::CompareListItems(FileTree::Node* node1,
                                      FileTree::Node* node2)
{
   AddFilesEntryTreeData* treeData1 = (AddFilesEntryTreeData*) node1->GetUserData();
   AddFilesDialog* dialog = treeData1->m_Dialog;
   AddFilesEntryTreeData* treeData2 = (AddFilesEntryTreeData*) node2->GetUserData();

   bool result = false;
   bool bProcessed = false;
   if (dialog->myCurrentSortColumn == COL_ID_NAME)
   {
      if (dialog->myCurrentIncludeSubdirs)
      {
         result = strcmpi(node1->GetPathFrom(dialog->myCurrentNode).c_str(),
                          node2->GetPathFrom(dialog->myCurrentNode).c_str()) < 0;
         bProcessed = true;
      }
      else
      {
         result = strcmpi(node1->GetName().c_str(), node2->GetName().c_str()) < 0;
         bProcessed = true;
      }
   }
   else if (dialog->myCurrentSortColumn == COL_ID_TYPE)
   {
      result = _tcsicmp(treeData1->GetFileType().c_str(), treeData2->GetFileType().c_str()) < 0;
      bProcessed = true;
   }
   else if (dialog->myCurrentSortColumn == COL_ID_ADD)
   {
      result = (treeData1->m_IsSelected && treeData1->m_IsEnabled)
         && !(treeData2->m_IsSelected && treeData2->m_IsEnabled); 
      bProcessed = true;
   }
   else if (dialog->myCurrentSortColumn == COL_ID_FORMAT)
   { 
      result = _tcsicmp(treeData1->GetFormatText().c_str(), treeData2->GetFormatText().c_str()) < 0;
      bProcessed = true;
   }
   else if (dialog->myCurrentSortColumn == COL_ID_KEYWORDS)
   {
      result = _tcsicmp(treeData1->GetKeywordsText().c_str(), treeData2->GetKeywordsText().c_str()) < 0;
      bProcessed = true;
   }
   else if (dialog->myCurrentSortColumn == COL_ID_OPTIONS && dialog->myShowOptions)
   {
      result = _tcsicmp(treeData1->GetOptionsText().c_str(), treeData2->GetOptionsText().c_str()) < 0;
      bProcessed = true;
   }

   if (bProcessed && (dialog->myCurrentSortAsc == false))
      result = !result;
   return result;
}


// Expand tree nodes
void AddFilesDialog::ExpandTreeNodes(wxTreeItemId item, int levels)
{
   if (item != myTreeCtrl->GetRootItem())
      myTreeCtrl->Expand(item);

   if (levels == -1 || levels > 0)
   {
      if (levels > 0)
      {
         levels--;
      }
      wxTreeItemIdValue cookie;
      item = myTreeCtrl->GetFirstChild(item, cookie);
      while (item.IsOk())
      {
         ExpandTreeNodes(item, levels);
         item = myTreeCtrl->GetNextChild(item, cookie);
      }
   }
}



// Sort items in listview
void AddFilesDialog::SortListItems()
{
   std::stable_sort(myCurrentNodes.begin(), myCurrentNodes.end(), 
                    AddFilesDialog::CompareListItems);
   myListCtrl->SetSortIndicator(myCurrentSortColumn, myCurrentSortAsc);
   UpdateListItems();
}



//////////////////////////////////////////////////////////////////////////////
// AddFilesTreeData

AddFilesEntryTreeData::AddFilesEntryTreeData(FileTree::Node& node,
                                             DirectoryGroup::Entry& entry,
                                             void* data)
   : m_AddFilesData(*(DirectoryGroup::AddFilesData*) entry.myUserData),
     m_Entry(entry),
     m_Dialog((AddFilesDialog*) data),
     m_IsTypeValid(false)
     
{ 
   m_Type = TYPE_ENTRY; 

   if (node.GetNodeType() == FileTree::TYPE_DIR)
   {
      m_IsDirectory = true;
   }
   else
   {
      m_IsDirectory = false;
   }
   
   m_IsSelected = m_AddFilesData.mySelected;

   CVSStatus::GuessFileFormat(entry.myAbsoluteFile, m_AddFilesData.myFileFormat, m_AddFilesData.myFileOptions);
   // If Unicode is not supported, use binary instead
   if (m_AddFilesData.myFileFormat == CVSStatus::fmtUnicode
      && !entry.myDirectoryGroup.myCVSServerFeatures.SupportsUnicode())
   {
      m_AddFilesData.myFileFormat = CVSStatus::fmtBinary;
   }
   m_AutoFileFormat = m_AddFilesData.myFileFormat;
   m_IsIgnored = !m_IsSelected;

   // Default settings for keyword options
   if (m_AddFilesData.myFileFormat == CVSStatus::fmtBinary)
   {
      m_AddFilesData.myKeywordOptions = 0;
   }
   else
   {
      m_AddFilesData.myKeywordOptions = CVSStatus::keyNormal;
   }
}



// Get format as text
wxString AddFilesEntryTreeData::GetFormatText() const
{
   wxString result;
   if (m_IsDirectory)
      result = _("Folder");
   else
      result = CVSStatus::FileFormatString(m_AddFilesData.myFileFormat);
   return result;
}


// Get keywords as text
wxString AddFilesEntryTreeData::GetKeywordsText() const
{
   wxString result;
   if (!m_IsDirectory)
   {
      result = CVSStatus::KeywordOptionsString(m_AddFilesData.myKeywordOptions);
   }
   return result;
}


// Get file options as text
wxString AddFilesEntryTreeData::GetOptionsText() const
{
   wxString result;
   if (!m_IsDirectory)
   {
      result = CVSStatus::FileOptionsString(m_AddFilesData.myFileOptions);
   }
   return result;
}



// Get file type
wxString AddFilesEntryTreeData::GetFileType() const
{
   if (!m_IsTypeValid)
   {
      SHFILEINFO sfi;
      if (m_IsDirectory)
      {
         return _("Folder");
      }
      SHGetFileInfo(wxTextCStr(m_Entry.myAbsoluteFile), FILE_ATTRIBUTE_NORMAL, &sfi, 
                    sizeof(sfi), SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES);
      if (sfi.szTypeName[0])
      {
         m_FileType = sfi.szTypeName;
      }
      else
      {
         std::string s = GetExtension(m_Entry.myAbsoluteFile);
         MakeUpperCase(s);
         if (s.length() > 0)
         {
            m_FileType = Printf(_("%s File"), wxTextCStr(s));
         }
         else
         {
            m_FileType = _("File");
         }
      }
      m_IsTypeValid = true;
   }
   return m_FileType;
}

// Tree right click
void AddFilesDialog::TreeRightClick(wxTreeEvent& event)
{
   ShowTreeContextMenu(event.GetPoint());
}


// Tree selection changed
void AddFilesDialog::TreeSelectionChanged(wxTreeEvent& event)
{
   if (!myInitialized)
      return;
   wxTreeItemId id = event.GetItem();
   if (id.IsOk())
      RebuildListView();
}

void AddFilesDialog::DoubleClick(wxListEvent& event)
{
   long item = event.GetIndex();
   if (item >= 0)
   {
      CheckedListItem(item, !myListCtrl->IsChecked(item), false, false);
   }
}

void AddFilesDialog::ColumnClick(wxListEvent& e)
{
   if (myCurrentSortColumn != e.GetColumn())
   {
      myCurrentSortColumn = e.GetColumn();
      myCurrentSortAsc = true;
   }
   else
   {
      myCurrentSortAsc = !myCurrentSortAsc;
   }
   SortListItems();
}

void AddFilesDialog::ItemChecked(wxListEvent& e)
{
   CheckedListItem(e.GetIndex(), true, false, false);
}


void AddFilesDialog::ItemUnchecked(wxListEvent& e)
{
   CheckedListItem(e.GetIndex(), false, false, false);
}



// Show context menu for listview
void ListCtrlEventHandler::ContextMenu(wxContextMenuEvent& event)
{
   // Check if shift is down
   bool bShiftIsDown = IsShiftDown();

   // Get number of selected items
   int iSelectedItems = myParent->myListCtrl->GetSelectedItemCount();

   // Check if an item is selected
   bool bItemsSelected = iSelectedItems > 0;
   wxMenu popupMenu;

   wxTreeItemId id = myParent->myTreeCtrl->GetSelection();

   AddFilesGuiTreeData *guiTreeData = (AddFilesGuiTreeData *) myParent->myTreeCtrl->GetItemData(id);
   ASSERT(guiTreeData != 0);

   // Get server features
   const CVSServerFeatures& serverFeatures = guiTreeData->m_DirectoryGroup->myCVSServerFeatures;

   if (!bItemsSelected)
   {
      if (!myParent->PopulateTreeContextMenu(id, popupMenu))
         return;
   }
   else
   {
      bool bAllDirs = false;
      const FileTree::Node* commonParent = 0;
      CVSStatus::KeywordOptions keywordOptions = 0;
      CVSStatus::FileFormat fileFormat = CVSStatus::fmtUnknown;
      CVSStatus::FileOptions fileOptions = 0;

      // Analyze list selection
      myParent->AnalyzeListSelection(bAllDirs, keywordOptions, fileFormat, 
                                     fileOptions, commonParent);

      // Always show "Check" and "Uncheck"
      popupMenu.Append(ADDFILES_CONTEXT_ID_LIST_CHECK,
                       _("Check"), _("Check item(s)"));
      popupMenu.Append(ADDFILES_CONTEXT_ID_LIST_UNCHECK,
                       _("Uncheck"), _("Uncheck item(s)"));
      bool addIgnored = myParent->myDirectoryGroups.GetBooleanPreference("Add Ignored Files");
      if (addIgnored)
          popupMenu.Append(ADDFILES_CONTEXT_ID_LIST_AUTOCHECK,
                           _("Autoignore"), _("Ignore item(s) according to CVS ignore rules"));

      // For directories show "Check recursively" and "Uncheck recursively"
      if (bAllDirs)
      {
         popupMenu.AppendSeparator();
         // Show recursive (un)check
         popupMenu.Append(ADDFILES_CONTEXT_ID_LIST_CHECK_REC,
                          _("Check recursively"), 
                          _("Check item(s) recursively"));
         popupMenu.Append(ADDFILES_CONTEXT_ID_LIST_UNCHECK_REC,
                          _("Uncheck recursively"), _("Uncheck item(s) recursively"));
         if (addIgnored)
             popupMenu.Append(ADDFILES_CONTEXT_ID_LIST_AUTOCHECK_REC,
                              _("Autoignore recursively"), _("Ignore item(s) recursively according to CVS ignore rules"));
      }

      // If selected files are not only directories, show keywords, format and options
      if (!bAllDirs)
      {
         popupMenu.AppendSeparator();
         // Add keyword options
         {
            wxMenu* subMenu = new wxMenu();
            typedef std::pair<CVSStatus::KeywordOptions, int> KeywordEntry;
            std::vector<KeywordEntry> keywords;
            keywords.push_back(KeywordEntry(CVSStatus::keyNames | CVSStatus::keyValues, 
                                            ADDFILES_CONTEXT_ID_LIST_KEYWORDS_NORMAL));
            keywords.push_back(KeywordEntry(CVSStatus::keyNames, 
                                            ADDFILES_CONTEXT_ID_LIST_KEYWORDS_NAMES));
            keywords.push_back(KeywordEntry(CVSStatus::keyValues, 
                                            ADDFILES_CONTEXT_ID_LIST_KEYWORDS_VALUES));
            keywords.push_back(KeywordEntry(CVSStatus::keyNames | CVSStatus::keyValues
                                            | CVSStatus::keyLocker, ADDFILES_CONTEXT_ID_LIST_KEYWORDS_LOCKER));
            keywords.push_back(KeywordEntry(CVSStatus::keyDisabled, 
                                            ADDFILES_CONTEXT_ID_LIST_KEYWORDS_DISABLED));
            for (size_t i = 0; i < keywords.size(); i++)
            {
               wxString s1 = CVSStatus::KeywordOptionsString(keywords[i].first).c_str();
               wxString s2 = Printf(_("Set keyword expansion to %s"),
                                    CVSStatus::KeywordOptionsString(keywords[i].first).c_str()).c_str();

               subMenu->Append(keywords[i].second, s1, s2, wxITEM_CHECK);
               if (keywordOptions == keywords[i].first)
               {
                  subMenu->Check(keywords[i].second, true);
               }
            }
            popupMenu.Append(0, _("Keywords"), subMenu);
         }

         // Add file formats
         {
            wxMenu* subMenu = new wxMenu();
            typedef std::pair<CVSStatus::FileFormat, int> FormatEntry;
            std::vector<FormatEntry> formats;
            formats.push_back(FormatEntry(CVSStatus::fmtASCII, ADDFILES_CONTEXT_ID_LIST_FORMAT_ASCII));
            if (serverFeatures.SupportsUnicode())
               formats.push_back(FormatEntry(CVSStatus::fmtUnicode, ADDFILES_CONTEXT_ID_LIST_FORMAT_UNICODE));
            formats.push_back(FormatEntry(CVSStatus::fmtBinary, ADDFILES_CONTEXT_ID_LIST_FORMAT_BINARY));
            for (size_t i = 0; i < formats.size(); i++)
            {
               wxString s1 = CVSStatus::FileFormatString(formats[i].first).c_str();
               wxString s2 = Printf(_("Set format to %s"),
                                    CVSStatus::FileFormatString(formats[i].first).c_str()).c_str();

               subMenu->Append(formats[i].second, s1, s2, wxITEM_CHECK);
               if (fileFormat == formats[i].first)
               {
                  subMenu->Check(formats[i].second, true);
               }
            }
            subMenu->AppendSeparator();
            subMenu->Append(ADDFILES_CONTEXT_ID_LIST_FORMAT_AUTO, 
                            _("Autodetect"), _("Autodetect file format"));
            popupMenu.Append(0, _("Format"), subMenu);
         }

         // Add file options
         if (serverFeatures.SupportsFileOptions())
         {
            wxMenu* subMenu = new wxMenu();
            typedef std::pair<CVSStatus::FileOption, int> FileOptionsEntry;
            std::vector<FileOptionsEntry> options;
            options.push_back(FileOptionsEntry(CVSStatus::foBinaryDiff, 
                                               ADDFILES_CONTEXT_ID_LIST_OPTIONS_BINDIFF));
            options.push_back(FileOptionsEntry(CVSStatus::foCompressed, 
                                               ADDFILES_CONTEXT_ID_LIST_OPTIONS_COMPRESSED));
            options.push_back(FileOptionsEntry(CVSStatus::foUnixLines, 
                                               ADDFILES_CONTEXT_ID_LIST_OPTIONS_UNIX));
            options.push_back(FileOptionsEntry(CVSStatus::foStaticFile, ADDFILES_CONTEXT_ID_LIST_OPTIONS_STATIC));
            for (size_t i = 0; i < options.size(); i++)
            {
               wxString s1 = CVSStatus::FileOptionString(options[i].first).c_str();
               wxString s2 = Printf(_("Set options to %s"),
                                    CVSStatus::FileOptionString(options[i].first).c_str()).c_str();

               subMenu->AppendCheckItem(options[i].second, s1, s2);
               if (fileOptions & options[i].first)
               {
                  subMenu->Check(options[i].second, true);
               }
            }
            popupMenu.Append(0, _("Options"), subMenu);
         }
      }

      // Add Explorer menu
      popupMenu.AppendSeparator();
      ExplorerMenu* explorerMenu = new ExplorerMenu();
      const FileTree::Node* node = guiTreeData->m_Node;
      std::vector<std::string> files;
      
      myParent->GetListSelectionRelativePaths(node, files);
      
      explorerMenu->FillMenu((HWND) myParent->GetHandle(), node->GetFullName(), files);
      popupMenu.Append(0, _("Explorer"), explorerMenu);
   }

   wxPoint pos = event.GetPosition();
   // Calc selection
   if (pos == wxPoint(-1, -1))
   {
      pos = myParent->myListCtrl->GetContextMenuPos(myParent->myListCtrl->GetNextItem(-1, 
         wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED));
      pos = myParent->myListCtrl->ClientToScreen(pos);
   }
   myParent->PopupMenu(&popupMenu, myParent->ScreenToClient(pos));
}
