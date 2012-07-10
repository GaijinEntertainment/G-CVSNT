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

#ifndef EDITOR_LIST_DIALOG_H
#define EDITOR_LIST_DIALOG_H

#include <string>
#include <vector>
#include <wx/wx.h>
#include "ExtDialog.h"


class wxListEvent;
class EditorListListCtrl;


class EditorListDialog : ExtDialog
{
public:
   struct EditedFile 
   {
   public:
      struct Editor
      {
         Editor(const std::string& author,
                time_t date,
                const std::string& host,
                const std::string& path,
                const std::string& bugNumber)
            : myAuthor(author),
              myDate(date),
              myHost(host),
              myPath(path),
              myBugNumber(bugNumber)
         {
         }
         
         std::string    myAuthor;
         time_t         myDate;
         std::string    myHost;
         std::string    myPath;
         std::string    myBugNumber;
      };
      
      EditedFile(const std::string& filename, const std::vector<Editor>& editors)
         : myFilename(filename),
           myEditors(editors)
      {
      }

      std::string               myFilename;
      std::vector<Editor>       myEditors;
   };

   typedef std::vector<EditedFile> EditedFileList;
   
   friend bool DoEditorListDialog(wxWindow* parent, 
                                  EditedFileList& editors);
  
private:
   enum SortColumn
   {
      SORT_FILE,
      SORT_AUTHOR,
      SORT_DATE,
      SORT_HOST,
      SORT_PATH,
      SORT_BUGNUMBER
   };

   EditorListDialog(wxWindow* parent, EditedFileList& editors);
   ~EditorListDialog();

   void OnContextMenu(wxListEvent& event);
   void OnDblClick(wxListEvent& event);

   void ListColumnClick(wxListEvent& e);

   // Add list of edited files to ExtListCtrl
   void AddEditors(const std::vector<std::string>& filenames,
                   const EditedFileList& editors);
   
   static int wxCALLBACK CompareFunc(long item1, long item2, long sortData);
   
   EditorListListCtrl*          myEditors;
   wxButton*                    myOK;
   std::string                  myStub;
   wxStatusBar*                 myStatusBar;
   wxColour                     myEditColour;
   SortColumn                   mySortCol;
   bool                         mySortAscending;
   
   static const EditedFileList* myEditedFiles;

   friend class EditorListListCtrl;
   DECLARE_EVENT_TABLE()
};

#endif
