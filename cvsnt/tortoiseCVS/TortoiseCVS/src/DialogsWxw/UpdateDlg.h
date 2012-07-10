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

#ifndef UPDATEDLG_H
#define UPDATEDLG_H

#include <string>
#include <vector>
#include <wx/wx.h>
#include <wx/calctrl.h>
#include "ExtDialog.h"


class UpdateBasicsPage;
class UpdateOptions;


class UpdateDlg : ExtDialog
{
public:
   friend bool DoUpdateDialog(wxWindow* parent, 
                              std::string& getrevision,
                              std::string& getdate,
                              bool& clean,
                              bool& createdirs,
                              bool& simulate,
                              bool& forceHead,
                              bool& pruneEmptyDirectories,
                              bool& recurse,
                              // Set to 0 if more than one folder selected
                              std::string* directory,
                              const std::vector<std::string>& dirs,
                              const std::string& initialTag);

private:
   UpdateDlg(wxWindow* parent, const std::vector<std::string>& dirs, 
             const std::string& initialTag,
             bool& createdirs,
             bool& simulate,
             bool& forceHead,
             bool& pruneEmptyDirectories,
             bool& recurse,
             bool allowDirectory);
   ~UpdateDlg();

   std::string GetDate() const;   // "" for no date
   std::string GetRevision() const;
   bool GetClean() const;
   bool GetCreateDirs() const;
   bool GetSimulate() const;
   bool GetForceHead() const;
   std::string GetDirectory() const;
   bool GetPruneEmptyDirectories() const;
   bool GetRecurse() const;

   UpdateBasicsPage* myUpdateBasicsPage;
   UpdateOptions*    myUpdateOptions;
};

#endif
