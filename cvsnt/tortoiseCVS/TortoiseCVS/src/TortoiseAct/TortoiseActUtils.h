// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - September 2000

// Copyright (C) 2004 - Torsten Martinsen
// <torsten@tiscali.dk> - April 2004

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

#ifndef TORTOISEACTUTILS_H
#define TORTOISEACTUTILS_H

#include <string>
#include <set>
#include <DialogsWxw/CommitDialog.h>


class DirectoryGroups;


// Get external application for specified application (Diff/Merge).
// If 'forceQuery' is true, always ask user.
std::string GetExternalApplication(const char* appType,
                                   const DirectoryGroups& dirGroups,
                                   bool forceQuery);

// Run external diff 
bool RunExternalDiff(std::string filename1,
                     std::string filename2,
                     DirectoryGroups& dirGroups,
                     std::string filename3 = "");

// Run external merge
bool RunExternalMerge(std::string fileMine,
                      std::string fileYours,
                      DirectoryGroups& dirGroups,
                      std::string fileOlder = "");

class DirectoryGroups;

// Perform a "CVS Diff"
bool DoDiff(DirectoryGroups& dirGroups,
            std::string rev1,
            std::string rev2,
            bool forceQuery);

// View a revision
bool DoView(DirectoryGroups& dirGroups, std::string rev);

// Calculate candidates for adding to CVS
void GetAddableFiles(const DirectoryGroups& dirGroups,
                     bool includeFolders,
                     const std::vector<std::string>& allFiles,
                     std::vector<std::string>& files,
                     std::map<std::string, bool>& selected);

// Does list1 include all elements in list2?
bool Includes(const CommitDialog::SelectedFiles& list1, const std::vector<std::string>& list2);

// Does list1 include all elements in list2?
bool Includes(const std::vector<std::string>& list1, const std::vector<std::string>& list2);

std::set<std::string> GetTopLevelDirs(const std::vector<std::string>& files);

bool ComputeCommitSelection(const CommitDialog::SelectedFiles& userSelection,
                            const FilesWithBugnumbers* modified,
                            const FilesWithBugnumbers* added,
                            const FilesWithBugnumbers* removed,
                            const FilesWithBugnumbers* renamed,
                            std::vector<std::string>& result);

std::string GetPreferenceKey(const std::string& dir);

#endif
