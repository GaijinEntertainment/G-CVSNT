// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Ben Campbell, 2005 - Arthur Barrett
// <ben.campbell@creaturelabs.com> - September 2000
// <arthur.barrett@march-hare.com> - March 2005

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

#ifndef EDIT_DIALOG_H
#define EDIT_DIALOG_H

#include <string>
#include <vector>
#include <wx/wx.h>
#include "../CVSGlue/CVSStatus.h"
#include "ExplorerMenu.h"
#include "ExtDialog.h"
#include "ExtListCtrl.h"


class ExtTextCtrl;
class MyEditKeyHandler;


class EditDialog : private ExtDialog
{
public:
   friend bool DoEditDialog(wxWindow* parent, 
                            std::string& comment, 
                            std::string& bugnumber, 
                            bool& usebug);

private:
   EditDialog(wxWindow* parent, const std::string& defaultComment);
   ~EditDialog();

   void OnOk(wxCommandEvent& e);
   void WrapTextClick(wxListEvent& e);
   void UseBugClick(wxListEvent& e);

   void CommentHistoryClick(wxCommandEvent& event);

   ExtTextCtrl*                 myComment;
   ExtTextCtrl*                 myBugNumber;
   wxComboBox*                  myCommentHistory;
   wxButton*                    myOK;
   wxStaticText*                myCommentWarning;
   std::string                  myStub;
   std::vector<std::string>     myComments;
   wxCheckBox*                  myWrapCheckBox;
   wxCheckBox*                  myUseBugCheckBox;
   MyEditKeyHandler*            myEditKeyHandler;
   wxStatusBar*                 myStatusBar;

   friend class MyEditKeyHandler;
};

#endif
