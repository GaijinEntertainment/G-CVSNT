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

#ifndef COMMIT_DIALOG_H
#define COMMIT_DIALOG_H

#include <string>
#include <vector>
#include <wx/wx.h>
#include <CVSGlue/CVSStatus.h>
#include "ExtDialog.h"

class ExtSplitterWindow;
class ExtListCtrl;
class ExtTextCtrl;
class MyKeyHandler;
class wxListEvent;
class DirectoryGroups;


class CommitDialog : ExtDialog
{
public:
    typedef std::vector< std::pair<std::string, CVSStatus::FileStatus> > SelectedFiles;

    friend bool DoCommitDialog(wxWindow* parent, 
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
                               SelectedFiles& userSelection,
                               SelectedFiles& userSelectionStatic);

private:
    struct ItemData 
    {
        std::string fullName;
        std::string displayedName;
        wxString filetype;
        std::string bugnumber;
        CVSStatus::FileStatus status;
        CVSStatus::FileFormat format;
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
        ListCtrlEventHandler(CommitDialog* parent)
            : myParent(parent)
        {
        }

        void OnContextMenu(wxContextMenuEvent&);

        void ListColumnClick(wxListEvent& e);

        void OnDblClick(wxListEvent& event);

    private:
        CommitDialog* myParent;

        DECLARE_EVENT_TABLE()
    };

   
    CommitDialog(wxWindow* parent, 
                 const DirectoryGroups& dirGroups,
                 const FilesWithBugnumbers* modified, 
                 const FilesWithBugnumbers* modifiedStatic, 
                 const FilesWithBugnumbers* added,
                 const FilesWithBugnumbers* removed,
                 const FilesWithBugnumbers* renamed,
                 const std::string& defaultComment,
                 const std::string& defaultBugNumber);
    ~CommitDialog();

    void OnContextMenu(wxContextMenuEvent& event);

    void OnMenuWebLog(wxCommandEvent& event);
    void OnMenuHistory(wxCommandEvent& event);
    void OnMenuGraph(wxCommandEvent& event);
    void OnMenuDiff(wxCommandEvent& e);
    void OnMenuCheck(wxCommandEvent& e);
    void OnMenuUncheck(wxCommandEvent& e);
    void OnMenuCopyNames(wxCommandEvent& e);
    void OnMenuCopyDirs(wxCommandEvent& e);
    void OnMenuCommit(wxCommandEvent&);
    void OnMenuCleanCopy(wxCommandEvent&);
    void OnMenuRestore(wxCommandEvent& e);
    void OnOk(wxCommandEvent& e);
    void UseBugClick(wxCommandEvent& e);
    void MarkBugClick(wxCommandEvent& e);
    void WrapTextClick(wxCommandEvent& e);
    void BugNumberTextUpdated(wxCommandEvent&);

    void RunCommandOnSelectedItems(std::string verb, bool isCommit);
   
    void CommentHistoryClick(wxCommandEvent& event);
    void CopyFilesToClip(bool names);
   
    void PrepareItemData(const FilesWithBugnumbers* modified,
                         const FilesWithBugnumbers* modifiedStatic,
                         const FilesWithBugnumbers* added,
                         const FilesWithBugnumbers* removed,
                         const FilesWithBugnumbers* renamed);
    
    // Add files to ExtListCtrl
    void AddFiles(const std::string& bugnumber);

    static int wxCALLBACK CompareFunc(long item1, long item2, long data);

    // Get file type
    static wxString GetFileType(const char* filename);

    void RemoveItem(int index);
    
    ExtListCtrl*                 myFiles;
    ExtTextCtrl*                 myComment;
    ExtTextCtrl*                 myBugNumber;
    wxComboBox*                  myCommentHistory;
    wxButton*                    myOK;
    wxStaticText*                myCommentWarning;
    std::string                  myStub;
    std::vector<std::string>     myComments;
    ExtSplitterWindow*           mySplitter;
    wxCheckBox*                  myWrapCheckBox;
    wxCheckBox*                  myUseBugCheckBox;
    wxCheckBox*                  myMarkBugCheckBox;
    MyKeyHandler*                myKeyHandler;
    SortData                     mySortData;
    wxStatusBar*                 myStatusBar;
    const DirectoryGroups&       myDirGroups;

    std::vector<ItemData*>       myItemData;

    bool                         myReady;
    
    friend class ListCtrlEventHandler;
    friend class MyKeyHandler;

    DECLARE_EVENT_TABLE()
};

#endif
