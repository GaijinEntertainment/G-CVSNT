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

#ifndef MAKE_PATCH_DIALOG_H
#define MAKE_PATCH_DIALOG_H

#include <string>
#include <vector>
#include <wx/wx.h>
#include "../CVSGlue/CVSStatus.h"
#include "ExtDialog.h"

class ExtTextCtrl;
class ExtListCtrl;
class MyKeyHandler;
class ExtSplitterWindow;
class wxListEvent;
class DirectoryGroups;

class MakePatchDialog : ExtDialog
{
public:
   friend bool DoMakePatchDialog(wxWindow* parent,
                                 DirectoryGroups& groups,
                                 const FilesWithBugnumbers* modified,
                                 const FilesWithBugnumbers* added,
                                 const FilesWithBugnumbers* removed,
                                 std::vector<std::string>& userSelection,
                                 std::vector<std::string>& additionalFiles);

private:
   class ItemData 
   {
   public:
      ItemData(bool additionalFile = false)
         : m_Status(CVSStatus::STATUS_NOWHERENEAR_CVS),
           m_AdditionalFile(additionalFile)
      {
      }
      
      std::string               m_Filename;
      wxString                  m_Filetype;
      CVSStatus::FileStatus     m_Status;
      std::string               m_Bugnumber;
      bool                      m_AdditionalFile;
   };

   class SortData
   {
   public:
      int column;
      bool asc;
   };

   class ListCtrlEventHandler : public wxEvtHandler
   {
   public:
      ListCtrlEventHandler(MakePatchDialog* parent)
         : myParent(parent)
      {
      }

      void OnContextMenu(wxContextMenuEvent&);

      void ListColumnClick(wxListEvent& e);

      void OnDblClick(wxListEvent& event);

   private:
      MakePatchDialog* myParent;

      DECLARE_EVENT_TABLE()
   };

   MakePatchDialog(wxWindow* parent,
                   DirectoryGroups& groups,
                   const FilesWithBugnumbers* modified, 
                   const FilesWithBugnumbers* added,
                   const FilesWithBugnumbers* removed,
                   std::vector<std::string>& additionalFiles);
   ~MakePatchDialog();

   void OnAddFiles(wxCommandEvent&);
   void OnContextMenu(wxContextMenuEvent& event);

   void OnMenuDiff(wxCommandEvent& e);
   void OnMenuCheck(wxCommandEvent& e);
   void OnMenuUncheck(wxCommandEvent& e);
   void OnMenuCopyNames(wxCommandEvent& e);
   void OnMenuCopyDirs(wxCommandEvent& e);

   void CopyFilesToClip(bool names);
   
   // Add files to ExtListCtrl
   void AddFiles(const std::vector<std::string>& filenames,
                 const std::vector<ItemData*>& itemData);

   static int wxCALLBACK CompareFunc(long item1, long item2, long data);

   // Get file type
   wxString GetFileType(const char* filename);
   
   ExtListCtrl*                 myFiles;
   wxButton*                    myAddButton;
   wxButton*                    myOK;
   std::string                  myStub;
   SortData                     mySortData;
   wxStatusBar*                 myStatusBar;

   DirectoryGroups&             myDirectoryGroups;

   friend class ListCtrlEventHandler;

   DECLARE_EVENT_TABLE()
};

#endif
