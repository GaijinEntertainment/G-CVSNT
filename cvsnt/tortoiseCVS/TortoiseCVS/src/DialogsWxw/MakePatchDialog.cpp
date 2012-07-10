// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2005 - Torsten Martinsen
// <torsten@tiscali.dk> - April 2005

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
#include "MakePatchDialog.h"
#include "WxwHelpers.h"
#include "WxwMain.h"
#include <wx/clipbrd.h>
#include <wx/filename.h>
#include "AddFilesDialog.h"
#include "FilenameText.h"
#include "ExtTextCtrl.h"
#include "ExplorerMenu.h"
#include "ExtListCtrl.h"
#include "MessageDialog.h"
#include "../CVSGlue/CVSAction.h"
#include "../Utils/PathUtils.h"
#include "../Utils/TortoiseDebug.h"
#include "../Utils/TortoiseUtils.h"
#include "../Utils/TortoiseRegistry.h"
#include "../Utils/ShellUtils.h"
#include "../Utils/StringUtils.h"
#include "../Utils/Translate.h"
#include <Utils/Preference.h>
#include "../TortoiseAct/TortoiseActVerbs.h"
#include "../TortoiseAct/TortoiseActUtils.h"
#include "../Utils/FileTree.h"
#include <Utils/LaunchTortoiseAct.h>
#include "YesNoDialog.h"

class MyUserData : public FileTree::UserData 
{
public:
};


enum 
{
   CD_DIFF = 10001,
   CD_CHECK,
   CD_UNCHECK,
   CD_COPYNAMES,
   CD_COPYDIRS,

   COMMITDLG_ID_FILES,
   COMMITDLG_ID_ADD
};


BEGIN_EVENT_TABLE(MakePatchDialog, ExtDialog)
   EVT_MENU(CD_DIFF,            MakePatchDialog::OnMenuDiff)
   EVT_MENU(CD_CHECK,           MakePatchDialog::OnMenuCheck)
   EVT_MENU(CD_UNCHECK,         MakePatchDialog::OnMenuUncheck)
   EVT_MENU(CD_COPYNAMES,       MakePatchDialog::OnMenuCopyNames)
   EVT_MENU(CD_COPYDIRS,        MakePatchDialog::OnMenuCopyDirs)
   EVT_COMMAND(COMMITDLG_ID_ADD, wxEVT_COMMAND_BUTTON_CLICKED, MakePatchDialog::OnAddFiles)
END_EVENT_TABLE()

void MakePatchDialog::ListCtrlEventHandler::OnContextMenu(wxContextMenuEvent& event)
{
   TDEBUG_ENTER("MakePatchDialog::OnContextMenu");

   // Count selected items of type REMOVED, ADDED, CHANGED
   int countAdded = 0;
   int countChanged = 0;
   int countRemoved = 0;
   int countNotInCvs = 0;
   std::string sDir;
   bool bNeedsSeparator = false;
   int i;
   std::vector<std::string> selectedFiles;
   ExplorerMenu* explorerMenu = 0;
   for (i = 0; i < myParent->myFiles->GetItemCount(); i++)
   {
      if (myParent->myFiles->IsSelected(i))
      {
         MakePatchDialog::ItemData* data = (MakePatchDialog::ItemData*) myParent->myFiles->GetItemData(i);
         if (data->m_Status == CVSStatus::STATUS_ADDED)
            ++countAdded;
         else if (data->m_Status == CVSStatus::STATUS_CHANGED)
            ++countChanged;
         else if (data->m_Status == CVSStatus::STATUS_REMOVED)
            ++countRemoved;
         else if ((data->m_Status == CVSStatus::STATUS_NOTINCVS) ||
                  (data->m_Status == CVSStatus::STATUS_NOWHERENEAR_CVS))
            ++countNotInCvs;
         selectedFiles.push_back(data->m_Filename);
      }
   }

   // if no items selected, return
   if ((countAdded + countChanged + countRemoved + countNotInCvs) == 0)
      return;

   // Add common contextmenu entries
   std::auto_ptr<wxMenu> popupMenu(new wxMenu);
   popupMenu->Append(CD_CHECK, _("Check"), wxT(""));
   popupMenu->Append(CD_UNCHECK, _("Uncheck"), wxT(""));
   popupMenu->Append(CD_COPYNAMES, _("&Copy filename(s)"), wxT(""));
   popupMenu->Append(CD_COPYDIRS, _("Copy &filename(s) with folders"), wxT(""));

   bNeedsSeparator = true;

   if ((countRemoved == 0) && (countAdded == 0) && (countChanged == 1))
   {
      if (bNeedsSeparator)
      {
         popupMenu->AppendSeparator();
         bNeedsSeparator = false;
      }
      popupMenu->Append(CD_DIFF, _("&Diff..."), wxT(""));
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

void MakePatchDialog::ListCtrlEventHandler::ListColumnClick(wxListEvent& e)
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

void MakePatchDialog::ListCtrlEventHandler::OnDblClick(wxListEvent& event)
{
   TDEBUG_ENTER("MakePatchDialog::OnDblClick");
   int item = event.GetIndex();
   TDEBUG_TRACE("Item " << item);
   MakePatchDialog::ItemData* data = (MakePatchDialog::ItemData*) event.GetData();

   // Modified file: Run diff
   if (data->m_Status == CVSStatus::STATUS_CHANGED)
   {
      std::string file = data->m_Filename;
      LaunchTortoiseAct(false, CvsDiffVerb, file, "", GetRemoteHandle());
   }
   // otherwise just switch checked state
   else
   {
      myParent->myFiles->SetChecked(item, !myParent->myFiles->IsChecked(item));
   }
}


BEGIN_EVENT_TABLE(MakePatchDialog::ListCtrlEventHandler, wxEvtHandler)
   EVT_CONTEXT_MENU(ListCtrlEventHandler::OnContextMenu)
   EVT_LIST_COL_CLICK(-1, ListCtrlEventHandler::ListColumnClick)
   EVT_LIST_ITEM_ACTIVATED(-1, ListCtrlEventHandler::OnDblClick)
END_EVENT_TABLE()

//static
bool DoMakePatchDialog(wxWindow* parent,
                       DirectoryGroups& groups,
                       const FilesWithBugnumbers* modified,
                       const FilesWithBugnumbers* added,
                       const FilesWithBugnumbers* removed,
                       std::vector<std::string>& userSelection,
                       std::vector<std::string>& additionalFiles)
{
   MakePatchDialog* dlg = new MakePatchDialog(parent, groups, modified, added, removed, additionalFiles);
   bool ret = (dlg->ShowModal() == wxID_OK);
   
   if (ret)
   {
      // Fill in user selection of ticked items
      for (int i = 0; i < dlg->myFiles->GetItemCount(); ++i)
      {
         if (dlg->myFiles->IsChecked(i))
         {
            MakePatchDialog::ItemData* data = (MakePatchDialog::ItemData*) dlg->myFiles->GetItemData(i);
            if (data->m_AdditionalFile)
               additionalFiles.push_back(data->m_Filename);
            else
               userSelection.push_back(data->m_Filename);
         }
      }
   }
   
   DestroyDialog(dlg);
   return ret;
}



MakePatchDialog::MakePatchDialog(wxWindow* parent,
                                 DirectoryGroups& groups,
                                 const FilesWithBugnumbers* modified,
                                 const FilesWithBugnumbers* added,
                                 const FilesWithBugnumbers* removed,
                                 std::vector<std::string>& additionalFiles)
   : ExtDialog(parent, -1, _("TortoiseCVS - Make Patch"),
               wxDefaultPosition, wxDefaultSize,
               wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxRESIZE_BORDER | 
               wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN),
     myDirectoryGroups(groups)
{
   TDEBUG_ENTER("MakePatchDialog::MakePatchDialog");
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

   std::string bugnumber;
   std::vector<std::string> tmp;
   tmp.reserve(modified->size() + added->size() + removed->size());

   // Add modified
   std::vector<ItemData*> itemData;
   itemData.reserve(modified->size() + added->size() + removed->size());
   FilesWithBugnumbers::const_iterator it = modified->begin();
   while (it != modified->end())
   {
      ItemData* data = new ItemData();
      data->m_Filename = it->first;
      data->m_Status = CVSStatus::STATUS_CHANGED;
      data->m_Bugnumber = it->second;
      itemData.push_back(data);
      tmp.push_back(it->first);
      ++it;
   }

   // Add added
   it = added->begin();
   while (it != added->end())
   {
      ItemData* data = new ItemData(true);
      data->m_Filename = it->first;
      data->m_Status = CVSStatus::STATUS_ADDED;
      data->m_Bugnumber = it->second;
      itemData.push_back(data);
      tmp.push_back(it->first);
      ++it;
   }

   // Add removed
   it = removed->begin();
   while (it != removed->end())
   {
      ItemData* data = new ItemData();
      data->m_Filename = it->first;
      data->m_Status = CVSStatus::STATUS_REMOVED;
      data->m_Bugnumber = it->second;
      itemData.push_back(data);
      tmp.push_back(it->first);
      ++it;
   }

   // Trim paths to sensible length
   ShortenPaths(tmp, myStub);

   // Status bar must be constructed before calling AddFiles()
   myStatusBar = new wxStatusBar(this, -1);
   
   FilenameText* label1 = new FilenameText(this, -1, _("Folder: %s"),
                                           wxText(RemoveTrailingDelimiter(myStub)));
   label1->SetWindowStyle(label1->GetWindowStyle() | wxST_NO_AUTORESIZE);

   // "Add files" button
   wxBoxSizer* sizerAdd = new wxBoxSizer(wxHORIZONTAL);
   myAddButton = new wxButton(this, COMMITDLG_ID_ADD, _("&Add files..."));
   sizerAdd->Add(myAddButton, 0, wxGROW | wxALL, 5);

   myFiles = new ExtListCtrl(this, COMMITDLG_ID_FILES, wxDefaultPosition, wxDefaultSize, 
                             wxLC_REPORT | wxLC_ALIGN_LEFT | wxBORDER_NONE);
   myFiles->PushEventHandler(new ListCtrlEventHandler(this));
   myFiles->InsertColumn(0, _("Filename"), wxLIST_FORMAT_LEFT, 0);
   myFiles->InsertColumn(1, _("Filetype"), wxLIST_FORMAT_LEFT, 0);
   myFiles->InsertColumn(2, _("Status"), wxLIST_FORMAT_LEFT, 0);
   myFiles->InsertColumn(3, _("Bug Number"), wxLIST_FORMAT_LEFT, 0);
   myFiles->SetCheckboxes(true);
   AddFiles(tmp, itemData);
   myFiles->ShowFocusRect();
   myFiles->SetBestColumnWidth(0);
   myFiles->SetBestColumnWidth(1);
   myFiles->SetBestColumnWidth(2);
   myFiles->SetBestColumnWidth(3);
   myFiles->SetBestSize(wxDLG_UNIT(this, wxSize(150, 150)), wxDefaultSize);
   mySortData.column = -1;
   mySortData.asc = true;
   myFiles->SetSortIndicator(-1, true);
   myFiles->SetFocus();

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

   // Main box with everything in it
   wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(label1, 0, wxGROW | wxALL, 3);
   sizerTop->Add(sizerAdd, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, 10);
   sizerTop->Add(myFiles, 2, wxGROW | wxALL, 3);
   sizerTop->Add(tip, 0, wxALIGN_LEFT | wxALL, 3);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, 10);
   sizerTop->Add(myStatusBar, 0, wxGROW | wxALL, 0);

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);

   RestoreTortoiseDialogSize(this, "Make Patch", wxDLG_UNIT(this, wxSize(140, 120)));
   SetTortoiseDialogPos(this, GetRemoteHandle());
   RestoreTortoiseDialogState(this, "Make Patch");
}


MakePatchDialog::~MakePatchDialog()
{
   myFiles->PopEventHandler(true);

   // Delete item data
   int nItems = myFiles->GetItemCount();
   for (int i = 0; i < nItems; ++i)
   {
      MakePatchDialog::ItemData* data = (MakePatchDialog::ItemData*) myFiles->GetItemData(i);
      delete data;
   }

   StoreTortoiseDialogSize(this, "Make Patch");
}


void MakePatchDialog::OnMenuCheck(wxCommandEvent&)
{
   int nItems = myFiles->GetItemCount();

   for (int i = 0; i < nItems; ++i)
   {
      if (myFiles->IsSelected(i))
         myFiles->SetChecked(i, true);
   }
}



void MakePatchDialog::OnMenuUncheck(wxCommandEvent&)
{
   int nItems = myFiles->GetItemCount();

   for (int i = 0; i < nItems; ++i)
   {
      if (myFiles->IsSelected(i))
         myFiles->SetChecked(i, false);
   }
}

void MakePatchDialog::OnMenuDiff(wxCommandEvent&)
{
   int nItems = myFiles->GetItemCount();

   for (int i = 0; i < nItems; ++i)
   {
      if (myFiles->IsSelected(i))
      {
         std::string file = ((MakePatchDialog::ItemData*) myFiles->GetItemData(i))->m_Filename;
         LaunchTortoiseAct(false, CvsDiffVerb, file, "", GetRemoteHandle());
         return;
      }
   }
}


// Add files to ExtListCtrl
void MakePatchDialog::AddFiles(const std::vector<std::string>& filenames,
                               const std::vector<ItemData*>& itemData)
{
   std::vector<std::string>::const_iterator it = filenames.begin();
   int i = 0;
   while (it != filenames.end())
   {
      myFiles->InsertItem(i, wxText(*it));
      myFiles->SetChecked(i, true);
      myFiles->SetItemData(i, (long) itemData[i]);

      itemData[i]->m_Status = CVSStatus::GetFileStatus(itemData[i]->m_Filename);
      itemData[i]->m_Filetype = GetFileType(itemData[i]->m_Filename.c_str());
      myFiles->SetItem(i, 1, itemData[i]->m_Filetype.c_str());
      myFiles->SetItem(i, 2, CVSStatus::FileStatusString(itemData[i]->m_Status).c_str());
      ++it;
      ++i;
   }
   myStatusBar->SetStatusText(Printf(_("%d file(s)"), myFiles->GetItemCount()).c_str());
}


// Copy filenames from specified list to clipboard
// names means omit subdirectory names
void MakePatchDialog::CopyFilesToClip(bool names)
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
            if (filename.GetPath() != dir.c_str())
            {
               dir = filename.GetPath();
               if (!text.empty())
                   text += wxT("\r\n");
               text += dir;
               text += wxT(":");
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


void MakePatchDialog::OnMenuCopyNames(wxCommandEvent&)
{
   CopyFilesToClip(true);
}

void MakePatchDialog::OnMenuCopyDirs(wxCommandEvent&)
{
   CopyFilesToClip(false);
}

int wxCALLBACK MakePatchDialog::CompareFunc(long item1, long item2, long data)
{
   MakePatchDialog::ItemData* itemdata1 = reinterpret_cast<MakePatchDialog::ItemData*>(item1);
   MakePatchDialog::ItemData* itemdata2 = reinterpret_cast<MakePatchDialog::ItemData*>(item2);
   SortData *sortData = (SortData *) data;
   int result = 0;
   if (sortData->column == 0)
   {
      result = lstrcmpiA(itemdata1->m_Filename.c_str(), itemdata2->m_Filename.c_str());
   }
   else if (sortData->column == 1)
   {
      result = lstrcmpi(itemdata1->m_Filetype.c_str(),
                         itemdata2->m_Filetype.c_str());
   }
   else if (sortData->column == 2)
   {
      result = lstrcmpi(CVSStatus::FileStatusString(itemdata1->m_Status).c_str(),
                         CVSStatus::FileStatusString(itemdata2->m_Status).c_str());
   }

   if (!sortData->asc)
   {
      result *= -1;
   }

   return result;
}

// Get file type
wxString MakePatchDialog::GetFileType(const char* fn)
{
   SHFILEINFO sfi;
   std::string filename(fn);
   SHGetFileInfo(wxTextCStr(filename), FILE_ATTRIBUTE_NORMAL, &sfi, 
                 sizeof(sfi), SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES);
   wxString result;
   if (sfi.szTypeName[0])
      result = sfi.szTypeName;
   else
   {
      std::string s = GetExtension(filename);
      MakeUpperCase(s);
      if (s.length() > 0)
      {
         result = Printf(_("%s File"), wxText(s).c_str());
      }
      else
      {
         result = _("File");
      }
   }
   return result;
}

void MakePatchDialog::OnAddFiles(wxCommandEvent&)
{
    std::vector<std::string> files;
    std::map<std::string, bool> selected;
    {
        wxBusyCursor busyCursor;
        std::vector<std::string> allFiles;
        // Generate list of files and folders
        for (unsigned int i = 0; i < myDirectoryGroups.size(); i++)
        {
            DirectoryGroup& group = myDirectoryGroups[i];
            for (unsigned int j = 0; j < group.size(); j++)
                allFiles.push_back(group[j].myAbsoluteFile);
        }
        GetAddableFiles(myDirectoryGroups, false, allFiles, files, selected);
    }
    if (files.empty())
    {
       bool includeIgnored = myDirectoryGroups.GetBooleanPreference("Add Ignored Files");
       if (includeIgnored)
           DoMessageDialog(0, _("There is nothing to add.\n\n\
All the files, folders and their contents that you selected are already in CVS."));
       else
           DoMessageDialog(0, _("There is nothing to add.\n\n\
All the files, folders and their contents that you selected are already in CVS, \
or are specified in a .cvsignore file."));
      return;
   }

   // Preprocess the files
   myDirectoryGroups.PreprocessFileList(files);

   // Create AddFiles data for each selected file
   for (unsigned int i = 0; i < myDirectoryGroups.size(); i++)
   {
      DirectoryGroup& group = myDirectoryGroups[i];
      for (unsigned int j = 0; j < group.size(); j++)
      {
         DirectoryGroup::AddFilesData* data = new DirectoryGroup::AddFilesData(group[j]);
         group[j].myUserData = data;
         data->mySelected = selected[group[j].myAbsoluteFile];
      }
   }
   // Show dialog
   if (DoAddFilesDialog(this,
                        myDirectoryGroups,
                        _("TortoiseCVS - Add files to patch"),
                        _("Select files to add to the patch"),
                        true))
   {
      // Extract selected files
      for (unsigned int j = 0; j < myDirectoryGroups.size(); ++j)
      {
         DirectoryGroup& group = myDirectoryGroups[j];
         
         // Remove deselected entries
         std::vector<DirectoryGroup::Entry*> entries;
         DirectoryGroup::Entry* entry;
         DirectoryGroup::AddFilesData* addFilesData;
         std::vector<DirectoryGroup::Entry*>::iterator itEntries = group.myEntries.begin();
         entries.reserve(group.myEntries.size());
         // Copy all selected entries
         while (itEntries != group.myEntries.end())
         {
            entry = *itEntries;
            addFilesData = (DirectoryGroup::AddFilesData*) entry->myUserData;
            if (addFilesData->mySelected)
            {
               int i = myFiles->GetItemCount();
               myFiles->InsertItem(i, wxText(entry->myRelativeFile));
               myFiles->SetChecked(i, true);
               ItemData* itemdata = new ItemData(true);
               itemdata->m_Filename = entry->myAbsoluteFile;
               itemdata->m_Status = CVSStatus::STATUS_ADDED;
               myFiles->SetItemData(i, (long) itemdata);
               itemdata->m_Status = CVSStatus::GetFileStatus(itemdata->m_Filename);
               itemdata->m_Filetype = GetFileType(itemdata->m_Filename.c_str());
               myFiles->SetItem(i, 1, itemdata->m_Filetype.c_str());
               myFiles->SetItem(i, 2, CVSStatus::FileStatusString(itemdata->m_Status).c_str());
            }
            ++itEntries;
         }
      }
   }
}
