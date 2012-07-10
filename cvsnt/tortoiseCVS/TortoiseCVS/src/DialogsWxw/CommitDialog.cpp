// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Ben Campbell
// <ben.campbell@creaturelabs.com> - September 2000

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
#include "CommitDialog.h"
#include "WxwHelpers.h"
#include "WxwMain.h"
#include <wx/clipbrd.h>
#include <wx/filename.h>
#include "FilenameText.h"
#include "ExtTextCtrl.h"
#include "ExplorerMenu.h"
#include "ExtSplitterWindow.h"
#include "ExtListCtrl.h"
#include <CVSGlue/CVSAction.h>
#include <Utils/PathUtils.h>
#include <Utils/TortoiseDebug.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/TortoiseRegistry.h>
#include <Utils/ShellUtils.h>
#include <Utils/StringUtils.h>
#include <Utils/Translate.h>
#include <Utils/Preference.h>
#include <Utils/LaunchTortoiseAct.h>
#include <TortoiseAct/TortoiseActVerbs.h>
#include <Utils/FileTree.h>
#include "YesNoDialog.h"
#include <TortoiseAct/DirectoryGroup.h>

#ifdef MARCH_HARE_BUILD
#include "InfoPanel.h"
#endif

class MyUserData : public FileTree::UserData 
{
public:
};


class MyKeyHandler : public ExtTextCtrl::KeyHandler
{
public:
   MyKeyHandler(CommitDialog* parent);
   virtual ~MyKeyHandler();
   bool OnArrowUpDown(bool up);
   bool OnSpace();
private:
   // Currently selected comment. -1 means the user's new entry.
   int           myIndex;
   wxString      myEditedComment;
   CommitDialog* myParent;
};


enum 
{
   CD_DIFF = 10001,
   CD_HISTORY,
   CD_GRAPH,
   CD_CHECK,
   CD_UNCHECK,
   CD_COPYNAMES,
   CD_COPYDIRS,
   CD_COMMIT,
   CD_CLEANCOPY,
   CD_RESTORE,
   CD_WEB_LOG,

   COMMITDLG_ID_HISTORY,
   COMMITDLG_ID_FILES,
   COMMITDLG_ID_WRAP,
   COMMITDLG_ID_USEBUG,
   COMMITDLG_ID_MARKBUG,
   COMMITDLG_ID_BUGNUMBER
};


BEGIN_EVENT_TABLE(CommitDialog, ExtDialog)
   EVT_MENU(CD_DIFF,            CommitDialog::OnMenuDiff)
   EVT_MENU(CD_HISTORY,         CommitDialog::OnMenuHistory)
   EVT_MENU(CD_GRAPH,           CommitDialog::OnMenuGraph)
   EVT_MENU(CD_CHECK,           CommitDialog::OnMenuCheck)
   EVT_MENU(CD_UNCHECK,         CommitDialog::OnMenuUncheck)
   EVT_MENU(CD_COPYNAMES,       CommitDialog::OnMenuCopyNames)
   EVT_MENU(CD_COPYDIRS,        CommitDialog::OnMenuCopyDirs)
   EVT_MENU(CD_COMMIT,          CommitDialog::OnMenuCommit)
   EVT_MENU(CD_CLEANCOPY,       CommitDialog::OnMenuCleanCopy)
   EVT_MENU(CD_RESTORE,         CommitDialog::OnMenuRestore)
   EVT_MENU(CD_WEB_LOG,         CommitDialog::OnMenuWebLog)
   EVT_COMMAND(COMMITDLG_ID_HISTORY, wxEVT_COMMAND_COMBOBOX_SELECTED, CommitDialog::CommentHistoryClick)
   EVT_COMMAND(COMMITDLG_ID_WRAP, wxEVT_COMMAND_CHECKBOX_CLICKED, CommitDialog::WrapTextClick)
   EVT_COMMAND(COMMITDLG_ID_USEBUG, wxEVT_COMMAND_CHECKBOX_CLICKED, CommitDialog::UseBugClick)
   EVT_COMMAND(COMMITDLG_ID_MARKBUG, wxEVT_COMMAND_CHECKBOX_CLICKED, CommitDialog::MarkBugClick)
   EVT_COMMAND(COMMITDLG_ID_BUGNUMBER, wxEVT_COMMAND_TEXT_UPDATED, CommitDialog::BugNumberTextUpdated)
   EVT_COMMAND(wxID_OK, wxEVT_COMMAND_BUTTON_CLICKED, CommitDialog::OnOk)
END_EVENT_TABLE()

void CommitDialog::ListCtrlEventHandler::OnContextMenu(wxContextMenuEvent& event)
{
   TDEBUG_ENTER("CommitDialog::OnContextMenu");

   // Count selected items of type REMOVED, ADDED, CHANGED
   int countAdded = 0;
   int countChanged = 0;
   int countRemoved = 0;
   std::string sDir;
   bool bNeedsSeparator = false;
   int i;
   std::vector<std::string> selectedFiles;
   ExplorerMenu* explorerMenu = 0;
   for (i = 0; i < myParent->myFiles->GetItemCount(); i++)
   {
      if (myParent->myFiles->IsSelected(i))
      {
         CommitDialog::ItemData* data = reinterpret_cast<CommitDialog::ItemData*>(myParent->myFiles->GetItemData(i));
         if (data->status == CVSStatus::STATUS_ADDED)
            ++countAdded;
         else if (data->status == CVSStatus::STATUS_CHANGED)
            ++countChanged;
         if (data->status == CVSStatus::STATUS_REMOVED)
            ++countRemoved;
         selectedFiles.push_back(data->fullName);
      }
   }

   // if no items selected, return
   if ((countAdded + countChanged + countRemoved) == 0)
      return;

   // Add common contextmenu entries
   std::auto_ptr<wxMenu> popupMenu(new wxMenu);
   popupMenu->Append(CD_CHECK, _("Check"), wxT(""));
   popupMenu->Append(CD_UNCHECK, _("Uncheck"), wxT(""));
   popupMenu->Append(CD_COPYNAMES, _("&Copy filename(s)"), wxT(""));
   popupMenu->Append(CD_COPYDIRS, _("Copy &filename(s) with folders"), wxT(""));

   popupMenu->AppendSeparator();

   popupMenu->Append(CD_COMMIT, _("C&ommit selected files..."), wxT(""));

   popupMenu->AppendSeparator();

   popupMenu->Append(CD_CLEANCOPY, _("Get c&lean copy of selected files"), wxT(""));

   bNeedsSeparator = true;

   if ((countRemoved == 0) && (countAdded == 0) && (countChanged == 1))
   {
      if (bNeedsSeparator)
      {
         popupMenu->AppendSeparator();
         bNeedsSeparator = false;
      }
      popupMenu->Append(CD_DIFF, _("&Diff..."), wxT(""));
      popupMenu->Append(CD_HISTORY, _("H&istory..."), wxT(""));
      popupMenu->Append(CD_GRAPH, _("Revision &Graph..."), wxT(""));
      bNeedsSeparator = true;
   }

   if ((countAdded == 0) && ((countChanged == 1) || (countRemoved >= 1)))
   {
      if (bNeedsSeparator)
      {
         popupMenu->AppendSeparator();
         bNeedsSeparator = false;
      }
      if ((countChanged == 1) || (countRemoved == 1))
      {
         popupMenu->Append(CD_WEB_LOG, _("CVS &Web Log..."), wxT(""));
      }

      if ((countChanged == 0) || (countRemoved >= 1))
      {
         popupMenu->Append(CD_RESTORE, _("R&estore"), wxT(""));
      }
      bNeedsSeparator = true;
   }


   // Add explorer menu
   if ((countRemoved == 0) && (StripCommonPath(selectedFiles, sDir)))
   {
      if (bNeedsSeparator)
      {
         popupMenu->AppendSeparator();
         bNeedsSeparator = false;
      }
      explorerMenu = new ExplorerMenu();
      explorerMenu->FillMenu((HWND) myParent->GetHandle(), sDir, selectedFiles);
      popupMenu->Append(0, _("Explorer"), explorerMenu);
   }

   wxPoint pos = event.GetPosition();
   // Calc selection
   if (pos == wxPoint(-1, -1))
   {
      pos = myParent->myFiles->GetContextMenuPos(myParent->myFiles->GetNextItem(-1,
                                                                                wxLIST_NEXT_ALL,
                                                                                wxLIST_STATE_SELECTED));
      pos = myParent->myFiles->ClientToScreen(pos);
   }

   myParent->PopupMenu(popupMenu.get(), myParent->ScreenToClient(pos));
}

void CommitDialog::ListCtrlEventHandler::ListColumnClick(wxListEvent& e)
{
   if (e.GetColumn() == myParent->mySortData.column)
   {
      myParent->mySortData.asc = !myParent->mySortData.asc;
   }
   else
   {
      myParent->mySortData.asc = true;
   }
   myParent->mySortData.column = e.GetColumn();
   myParent->myFiles->SortItems(CompareFunc, (long) &myParent->mySortData);
   myParent->myFiles->SetSortIndicator(myParent->mySortData.column, myParent->mySortData.asc);
}

void CommitDialog::ListCtrlEventHandler::OnDblClick(wxListEvent& event)
{
   TDEBUG_ENTER("CommitDialog::OnDblClick");
   int item = event.GetIndex();
   TDEBUG_TRACE("Item " << item);
   CommitDialog::ItemData* data = reinterpret_cast<CommitDialog::ItemData*>(event.GetData());

   // Modified file: Run diff
   if (data->status == CVSStatus::STATUS_CHANGED)
   {
      std::string file = data->fullName;
      LaunchTortoiseAct(false, CvsDiffVerb, file, "", GetRemoteHandle());
   }
   // otherwise just switch checked state
   else
   {
      myParent->myFiles->SetChecked(item, !myParent->myFiles->IsChecked(item));
   }
}


BEGIN_EVENT_TABLE(CommitDialog::ListCtrlEventHandler, wxEvtHandler)
   EVT_CONTEXT_MENU(ListCtrlEventHandler::OnContextMenu)
   EVT_LIST_COL_CLICK(-1, ListCtrlEventHandler::ListColumnClick)
   EVT_LIST_ITEM_ACTIVATED(-1, ListCtrlEventHandler::OnDblClick)
END_EVENT_TABLE()

//static
bool DoCommitDialog(wxWindow* parent,
                    const DirectoryGroups& dirGroups,
                    const FilesWithBugnumbers* modified,
                    const FilesWithBugnumbers* modifiedStatic,
                    const FilesWithBugnumbers* added,
                    const FilesWithBugnumbers* removed,
                    const FilesWithBugnumbers* renamed,
                    std::string& comment,
                    std::string& bugnumber,
                    bool& usebug,
                    bool& markbug,
                    CommitDialog::SelectedFiles& userSelection,
                    CommitDialog::SelectedFiles& userSelectionStatic)
{
   CommitDialog* dlg = new CommitDialog(parent, dirGroups, modified, modifiedStatic, added, removed, renamed, comment, bugnumber);
   bool ret = (dlg->ShowModal() == wxID_OK);

   if (ret)
   {
      comment = TrimRight(wxAscii(dlg->myComment->GetValue()));
      bugnumber = TrimRight(wxAscii(dlg->myBugNumber->GetValue()));
      usebug = dlg->myUseBugCheckBox->GetValue();
      markbug = dlg->myMarkBugCheckBox->GetValue();
      if (!markbug && !usebug)
         dlg->myBugNumber->SetValue(wxT(""));
      TortoiseRegistry::WriteBoolean("Dialogs\\Commit\\Word Wrap", dlg->myWrapCheckBox->GetValue());
      TortoiseRegistry::WriteBoolean("Dialogs\\Commit\\Use Bug", dlg->myUseBugCheckBox->GetValue());
      TortoiseRegistry::WriteString("Dialogs\\Commit\\Bug Number", wxAscii(dlg->myBugNumber->GetValue().c_str()));

      // Fill in user selection of ticked items
      for (int i = 0; i < dlg->myFiles->GetItemCount(); ++i)
      {
         if (dlg->myFiles->IsChecked(i))
         {
            CommitDialog::ItemData* data = reinterpret_cast<CommitDialog::ItemData*>(dlg->myFiles->GetItemData(i));
            if (data->status == CVSStatus::STATUS_STATIC)
                userSelectionStatic.push_back(make_pair(data->fullName, data->status));
            else
                userSelection.push_back(make_pair(data->fullName, data->status));
         }
      }
   }
   
   DestroyDialog(dlg);
   return ret;
}


CommitDialog::CommitDialog(wxWindow* parent, 
                           const DirectoryGroups& dirGroups,
                           const FilesWithBugnumbers* modified, 
                           const FilesWithBugnumbers* modifiedStatic, 
                           const FilesWithBugnumbers* added,
                           const FilesWithBugnumbers* removed,
                           const FilesWithBugnumbers* renamed,
                           const std::string& defaultComment,
                           const std::string& defaultBugNumber)
   : ExtDialog(parent, -1, _("TortoiseCVS - Commit"),
               wxDefaultPosition, wxDefaultSize,
               wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxRESIZE_BORDER | 
               wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN),
     mySplitter(0),
     myDirGroups(dirGroups),
     myOK(0),
     myReady(false)
{
    TDEBUG_ENTER("CommitDialog::CommitDialog");
    SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

    PrepareItemData(modified, modifiedStatic, added, removed, renamed);
   
    wxPanel* headerPanel = new wxPanel(this, -1);

    FilenameText* label1 = new FilenameText(headerPanel, -1, _("Folder: %s"), wxText(myStub));
    label1->SetWindowStyle(label1->GetWindowStyle() | wxST_NO_AUTORESIZE);

    // Comment history
    wxString s(_("Comment History"));
    s += _(":");
    wxStaticText* labelCH = new wxStaticText(headerPanel, -1, s);
    myCommentHistory = MakeComboList(headerPanel, COMMITDLG_ID_HISTORY, "History\\Comments", "",
                                     COMBO_FLAG_READONLY | COMBO_FLAG_ADDEMPTY, &myComments);
    myCommentHistory->SetToolTip(_("Previous comments (use Ctrl-Up and Ctrl-Down in the Comment field to select)"));
    mySplitter = new ExtSplitterWindow(this, -1, wxDefaultPosition, 
                                       wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
    wxPanel* topPanel = new wxPanel(mySplitter, -1, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    wxBoxSizer* topPanelSizer = new wxBoxSizer(wxVERTICAL);
    wxPanel* bottomPanel = new wxPanel(mySplitter, -1, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    wxBoxSizer* bottomPanelSizer = new wxBoxSizer(wxVERTICAL);

    topPanel->SetSizer(topPanelSizer);
    bottomPanel->SetSizer(bottomPanelSizer);
    mySplitter->SplitHorizontally(topPanel, bottomPanel, 0);
    mySplitter->SetMinimumPaneSize(wxDLG_UNIT_Y(this, 50));
    mySplitter->SetSize(wxDLG_UNIT(this, wxSize(150, 100)));

    // Bug Number
    s = _("Bug Number");
    s += _(":");
    wxStaticText* label11 = new wxStaticText(this, -1, s);
    wxTextValidator validator(wxFILTER_ALPHANUMERIC);
    myBugNumber = new ExtTextCtrl(this, COMMITDLG_ID_BUGNUMBER, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_RICH2, 
                                  validator);
    myBugNumber->SetPlainText(true);
    myBugNumber->SetMaxLength(100);
    myBugNumber->SetSelection(-1, -1);
    myBugNumber->SetValue(wxText(defaultBugNumber));

    // Comment
    s = _("Comment");
    s += _(":");
    wxStaticText* label0 = new wxStaticText(headerPanel, -1, s);
    myComment = new ExtTextCtrl(topPanel, -1, wxT(""), wxDefaultPosition,
                                wxDefaultSize, wxTE_MULTILINE | wxTE_RICH2 | wxBORDER_NONE);
    myComment->SetPlainText(true);
    myComment->SetMaxLength(20000);
    myComment->SetFocus();
    myComment->SetSelection(-1, -1);

    if (!defaultComment.empty())
    {
        myComment->SetValue(wxText(defaultComment));
        myCommentHistory->SetSelection(-1, -1);
    }

    // Status bar
    myStatusBar = new wxStatusBar(this, -1);
   
    myFiles = new ExtListCtrl(bottomPanel, COMMITDLG_ID_FILES, wxDefaultPosition, wxDefaultSize, 
                              wxLC_REPORT | wxLC_ALIGN_LEFT | wxBORDER_NONE);
    myFiles->PushEventHandler(new ListCtrlEventHandler(this));
    myFiles->InsertColumn(0, _("Filename"), wxLIST_FORMAT_LEFT, 0);
    myFiles->InsertColumn(1, _("Filetype"), wxLIST_FORMAT_LEFT, 0);
    myFiles->InsertColumn(2, _("Format"), wxLIST_FORMAT_LEFT, 0);
    myFiles->InsertColumn(3, _("Status"), wxLIST_FORMAT_LEFT, 0);
    myFiles->InsertColumn(4, _("Bug number"), wxLIST_FORMAT_LEFT, 0);

    myFiles->SetCheckboxes(true);
    AddFiles(defaultBugNumber);
    myFiles->ShowFocusRect();
    myFiles->SetBestSize(wxDLG_UNIT(this, wxSize(150, 150)), wxDefaultSize);
    mySortData.column = -1;
    mySortData.asc = true;
    myFiles->SetSortIndicator(-1, true);

    bottomPanelSizer->Add(myFiles, 1, wxGROW | wxALL, 0);
   
    wxStaticText* tip = new wxStaticText(this, -1,
                                         _("To see the changes you have made, double or right click on the files above."));
    tip->SetForegroundColour(ColorRefToWxColour(GetIntegerPreference("Colour Tip Text")));

    // OK/Cancel button
    wxBoxSizer* sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
    myOK = new wxButton(this, wxID_OK, _("OK"));
    myOK->SetDefault();
    wxButton* cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
    sizerConfirm->Add(myOK, 0, wxGROW | wxALL, 5);
    sizerConfirm->Add(cancel, 0, wxGROW | wxALL, 5);

    // Wrap Text checkbox
    myWrapCheckBox = new wxCheckBox(headerPanel, COMMITDLG_ID_WRAP, _("&Wrap lines"));
    myWrapCheckBox->SetValue(TortoiseRegistry::ReadBoolean("Dialogs\\Commit\\Word Wrap", false));

    // Use Bug checkbox
    myUseBugCheckBox = new wxCheckBox(this, COMMITDLG_ID_USEBUG, _("&Use Bug"));
    myUseBugCheckBox->SetValue(!defaultBugNumber.empty());
    myBugNumber->Enable(myUseBugCheckBox->GetValue());

    // Mark Bug checkbox
    myMarkBugCheckBox = new wxCheckBox(this, COMMITDLG_ID_MARKBUG, _("&Mark Bug"));

    wxBoxSizer* headerSizer = new wxBoxSizer(wxVERTICAL);
    headerSizer->Add(label1, 0, wxGROW | wxALL, 3);
    headerSizer->Add(labelCH, 0, wxGROW | wxALL, 3);
    headerSizer->Add(myCommentHistory, 1, wxGROW | wxALL, 3);
    wxBoxSizer* bugsizer = new wxBoxSizer(wxHORIZONTAL);
    bugsizer->Add(label11, 0, 0, 0);
    bugsizer->Add(10, 0);
    bugsizer->Add(myBugNumber, 0, 0, 0);
    bugsizer->Add(50, 0, 5);
    bugsizer->Add(myUseBugCheckBox, 0, 0, 0);
    bugsizer->Add(myMarkBugCheckBox, 0, 0, 0);
    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(label0, 0, 0, 0);
    sizer->Add(50, 0, 5);
    sizer->Add(myWrapCheckBox, 0, 0, 0);
    headerSizer->Add(sizer, 0, wxGROW | wxALL, 3);
    headerPanel->SetSizer(headerSizer);

#ifdef MARCH_HARE_BUILD
    tcvsInfoPanel* subPanel = new tcvsInfoPanel(this);
    wxBoxSizer *infoPanel = new wxBoxSizer(wxVERTICAL);
    infoPanel->Add(subPanel, 0, wxGROW | wxALL, 0);
#endif

    // Main box with everything in it
    wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);
    sizerTop->Add(headerPanel, 0, wxGROW | wxALL, 0);
    topPanelSizer->Add(myComment, 1, wxGROW | wxALL, 0);
    sizerTop->Add(mySplitter, 2, wxGROW | wxALL, 3);
    sizerTop->Add(bugsizer, 0, wxGROW | wxALL, 3);
    sizerTop->Add(tip, 0, wxALIGN_LEFT | wxALL, 3);
    sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, 10);
#ifdef MARCH_HARE_BUILD
    sizerTop->Add(infoPanel, 0, wxGROW | wxALL, 0);
#endif
    sizerTop->Add(myStatusBar, 0, wxGROW | wxALL, 0);

    myKeyHandler = new MyKeyHandler(this);
    myComment->SetKeyHandler(myKeyHandler);

    // Overall dialog layout settings
    SetAutoLayout(TRUE);
    SetSizer(sizerTop);
    sizerTop->SetSizeHints(this);
    sizerTop->Fit(this);

    int pos = 80;
    TortoiseRegistry::ReadInteger("Dialogs\\Commit\\Sash Position", pos);
    mySplitter->SetSashPosition(wxDLG_UNIT_Y(this, pos));

    RestoreTortoiseDialogSize(this, "Commit", wxDLG_UNIT(this, wxSize(140, 120)));
    SetTortoiseDialogPos(this, GetRemoteHandle());
    RestoreTortoiseDialogState(this, "Commit");

    myReady = true;
}


CommitDialog::~CommitDialog()
{
   myFiles->PopEventHandler(true);

   int h = mySplitter->GetSashPosition();
   h = this->ConvertPixelsToDialog(wxSize(0, h)).GetHeight();
   TortoiseRegistry::WriteInteger("Dialogs\\Commit\\Sash Position", h);

   // Delete item data
   int nItems = myFiles->GetItemCount();
   for (int i = 0; i < nItems; ++i)
   {
      CommitDialog::ItemData* data = reinterpret_cast<CommitDialog::ItemData*>(myFiles->GetItemData(i));
      delete data;
   }

   StoreTortoiseDialogSize(this, "Commit");
}


void CommitDialog::OnMenuWebLog(wxCommandEvent&)
{
   int nItems = myFiles->GetItemCount();

   for (int i = 0; i < nItems; ++i)
   {
      if (myFiles->IsSelected(i))
      {
         std::string file = reinterpret_cast<CommitDialog::ItemData*>(myFiles->GetItemData(i))->fullName;
         LaunchTortoiseAct(false, CvsWebLogVerb, file, "", GetRemoteHandle());
         return;
      }
   }
}


void CommitDialog::OnMenuHistory(wxCommandEvent&)
{
   int nItems = myFiles->GetItemCount();

   for (int i = 0; i < nItems; ++i)
   {
      if (myFiles->IsSelected(i))
      {
         std::string file = reinterpret_cast<CommitDialog::ItemData*>(myFiles->GetItemData(i))->fullName;
         LaunchTortoiseAct(false, CvsHistoryVerb, file, "", GetRemoteHandle());
         return;
      }
   }
}



void CommitDialog::OnMenuGraph(wxCommandEvent&)
{
   int nItems = myFiles->GetItemCount();

   for (int i = 0; i < nItems; ++i)
   {
      if (myFiles->IsSelected(i))
      {
         std::string file = reinterpret_cast<CommitDialog::ItemData*>(myFiles->GetItemData(i))->fullName;
         LaunchTortoiseAct(false, CvsRevisionGraphVerb, file, "", GetRemoteHandle());
         return;
      }
   }
}



void CommitDialog::OnMenuCheck(wxCommandEvent&)
{
   int nItems = myFiles->GetItemCount();

   for (int i = 0; i < nItems; ++i)
   {
      if (myFiles->IsSelected(i))
         myFiles->SetChecked(i, true);
   }
}



void CommitDialog::OnMenuUncheck(wxCommandEvent&)
{
   TDEBUG_ENTER("OnMenuUncheck");
   int nItems = myFiles->GetItemCount();

   for (int i = 0; i < nItems; ++i)
   {
      if (myFiles->IsSelected(i))
         myFiles->SetChecked(i, false);
   }
}



void CommitDialog::OnMenuRestore(wxCommandEvent&)
{
   int nItems = myFiles->GetItemCount();
   std::vector<std::string> files, args;
   int i;

   for (i = 0; i < nItems; ++i)
   {
      if (myFiles->IsSelected(i))
      {
         std::string file = reinterpret_cast<CommitDialog::ItemData*>(myFiles->GetItemData(i))->fullName;
         files.push_back(file);
      }
   }
   // Restore items
   if (!files.empty())
   {
      Enable(false);
      LaunchTortoiseAct(true, CvsRestoreVerb, &files, 0, GetRemoteHandle());
      Enable(true);
   }
   // remove restored items from list
   for (i = nItems - 1; i >= 0; i--)
   {
      std::string file = reinterpret_cast<CommitDialog::ItemData*>(myFiles->GetItemData(i))->fullName;
      if (myFiles->IsSelected(i) && FileExists(file.c_str()))
         RemoveItem(i);
   }
   myStatusBar->SetStatusText(Printf(_("%d file(s)"), myFiles->GetItemCount()));
}



void CommitDialog::OnMenuDiff(wxCommandEvent&)
{
   int nItems = myFiles->GetItemCount();

   for (int i = 0; i < nItems; ++i)
   {
      if (myFiles->IsSelected(i))
      {
         std::string file = reinterpret_cast<CommitDialog::ItemData*>(myFiles->GetItemData(i))->fullName;
         LaunchTortoiseAct(false, CvsDiffVerb, file, "", GetRemoteHandle());
         return;
      }
   }
}



void CommitDialog::CommentHistoryClick(wxCommandEvent&)
{
   size_t sel = myCommentHistory->GetSelection();
   if (sel)
        myComment->SetValue(wxText(myComments[sel-1]));
   else
        myComment->SetValue(wxT(""));
}


// Add files to ExtListCtrl
void CommitDialog::AddFiles(const std::string& bugnumber)
{
    std::vector<ItemData*>::const_iterator it = myItemData.begin();
    int myFilesIndex = 0;
    while (it != myItemData.end())
    {
        const ItemData* item = *it;
        if (bugnumber.empty() ||
            (item->status == CVSStatus::STATUS_RENAMED) ||
            (item->status == CVSStatus::STATUS_REMOVED) ||
            (item->status == CVSStatus::STATUS_ADDED) ||
            (bugnumber == item->bugnumber))
        {
            myFiles->InsertItem(myFilesIndex, wxText(item->displayedName));
            myFiles->SetChecked(myFilesIndex, true);
            myFiles->SetItemData(myFilesIndex, (long) item);
            myFiles->SetItem(myFilesIndex, 1, item->filetype);
            myFiles->SetItem(myFilesIndex, 2, CVSStatus::FileFormatString(item->format));
            myFiles->SetItem(myFilesIndex, 3, CVSStatus::FileStatusString(item->status));
            myFiles->SetItem(myFilesIndex, 4, wxText(item->bugnumber));
            ++myFilesIndex;
        }
        ++it;
    }
    myFiles->SetBestColumnWidth(0);
    myFiles->SetBestColumnWidth(1);
    myFiles->SetBestColumnWidth(2);
    myFiles->SetBestColumnWidth(3);
    myFiles->SetBestColumnWidth(4);
    myStatusBar->SetStatusText(Printf(_("%d file(s)"), myFiles->GetItemCount()));
}

void CommitDialog::PrepareItemData(const FilesWithBugnumbers* modified,
                                   const FilesWithBugnumbers* modifiedStatic,
                                   const FilesWithBugnumbers* added,
                                   const FilesWithBugnumbers* removed,
                                   const FilesWithBugnumbers* renamed)
{
    std::vector<std::string> tmp;
    size_t size =
        (modified ? modified->size() : 0) +
        (modifiedStatic ? modifiedStatic->size() : 0) +
        (added ? added->size() : 0) +
        (removed ? removed->size() : 0) +
        (renamed ? renamed->size() : 0);
    tmp.reserve(size);

    // Add modified
    myItemData.reserve(size);
    if (modified)
    {
        FilesWithBugnumbers::const_iterator it = modified->begin();
        while (it != modified->end())
        {
            ItemData* data = new ItemData();
            data->fullName = it->first;
            data->status = CVSStatus::STATUS_CHANGED;
            data->bugnumber = it->second;
            myItemData.push_back(data);
            tmp.push_back(it->first);
            ++it;
        }
    }

    if (modifiedStatic)
    {
        FilesWithBugnumbers::const_iterator it = modifiedStatic->begin();
        while (it != modifiedStatic->end())
        {
            ItemData* data = new ItemData();
            data->fullName = it->first;
            data->status = CVSStatus::STATUS_CHANGED;
            data->bugnumber = it->second;
            myItemData.push_back(data);
            tmp.push_back(it->first);
            ++it;
        }
    }

    // Add added
    if (added)
    {
        FilesWithBugnumbers::const_iterator it = added->begin();
        while (it != added->end())
        {
            ItemData* data = new ItemData();
            data->fullName = it->first;
            data->status = CVSStatus::STATUS_ADDED;
            data->bugnumber = it->second;
            myItemData.push_back(data);
            tmp.push_back(it->first);
            ++it;
        }
    }

    // Add removed
    if (removed)
    {
        FilesWithBugnumbers::const_iterator it = removed->begin();
        while (it != removed->end())
        {
            ItemData* data = new ItemData();
            data->fullName = it->first;
            data->status = CVSStatus::STATUS_REMOVED;
            data->bugnumber = it->second;
            myItemData.push_back(data);
            tmp.push_back(it->first);
            ++it;
        }
    }
    
    // Add renamed
    if (renamed)
    {
        FilesWithBugnumbers::const_iterator it = renamed->begin();
        while (it != renamed->end())
        {
            ItemData* data = new ItemData();
            data->fullName = it->first;
            data->status = CVSStatus::STATUS_RENAMED;
            data->bugnumber = it->second;
            myItemData.push_back(data);
            tmp.push_back(it->first);
            ++it;
        }
    }
    
    myStub = FindCommonStub(tmp);
    
    std::vector<ItemData*>::const_iterator itemDataIter = myItemData.begin();
    while (itemDataIter != myItemData.end())
    {
        (*itemDataIter)->displayedName = (*itemDataIter)->fullName.substr(myStub.length());
        (*itemDataIter)->format = CVSStatus::GetFileFormat((*itemDataIter)->fullName);
        (*itemDataIter)->status = CVSStatus::GetFileStatus((*itemDataIter)->fullName);
        (*itemDataIter)->filetype = GetFileType((*itemDataIter)->fullName.c_str());
        ++itemDataIter;
    }
}

// Copy filenames from specified list to clipboard
// names means omit subdirectory names
void CommitDialog::CopyFilesToClip(bool names)
{
   wxString text;
   wxString dir;

   int nItems = myFiles->GetItemCount();

   for (int i = 0; i < nItems; ++i)
   {
      if (myFiles->IsSelected(i))
      {
         wxString file = myFiles->GetItemText(i).GetData();
         wxString newstr;
         if (names)
         {
            wxFileName filename(file);
            newstr = filename.GetFullName();
            // put out directory headers every time directory changes
            if (filename.GetPath() != dir)
            {
               dir = filename.GetPath();
               if (!text.empty()) 
                  text += wxT("\r\n");
               text += dir;
               text + _(":");
            }
         }
         else
             newstr = file;
         if (!text.empty()) 
            text += wxT("\r\n");
         text += newstr;
      }
   }
   if (wxTheClipboard->Open())
   {
       // These data objects are held by the clipboard, 
       // so do not delete them in the app.
       wxTheClipboard->SetData(new wxTextDataObject(text));
       wxTheClipboard->Close();
   }
}


void CommitDialog::OnMenuCopyNames(wxCommandEvent&)
{
   CopyFilesToClip(true);
}

void CommitDialog::OnMenuCopyDirs(wxCommandEvent&)
{
   CopyFilesToClip(false);
}

void CommitDialog::OnMenuCommit(wxCommandEvent&)
{
   RunCommandOnSelectedItems(CvsCommitVerb, true);
}

void CommitDialog::OnMenuCleanCopy(wxCommandEvent&)
{
   RunCommandOnSelectedItems(CvsUpdateCleanVerb, false);
}

void CommitDialog::RunCommandOnSelectedItems(std::string verb, bool isCommit)
{
    TDEBUG_ENTER("RunCommandOnSelectedItems");
    int nItems = myFiles->GetItemCount();
    std::vector<std::string> files, args;

    for (int i = 0; i < nItems; ++i)
        if (myFiles->IsSelected(i))
        {
            std::string file = reinterpret_cast<CommitDialog::ItemData*>(myFiles->GetItemData(i))->fullName;
            files.push_back(file);
        }

    // Run command on items
    if (!files.empty())
    {
        Enable(false);
        LaunchTortoiseAct(true, verb, &files, 0, GetRemoteHandle());
        Enable(true);
    }
    // remove committed items from list
    size_t index = files.size()-1;
    for (int i = nItems - 1; i >= 0; i--)
    {
        if (myFiles->IsSelected(i))
        {
            if (isCommit)
            {
                int status = CVSStatus::GetFileStatus(files[index]);
                if ((status == CVSStatus::STATUS_CHANGED) ||
                    (status == CVSStatus::STATUS_RENAMED) ||
                    (status == CVSStatus::STATUS_ADDED) ||
                    (status == CVSStatus::STATUS_REMOVED))
                    continue;
            }
            RemoveItem(i);
            --index;
        }
    }
    myStatusBar->SetStatusText(Printf(_("%d file(s)"), myFiles->GetItemCount()));
}

void CommitDialog::WrapTextClick(wxCommandEvent&)
{
   // nop
}

void CommitDialog::UseBugClick(wxCommandEvent&)
{
   bool enabled = myUseBugCheckBox->GetValue();
   myBugNumber->Enable(enabled);
   if (enabled)
   {
       myMarkBugCheckBox->SetValue(false); 
       myBugNumber->SetFocus();
   }
   myFiles->DeleteAllItems();
   AddFiles(enabled ? wxAscii(myBugNumber->GetValue()) : "");
}

void CommitDialog::MarkBugClick(wxCommandEvent&)
{
    bool enabled = myMarkBugCheckBox->GetValue();
    myBugNumber->Enable(enabled);
    if (enabled)
    {
        if (myUseBugCheckBox->GetValue())
        {
            myFiles->DeleteAllItems();
            AddFiles("");
        }
        myBugNumber->SetFocus();
        myUseBugCheckBox->SetValue(false); 
    }
}


void CommitDialog::OnOk(wxCommandEvent&)
{
   bool userWantsReminder = myDirGroups.GetBooleanPreference("Warn On Missing Commit Comment");
   if (userWantsReminder && myComment->GetValue().empty())
   {
      if (!DoYesNoDialog(this, _(
"You are attempting to commit but have written no comment.\nDo you wish to continue to commit without a comment?"), 
                         false))
         return;
   }
   if ((myUseBugCheckBox->GetValue() || myMarkBugCheckBox->GetValue()) && 
       myBugNumber->GetValue().empty())
   {
      if (!DoYesNoDialog(this, _(
"You are attempting to commit but have no bug number.\nDo you wish to continue the commit without the bug number?"), 
                         false))
         return;
      else
      {
         myUseBugCheckBox->SetValue(0);
         myMarkBugCheckBox->SetValue(0);
      }
   }
   wxDialog::EndModal(wxID_OK);
}

int wxCALLBACK CommitDialog::CompareFunc(long item1, long item2, long data)
{
   CommitDialog::ItemData* itemdata1 = reinterpret_cast<CommitDialog::ItemData*>(item1);
   CommitDialog::ItemData* itemdata2 = reinterpret_cast<CommitDialog::ItemData*>(item2);
   SortData* sortData = reinterpret_cast<SortData*>(data);
   int result = 0;
   if (sortData->column == 0)
   {
      result = strcmpi(itemdata1->displayedName.c_str(), itemdata2->displayedName.c_str());
   }
   else if (sortData->column == 1)
   {
      result = _tcsicmp(itemdata1->filetype.c_str(),
                        itemdata2->filetype.c_str());
   }
   else if (sortData->column == 2)
   {
      result = lstrcmpi(CVSStatus::FileFormatString(itemdata1->format).c_str(),
                        CVSStatus::FileFormatString(itemdata2->format).c_str());
   }
   else if (sortData->column == 3)
   {
      result = lstrcmpi(CVSStatus::FileStatusString(itemdata1->status).c_str(),
                        CVSStatus::FileStatusString(itemdata2->status).c_str());
   }

   if (!sortData->asc)
   {
      result *= -1;
   }

   return result;
}

// Get file type
wxString CommitDialog::GetFileType(const char* filename)
{
   SHFILEINFO sfi;
   wxString result;
   wxString filenameString(wxText(filename));
   SHGetFileInfo(filenameString.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, 
                 sizeof(sfi), SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES);
   if (sfi.szTypeName[0])
      result = sfi.szTypeName;
   else
   {
      std::string s = GetExtension(filename);
      MakeUpperCase(s);
      if (!s.empty())
         result = Printf(_("%s File"), wxTextCStr(s));
      else
         result = _("File");
   }
   return result;
}




// MyKeyHandler - handles Ctrl-Up/Down in the text control

MyKeyHandler::MyKeyHandler(CommitDialog* parent)
   : myIndex(-1),
     myParent(parent)
{
}

MyKeyHandler::~MyKeyHandler()
{
}

bool MyKeyHandler::OnArrowUpDown(bool up)
{
   wxString currentComment = myParent->myComment->GetValue();
   int oldIndex = myIndex;
   if (up)
   {
      // Get next log message
      if (myIndex < static_cast<int>(myParent->myComments.size()-1))
         ++myIndex;
   }
   else
   {
      // Get previous log message
      if (myIndex >= 0)
         --myIndex;
   }
   if (oldIndex == -1)
   {
      // Save current user input
      myEditedComment = currentComment;
   }
   else if (myIndex == -1)
   {
      // Restore saved user input
      myParent->myComment->SetValue(myEditedComment);
   }
   if ((myIndex >= 0) && (myIndex < (int) myParent->myComments.size()))
   {
      // Insert selected history entry
      myParent->myComment->SetValue(wxText(myParent->myComments[myIndex]));
   }
   return false;
}

bool MyKeyHandler::OnSpace()
{
   if (myParent->myWrapCheckBox->GetValue())
   {
      // Calculate current line number
      int pos = myParent->myComment->GetInsertionPoint();
      int currentline = 0;
      while (pos > myParent->myComment->GetLineLength(currentline))
      {
         pos -= myParent->myComment->GetLineLength(currentline) + 1;
         ++currentline;
      }
      // Check if a line break should be inserted
      int linelength = myParent->myComment->GetLineLength(currentline);
      if (linelength > 80)
         myParent->myComment->WriteText(wxT("\n"));
   }
   return false;
}


void CommitDialog::BugNumberTextUpdated(wxCommandEvent&)
{
    if (!myReady)
        return;
    if (!myUseBugCheckBox->GetValue())
        return;
    myFiles->DeleteAllItems();
    AddFiles(wxAscii(myBugNumber->GetValue()));
}


void CommitDialog::RemoveItem(int i)
{
    CommitDialog::ItemData* data = reinterpret_cast<CommitDialog::ItemData*>(myFiles->GetItemData(i));
    std::vector<ItemData*>::iterator it = std::find(myItemData.begin(), myItemData.end(), data);
    myItemData.erase(it);
    delete data;
    myFiles->DeleteItem(i);
}
