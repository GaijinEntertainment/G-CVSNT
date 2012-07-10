// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2004 - Torsten Martinsen
// <torsten@tiscali.dk> - August 2004

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
#include "EditorListDialog.h"
#include "EditorListListCtrl.h"
#include "WxwHelpers.h"
#include "WxwMain.h"
#include "FilenameText.h"
#include "ExtStaticText.h"
#include "ExplorerMenu.h"
#include <Utils/ProcessUtils.h>
#include <Utils/TimeUtils.h>
#include <Utils/TortoiseDebug.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/TortoiseRegistry.h>
#include <Utils/Translate.h>
#include <Utils/Preference.h>


const EditorListDialog::EditedFileList* EditorListDialog::myEditedFiles = 0;


//static
bool DoEditorListDialog(wxWindow* parent, 
                        std::vector<EditorListDialog::EditedFile>& editors)
{
   EditorListDialog* dlg = new EditorListDialog(parent, editors);
   bool ret = (dlg->ShowModal() == wxID_OK);
   
   DestroyDialog(dlg);
   return ret;
}




enum
{
   EDITORLISTDLG_ID_EDITORS = 10001
};

BEGIN_EVENT_TABLE(EditorListDialog, ExtDialog)
   EVT_LIST_COL_CLICK(EDITORLISTDLG_ID_EDITORS, EditorListDialog::ListColumnClick)
END_EVENT_TABLE()

EditorListDialog::EditorListDialog(wxWindow* parent, 
                                   EditorListDialog::EditedFileList& editors)
   : ExtDialog(parent, -1, _("TortoiseCVS - List Editors"),
               wxDefaultPosition, wxDefaultSize,
               wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxRESIZE_BORDER | 
               wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN),
     mySortCol(SORT_FILE),
     mySortAscending(true)   
{
   TDEBUG_ENTER("EditorListDialog::EditorListDialog");
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

   myEditedFiles = &editors;
   
   std::vector<std::string> tmp;
   EditedFileList::iterator it;

   // Add editors
   it = editors.begin();
   while (it != editors.end())
   {
      std::string s(it->myFilename);
      FindAndReplace<std::string>(s, "/", "\\");
      it->myFilename = s;
      tmp.push_back(s);
      it++;
   }

   // Trim paths to sensible length
   ShortenPaths(tmp, myStub);

   // Maybe use dedicated colour later
   myEditColour = ColorRefToWxColour(GetIntegerPreference("Colour Updated Files"));

   FilenameText* label1 = new FilenameText(this, -1, _("Folder: %s"), wxText(RemoveTrailingDelimiter(myStub)));
   label1->SetWindowStyle(label1->GetWindowStyle() | wxST_NO_AUTORESIZE);
   myEditors = new EditorListListCtrl(this, this, EDITORLISTDLG_ID_EDITORS, myEditColour);
   myEditors->SetSortIndicator(mySortCol, mySortAscending);
   AddEditors(tmp, editors);
   myEditors->SetBestColumnWidth(0);
   myEditors->SetBestColumnWidth(1);
   myEditors->SetBestSize(wxDLG_UNIT(this, wxSize(150, 150)), wxDefaultSize);

   // OK button
   wxBoxSizer* sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   myOK = new wxButton(this, wxID_OK, _("Close"));
   myOK->SetDefault();
   sizerConfirm->Add(myOK, 0, wxGROW | wxALL, 5);

   // Status bar
   myStatusBar = new wxStatusBar(this, -1);
   myStatusBar->SetStatusText(Printf(_("%d edited file(s)"), editors.size()).c_str());

   // Main box with everything in it
   wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(label1, 0, wxGROW | wxALL, 3);
   sizerTop->Add(myEditors, 2, wxGROW | wxALL, 3);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, 10);
   sizerTop->Add(myStatusBar, 0, wxGROW | wxALL, 0);

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);

   RestoreTortoiseDialogSize(this, "ListEditors");
   SetTortoiseDialogPos(this, GetRemoteHandle());
   RestoreTortoiseDialogState(this, "ListEditors");
}


EditorListDialog::~EditorListDialog()
{
   StoreTortoiseDialogSize(this, "ListEditors");
}


// Add files to EditorListListCtrl
void EditorListDialog::AddEditors(const std::vector<std::string>& filenames,
                                  const EditorListDialog::EditedFileList& editors)
{
   TDEBUG_ENTER("EditorListDialog::AddEditors");
   
   int index = 0;
   for (size_t i = 0; i < filenames.size(); ++i)
   {
      const std::vector<EditedFile::Editor>& Editors = editors[i].myEditors;
      for (size_t j = 0; j < Editors.size(); ++j)
      {
         myEditors->InsertItem(index, wxText(j ? "" : filenames[i]));
         myEditors->SetItem(index, 1, wxText(Editors[j].myAuthor));
         struct tm* tm = gmtime(&(Editors[j].myDate));
         wxChar date[30];
         tm_to_local_DateTimeFormatted(tm, date, sizeof(date), false, true);
         myEditors->SetItem(index, 2, date);
         myEditors->SetItem(index, 3, wxText(Editors[j].myHost));
         myEditors->SetItem(index, 4, wxText(Editors[j].myPath));
         myEditors->SetItem(index, 5, wxText(Editors[j].myBugNumber));
         myEditors->SetItemData(index, static_cast<long>(i));
         ++index;
      }
   }
}



void EditorListDialog::ListColumnClick(wxListEvent& e)
{
   SortColumn col = SORT_FILE;
   switch (e.GetColumn())
   {
   case 0:
      col = SORT_FILE;
      break;
   case 1:
      col = SORT_AUTHOR;
      break;
   case 2:
      col = SORT_DATE;
      break;
   case 3:
      col = SORT_HOST;
      break;
   case 4:
      col = SORT_PATH;
      break;
   case 5:
      col = SORT_BUGNUMBER;
      break;
   default:
      ASSERT(false);
   }

   int column = e.GetColumn();
   if (column == mySortCol) 
      mySortAscending = !mySortAscending;
   else
   {
      mySortCol = (SortColumn )column;
      mySortAscending = true;
   }
   myEditors->SetSortIndicator(e.GetColumn(), mySortAscending);
   myEditors->SortItems(CompareFunc, e.GetColumn() + (mySortAscending << 8));
}



int wxCALLBACK EditorListDialog::CompareFunc(long item1, long item2, long sortData)
{
   // Check "ascending" flag
   if (!(sortData & 256))
      std::swap(item1, item2);
   int col = sortData & 255;
   //!!
   int subitem = 0;
   switch (col)
   {
   case 0:
      // File
      return strcmpi((*myEditedFiles)[item1].myFilename.c_str(), (*myEditedFiles)[item2].myFilename.c_str());

   case 1:
      // Author
      return strcmpi((*myEditedFiles)[item1].myEditors[subitem].myAuthor.c_str(),
                     (*myEditedFiles)[item2].myEditors[subitem].myAuthor.c_str());

   case 2:
      // Date
      return (*myEditedFiles)[item1].myEditors[subitem].myDate - (*myEditedFiles)[item2].myEditors[subitem].myDate;
      break;

   case 3:
      // Host
      return strcmpi((*myEditedFiles)[item1].myEditors[subitem].myHost.c_str(),
                     (*myEditedFiles)[item2].myEditors[subitem].myHost.c_str());

   case 4:
      // Path
      return strcmpi((*myEditedFiles)[item1].myEditors[subitem].myPath.c_str(),
                     (*myEditedFiles)[item2].myEditors[subitem].myPath.c_str());

   case 5:
      // Bug number
      return strcmp((*myEditedFiles)[item1].myEditors[subitem].myBugNumber.c_str(),
                    (*myEditedFiles)[item2].myEditors[subitem].myBugNumber.c_str());

   default:
      ASSERT(false);
      break;
   }
   return 0;
}
