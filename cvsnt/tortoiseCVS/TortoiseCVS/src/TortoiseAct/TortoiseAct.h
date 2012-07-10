// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - September 2000

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

#ifndef TORTOISEACT_H
#define TORTOISEACT_H

#include <string>
#include <vector>
#include <list>
#include <windows.h> // For HWND type only
#include <Utils/StringUtils.h>
#include <CVSGlue/CVSAction.h>
#include "TortoiseActVerbs.h"
#include "TortoiseParams.h"
#include "DirectoryGroup.h"



class CLogNode;


class TortoiseAct
{
public:
    TortoiseAct();
   
    // Returns true if args seem valid
    bool SussArgs(int ac, char* av[]);
   
    // Carries out the command
    void PerformAction();
    HWND myRemoteHandle;

    bool GetBooleanPreference(const std::string& name);

    std::string GetStringPreference(const std::string& name);

    int GetIntegerPreference(const std::string& name);

private:
   void PerformAddMenu();
   void AddFiles(bool honourAutoCommit);
   void PerformAddMenuRecursive();
   void PerformCheckoutMenu();
   void PerformTortoiseCheckout();
   void PerformMakeModuleMenu();
   void PerformCommand();
   // Return true if any committable files found
   bool PerformCommit(bool messageIfNoAction);
   void PerformDiffMenu();
   void PerformMakePatchMenu();
   void PerformApplyPatchMenu();
   void PerformLogMenu();
   void PerformReleaseMenu();
   void PerformRemoveMenu();
   void PerformRenameMenu();
   void PerformRestoreMenu();
   void PerformSwitchMenu();
   void PerformUpdateMenu(bool dialog = false);
   void PerformUpdateDialogMenu();
   void PerformCleanUpdate();
   void PerformWebLogMenu();
   void PerformTagMenu();
   void PerformBranchMenu();
   void PerformMergeMenu();
   void PerformHelp();
   void PerformPrefs();
   void PerformEdit(bool exclusive);
   void PerformUnedit();
   void PerformListEditors();
   void PerformGet();
   void PerformHistory();
   void PerformAnnotate();
   void PerformRevisionGraph();
   void PerformAbout();
   void PerformAddIgnore(bool useextension = false);
   void PerformResolveConflicts();
   void PerformMergeConflicts();
   void PerformUrl();
   void PerformRebuildIcons();
   void PerformSaveAs();
   void PerformRefreshStatus();
   void PerformViewMenu();

   void HandleURL(std::string url);
   
   bool PreprocessFileList(bool groupByTopDir = false);
   bool VerbNeedsFiles();
   bool VerbNeedsPreprocessing(bool& groupByTopDir);
   void RefreshExplorerWindow(const std::vector<std::string>* dirs = 0);
   void RefreshExplorerWindow(const std::string& dir);
   bool FileSelected();
   std::vector<std::string> GetFetchButtonDirs();
   std::string GetFetchButtonTag();
   
   void AddToIgnoreList(const std::string& directory, const std::string& file);

   // Encode diff files
   bool EncodeDiffFiles(const std::string& sFilename1, const std::string& sFilename2,
                        bool& bUseUtf8);

   // Decode diff files
   void DecodeDiffFiles(const std::string& sFilename1, const std::string& sFilename2,
                        bool bUseUtf8);
   
   bool ParseConflicts(const DirectoryGroup& group,
                       const std::list<std::string>& cvsOutput, 
                       std::vector<std::string>& conflictFiles);
   
   bool ParseCommitted(const DirectoryGroup& group,
                       const std::list<std::string>& cvsOutput, 
                       std::vector<std::string>& committedFiles);
   
   bool ParseIdentical(const DirectoryGroup& group,
                       const std::list<std::string>& cvsOutput, 
                       std::vector<std::string>& identicalFiles);

   void DoHistoryGraph(void* dlg, bool graph);
   void DoAnnotate(void* dlg);
   
   bool EditConflictFile(const std::string& filename);

   // Do committing
   bool DoCommit(const FilesWithBugnumbers* modified,
                 const FilesWithBugnumbers* modifiedStatic,
                 const FilesWithBugnumbers* added, 
                 const FilesWithBugnumbers* removed,
                 const FilesWithBugnumbers* renamed,
                 CVSAction*& glue,
                 bool* isWatchOn = 0);

    // Commit a single directory
    bool DoCommitDir(DirectoryGroup& group, CVSAction* pglue,
                     const std::string& comment,
                     bool usebug, bool markbug, const std::string& bugnumber,
                     bool recurse = true,
                     bool staticFiles = false);
    
   bool DoAutoUnedit(CVSAction* pglue);

   // Return list of processing directories
   wxString GetDirList();

   // Calculate candidates for adding to CVS
   void GetAddableFiles(const DirectoryGroups& dirGroups, bool includeFolders);

    
   // These are the full list of parameters passed in
   cvsCommandVerb               myVerb;
   std::string                  myVerbString;
   std::vector<std::string>     myAllFiles;
   TortoiseParams               myParameters;
   
   // These are grouped by directory
   DirectoryGroups              myDirectoryGroups;
};
#endif
