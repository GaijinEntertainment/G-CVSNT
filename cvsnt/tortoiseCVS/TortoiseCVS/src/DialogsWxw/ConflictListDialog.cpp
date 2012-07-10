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


#include "StdAfx.h"
#include "ConflictListDialog.h"
#include "WxwHelpers.h"
#include "WxwMain.h"
#include "FilenameText.h"
#include "ExtStaticText.h"
#include "ExtListCtrl.h"
#include "ExplorerMenu.h"
#include "../Utils/TortoiseDebug.h"
#include "../Utils/TortoiseUtils.h"
#include "../Utils/TortoiseRegistry.h"
#include "../Utils/Translate.h"
#include <Utils/Preference.h>
#include <Utils/LaunchTortoiseAct.h>
#include "../TortoiseAct/TortoiseActVerbs.h"


enum
{
   CONFLICTLISTDLG_ID_FILES = 10001,

   CD_DIFF,
   CD_FAVOR_LOCAL,
   CD_FAVOR_REPOS,
   CD_MERGE,
   CD_HISTORY,
   CD_GRAPH,
   CD_CHECK
};




BEGIN_EVENT_TABLE(ConflictListDialog::ListCtrlEventHandler, wxEvtHandler)
   EVT_CONTEXT_MENU(ListCtrlEventHandler::OnContextMenu)
END_EVENT_TABLE()

   
//static
bool DoConflictListDialog(wxWindow* parent, 
                          const std::vector<std::string>& files)
{
   ConflictListDialog* dlg = new ConflictListDialog(parent, files);
   bool ret = (dlg->ShowModal() == wxID_OK);
   
   DestroyDialog(dlg);
   return ret;
}


BEGIN_EVENT_TABLE(ConflictListDialog, ExtDialog)
   EVT_LIST_ITEM_ACTIVATED(CONFLICTLISTDLG_ID_FILES, ConflictListDialog::OnDblClick)
   EVT_LIST_COL_CLICK(CONFLICTLISTDLG_ID_FILES,      ConflictListDialog::ListColumnClick)
   EVT_MENU(CD_MERGE, ConflictListDialog::OnMenuMerge)
   EVT_MENU(CD_FAVOR_LOCAL, ConflictListDialog::OnMenuFavorLocal)
   EVT_MENU(CD_FAVOR_REPOS, ConflictListDialog::OnMenuFavorRepository)
   EVT_MENU(CD_DIFF, ConflictListDialog::OnMenuDiff)
   EVT_MENU(CD_HISTORY, ConflictListDialog::OnMenuHistory)
   EVT_MENU(CD_GRAPH, ConflictListDialog::OnMenuGraph)
END_EVENT_TABLE()
   
ConflictListDialog::ConflictListDialog(wxWindow* parent, 
                                       const std::vector<std::string>& files)
   : ExtDialog(parent, -1, _("TortoiseCVS - Resolve Conflicts"),
               wxDefaultPosition, wxDefaultSize,
               wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxRESIZE_BORDER | 
               wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN)

{
   TDEBUG_ENTER("ConflictListDialog::ConflictListDialog");
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

   std::vector<ItemData*> itemData;
   std::vector<std::string> tmp;
   std::vector<std::string>::const_iterator it;

   // Add files
   it = files.begin();
   while (it != files.end())
   {
      ItemData *data = new ItemData();
      data->m_Filename = *it;
      data->m_Status = CVSStatus::STATUS_CONFLICT;
      itemData.push_back(data);
      tmp.push_back(*it);
      it++;
   }


   // Trim paths to sensible length
   ShortenPaths(tmp, myStub);

   wxStaticText* label2 = new ExtStaticText(this, -1, _(
"CVS encountered conflicts when trying to merge your changes in the files below. Please merge your changes manually."),
                                            wxDefaultPosition, 
                                            wxDLG_UNIT(this, wxSize(60, 15)));
   
   FilenameText* label1 = new FilenameText(this, -1, _("Folder: %s"), wxText(RemoveTrailingDelimiter(myStub)));
   label1->SetWindowStyle(label1->GetWindowStyle() | wxST_NO_AUTORESIZE);
   myFiles = new ExtListCtrl(this, CONFLICTLISTDLG_ID_FILES, wxDefaultPosition, wxDefaultSize, 
                             wxLC_REPORT | wxLC_ALIGN_LEFT);
   myFiles->PushEventHandler(new ListCtrlEventHandler(this));
   myFiles->InsertColumn(0, _("Filename"), wxLIST_FORMAT_LEFT, 0);
   myFiles->InsertColumn(1, _("Format"), wxLIST_FORMAT_LEFT, 0);
   myFiles->InsertColumn(2, _("Status"), wxLIST_FORMAT_LEFT, 0);
   AddFiles(tmp, itemData);
   myFiles->SetBestColumnWidth(0);
   myFiles->SetBestColumnWidth(1);
   myFiles->SetBestColumnWidth(2);
   myFiles->SetBestSize(wxDLG_UNIT(this, wxSize(150, 150)), wxDefaultSize);

   wxStaticText* tip = new wxStaticText(this, -1,
                                        _("To resolve the conflicts, double or right click on the files above."));
   tip->SetForegroundColour(SetForegroundColour(ColorRefToWxColour(GetIntegerPreference("Colour Tip Text"))));

   // OK/Cancel button
   wxBoxSizer* sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   myOK = new wxButton(this, wxID_OK, _("Close"));
   myOK->SetDefault();
   sizerConfirm->Add(myOK, 0, wxGROW | wxALL, 5);

   // Status bar
   myStatusBar = new wxStatusBar(this, -1);
   myStatusBar->SetStatusText(Printf(_("%d file(s)"), files.size()).c_str());

   // Main box with everything in it
   wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(label2, 0, wxGROW | wxALL, 3);
   sizerTop->Add(label1, 0, wxGROW | wxALL, 3);
   sizerTop->Add(myFiles, 2, wxGROW | wxALL, 3);
   sizerTop->Add(tip, 0, wxALIGN_LEFT | wxALL, 3);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, 10);
   sizerTop->Add(myStatusBar, 0, wxGROW | wxALL, 0);

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);

   RestoreTortoiseDialogSize(this, "Conflict");
   SetTortoiseDialogPos(this, GetRemoteHandle());
   RestoreTortoiseDialogState(this, "Conflict");
}


ConflictListDialog::~ConflictListDialog()
{
   myFiles->PopEventHandler(true);

   // Delete item data
   int nItems = myFiles->GetItemCount();
   for (int i = 0; i < nItems; ++i)
   {
      ConflictListDialog::ItemData* data = (ConflictListDialog::ItemData*) myFiles->GetItemData(i);
      if (data)
      {
         delete data;
      }
   }

   StoreTortoiseDialogSize(this, "Conflict");
}





// Add files to ExtListCtrl
void ConflictListDialog::AddFiles(const std::vector<std::string>& filenames,
                                  const std::vector<ItemData*>& itemData)
{
   std::vector<std::string>::const_iterator it = filenames.begin();
   int i = 0;
   while (it != filenames.end())
   {
      myFiles->InsertItem(i, wxText(*it));
      myFiles->SetChecked(i, true);
      myFiles->SetItemData(i, (long) itemData[i]);
      itemData[i]->m_Format = CVSStatus::GetFileFormat(itemData[i]->m_Filename);
      myFiles->SetItem(i, 1, CVSStatus::FileFormatString(itemData[i]->m_Format).c_str());
      myFiles->SetItem(i, 2, CVSStatus::FileStatusString(itemData[i]->m_Status).c_str());
      ++it;
      ++i;
   }
}



void ConflictListDialog::RefreshItem(int WXUNUSED(i))
{
   // TODO: Refresh list entry
}



int wxCALLBACK ConflictListDialog::CompareFunc(long item1, long item2, long sortData)
{
   ConflictListDialog::ItemData* itemdata1 = reinterpret_cast<ConflictListDialog::ItemData*>(item1);
   ConflictListDialog::ItemData* itemdata2 = reinterpret_cast<ConflictListDialog::ItemData*>(item2);
   if (sortData == 0)
   {
      return strcmpi(itemdata1->m_Filename.c_str(), itemdata2->m_Filename.c_str());
   }
   else if (sortData == 1)
   {
      return _tcsicmp(CVSStatus::FileFormatString(itemdata1->m_Format).c_str(),
                      CVSStatus::FileFormatString(itemdata2->m_Format).c_str());
   }
   else if (sortData == 2)
   {
      return _tcsicmp(CVSStatus::FileStatusString(itemdata1->m_Status).c_str(),
                      CVSStatus::FileStatusString(itemdata2->m_Status).c_str());
   }
   return 0;
}

void ConflictListDialog::OnDblClick(wxListEvent& event)
{
   TDEBUG_ENTER("ConflictListDialog::OnDblClick");
   std::string file = ((ConflictListDialog::ItemData*) event.GetData())->m_Filename;
   LaunchTortoiseAct(false, CvsMergeConflictsVerb, file, "", GetHwnd()); 
}

void ConflictListDialog::ListColumnClick(wxListEvent& e)
{
   myFiles->SortItems(CompareFunc, e.GetColumn());
}



// ListCtrlEventHandler

void ConflictListDialog::ListCtrlEventHandler::OnContextMenu(wxContextMenuEvent& event)
{
   wxMenu* popupMenu = 0;
   ExplorerMenu* explorerMenu = 0;
   int i;
   bool bNeedsSeparator = false;
   std::string sDir;

   std::vector<std::string> selectedFiles;
   for (i = 0; i < myParent->myFiles->GetItemCount(); i++)
   {
      if (myParent->myFiles->IsSelected(i))
      {
         ConflictListDialog::ItemData* data =
            (ConflictListDialog::ItemData*) myParent->myFiles->GetItemData(i);
         selectedFiles.push_back(data->m_Filename);
      }
   }

   if (selectedFiles.size() == 0)
      return;

   // Add common contextmenu entries
   popupMenu = new wxMenu();
   if (selectedFiles.size() == 1)
   {
      popupMenu->Append(CD_MERGE,
                        _("&Merge Conflicts..."), _("Merge conflicts manually"));
   }
#if 0 // Don't use that part yet
   if (selectedFiles.size() >= 1)
   {
      popupMenu->Append(0,
                        _("&Discard local changes..."), 
                        _("Delete file and get a clean copy from CVS..."));
      bNeedsSeparator = true;
   }
   if (selectedFiles.size() == 1)
   {
      popupMenu->AppendSeparator();
      popupMenu->Append(CD_DIFF,
                        _("&Diff..."), _("Show differences to original revision"));
      popupMenu->Append(CD_HISTORY, _("H&istory..."),
                        _("Show history"));
      popupMenu->Append(CD_GRAPH, _("Revision &Graph..."),
                        _("Show revision graph"));
      bNeedsSeparator = true;
   }
#endif
   // Add explorer menu
   if (StripCommonPath(selectedFiles, sDir))
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

   myParent->PopupMenu(popupMenu, myParent->ScreenToClient(pos));

   delete popupMenu;
}

void ConflictListDialog::OnMenuMerge(wxCommandEvent&)
{
   int nItems = myFiles->GetItemCount();

   for (int i = 0; i < nItems; ++i)
   {
      if (myFiles->IsSelected(i))
      {
         std::string file = ((ConflictListDialog::ItemData*) myFiles->GetItemData(i))->m_Filename;
         LaunchTortoiseAct(false, CvsMergeConflictsVerb, file, "", GetHwndOf(this));
         return;
      }
   }
}



void ConflictListDialog::OnMenuFavorLocal(wxCommandEvent&)
{
   // TODO: Favor local changes
}



void ConflictListDialog::OnMenuFavorRepository(wxCommandEvent&)
{
   // TODO: Favor repository changes
}



void ConflictListDialog::OnMenuHistory(wxCommandEvent&)
{
   int nItems = myFiles->GetItemCount();

   for (int i = 0; i < nItems; ++i)
   {
      if (myFiles->IsSelected(i))
      {
         std::string file = ((ConflictListDialog::ItemData*) myFiles->GetItemData(i))->m_Filename;
         LaunchTortoiseAct(false, CvsHistoryVerb, file, "", GetHwndOf(this));
         return;
      }
   }
}



void ConflictListDialog::OnMenuGraph(wxCommandEvent&)
{
   int nItems = myFiles->GetItemCount();

   for (int i = 0; i < nItems; ++i)
   {
      if (myFiles->IsSelected(i))
      {
         std::string file = ((ConflictListDialog::ItemData*) myFiles->GetItemData(i))->m_Filename;
         LaunchTortoiseAct(false, CvsRevisionGraphVerb, file, "", GetHwndOf(this)); 
         return;
      }
   }
}



void ConflictListDialog::OnMenuDiff(wxCommandEvent&)
{
   int nItems = myFiles->GetItemCount();

   for (int i = 0; i < nItems; ++i)
   {
      if (myFiles->IsSelected(i))
      {
         std::string file = ((ConflictListDialog::ItemData*) myFiles->GetItemData(i))->m_Filename;
         LaunchTortoiseAct(false, CvsDiffVerb, file, "", GetHwndOf(this)); 
         return;
      }
   }
}
