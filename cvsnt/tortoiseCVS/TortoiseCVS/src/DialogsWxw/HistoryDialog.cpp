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



#include "StdAfx.h"

#include "HistoryDialog.h"
#include "BranchTagDlg.h"
#include "HistoryTreeCtrl.h"
#include "WxwHelpers.h"
#include "WxwMain.h"
#include <wx/statline.h>
#include <Utils/Translate.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/PathUtils.h>
#include <Utils/ShellUtils.h>
#include <Utils/TortoiseDebug.h>
#include <Utils/TortoiseRegistry.h>
#include <Utils/LogUtils.h>
#include <Utils/TortoiseException.h>
#include <Utils/ProcessUtils.h>
#include <Utils/StringUtils.h>
#include <Utils/Keyboard.h>
#include <Utils/LaunchTortoiseAct.h>
#include <Utils/Preference.h>
#include <CVSGlue/CVSStatus.h>
#include <CVSGlue/CVSAction.h>
#include <TortoiseAct/TortoiseActVerbs.h>
#include "FilenameText.h"
#include "ExtTextCtrl.h"

static const int ICONSIZE = 12;


enum 
{
   HISTORY_CONTEXT_ID_DIFF_WORKFILE = 10001,
   HISTORY_CONTEXT_ID_DIFF_SELECTED,
   HISTORY_CONTEXT_ID_VIEW,
   HISTORY_CONTEXT_ID_ANNOTATE,
   HISTORY_CONTEXT_ID_GET,
   HISTORY_CONTEXT_ID_GET_CLEAN,
   HISTORY_CONTEXT_ID_SAVE_AS,
   HISTORY_CONTEXT_ID_MAKE_TAG,
   HISTORY_CONTEXT_ID_MAKE_BRANCH,
   HISTORY_CONTEXT_ID_MERGE,
   HISTORY_CONTEXT_ID_UNDO,
   HISTORY_CONTEXT_ID_COPY_TO_CLIPBRD,

   HISTORYDLG_ID_TREECTRL,
   HISTORYDLG_ID_SHOWTAGS,
   HISTORYDLG_ID_APPLY,
   HISTORYDLG_ID_COMMENT
};



static const int DEF_COLUMN_SIZE = 100;
static const int MAX_COLUMN_SIZE = 2000;

#define HISTORY_BRANCH_RGB              0xFF,0xFF,0xD0
#define HISTORY_SYMBOLIC_RGB            0xE0,0xFF,0xE0

int HistoryDialog::myRefCount = 0;
wxColour* HistoryDialog::myBranchColor = 0;
wxColour* HistoryDialog::mySymbolicColor = 0;

enum
{
   IconRevision,        // Image list offset 0
   IconTag,             // Image list offset 1
   IconBranch           // Image list offset 2
};


bool DoHistoryDialog(wxWindow* parent, const std::string& filename)
{
   TDEBUG_ENTER("DoHistoryDialog");
   HistoryDialog* dlg = new HistoryDialog(parent, filename);

   bool ret = true;
   
   wxString sTitle = Printf(_("History for %s"), wxTextCStr(filename));
   if (IsControlDown())
      dlg->myTree = GetLogGraph(sTitle, parent, filename.c_str(), dlg->myParserData);
   else
   {
// TODO: Integrate this feature in the UI
//      dlg->myTree = GetLogGraph(sTitle, parent, filename.c_str(), true, 50);
      dlg->myTree = GetLogGraph(sTitle, parent, filename.c_str(), dlg->myParserData);
   }
   if (dlg->myTree)
   {
      dlg->Refresh();
      dlg->myListCtrl->ScrollTo(dlg->mySandboxItemId);
      dlg->myListCtrl->SelectItem(dlg->mySandboxItemId);

      ret = (dlg->ShowModal() == wxID_OK);
   }
   DestroyDialog(dlg);
   return ret;
}


BEGIN_EVENT_TABLE(HistoryDialog, ExtDialog)
   EVT_COMMAND(HISTORYDLG_ID_SHOWTAGS,  wxEVT_COMMAND_CHECKBOX_CLICKED, HistoryDialog::SymbolicCheckChanged)
   EVT_COMMAND(HISTORYDLG_ID_APPLY,     wxEVT_COMMAND_BUTTON_CLICKED,   HistoryDialog::Apply)
   EVT_COMMAND(HISTORYDLG_ID_COMMENT,   wxEVT_COMMAND_TEXT_UPDATED,     HistoryDialog::CommentChanged)
   EVT_COMMAND(HISTORYDLG_ID_COMMENT,   wxEVT_COMMAND_TEXT_URL,         HistoryDialog::OnTextUrl)

   EVT_MENU(HISTORY_CONTEXT_ID_MAKE_TAG,          HistoryDialog::OnMakeTag)
   EVT_MENU(HISTORY_CONTEXT_ID_MAKE_BRANCH,       HistoryDialog::OnMakeBranch)
   EVT_MENU(HISTORY_CONTEXT_ID_DIFF_WORKFILE,     HistoryDialog::OnMenuDiffWorkfile)
   EVT_MENU(HISTORY_CONTEXT_ID_DIFF_SELECTED,     HistoryDialog::OnMenuDiffSelected)
   EVT_MENU(HISTORY_CONTEXT_ID_VIEW,              HistoryDialog::OnMenuView)
   EVT_MENU(HISTORY_CONTEXT_ID_ANNOTATE,          HistoryDialog::OnMenuAnnotate)
   EVT_MENU(HISTORY_CONTEXT_ID_GET,               HistoryDialog::OnMenuGet)
   EVT_MENU(HISTORY_CONTEXT_ID_GET_CLEAN,         HistoryDialog::OnMenuGetClean)
   EVT_MENU(HISTORY_CONTEXT_ID_SAVE_AS,           HistoryDialog::OnMenuSaveAs)
   EVT_MENU(HISTORY_CONTEXT_ID_COPY_TO_CLIPBRD,   HistoryDialog::OnMenuCopyToClipboard)
   EVT_MENU(HISTORY_CONTEXT_ID_MERGE,             HistoryDialog::OnMenuMerge)
   EVT_MENU(HISTORY_CONTEXT_ID_UNDO,              HistoryDialog::OnMenuUndo)
END_EVENT_TABLE()


void HistoryDialog::ProcessNode(CLogNode* node, wxTreeItemId rootId)
{
   wxTreeItemId id;
   do
   {
      switch(node->GetType())
      {
         case kNodeHeader:
         {
            if (!myTreeUpsideDown)
            {
               id = myListCtrl->AppendItem(rootId, wxT("HEAD"), IconBranch, -1,
                                           new MyTreeItemData(node), false);
            }
            else
            {
               id = myListCtrl->PrependItem(rootId, wxT("HEAD"), IconBranch, -1,
                                            new MyTreeItemData(node), false);
            }
            if (mySandboxStickyTag.empty())
            {
               myListCtrl->SetItemBold(id, true);
            }
            myListCtrl->Expand(rootId);

            CLogNodeHeader* headernode = (CLogNodeHeader*) node;
            int c = headernode->RcsFile().TotRevisions();
            myStatusBar->SetStatusText(Printf(_("%d revision(s)"), c).c_str());
            break;
         }

         case kNodeBranch:
         {
            CLogNodeBranch* branchNode = static_cast<CLogNodeBranch*>(node);
            const CLogStr& branch = branchNode->Branch();
            id = myListCtrl->AppendItem(rootId, wxText(branch.c_str()), IconBranch, -1,
                                        new MyTreeItemData(node), false);
            myListCtrl->SetItemBackgroundColour(id, *myBranchColor);
            if (std::string(branch.c_str()) == mySandboxStickyTag)
            {
               myListCtrl->SetItemBold(id, true);
            }
            myListCtrl->Expand(rootId);
            rootId = id;
            break;
         }

         case kNodeRev:
         {
            const CRevFile& item = ((CLogNodeRev*) node)->Rev();
            wxString revision(wxText(item.RevNum().str()));
            if (!myTreeUpsideDown)
               id = myListCtrl->AppendItem(rootId, revision, IconRevision, -1, new MyTreeItemData(node));
            else
               id = myListCtrl->PrependItem(rootId, revision, IconRevision, -1, new MyTreeItemData(node));
            if (!strcmp("dead", item.State().c_str()))
                myListCtrl->SetItemTextColour(id, *wxRED);
            // Do not show newlines and tabs in list
            std::string comment = item.DescLog().c_str();
            FindAndReplace<std::string>(comment, "\n", " ");
            FindAndReplace<std::string>(comment, "\t", " ");
            myListCtrl->SetItemText(id, COL_CHECKEDIN, item.DateAsString());
            myListCtrl->SetItemText(id, COL_AUTHOR, wxText(item.Author().c_str()));
            myListCtrl->SetItemText(id, COL_CHANGES, wxText(item.Changes()));
            myListCtrl->SetItemText(id, COL_BUGNUMBER, wxText(item.BugNumber().c_str()));
            myListCtrl->SetItemText(id, COL_COMMENT, wxText(comment));
            if (std::string(item.RevNum().str()) == mySandboxRevision)
            {
               myListCtrl->SetItemBold(id, true);
               mySandboxItemId = id;
            }
            break;
         }

         case kNodeTag:
         {
            bool showTags = myShowTagsCheckBox->GetValue();
            if (showTags)
            {
               CLogNodeTag* tagNode = static_cast<CLogNodeTag*>(node);
               const CLogStr& tag = tagNode->Tag();
               id = myListCtrl->AppendItem(rootId, wxText(tag.c_str()), IconTag, -1, 
                                           new MyTreeItemData(node), false);
               myListCtrl->SetItemBackgroundColour(id, *mySymbolicColor);
               if (std::string(tag.c_str()) == mySandboxStickyTag)
               {
                  myListCtrl->SetItemBold(id, true);
               }
            }
            break;
         }

         default:
         {
            break;
         }
      }
      if (myTreeUpsideDown)
      {
         std::vector<CLogNode*>::iterator it;
         if (node->GetType() == kNodeRev)
         {
            // Process tags first
            it = node->Childs().begin();
            while (it != node->Childs().end())
            {
               if ((*it)->GetType() == kNodeTag)
                  ProcessNode(*it, id);
               it++;
            }
            // Process rest next
            it = node->Childs().begin();
            while (it != node->Childs().end())
            {
               if ((*it)->GetType() != kNodeTag)
                  ProcessNode(*it, id);
               it++;
            }
         }
         else
         {
            it = node->Childs().begin();
            while (it != node->Childs().end())
            {
               ProcessNode(*it, id);
               it++;
            }
         }
         
      }
      else
      {
         std::vector<CLogNode*>::reverse_iterator it;
         if (node->GetType() == kNodeRev)
         {
            // Process tags first
            it = node->Childs().rbegin();
            while (it != node->Childs().rend())
            {
               if ((*it)->GetType() == kNodeTag)
                  ProcessNode(*it, id);
               it++;
            }
            // Process rest next
            it = node->Childs().rbegin();
            while (it != node->Childs().rend())
            {
               if ((*it)->GetType() != kNodeTag)
                  ProcessNode(*it, id);
               it++;
            }
         }
         else
         {
            it = node->Childs().rbegin();
            while (it != node->Childs().rend())
            {
               ProcessNode(*it, id);
               it++;
            }
         }
      }
      node = node->Next();
   } while (node);
   myListCtrl->Expand(rootId);
}


void HistoryDialog::Refresh() 
{
   TDEBUG_ENTER("HistoryDialog::Populate");
   myListCtrl->DeleteAllItems();
   RefreshFileStatus();
   // Dummy root, not displayed
   myListCtrl->AddRoot(wxT("root"));
   ProcessNode(myTree, myListCtrl->GetRootItem());
}


void HistoryDialog::Update() 
{
   delete myTree;
   myParserData.Clear();
   myTree = GetLogGraph(Printf(_("History for %s"), wxTextCStr(myFilename)),
                        this, myFilename.c_str(), myParserData);
   Refresh();
}




CLogNode* HistoryDialog::GetTree() const
{
    return myTree;
}



class HistoryDialogEvtHandler : public wxEvtHandler
{
public:
   HistoryDialogEvtHandler(HistoryDialog* parent)
      : myParent(parent),
        myIgnoreChange(false)
   {
   }

   // TreeList control
   
   void OnListTreeSelChanged(wxTreeEvent& event);

   void OnListTreeRightClick(wxTreeEvent& event);

   void OnListTreeContextMenu(wxContextMenuEvent& event);

   void OnListTreeColClick(wxListEvent& event);

private:
   HistoryDialog*       myParent;
   bool                 myIgnoreChange;

   DECLARE_EVENT_TABLE()
};


HistoryDialog::HistoryDialog(wxWindow* parent, const std::string& filename)
   : ExtDialog(parent, -1, Printf(_("TortoiseCVS - History for %s"), wxText(filename).c_str()),
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX | 
               wxMAXIMIZE_BOX),
     myFilename(filename),
     myCommentChanged(false),
     myCommentChangedEnabled(false),
     myTree(0),
     myTreeUpsideDown(false),
     mySortColumn(0)
{
   if (!myRefCount++)
      InitializeStatics();

   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

   // Information about current revision
   mySandboxRevision = CVSStatus::GetRevisionNumber(filename);
   mySandboxStickyTag = CVSStatus::GetStickyTag(filename);

   myOfferView = FileIsViewable(filename);

   // Create splitter
   mySplitter = new ExtSplitterWindow(this, -1, wxDefaultPosition, 
                                      wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
   wxPanel* topPanel = new wxPanel(mySplitter, -1);
   wxPanel* bottomPanel = new wxPanel(mySplitter, -1, wxDefaultPosition, wxDefaultSize, wxDOUBLE_BORDER);
   mySplitter->SplitHorizontally(topPanel, bottomPanel, 0);
   mySplitter->SetMinimumPaneSize(wxDLG_UNIT_Y(this, 30));
   mySplitter->SetSize(wxDLG_UNIT(this, wxSize(150, 90)));
   
   std::string CVSRoot = CVSStatus::CVSRootForPath(filename);
   wxString labelText(_("File"));
   labelText += _(":");
   wxStaticText* labelFilename = new wxStaticText(this, -1, labelText,
                                                  wxDefaultPosition,
                                                  wxSize(wxDLG_UNIT_X(this, 55), -1));
   FilenameText* textFilename = new FilenameText(this, -1, wxT("%s"), wxText(filename));
   labelText = _("CVSROOT");
   labelText += _(":");
   wxStaticText* labelCVSRoot = new wxStaticText(this, -1, labelText);
   wxStaticText* textCvsRoot = new wxStaticText(this, -1, wxText(CVSRoot));

   labelText = _("Sticky tag");
   labelText += _(":");
   wxStaticText* labelTag = new wxStaticText(this, -1, labelText,
                                             wxDefaultPosition,
                                             wxSize(wxDLG_UNIT_X(this, 55), -1));
   myTextTag = new wxStaticText(this, -1, wxT(""));

   labelText = _("Status");
   labelText += _(":");
   wxStaticText* labelStatus = new wxStaticText(this, -1, labelText);
   myTextStatus = new wxStaticText(this, -1, wxT(""));


   CVSStatus::KeywordOptions keyword;
   CVSStatus::FileFormat fileformat;
   CVSStatus::FileOptions fileOptions;
   CVSStatus::GetParsedOptions(filename, &fileformat, &keyword, &fileOptions); 
    
   labelText = _("File format");
   labelText += _(":");
   wxStaticText* labelFileFormat = new wxStaticText(this, -1, labelText);
   wxStaticText* textFileFormat = new wxStaticText(this, -1, 
                                                   CVSStatus::FileFormatString(fileformat).c_str());
    
   labelText = _("Keywords");
   labelText += _(":");
   wxStaticText* labelKeyword = new wxStaticText(this, -1, labelText);
   myDefaultKWS = CVSStatus::KeywordOptionsString(keyword);
   myTextKeywordSubst = new wxStaticText(this, -1, myDefaultKWS);

   labelText = _("Options");
   labelText += _(":");
   wxStaticText* labelStorage = new wxStaticText(this, -1, labelText);
   wxStaticText* textStorage = new wxStaticText(this, -1, 
                                                CVSStatus::FileOptionsString(fileOptions).c_str());

   labelText = _("Other");
   labelText += _(":");
   wxStaticText* labelOther = new wxStaticText(this, -1, labelText);
   myTextOtherInfo = new wxStaticText(this, -1, wxT(""));

   myImageList = new wxImageList(ICONSIZE, ICONSIZE);
    
   myImageList->Add(wxIcon(wxT("IDI_TYPEREV"), wxBITMAP_TYPE_ICO_RESOURCE));
   myImageList->Add(wxIcon(wxT("IDI_TYPESYM"), wxBITMAP_TYPE_ICO_RESOURCE));
   myImageList->Add(wxIcon(wxT("IDI_TYPEBRA"), wxBITMAP_TYPE_ICO_RESOURCE));
    
   // List control with all revisions
    
   myListCtrl = new HistoryTreeCtrl(this, topPanel, HISTORYDLG_ID_TREECTRL, wxDefaultPosition, 
                                    wxDLG_UNIT(this, wxSize(250, 100)),
                                    wxTR_HIDE_ROOT |
                                    wxTR_HAS_BUTTONS | wxTR_MULTIPLE | wxTR_EXTENDED | wxBORDER_NONE);
   myListCtrl->AddColumn(wxString(_("Revision")));
   myListCtrl->AddColumn(wxString(_("Date")));
   myListCtrl->AddColumn(wxString(_("Author")));
   myListCtrl->AddColumn(wxString(_("Changes")));
   myListCtrl->AddColumn(wxString(_("Bug Number")));
   myListCtrl->AddColumn(wxString(_("Comment")));
   // Image list
   myListCtrl->AssignImageList(myImageList);

   // Text control for comment

   labelText = _("Comment");
   labelText += _(":"); 
   wxStaticText* commentLabel = new wxStaticText(bottomPanel, -1, labelText);

   myComment = new ExtTextCtrl(bottomPanel, HISTORYDLG_ID_COMMENT, wxT(""), wxDefaultPosition, wxDefaultSize, 
                               wxTE_RICH2 | wxTE_MULTILINE | wxTE_AUTO_URL);
   myComment->SetPlainText(true);
   myComment->SetMaxLength(20000);
   myComment->SetSize(-1, wxDLG_UNIT_Y(topPanel, 40));
    
   // Checkbox for showing/hiding symbolics
   myShowTagsCheckBox = new wxCheckBox(this, HISTORYDLG_ID_SHOWTAGS, _("Show &tags"));

   // Sizer for whole dialog
   wxBoxSizer *sizerMain = new wxBoxSizer(wxVERTICAL);

   // Sizer for header info
   wxBoxSizer *sizerHeader = new wxBoxSizer(wxVERTICAL);
   wxFlexGridSizer *gridSizer = new wxFlexGridSizer(2, 0, 0);
   gridSizer->AddGrowableCol(1);
   gridSizer->Add(labelFilename, 1, wxALL, 3);
   gridSizer->Add(textFilename, 2, wxALL | wxGROW, 3);
   gridSizer->Add(labelCVSRoot, 1, wxALL, 3);
   gridSizer->Add(textCvsRoot, 2, wxALL, 3);
   sizerHeader->Add(gridSizer, 0, wxALL | wxGROW, 3);
   gridSizer = new wxFlexGridSizer(4, 0, 0);
   gridSizer->AddGrowableCol(1);
   gridSizer->AddGrowableCol(3);
   gridSizer->Add(labelTag, 1, wxALL, 3);
   gridSizer->Add(myTextTag, 2, wxALL, 3);
   gridSizer->Add(labelStatus, 3, wxALL, 3);
   gridSizer->Add(myTextStatus, 4, wxALL, 3);
   gridSizer->Add(labelFileFormat, 1, wxALL, 3);
   gridSizer->Add(textFileFormat, 2, wxALL, 3);
   gridSizer->Add(labelKeyword, 3, wxALL, 3);
   gridSizer->Add(myTextKeywordSubst, 4, wxALL, 3);
   gridSizer->Add(labelStorage, 3, wxALL, 3);
   gridSizer->Add(textStorage, 4, wxALL, 3);
   gridSizer->Add(labelOther, 3, wxALL, 3);
   gridSizer->Add(myTextOtherInfo, 4, wxALL, 3);
   sizerHeader->Add(gridSizer, 0, wxALL | wxGROW, 3);
   sizerMain->Add(sizerHeader, 0, wxGROW);

   // Sizer for top panel
   wxBoxSizer *sizerTopPanel = new wxBoxSizer(wxVERTICAL);
   sizerHeader->Add(myShowTagsCheckBox, 0, wxALL, 5);

   sizerTopPanel->Add(myListCtrl, 3, wxGROW | wxALL, 0);
   topPanel->SetSizer(sizerTopPanel);

   // Sizer for bottom panel
   wxBoxSizer *sizerBottomPanel = new wxBoxSizer(wxVERTICAL);

   sizerBottomPanel->Add(commentLabel, 0, wxALL, 3);
   sizerBottomPanel->Add(myComment, 1, wxGROW | wxALL, 3);
   bottomPanel->SetSizer(sizerBottomPanel);

   // OK button
   wxBoxSizer *sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   wxButton* buttonOK = new wxButton(this, wxID_OK, _("OK"));
   buttonOK->SetDefault();
   wxButton* buttonCancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
   myButtonApply = new wxButton(this, HISTORYDLG_ID_APPLY, _("Apply"));
   myButtonApply->Disable();
   sizerConfirm->Add(buttonOK, 0, wxALL, 5);
   sizerConfirm->Add(buttonCancel, 0, wxALL, 5);
   sizerConfirm->Add(myButtonApply, 0, wxALL, 5);

   // Status bar
   myStatusBar = new wxStatusBar(this, -1);

   // Main box with everything in it
   sizerMain->Add(mySplitter, 1, wxGROW);
   sizerMain->Add(sizerConfirm, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, 10);
   sizerMain->Add(myStatusBar, 0, wxGROW | wxALL, 0);

   myListCtrl->PushEventHandler(new HistoryDialogEvtHandler(this));

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerMain);
   sizerMain->SetSizeHints(this);
   sizerMain->Fit(this);

   RestoreTortoiseDialogSize(this, "History", wxDLG_UNIT(this, wxSize(180, 120)));
   SetTortoiseDialogPos(this, GetRemoteHandle());
   RestoreTortoiseDialogState(this, "History");

   GetRegistryData();

   // Apply column widths
   size_t numColumns = std::min(size_t(NUM_COLUMNS-1), myColumnWidths.size());
   int w = myListCtrl->GetSize().GetWidth();
   for (size_t i = 0; i < numColumns; ++i)
   {
      int colWidth = myColumnWidths[i];
      if (colWidth <= 0 || colWidth > MAX_COLUMN_SIZE)
          colWidth = DEF_COLUMN_SIZE;
      int pixels = wxDLG_UNIT_X(this, colWidth);
      if (w > pixels)
          w -= pixels;
      myListCtrl->SetColumnWidth(i, pixels);
   }

   myListCtrl->SetColumnWidth(numColumns, w);

   myCommentChangedEnabled = true;
}

HistoryDialog::~HistoryDialog()
{
   if (!--myRefCount)
      CleanupStatics();

   if (myCommentChanged && GetReturnCode() == wxID_OK)
      UpdateComment();
   SaveRegistryData();
   myListCtrl->PopEventHandler(true);
   
   StoreTortoiseDialogSize(this, "History");

    if (myTree)
       delete myTree;
}

void HistoryDialog::InitializeStatics()
{
    myBranchColor = new wxColour(HISTORY_BRANCH_RGB);
    mySymbolicColor = new wxColour(HISTORY_SYMBOLIC_RGB);
}

void HistoryDialog::CleanupStatics()
{
    delete myBranchColor;
    delete mySymbolicColor;
}

void HistoryDialog::CommentChanged(wxCommandEvent&)
{
   TDEBUG_ENTER("CommentChanged");
   if (!myCommentChangedEnabled)
      return;
   wxArrayTreeItemIds sel;
   myCommentId = myListCtrl->GetSelection();
   CLogNode* thisnode = ((MyTreeItemData*) myListCtrl->GetItemData(myCommentId))->m_Node;
   if (thisnode->GetType() == kNodeRev)
   {
      CLogNodeRev* revnode = (CLogNodeRev*) (thisnode);
      std::string comment = revnode->Rev().DescLog().c_str();
      FindAndReplace<std::string>(comment, "\r\n", "\n");
      comment = TrimRight(comment);
      std::string oldComment = wxAscii(myComment->GetValue().c_str());
      FindAndReplace<std::string>(oldComment, "\r\n", "\n");
      oldComment = UnhyperlinkComment(TrimRight(oldComment));
      if (comment != oldComment)
      {
         TDEBUG_TRACE("Old: " << oldComment);
         TDEBUG_TRACE("New: " << comment);
         myCommentChanged = true;
         myButtonApply->Enable(true);
         return;
      }
   }
   myCommentChanged = false;
   myButtonApply->Enable(false);
}

void HistoryDialog::Apply(wxCommandEvent&)
{
   UpdateComment();
   myCommentChanged = false;
}



void HistoryDialog::SymbolicCheckChanged(wxCommandEvent&)
{
   Refresh();
}


void HistoryDialog::SortItems()
{
   myListCtrl->SetSortParams(!myTreeUpsideDown, mySortColumn);
   long c = 0; // Cookie
   myListCtrl->SortChildren(myListCtrl->GetFirstChild(myListCtrl->GetRootItem(), c));
}

void HistoryDialog::GetRegistryData()
{
   myColumnWidths.clear();
   TortoiseRegistry::ReadVector("Dialogs\\History\\Column Width", myColumnWidths);
   bool showTags = TortoiseRegistry::ReadBoolean("Dialogs\\History\\Show Tags", true);
   myTreeUpsideDown = TortoiseRegistry::ReadBoolean("Dialogs\\History\\Tree Upside Down", false);
   int pos = 80;
   TortoiseRegistry::ReadInteger("Dialogs\\History\\Sash Position", pos);
   mySplitter->SetRightSashPosition(wxDLG_UNIT_Y(this, pos));
   myShowTagsCheckBox->SetValue(showTags);
   myTreeUpsideDown = TortoiseRegistry::ReadBoolean("Dialogs\\History\\Tree Upside Down", false);

}

void HistoryDialog::SaveRegistryData()
{
   myColumnWidths.clear();
   for (int i = 0; i < NUM_COLUMNS-1; ++i)
   {
      int w = myListCtrl->GetColumnWidth(i);
      w = this->ConvertPixelsToDialog(wxSize(w, 0)).GetWidth();
      myColumnWidths.push_back(w);
   }
   bool showTags = myShowTagsCheckBox->GetValue();
   TortoiseRegistry::WriteVector("Dialogs\\History\\Column Width", myColumnWidths);   
   TortoiseRegistry::WriteBoolean("Dialogs\\History\\Show Tags", showTags);
   TortoiseRegistry::WriteBoolean("Dialogs\\History\\Tree Upside Down", myTreeUpsideDown);
   int h = mySplitter->GetRightSashPosition();
   h = this->ConvertPixelsToDialog(wxSize(0, h)).GetHeight();
   TortoiseRegistry::WriteInteger("Dialogs\\History\\Sash Position", h);
}

void HistoryDialog::UpdateComment()
{
   TDEBUG_ENTER("UpdateComment");
   
   bool updateComment = false;

   CLogNode* thisnode = ((MyTreeItemData*) myListCtrl->GetItemData(myCommentId))->m_Node;
   if (thisnode->GetType() == kNodeRev)
   {
      CLogNodeRev* revnode = (CLogNodeRev*) (thisnode);
      std::string oldComment = revnode->Rev().DescLog().c_str();
      FindAndReplace<std::string>(oldComment, "\r\n", "\n");
      oldComment = TrimRight(oldComment);
      std::string newComment = UnhyperlinkComment(wxAscii(myComment->GetValue().c_str()));
      FindAndReplace<std::string>(newComment, "\r\n", "\n");
   
      if (newComment != oldComment)
      {
         // Update comment in repository
         CVSAction glue(this);
         glue.SetCloseIfOK(true);
         glue.SetProgressCaption(_("Changing comment"));
         MakeArgs args;
         args.add_option("admin");
         args.add_option(std::string("-m") + std::string(revnode->Rev().RevNum().str()) +
                         std::string(":") + newComment);
         args.add_arg(GetLastPart(myFilename));
         updateComment = glue.Command(GetDirectoryPart(myFilename), args);
      }
      else
      {
         // No changes
         updateComment = true;
      }

      if (updateComment)
      {
          // Update list control
          std::string bugtracker = GetStringPreference("Bug tracker URL", GetDirectoryPart(myFilename));
          myComment->SetValue(wxText(HyperlinkComment(newComment, bugtracker)));
          // Update tree
          revnode->Rev().SetDescLog(newComment.c_str());
          // Do not show newlines and tabs in list
          FindAndReplace<std::string>(newComment, "\n", " ");
          FindAndReplace<std::string>(newComment, "\t", " ");
          myListCtrl->SetItemText(myCommentId, COL_COMMENT, wxText(newComment));
          myButtonApply->Disable();
      }
   }
}



void HistoryDialog::RefreshFileStatus()
{
   // Information about current revision
   mySandboxRevision = CVSStatus::GetRevisionNumber(myFilename);
   mySandboxStickyTag = CVSStatus::GetStickyTag(myFilename);

   // Set sticky tag
   wxString stickyTag(_("(None)"));
   if (CVSStatus::HasStickyTag(myFilename))
      stickyTag = wxText(CVSStatus::GetStickyTag(myFilename));
   myTextTag->SetLabel(stickyTag);

   CVSStatus::FileStatus status = CVSStatus::GetFileStatus(myFilename);
   myTextStatus->SetLabel(CVSStatus::FileStatusString(status).c_str());
}

void HistoryDialog::OnTextUrl(wxCommandEvent& cmdevent)
{
   if (!myCommentChangedEnabled)
      return;
   // TODO: Downcast should not be necessary
   wxTextUrlEvent& event = (wxTextUrlEvent&) cmdevent;
   if (!event.GetMouseEvent().ButtonDown(wxMOUSE_BTN_LEFT))
      return;
   std::string url = wxAscii(myComment->GetValue().c_str());
   url = url.substr(event.GetURLStart(), event.GetURLEnd() - event.GetURLStart());
   LaunchURL(url);
}



BEGIN_EVENT_TABLE(HistoryDialogEvtHandler, wxEvtHandler)
  EVT_TREE_SEL_CHANGED(HISTORYDLG_ID_TREECTRL,   HistoryDialogEvtHandler::OnListTreeSelChanged)
  EVT_TREE_ITEM_RIGHT_CLICK(HISTORYDLG_ID_TREECTRL,    HistoryDialogEvtHandler::OnListTreeRightClick) 
  EVT_CONTEXT_MENU(                              HistoryDialogEvtHandler::OnListTreeContextMenu) 
  EVT_LIST_COL_CLICK(HISTORYDLG_ID_TREECTRL,     HistoryDialogEvtHandler::OnListTreeColClick)
END_EVENT_TABLE()






void HistoryDialog::OnMenuDiffWorkfile(wxCommandEvent&)
{
   LaunchTortoiseAct(false, CvsDiffVerb, GetFilename(), 
                     "-r " + Quote(myRevision1), GetHwndOf(this));
}


void HistoryDialog::OnMenuDiffSelected(wxCommandEvent&)
{
   std::string rev = "-r ";
   rev += Quote(myRevision1);
   if (!myRevision2.empty())
   {
      // Pass revisions in the right order
      if (CVSRevisionDataSortCriteria::StaticCompareRevisions(myRevision1, 
                                                              myRevision2, true) < 0)
      {
         rev += " -r " + Quote(myRevision2);
      }
      else
      {
         rev = "-r " + Quote(myRevision2) + " " + rev;
      }
   }
   LaunchTortoiseAct(false, CvsDiffVerb, myFilename, rev, GetHwndOf(this)); 
}



void HistoryDialog::OnMenuMerge(wxCommandEvent&)
{
   std::string rev = "-r ";
   rev += Quote(myRevision1);
   if (!myRevision2.empty())
   {
      // Pass revisions in the right order
      if (CVSRevisionDataSortCriteria::StaticCompareRevisions(myRevision1, 
                                                              myRevision2, true) < 0)
      {
         rev += " -r " + Quote(myRevision2);
      }
      else
      {
         rev = "-r " + Quote(myRevision2) + " " + rev;
      }
   }
   LaunchTortoiseAct(false, CvsMergeVerb, myFilename, rev, GetHwndOf(this)); 
}


void HistoryDialog::OnMenuUndo(wxCommandEvent&)
{
   std::string rev = "-r ";
   rev += Quote(myRevision1);
   if (!myRevision2.empty())
   {
      // Pass revisions in the right order
      if (CVSRevisionDataSortCriteria::StaticCompareRevisions(myRevision1, 
                                                              myRevision2, true) < 0)
      {
         rev = "-r " + Quote(myRevision2) + " " + rev;
      }
      else
      {
         rev += " -r " + Quote(myRevision2);
      }
   }
   LaunchTortoiseAct(false, CvsMergeVerb, myFilename, rev, GetHwndOf(this)); 
}


void HistoryDialog::OnMenuView(wxCommandEvent&)
{
   LaunchTortoiseAct(false, CvsViewVerb, GetFilename(), 
                     "-r " + Quote(myRevision1), GetHwndOf(this)); 
}


void HistoryDialog::OnMenuGet(wxCommandEvent&)
{
   LaunchTortoiseAct(true, CvsGetVerb, myFilename, 
                     "-r " + Quote(myRevision1), GetHwndOf(this)); 
   CVSStatus::InvalidateCache();
   Refresh();
}



void HistoryDialog::OnMenuGetClean(wxCommandEvent&)
{
   int id = MessageBox(GetHwndOf(this), 
                       _("Retrieving a clean copy deletes your changes."), _("Warning"),
                       MB_OKCANCEL | MB_ICONWARNING);
   if (id != IDOK)
      return;

   LaunchTortoiseAct(true, CvsGetVerb, myFilename, 
                     "-C -r " + Quote(myRevision1), GetHwndOf(this)); 
   CVSStatus::InvalidateCache();
   Refresh();
}



void HistoryDialog::OnMenuSaveAs(wxCommandEvent&)
{
   LaunchTortoiseAct(false, CvsSaveAsVerb, myFilename, 
                     "-r " + Quote(myRevision1), GetHwndOf(this)); 
}

void HistoryDialog::OnMenuCopyToClipboard(wxCommandEvent&)
{
   myCommentId = myListCtrl->GetSelection();
   CLogNode* thisnode = ((MyTreeItemData*) myListCtrl->GetItemData(myCommentId))->m_Node;
   if (thisnode->GetType() == kNodeRev)
   {
      CLogNodeRev* revnode = static_cast<CLogNodeRev*>(thisnode);
      std::string comment = revnode->Rev().DescLog().c_str();
      if (wxTheClipboard->Open())
      {
         // These data objects are held by the clipboard, so do not delete them in the app.
         wxTheClipboard->SetData(new wxTextDataObject(wxText(comment)));
         wxTheClipboard->Close();
      }
   }
}



void HistoryDialog::OnMenuAnnotate(wxCommandEvent&)
{
   LaunchTortoiseAct(false, CvsAnnotateVerb, myFilename, 
                     "-r " + Quote(myRevision1), GetHwndOf(this)); 
}



void HistoryDialog::OnMakeTag(wxCommandEvent&)
{
   std::string tagname = TortoiseRegistry::ReadString("FailedTagName");
   bool checkunmodified;
   int action;
   if (DoTagDialog(this, tagname, checkunmodified, action, 
                   std::vector<std::string>(1, myDir)))
   {
      CVSAction glue(this);
      glue.SetProgressFinishedCaption(Printf(_("Finished tagging in %s"), wxText(myDir).c_str()));

      MakeArgs args;
      args.add_option("tag");

      if (checkunmodified)
         args.add_option("-c");

      // Add the revision to tag
      args.add_option("-r" + myRevision1);

      switch (action)
      {
      case BranchTagDlg::TagActionCreate:
         break;
      case BranchTagDlg::TagActionMove:
         args.add_option("-F");
         break;
      case BranchTagDlg::TagActionDelete:
         args.add_option("-d");
         break;
      default:
         TortoiseFatalError(_("Internal error: Invalid action"));
         break;
      }
      args.add_option(tagname);

      args.add_arg(myFile);

      glue.SetProgressCaption(Printf(_("Tagging in %s"), wxText(myDir).c_str()));

      if (!glue.Command(myDir, args))
         TortoiseRegistry::WriteString("FailedTagName", tagname);
      else
         TortoiseRegistry::WriteString("FailedTagName", "");

      Update();
   }
}


void HistoryDialog::OnMakeBranch(wxCommandEvent&)
{
   std::string branchname;
   bool checkunmodified;
   int action;
   if (DoBranchDialog(this, branchname, checkunmodified, action,
                      std::vector<std::string>(1, myDir)))
   {
      CVSAction glue(this);
      glue.SetProgressFinishedCaption(Printf(_("Finished branching in %s"), wxText(myDir).c_str()));

      MakeArgs args;
      args.add_option("tag");
      if (checkunmodified)
         args.add_option("-c");

      // Add the revision to branch
      args.add_option("-r" + myRevision1);

      CVSStatus::CVSVersionInfo serverversion = CVSStatus::GetServerCVSVersion(myDir, &glue);
      switch (action)
      {
      case BranchTagDlg::TagActionCreate:
         args.add_option("-b");
         break;
      case BranchTagDlg::TagActionMove:
         args.add_option("-b");
         args.add_option("-F");
         if (serverversion >= CVSStatus::CVSVersionInfo("1.11.1p2"))
            args.add_option("-B");
         break;
      case BranchTagDlg::TagActionDelete:
         args.add_option("-d");
         if (serverversion >= CVSStatus::CVSVersionInfo("1.11.1p2"))
            args.add_option("-B");
         break;
      default:
         TortoiseFatalError(_("Internal error: Invalid action"));
         break;
      }
      args.add_option(branchname);

      args.add_arg(myFile);

      glue.SetProgressCaption(Printf(_("Branching in %s"), wxText(myDir).c_str()));
      glue.Command(myDir, args);

      Update();
   }
}



void HistoryDialogEvtHandler::OnListTreeSelChanged(wxTreeEvent& event)
{
   TDEBUG_ENTER("HistoryDialogEvtHandler::OnListTreeSelChanged");

   if (myIgnoreChange)
   {
      myIgnoreChange = false;
      return;
   }
   if (myParent->myCommentChanged)
   {
      std::string revno;
      CLogNode* thisnode = ((HistoryDialog::MyTreeItemData*)
                            myParent->myListCtrl->GetItemData(myParent->myCommentId))->m_Node;
      if (thisnode->GetType() == kNodeRev)
         revno = ((CLogNodeRev*) thisnode)->Rev().RevNum().str();
      wxMessageDialog dlg(myParent,
                          Printf(_("You have changed the comment of revision %s.\nDo you want to save it?"),
                                 wxTextCStr(revno)),
                          _("TortoiseCVS Question"),
                          wxYES_NO | wxCANCEL | wxYES_DEFAULT);
      switch (dlg.ShowModal())
      {
      case wxID_YES:
         myParent->UpdateComment();
         break;
      case wxID_CANCEL:
         myIgnoreChange = true; // Prevent infinite loop
         myParent->myListCtrl->SelectItem(event.GetOldItem());
         return;
      case wxID_NO:
         break;
      }
   }
   myParent->myCommentChanged = false;
   wxArrayTreeItemIds sel;
   int selCount = static_cast<int>(myParent->myListCtrl->GetSelections(sel));
   if (selCount == 1)
   {
      CLogNode* thisnode = ((HistoryDialog::MyTreeItemData*) myParent->myListCtrl->GetItemData(sel[0]))->m_Node;
      if (thisnode->GetType() == kNodeRev)
      {
         CLogNodeRev* revnode = (CLogNodeRev*) (thisnode);

         // Prevent that we get called for our own changes
         myParent->myCommentChangedEnabled = false;
         myParent->myComment->Enable();
         std::string bugtracker = GetStringPreference("Bug tracker URL", GetDirectoryPart(myParent->myFilename));
         myParent->myComment->SetValue(wxText(HyperlinkComment(revnode->Rev().DescLog().c_str(), bugtracker)));
         myParent->myButtonApply->Disable();
         // Re-enable callbacks
         myParent->myCommentChangedEnabled = true;

         // Keyword substitution may be versioned
         const CLogStr& kws = revnode->Rev().KeywordSubst();
         if (!kws.empty())
            myParent->myTextKeywordSubst->SetLabel(wxText(kws.c_str()));
         wxString otherInfo;
         const CLogStr& filename = revnode->Rev().Filename();
         if (!filename.empty())
         {
            otherInfo = _("Filename");
            otherInfo += _(":");
            otherInfo += wxT(" ");
            otherInfo += wxText(filename.c_str());
         }
         if (!otherInfo.empty())
            myParent->myTextOtherInfo->SetLabel(otherInfo);
         return;
      }
   }

   // Revert keyword substitution to default
   myParent->myTextKeywordSubst->SetLabel(myParent->myDefaultKWS);
   // Clear other info field
   myParent->myTextOtherInfo->SetLabel(wxT(""));

   myParent->myComment->Clear();
   myParent->myComment->Disable();
   myParent->myButtonApply->Disable();
   myParent->myCommentChanged = false;
}


void HistoryDialogEvtHandler::OnListTreeRightClick(wxTreeEvent& event)
{
   TDEBUG_ENTER("HistoryDialogEvtHandler::OnListTreeRightClick");
   if (!myParent->myListCtrl->IsSelected(event.GetItem())
      && GetAsyncKeyState(VK_APPS) >= 0)
   {
      myParent->myListCtrl->SelectItem(event.GetItem());
   }
}


void HistoryDialogEvtHandler::OnListTreeContextMenu(wxContextMenuEvent& event)
{
   TDEBUG_ENTER("HistoryDialogEvtHandler::OnListTreeContextMenu");

   wxArrayTreeItemIds sel;
   wxPoint pos = event.GetPosition();
   int selectionCount = static_cast<int>(myParent->myListCtrl->GetSelections(sel));

   // We can only deal with one or two selected revisions
   if (selectionCount == 0 || selectionCount > 2)
      return;

   std::auto_ptr<wxMenu> popupMenu(new wxMenu);
   myParent->myRevision1.erase();
   myParent->myRevision2.erase();

   if (pos == wxPoint(-1, -1))
   {
      pos = wxPoint(0, 0);
      pos = myParent->myListCtrl->ClientToScreen(pos);
      wxTreeItemId id = myParent->myListCtrl->GetSelection();
      if (id.IsOk())
      {
         wxRect rect;
         if (myParent->myListCtrl->GetBoundingRect(id, rect))
         {
            int d = rect.GetHeight() / 2;
            pos.x = rect.GetLeft() + d;
            pos.y = rect.GetTop() + d;
            pos = ((wxWindow*) myParent->myListCtrl->GetMainWindow())->ClientToScreen(pos);
         }
      }
   }
   pos = myParent->ScreenToClient(pos);

   // Option for one selected node
   if (selectionCount == 1)
   {
      CLogNode* thisnode = ((HistoryDialog::MyTreeItemData*) myParent->myListCtrl->GetItemData(sel[0]))->m_Node;

      switch (thisnode->GetType())
      {
         case kNodeRev:
         case kNodeTag:
         {
            // Store file and directory name
            std::string filename = myParent->GetFilename();
            myParent->myFile = reinterpret_cast<CLogNodeHeader*>(myParent->GetTree())->RcsFile().WorkingFile().c_str();
            myParent->myDir = filename.substr(0, filename.length()- myParent->myFile.length()- 1);

            if (thisnode->GetType() == kNodeRev)
            {
               CLogNodeRev* revnode = (CLogNodeRev*) (thisnode);
               myParent->myRevision1 = reinterpret_cast<CLogNodeRev*>(thisnode)->Rev().RevNum().str();
            }
            else
            {
               CLogNodeTag* tagnode = (CLogNodeTag*) (thisnode);
               myParent->myRevision1 = tagnode->Tag().c_str();
            }

            // No revisions selected, OR this revision is the selected one
            popupMenu->Append(HISTORY_CONTEXT_ID_DIFF_WORKFILE, _("&Diff (working file)"), wxT(""));
            popupMenu->Append(HISTORY_CONTEXT_ID_VIEW, _("&View this revision"), wxT(""));
            if (!myParent->myOfferView)
               popupMenu->Enable(HISTORY_CONTEXT_ID_VIEW, false);
            popupMenu->Append(HISTORY_CONTEXT_ID_ANNOTATE, _("&Annotate this revision"), wxT(""));
            popupMenu->Append(HISTORY_CONTEXT_ID_GET, _("&Get this revision (sticky)"), wxT(""));
            popupMenu->Append(HISTORY_CONTEXT_ID_GET_CLEAN, _("Get &clean copy of this revision (sticky)"), wxT(""));
            popupMenu->Append(HISTORY_CONTEXT_ID_MAKE_TAG, _("&Tag..."));
            popupMenu->Append(HISTORY_CONTEXT_ID_MAKE_BRANCH, _("&Branch..."));
            popupMenu->AppendSeparator();
            popupMenu->Append(HISTORY_CONTEXT_ID_SAVE_AS, _("&Save this revision as..."), wxT(""));
            if (thisnode->GetType() == kNodeRev)
            {
               popupMenu->AppendSeparator();
               popupMenu->Append(HISTORY_CONTEXT_ID_COPY_TO_CLIPBRD, _("C&opy comment to clipboard"), wxT(""));
            }
            myParent->PopupMenu(popupMenu.get(), pos);         
         }
         break;

         case kNodeBranch:
         {
            // Store file and directory name
            std::string filename = myParent->GetFilename();
            myParent->myFile = reinterpret_cast<CLogNodeHeader*>(myParent->GetTree())->RcsFile().WorkingFile().c_str();
            myParent->myDir = filename.substr(0, filename.length()- myParent->myFile.length()- 1);

            CLogNodeBranch* branchNode = (CLogNodeBranch*)(thisnode);
            std::string branch(branchNode->Branch().c_str());
            myParent->myRevision1 = branch.c_str();
            popupMenu->Append(HISTORY_CONTEXT_ID_GET, _("&Get this branch (sticky)"), wxT(""));
            popupMenu->Append(HISTORY_CONTEXT_ID_GET_CLEAN, _("Get &clean copy of this branch (sticky)"), wxT(""));

            myParent->PopupMenu(popupMenu.get(), pos);         
         }
         break;

         case kNodeHeader:
         {
            // Store file and directory name
            std::string filename = myParent->GetFilename();
            myParent->myFile = reinterpret_cast<CLogNodeHeader*>(myParent->GetTree())->RcsFile().WorkingFile().c_str();
            myParent->myDir = filename.substr(0, filename.length()- myParent->myFile.length()- 1);
            myParent->myRevision1 = "HEAD";
            popupMenu->Append(HISTORY_CONTEXT_ID_GET, _("&Return to HEAD branch"), wxT(""));
            popupMenu->Append(HISTORY_CONTEXT_ID_GET_CLEAN, _("R&eturn to clean copy of HEAD branch"), wxT(""));

            myParent->PopupMenu(popupMenu.get(), pos);         
         }
         break;

         default:
            break;
      }
   }

   // Options for two selected nodes
   if (selectionCount == 2) 
   {
      CLogNode *node1 = ((HistoryDialog::MyTreeItemData*) myParent->myListCtrl->GetItemData(sel[0]))->m_Node;
      CLogNode *node2 = ((HistoryDialog::MyTreeItemData*) myParent->myListCtrl->GetItemData(sel[1]))->m_Node;

      // Both nodes must be revisions or tags
      if (!((node1->GetType() == kNodeRev || node1->GetType() == kNodeTag)
            && (node2->GetType() == kNodeRev || node2->GetType() == kNodeTag)))
      {
         return;
      }

      if (node1->GetType() == kNodeRev)
      {
         myParent->myRevision1 = reinterpret_cast<CLogNodeRev*>(node1)->Rev().RevNum().str();
      }
      else
      {
         CLogNodeTag* tagnode = (CLogNodeTag*) (node1);
         myParent->myRevision1 = tagnode->Tag().c_str();
      }

      if (node2->GetType() == kNodeRev)
      {
         myParent->myRevision2 = reinterpret_cast<CLogNodeRev*>(node2)->Rev().RevNum().str();
      }
      else
      {
         CLogNodeTag* tagnode = (CLogNodeTag*) (node2);
         myParent->myRevision2 = tagnode->Tag().c_str();
      }

      popupMenu->Append(HISTORY_CONTEXT_ID_DIFF_SELECTED,
                        _("&Diff (selected revisions)"), wxT(""));
      popupMenu->Append(HISTORY_CONTEXT_ID_MERGE,
                        _("&Merge changes (selected revisions)"), wxT(""));
      popupMenu->Append(HISTORY_CONTEXT_ID_UNDO,
                        _("&Undo changes (selected revisions)"), wxT(""));

      myParent->PopupMenu(popupMenu.get(), pos);         
   }
}


void HistoryDialogEvtHandler::OnListTreeColClick(wxListEvent& event)
{
   TDEBUG_ENTER("HistoryDialogEvtHandler::OnListTreeColClick");
   TDEBUG_TRACE("Column: " << event.GetColumn());
   if (myParent->mySortColumn != event.GetColumn())
   {
      // Change sort column
      myParent->mySortColumn = event.GetColumn();
   }
   else
   {
      // Change sort direction
      myParent->myTreeUpsideDown = !myParent->myTreeUpsideDown;
   }
   myParent->SortItems();
}
