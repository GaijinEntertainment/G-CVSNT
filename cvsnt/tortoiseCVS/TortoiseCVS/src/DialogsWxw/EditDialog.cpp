// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Ben Campbell, 2005 Arthur Barrett
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


#include "StdAfx.h"
#include "EditDialog.h"
#include "WxwHelpers.h"
#include "WxwMain.h"
#include <wx/clipbrd.h>
#include <wx/filename.h>
#include "FilenameText.h"
#include "ExtTextCtrl.h"
#include "ExtSplitterWindow.h"
#include "../Utils/PathUtils.h"
#include "../Utils/TortoiseDebug.h"
#include "../Utils/TortoiseUtils.h"
#include "../Utils/TortoiseRegistry.h"
#include "../Utils/ShellUtils.h"
#include "../Utils/StringUtils.h"
#include "../Utils/Translate.h"
#include "../TortoiseAct/TortoiseActVerbs.h"
#include "../Utils/FileTree.h"
#include "YesNoDialog.h"

class MyUserData : public FileTree::UserData 
{
public:
};


class MyEditKeyHandler : public ExtTextCtrl::KeyHandler
{
public:
   MyEditKeyHandler(EditDialog* parent);
   virtual ~MyEditKeyHandler();
   bool OnArrowUpDown(bool up);
   bool OnSpace();
   
private:
   size_t      myIndex;
   EditDialog* myParent;
};


// static
bool DoEditDialog(wxWindow* parent, 
                  std::string& comment,
                  std::string& bugnumber,
                  bool& usebug)
{
   EditDialog* dlg = new EditDialog(parent, comment);
   bool ret = (dlg->ShowModal() == wxID_OK);
   
   if (ret)
   {
      comment = TrimRight(wxAscii(dlg->myComment->GetValue().c_str()));
      bugnumber = TrimRight(wxAscii(dlg->myBugNumber->GetValue().c_str()));
      usebug = dlg->myUseBugCheckBox->GetValue();
      TortoiseRegistry::WriteBoolean("Dialogs\\Commit\\Word Wrap", dlg->myWrapCheckBox->GetValue());
      TortoiseRegistry::WriteBoolean("Dialogs\\Commit\\Use Bug", usebug);
      TortoiseRegistry::WriteString("Dialogs\\Commit\\Bug Number", bugnumber);
   }
   
   DestroyDialog(dlg);
   return ret;
}



EditDialog::EditDialog(wxWindow* parent, 
                       const std::string& defaultComment)
   : ExtDialog(parent, -1, _("TortoiseCVS - Edit"),
               wxDefaultPosition, wxDefaultSize,
               wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxRESIZE_BORDER | 
               wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN)
{
   TDEBUG_ENTER("EditDialog::EditDialog");
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

   wxPanel* headerPanel = new wxPanel(this, -1);

   // Comment history
   wxString s(_("Comment History"));
   s += _(":");
   wxStaticText* labelCH = new wxStaticText(headerPanel, -1, s);
   myCommentHistory = MakeComboList(headerPanel, -1, "History\\Comments", "",
                                    COMBO_FLAG_READONLY | COMBO_FLAG_ADDEMPTY, &myComments);
   myCommentHistory->SetToolTip(_("Previous comments (use Ctrl-Up and Ctrl-Down in the Comment field to select)"));

   // Wrap Text checkbox
   myWrapCheckBox = new wxCheckBox(headerPanel, -1, _("&Wrap lines"));
   myWrapCheckBox->SetValue(TortoiseRegistry::ReadBoolean("Dialogs\\Commit\\Word Wrap", false));

   // Comment
   wxStaticText* label0 = new wxStaticText(this, -1, wxString(_("Comment")) + _(":"));
   myComment = new ExtTextCtrl(this, -1, wxT(""), wxDefaultPosition,
                               wxDefaultSize, wxTE_MULTILINE | wxTE_RICH2);
   myComment->SetPlainText(true);
   myComment->SetMaxLength(20000);
   myComment->SetFocus();
   myComment->SetSelection(-1, -1);

   if (!defaultComment.empty())
   {
      myComment->SetValue(wxText(defaultComment));
      myCommentHistory->SetSelection(-1, -1);
   }

   // BugNumber
   wxStaticText* label1 = new wxStaticText(this, -1, wxString(_("Bug &Number")) + _(":"));
   myBugNumber = new ExtTextCtrl(this, -1, wxT(""), wxDefaultPosition,
                                 wxDefaultSize, wxTE_RICH2 );
   myBugNumber->SetPlainText(true);
   myBugNumber->SetMaxLength(100);
   myBugNumber->SetFocus();
   myBugNumber->SetSelection(-1, -1);
   std::string defaultbugnumber = TortoiseRegistry::ReadString("Dialogs\\Commit\\Bug Number");
   myBugNumber->SetValue(wxText(defaultbugnumber));

   // Use Bug checkbox
   myUseBugCheckBox = new wxCheckBox(this, -1, _("&Use Bug"));
   myUseBugCheckBox->SetValue(TortoiseRegistry::ReadBoolean("Dialogs\\Commit\\Use Bug", false));
   myBugNumber->Enable(myUseBugCheckBox->GetValue());

   wxBoxSizer* bugsizer = new wxBoxSizer(wxHORIZONTAL);
   bugsizer->Add(label1, 0, 0, 0);
   bugsizer->Add(10, 0);
   bugsizer->Add(myBugNumber, 0, 0, 0);
   bugsizer->Add(50, 0, 5);
   bugsizer->Add(myUseBugCheckBox, 0, 0, 0);

   // Status bar
   myStatusBar = new wxStatusBar(this, -1);
   
   // OK/Cancel button
   wxBoxSizer* sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   myOK = new wxButton(this, wxID_OK, _("OK"));
   myOK->SetDefault();
   wxButton* cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
   sizerConfirm->Add(myOK, 0, wxGROW | wxALL, 5);
   sizerConfirm->Add(cancel, 0, wxGROW | wxALL, 5);

   wxBoxSizer* headerSizer = new wxBoxSizer(wxVERTICAL);
   headerSizer->Add(labelCH, 0, wxGROW | wxALL, 3);
   headerSizer->Add(myCommentHistory, 1, wxGROW | wxALL, 3);
   wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
   sizer->Add(label0);
   sizer->Add(50, 0, 5);
   sizer->Add(myWrapCheckBox);
   headerSizer->Add(sizer, 0, wxGROW | wxALL, 3);
   headerPanel->SetSizer(headerSizer);

   
   // Main box with everything in it
   wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(headerPanel, 0, wxGROW | wxALL, 0);
   sizerTop->Add(label0, 0, wxGROW | wxALL, 3);
   sizerTop->Add(myComment, 1, wxGROW | wxALL, 3);
   sizerTop->Add(bugsizer, 0, wxGROW | wxALL, 3);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, 10);
   sizerTop->Add(myStatusBar, 0, wxGROW | wxALL, 0);

   myEditKeyHandler = new MyEditKeyHandler(this);
   myComment->SetKeyHandler(myEditKeyHandler);

   // Wiring
   Connect(myCommentHistory->GetId(), wxEVT_COMMAND_COMBOBOX_SELECTED,
           reinterpret_cast<wxObjectEventFunction>(&EditDialog::CommentHistoryClick));
   Connect(myWrapCheckBox->GetId(), wxEVT_COMMAND_CHECKBOX_CLICKED,
           reinterpret_cast<wxObjectEventFunction>(&EditDialog::WrapTextClick));
   Connect(myUseBugCheckBox->GetId(), wxEVT_COMMAND_CHECKBOX_CLICKED,
           reinterpret_cast<wxObjectEventFunction>(&EditDialog::UseBugClick));
   Connect(wxID_OK, wxEVT_COMMAND_BUTTON_CLICKED,
           reinterpret_cast<wxObjectEventFunction>(&EditDialog::OnOk));
   
   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);

   RestoreTortoiseDialogSize(this, "Edit", wxDLG_UNIT(this, wxSize(140, 120)));
   SetTortoiseDialogPos(this, GetRemoteHandle());
   RestoreTortoiseDialogState(this, "Edit");
}


EditDialog::~EditDialog()
{
   StoreTortoiseDialogSize(this, "Edit");
}


void EditDialog::CommentHistoryClick(wxCommandEvent& WXUNUSED(event))
{
   int i = myCommentHistory->GetSelection();
   if (i == 0)
      myComment->SetValue(wxT(""));
   else if ((i > 0) && (i <= ((int) myComments.size())))
      myComment->SetValue(wxText(myComments[i - 1]));
}


void EditDialog::WrapTextClick(wxListEvent& e)
{
   // nop
}

void EditDialog::UseBugClick(wxListEvent& e)
{
   bool enabled = myUseBugCheckBox->GetValue();
   myBugNumber->Enable(enabled);
   if (enabled)
      myBugNumber->SetFocus();
}

void EditDialog::OnOk(wxCommandEvent&)
{
   if (myUseBugCheckBox->GetValue() && myBugNumber->GetValue().empty())
   {
      if (!DoYesNoDialog(this, _(
"You are attempting to edit but have no bug number. Do you wish to continue the edit without the bug number?"), false))
         return;
     else
        myUseBugCheckBox->SetValue(0);
   }
   wxDialog::EndModal(wxID_OK);
}

// MyEditKeyHandler - handles Ctrl-Up/Down in the text control

MyEditKeyHandler::MyEditKeyHandler(EditDialog* parent)
   : myIndex(0),
     myParent(parent)
{
}

MyEditKeyHandler::~MyEditKeyHandler()
{
}

bool MyEditKeyHandler::OnArrowUpDown(bool up)
{
   if (up)
   {
      // Get next log message
      if (myIndex < myParent->myCommentHistory->GetCount() - 2)
         ++myIndex;
      else
         myIndex = 0;
   }
   else
   {
      // Get previous log message
      if (myIndex)
         --myIndex;
      else
         myIndex = myParent->myCommentHistory->GetCount() - 2;
   }
   myParent->myComment->SetValue(wxText(myParent->myComments[myIndex]));
   return false;
}

bool MyEditKeyHandler::OnSpace()
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
