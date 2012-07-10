// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2002 - Francis Irving
// <francis@flourish.org> - June 2002

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

#ifndef REMOTE_LISTS_H
#define REMOTE_LISTS_H

#include <string>
#include <vector>
#include <wx/wx.h>
#include "CVSAction.h"

class CVSRoot;

namespace RemoteLists
{
    void GetEntriesList(wxWindow *parent, EntnodeMap& entries, const CVSRoot& cvsroot, const std::string& ofwhat = "");
    void GetModuleList(wxWindow *parent,
                       std::vector<std::string>& modules,
                       const CVSRoot& cvsroot,
                       const std::string& ofwhat = "/");
   void GetCachedModuleList(std::vector<std::string>& modules, const CVSRoot& cvsroot);
   void ParseEntries(const std::list<std::string>& text, EntnodeMap& entries, const std::string& fullpath, bool bIsLs);

   // Parse text for module names
   void ParseModuleNames(const std::list<std::string>& text, std::vector<std::string>& modules, bool bIsLs);

   // Get list of tags and branches
   void GetTagBranchList(wxWindow* parent, std::vector<std::string>&tags, 
                         std::vector<std::string>& branches, 
                         const std::vector<std::string>& dirs, bool recursive);
   
   // Get cached list of branches and tags for cvsroots and modules
   void GetCachedTagBranchList(std::vector<std::string>&tags, std::vector<std::string>& branches, 
                               const std::string& cvsroot, const std::string& module);
   
   // Get cached list of branches and tags for directories
   void GetCachedTagBranchList(std::vector<std::string>& tags, 
                               std::vector<std::string>& branches, const std::vector<std::string>& dirs);
   
   // Get tags using "cvs rlog"
   void GetTagBranchListRlog(wxWindow* parent, std::vector<std::string>& tags, 
                             std::vector<std::string>& branches, 
                             const std::vector<std::string>& dirs, const std::string& cvsRoot, 
                             const std::string& module, bool recursive, CVSAction *glue = 0);
};

#endif
