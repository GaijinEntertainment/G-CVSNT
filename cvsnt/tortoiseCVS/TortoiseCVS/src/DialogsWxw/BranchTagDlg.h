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

#ifndef BRANCHTAGDLG_H
#define BRANCHTAGDLG_H

#include <string>
#include <vector>
#include <wx/wx.h>
#include "ExtDialog.h"

class BranchTagDlg : private ExtDialog
{
public:
   enum
   {
      TagActionCreate,
      TagActionMove,
      TagActionDelete
   };

   friend bool DoBranchDialog(wxWindow* parent, std::string& branchname,
                              bool& checkunmodified,
                              int& action,
                              const std::vector<std::string>& dirs);
   friend bool DoTagDialog(wxWindow* parent, std::string& tagname,
                           bool& checkunmodified,
                           int& action,
                           const std::vector<std::string>& dirs);

private:
   enum { modeTag, modeBranch };
   
   BranchTagDlg(wxWindow* parent,int mode, const std::string& tagname, 
      const std::vector<std::string>& dirs);
   void CreateClicked(wxCommandEvent& event);
   void MoveClicked(wxCommandEvent& event);
   void DeleteClicked(wxCommandEvent& event);
   void FetchTagBranchList(wxCommandEvent& event);
   void FetchCachedTagBranchList();

   wxCheckBox*    myCheckUnmodifiedButton;
   wxRadioButton* myRadioCreate;
   wxRadioButton* myRadioMove;
   wxRadioButton* myRadioDelete;
   wxButton*      myFetchTagBranchList;
   wxCheckBox*    myFetchRecursive;
   
   wxComboBox*    myTagEntry;

   int            myAction;
   int            myMode;
   const std::vector<std::string>& myDirs;

   DECLARE_EVENT_TABLE()
};

#endif

