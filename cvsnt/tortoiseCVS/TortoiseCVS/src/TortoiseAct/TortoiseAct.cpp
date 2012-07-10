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



// The TortoiseAct program takes as parameters a command name (myVerb)
// and a list of files.  It then performs that TortoiseCVS action, including 
// any user interface necessary.

// The parameters can be passed by file rather than in command line
// arguments, with the command line pointing to the file. TortoiseShell dll
// calls TortoiseAct everytime a context menu is selected.

// The TortoiseAct class contains the guts of the program.

// The actual application starts in DialogsWxw/WxwMain, which simply 
// instantiates this class (TortoiseAct) and calls some simple functions in it.

#include "StdAfx.h"
#include <string>
#include <windows.h>
#include <fstream>
#include <sstream>
#include <set>
#include <algorithm>
#include <iterator>
#include <lmcons.h> // get user name

#include <io.h>
#include <fcntl.h>
#include "TortoiseActVerbs.h"
#include "TortoiseAct.h"
#include "TortoiseActUtils.h"
// Dialogs
#include <DialogsWxw/AboutDialog.h>
#include <DialogsWxw/AddFilesDialog.h>
#include <DialogsWxw/AnnotateDialog.h>
#include <DialogsWxw/BranchTagDlg.h>
#include <DialogsWxw/CheckQueryDlg.h>
#include <DialogsWxw/CheckoutDialog.h>
#include <DialogsWxw/ChooseFile.h>
#include <DialogsWxw/CommandDialog.h>
#include <DialogsWxw/CommitDialog.h>
#include <DialogsWxw/ConflictDialog.h>
#include <DialogsWxw/ConflictListDialog.h>
#include <DialogsWxw/CreditsDialog.h>
#include <DialogsWxw/EditDialog.h>
#include <DialogsWxw/EditorListDialog.h>
#include <DialogsWxw/GraphDialog.h>
#include <DialogsWxw/HistoryDialog.h>
#include <DialogsWxw/LogConfigDialog.h>
#include <DialogsWxw/MakeModuleDialog.h>
#include <DialogsWxw/MakePatchDialog.h>
#include <DialogsWxw/MergeDlg.h>
#include <DialogsWxw/MessageDialog.h>
#include <DialogsWxw/PreferencesDialog.h>
#include <DialogsWxw/ProgressDialog.h>
#include <DialogsWxw/ReleaseDialog.h>
#include <DialogsWxw/RenameDialog.h>
#include <DialogsWxw/SwitchDialog.h>
#include <DialogsWxw/UpdateDlg.h>
#include <DialogsWxw/WxwHelpers.h>
#include <DialogsWxw/YesNoDialog.h>

#include "TortoisePreferences.h"
#include "WebLog.h"
#include "ConflictParser.h"
#include "Annotation.h"
#include "ThreadSafeProgress.h"

#include <CVSGlue/CVSStatus.h>
#include <CVSGlue/CVSRoot.h>

#include <Patch/patch.h>

#include <Utils/AutoFileDeleter.h>
#include <Utils/AutoDirectoryDeleter.h>
#include <Utils/Keyboard.h>
#include <Utils/LineReader.h>
#include <Utils/LogUtils.h>
#include <Utils/PathUtils.h>
#include <Utils/ProcessUtils.h>
#include <Utils/SandboxUtils.h>
#include <Utils/ShellUtils.h>
#include <Utils/StringUtils.h>
#include <Utils/StringUtils.h>
#include <Utils/TimeUtils.h>
#include <Utils/TortoiseDebug.h>
#include <Utils/TortoiseException.h>
#include <Utils/TortoiseRegistry.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/Translate.h>
#include <Utils/UnicodeStrings.h>

#include <wx/utils.h>

static const std::string VersionString =
#include "../VersionNumber.inc"
;


static bool GetEditors(CVSAction& glue,
                       const DirectoryGroup& group,
                       EditorListDialog::EditedFileList& editedfiles);
   


//////////////////////////////////////////////////////////////////////////////
// TortoiseAct

TortoiseAct::TortoiseAct()
   : myRemoteHandle(0)
{
   // Force flushing of preferences to registry, so all values
   // exist, and defaults are as specified in TortoisePreferences.
   TortoisePreferences prefs;
   
   // Make sure that the TortoiseCVS folder is in the PATH
   SetEnvVar("PATH", GetEnvVar("PATH") + ";" + GetTortoiseDirectory());
}

// Add new verbs here, in TortoiseActVerbs.h/.cpp, and in TortoiseMenus.config.
// The same verbs are Explorer verbs, so there is the possibility of
// calling TortoiseCVS via DDE or OLE automation upon Explorer, but I've
// never tried it.  That's why they all begin with "CVS".
void TortoiseAct::PerformAction()
{
   switch (myVerb)                      
   {
   case cvsAdd:
      PerformAddMenu();
      break;

   case cvsAddRecursive:
      PerformAddMenuRecursive();
      break;

   case cvsCheckout:
      PerformCheckoutMenu();
      break;

   case cvsTortoiseCode:
      PerformTortoiseCheckout();
      break;

   case cvsMakeModule:
      PerformMakeModuleMenu();
      break;

   case cvsCommand:
      PerformCommand();
      break;

   case cvsCommit:
      PerformCommit(true);
      break;

   case cvsDiff:
      PerformDiffMenu();
      break;

   case cvsMakePatch:
      PerformMakePatchMenu();
      break;

#if 0
   case cvsApplyPatch:
      PerformApplyPatchMenu();
      break;
#endif
      
   case cvsLog:
      PerformLogMenu();
      break;

   case cvsRelease:
      PerformReleaseMenu();
      break;

   case cvsRemove:
      PerformRemoveMenu();
      break;

   case cvsRename:
      PerformRenameMenu();
      break;
      
   case cvsRestore:
      PerformRestoreMenu();
      break;
   
   case cvsSwitch:
      PerformSwitchMenu();
      break;

   case cvsUpdate:
      PerformUpdateMenu();
      break;

   case cvsUpdateDialog:
      PerformUpdateDialogMenu();
      break;

   case cvsUpdateClean:
      PerformCleanUpdate();
      break;

   case cvsRefreshStatus:
      PerformRefreshStatus();
      break;

   case cvsWebLog:
      PerformWebLogMenu();
      break;

   case cvsTag:
      PerformTagMenu();
      break;

   case cvsBranch:
      PerformBranchMenu();
      break;

   case cvsMerge:
      PerformMergeMenu();
      break;

   case cvsHelp:
      PerformHelp();
      break;

   case cvsPrefs:
      PerformPrefs();
      break;

   case cvsEdit:
      PerformEdit(false);
      break;

   case cvsExclusiveEdit:
      PerformEdit(true);
      break;
      
   case cvsUnedit:
      PerformUnedit();
      break;

   case cvsListEditors:
      PerformListEditors();
      break;

   case cvsGet:
      PerformGet();
      break;

   case cvsHistory:
      PerformHistory();
      break;

   case cvsRevisionGraph:
      PerformRevisionGraph();
      break;

   case cvsAbout:
      PerformAbout();
      break;

   case cvsAddIgnore:
      PerformAddIgnore();
      break;

   case cvsAddIgnoreExt:
      PerformAddIgnore(true);
      break;

   case cvsResolveConflicts:
      PerformResolveConflicts();
      break;

   case cvsMergeConflicts:
      PerformMergeConflicts();
      break;

   case cvsUrl:
      PerformUrl();

   case cvsRebuildIcons:
      PerformRebuildIcons();
      break;

   case cvsSaveAs:
      PerformSaveAs();
      break;
      
   case cvsAnnotate:
       PerformAnnotate();
       break;
      
   case cvsView:
      PerformViewMenu();
      break;

   default:
      DoMessageDialog(0, wxString(_("Unknown verb:")) + wxString(wxT("\n")) + wxString(wxText(myVerbString)));
      break;
   }
}

// Stores arguments inside the class.
// Returns true if args seem valid.
bool TortoiseAct::SussArgs(int ac, char* av[])
{
   TDEBUG_ENTER("SussArgs");
   if (ac < 2)
   {
      DoMessageDialog(0, _("Too few arguments passed to TortoiseAct.\n\n\
TortoiseAct is designed to be run by the Explorer extension TrtseShl.dll. \
You don't need to run it yourself unless you have a special purpose.\n\n\
Command line arguments:\n\
TortoiseAct cvs-verb [-h handle] [-f file-list] [-u URL] [-l files...]"));
      return false;
   }

   // 1st arg must be verb
   myVerbString = av[1];
   myVerb = TextToVerb(myVerbString);

   int i = myParameters.ParseCommandLine(av, ac, 2);
   if (i == -1)
      return false;

   if (myParameters.Exists("-h"))
   {
      std::string s = myParameters.GetValue("-h");
      long myRemoteHandleLong;
      int retval = sscanf(s.c_str(), "0x%lx", &myRemoteHandleLong);
      myRemoteHandle = (HWND) myRemoteHandleLong;
      if (retval != 1)
      {
         DoMessageDialog(0,
                         Printf(_("Invalid arguments passed to TortoiseAct (expected handle \"-h 0xXXXXXXXX\", got %s)"),
                                wxText(s).c_str()));
         return false;
      }
   }

   if (myParameters.Exists("-f"))
   {
      // Read filenames from a file...
      std::string sFile = myParameters.GetValue("-f");
      std::ifstream in(sFile.c_str());
      if (!in.good())
      {
         wxString message(_("File containing input filenames not found:"));
         message += wxT("\n");
         message += wxText(sFile);
         DoMessageDialog(0, message);
         return false;
      }
               
      // A file on each line...
      while (true)
      {
         std::string file;
         std::getline(in, file);
         if (file.empty())
            break;
         myAllFiles.push_back(file);
      }
      in.close();
            
      // Remove the file
      DeleteFileA(sFile.c_str());
   }

   if (myParameters.Exists("-l"))
   {
      int count = myParameters.GetValueCount("-l");
      for (i = 0; i < count; i++)
      {
         myAllFiles.push_back(myParameters.GetValue("-l", i));
      }
   }

   // The main verbs depend on there being at least one file
   if (VerbNeedsFiles() && myAllFiles.empty())
   {
      DoMessageDialog(0, _("Empty file list disallowed"));
      return false;
   }

   // Check if verb needs preprocessing
   bool groupByTopDir;
   if (VerbNeedsPreprocessing(groupByTopDir))
   {
      return PreprocessFileList(groupByTopDir);
   }
   else
   {
      return true;
   }
}



bool TortoiseAct::VerbNeedsFiles()
{
   return !(myVerb == cvsHelp || myVerb == cvsPrefs || myVerb == cvsAbout 
            || (myVerb == cvsRebuildIcons) || (myVerb == cvsUrl));
}


bool TortoiseAct::VerbNeedsPreprocessing(bool& groupByTopDir)
{   
    groupByTopDir = false;   
    if ((myVerb == cvsAdd) || (myVerb == cvsAddRecursive))
        groupByTopDir = true;
    return !(myVerb == cvsHelp || myVerb == cvsRebuildIcons || myVerb == cvsAddRecursive);
}

bool TortoiseAct::PreprocessFileList(bool groupByTopDir)
{
   return myDirectoryGroups.PreprocessFileList(myAllFiles, groupByTopDir);
}

///////////////////////////////////////////////////////////////////////////

void TortoiseAct::PerformAddMenu()
{
   TDEBUG_ENTER("TortoiseAct::PerformAddMenu");
   // Create AddFiles data for each selected file
   unsigned int i, j;
   for (i = 0; i < myDirectoryGroups.size(); i++)
   {
      DirectoryGroup& group = myDirectoryGroups[i];
      for (j = 0; j < group.size(); j++)
      {
         DirectoryGroup::AddFilesData* data = new DirectoryGroup::AddFilesData(group[j]);
         TDEBUG_TRACE("file: " << group[j].myAbsoluteFile);
         group[j].myUserData = data;
         data->mySelected = true;
      }
   }
   
   if (DoAddFilesDialog(0, myDirectoryGroups))
   {
      AddFiles(true);
   }
}



void TortoiseAct::AddFiles(bool honourAutoCommit)
{
   TDEBUG_ENTER("TortoiseAct::AddFiles");
   CVSAction glue(0);
   // Do not show '?' lines
   glue.GetProgressDialog()->SuppressLines(ProgressDialog::TTNotInCVS);
   bool ok = true;
   FileTree* tree = 0;

   FilesWithBugnumbers filesToCommit;
   myAllFiles.clear();
   
   glue.SetProgressFinishedCaption(Printf(_("Finished add in %s"), GetDirList().c_str()));
   
   for (unsigned int j = 0; j < myDirectoryGroups.size(); ++j)
   {
      DirectoryGroup& group = myDirectoryGroups[j];

      // Get server features
      if (!group.myCVSServerFeatures.Initialize())
         TortoiseFatalError(_("Could not connect to server"));
      
      // Remove deselected entries
      std::vector<DirectoryGroup::Entry*> entries;
      DirectoryGroup::Entry* entry;
      std::vector<DirectoryGroup::Entry*>::iterator itEntries = group.myEntries.begin();
      entries.reserve(group.myEntries.size());
      // Copy all selected entries to new temporary vector
      while (itEntries != group.myEntries.end())
      {
         entry = *itEntries;
         DirectoryGroup::AddFilesData* addFilesData = (DirectoryGroup::AddFilesData*) entry->myUserData;
         if (addFilesData->mySelected)
            entries.push_back(entry);
         else
            delete entry;

         itEntries++;
      }

      // Put entries back into group's myEntries
      group.myEntries = entries;

      // If no entries are selected, continue with next group
      if (group.myEntries.size() == 0)
         continue;

      // Parse list into a tree
      tree = group.BuildFileTree<DirectoryGroup::EntryTreeData>(0);

      // Get a list of all directory-nodes in the tree
      FileTree::NodeList dirNodes;
      dirNodes.push_back(tree->GetRoot());
      tree->GetRoot()->GetSubNodes(dirNodes, FileTree::TYPE_DIR);

      // Process each directory
      FileTree::NodeList::iterator itDirNodes = dirNodes.begin();
      FileTree::DirNode* dirNode;
      FileTree::Node* node;
      DirectoryGroup::EntryTreeData* entryTreeData;
      while (itDirNodes != dirNodes.end())
      {
         dirNode = (FileTree::DirNode*) *itDirNodes;
         
         std::map<std::string, std::vector<std::string> > files;

         // Group subnodes by k-flag
         FileTree::NodeIterator itNodes = dirNode->Begin();
         while (itNodes != dirNode->End())
         {
            node = itNodes->second;
            entryTreeData = (DirectoryGroup::EntryTreeData*) node->GetUserData();
            if (entryTreeData)
            {
               DirectoryGroup::Entry& entry = entryTreeData->m_Entry;
               DirectoryGroup::AddFilesData* addFilesData = (DirectoryGroup::AddFilesData*) entry.myUserData;
               ASSERT(addFilesData);

               // Special handling for directories
               std::string kflags;
               if (node->GetNodeType() != FileTree::TYPE_DIR)
               {
                  kflags = CVSStatus::BuildAddFlags(group.myDirectory,
                                                    addFilesData->myFileFormat,
                                                    addFilesData->myKeywordOptions,
                                                    addFilesData->myFileOptions,
                                                    group.myCVSServerFeatures);
                  filesToCommit.push_back(make_pair(entry.myAbsoluteFile, ""));
               }

               files[kflags].push_back(node->GetName());
            }
            itNodes++;
         }


         // Add files in batches sorted by kflag
         std::map<std::string, std::vector<std::string> >::iterator itFiles = files.begin();
         while (itFiles != files.end())
         {
            std::vector<std::string> &addFiles = itFiles->second;
            std::string kflags = itFiles->first;
            // Add files and directories
            std::string dir = dirNode->GetFullName();
            glue.SetProgressCaption(Printf(_("Adding in %s"), wxText(dir).c_str()));
            if (!addFiles.empty())
            {
               MakeArgs args;
               args.add_option("add");
               if (!kflags.empty())
               {
                  args.add_option(std::string("-k") + kflags);
               }
               for (unsigned int i = 0; i < addFiles.size(); ++i)
                  args.add_arg(addFiles[i]);
               ok = ok && glue.Command(dir, args);
               if (!ok)
                  goto Cleanup;
            }
            itFiles++;
         }

         // On to the next node
         itDirNodes++;
      }

      delete tree;
      tree = 0;
   }


   // Commit files
   if (honourAutoCommit &&
       GetIntegerPreference("Automatic commit") &&
       !filesToCommit.empty() &&
       ok)
   {
       CVSAction* pglue = &glue;
       DoCommit(0, 0, &filesToCommit, 0, 0, pglue);
   }

Cleanup:
   delete tree;

   if (glue.AtLeastOneSuccess())
      RefreshExplorerWindow();
}



void TortoiseAct::PerformAddMenuRecursive()
{
   TDEBUG_ENTER("TortoiseAct::PerformAddMenuRecursive");

   // Save original selection
   std::vector<std::string> savedSelection = myAllFiles;

   // Get all files that need adding recursively
   GetAddableFiles(myDirectoryGroups, true);

   bool autoCommit = (GetIntegerPreference("Automatic commit") > 0);

   if (!autoCommit && myAllFiles.empty())
   {
       bool includeIgnored = myDirectoryGroups.GetBooleanPreference("Add Ignored Files");
       if (includeIgnored)
           DoMessageDialog(0, _("There is nothing to add.\n\n\
All the files, folders and their contents that you selected are already in CVS."));
       else
           DoMessageDialog(0, _("There is nothing to add.\n\n\
All the files, folders and their contents that you selected are already in CVS, \
or are specified in a .cvsignore file."));
      return;
   }

   // Query the user
   if (!myAllFiles.empty())
   {
      if (!DoAddFilesDialog(0, myDirectoryGroups))
         return;
      AddFiles(false);
   }

   // Commit files
   if (autoCommit)
   {
      myAllFiles = savedSelection;
      PreprocessFileList();
      if (!PerformCommit(false))
         DoMessageDialog(0, _("There is nothing to add or commit."));
   }
}

void TortoiseAct::PerformAddIgnore(bool useextension)
{
    TDEBUG_ENTER("PerformAddIgnore");
    CVSAction* glue = 0;
    // Group files by directories
    typedef std::map<std::string, std::vector<std::string> > IgnoreMap;
    IgnoreMap fileGroups;

   // Pass 1: Create any nonexisting .cvsignore files,
   // and do 'cvs edit' for existing files if they are readonly

   for (int j = 0; j < static_cast<int>(myDirectoryGroups.size()); ++j)
   {
      DirectoryGroup& group = myDirectoryGroups[j];

      std::string& dir(group.myDirectory);
      if (!CVSDirectoryHere(dir))
      {
          std::string subdir = GetLastPart(dir);
          dir = dir.substr(0, dir.size() - subdir.size() - 1);
          for (unsigned int i = 0; i < group.size(); i++)
              group[i].myRelativeFile = EnsureTrailingDelimiter(group[i].myRelativeFile) + subdir;
      }

      bool addIgnoreFile = false;
      bool editIgnoreFile = false;

      for (unsigned int i = 0; i < group.size(); i++)
      {
         std::string file = group[i].myRelativeFile;
         std::string dir = group.myDirectory;
         std::string path = StripLastPart(file);
         if (!path.empty())
         {
             dir = EnsureTrailingDelimiter(dir) + path;
             file = file.substr(path.size()+1);
         }
         fileGroups[dir].push_back(file);

         std::string cvsIgnoreFile = EnsureTrailingDelimiter(dir) + ".cvsignore";
         TDEBUG_TRACE("Checking " << cvsIgnoreFile);
         if (!FileExists(cvsIgnoreFile.c_str()))
         {
             // No existing .cvsignore, create it...
             std::ofstream out(cvsIgnoreFile.c_str());
             //... and schedule it for adding
             addIgnoreFile = true;
         }
         else if (IsReadOnly(cvsIgnoreFile.c_str()))
             // We must do a 'cvs edit .cvsignore' on this file
             editIgnoreFile = true;
      }

      // If any read-only .cvsignore files found, "cvs edit" them
      if (editIgnoreFile)
      {
         MakeArgs args;
         args.add_option("edit");
         args.add_option("-z");
         args.add_arg(".cvsignore");
         if (!glue)
         {
            glue = new CVSAction(0);
            glue->SetProgressFinishedCaption(Printf(_("Finished adding to .cvsignore in %s"),
                                                    GetDirList().c_str()));
         }
         if (!glue->Command(group.myDirectory, args))
            break;
      }
      // If any .cvsignore files found, "cvs add" them
      if (addIgnoreFile)
      {
         MakeArgs args;
         args.add_option("add");
         args.add_arg(".cvsignore");
         if (!glue)
         {
            glue = new CVSAction(0);
            glue->SetProgressFinishedCaption(Printf(_("Finished adding to .cvsignore in %s"),
                                                    GetDirList().c_str()));
         }
         if (!glue->Command(group.myDirectory, args))
            break;
      }
   }

   // Pass 2: Update .cvsignore files
   for (IgnoreMap::iterator it = fileGroups.begin(); it != fileGroups.end(); ++it)
   {
      // Add to .cvsignore file
      for (size_t i = 0; i < it->second.size(); ++i)
      {
         std::string file = it->second[i];
         std::string extension = GetExtension(file);
         if (useextension && !extension.empty())
         {
            file = "*." + extension;
         }
         AddToIgnoreList(it->first, file);
      }
   }

   CVSStatus::InvalidateCache();
   RefreshExplorerWindow();

   delete glue;
}

void TortoiseAct::PerformCheckoutMenu()
{
   // Only allow one directory
   if (myDirectoryGroups.mySingleAbsolute.empty())
      return;
   if (!IsDirectory(myDirectoryGroups.mySingleAbsolute.c_str()))
      return;
   
   CVSRoot cvsRoot;
   std::string module, tag, date, alternateDirectory;
   bool exportMode = false;
   bool unixLineEndings = false;
   bool forceHead = GetBooleanPreference("Force Most Recent Revision");
#ifdef MARCH_HARE_BUILD
   bool fileReadOnly = false;
#endif
   bool edit = false;
   std::string bugNumber;

   if (!myParameters.Exists("-q"))
   {
#ifdef MARCH_HARE_BUILD
       if (myDirectoryGroups.GetBooleanPreference("Checkout files Read Only"))
           fileReadOnly=true;
#endif
      if (!DoCheckoutDialog(0, cvsRoot, module, tag, date, exportMode, 
                            alternateDirectory, unixLineEndings,
#ifdef MARCH_HARE_BUILD
                            fileReadOnly,
#endif
                            myDirectoryGroups.mySingleAbsolute, forceHead, edit, bugNumber))
         return;
   }
   else
   {
      cvsRoot.SetCVSROOT(myParameters.GetValue("-cvsroot"));
      module = myParameters.GetValue("-module");
      tag = myParameters.GetValue("-tag");
      date = myParameters.GetValue("-date");
   }
   
   CVSAction glue(0);
   glue.SetProgressFinishedCaption(Printf(_("Finished checkout in %s"), 
                                          wxText(myDirectoryGroups.mySingleAbsolute).c_str()));
   glue.SetCVSRoot(cvsRoot);
   
   MakeArgs args;
   if (CVSDirectoryHere(myDirectoryGroups.mySingleAbsolute))
   {
       args.add_global_option("-d");
       args.add_global_option(cvsRoot.GetCVSROOT());
   }
   if (exportMode)
   {
       if (tag.empty())
       {
           glue.GetProgressDialog()->NewText(wxString(_("No tag specified for Export, using HEAD")) 
                                             + wxString(wxT("\n\n")), ProgressDialog::TTWarning);
           tag = "HEAD";
       }
       args.add_option("export");
       args.add_option("-r");
       args.add_option(tag);
   }
   else
   {
       args.add_option("checkout");
       if (!tag.empty() && (tag != "HEAD"))
       {
           args.add_option("-r");
           args.add_option(tag);
       }
   }
   if (!date.empty())
   {
       args.add_option("-D");
       args.add_option(date);
   }
   
   if (!alternateDirectory.empty())
   {
       args.add_option("-d");
       args.add_option(alternateDirectory);
   }
   CVSServerFeatures features;
   if (unixLineEndings || edit)
   {
       features.Initialize(&glue);
       if (unixLineEndings)
           features.AddUnixLineEndingsFlag(args);
   }
#ifdef MARCH_HARE_BUILD
   if (fileReadOnly)
       args.add_global_option("-r");
#endif
   
   if (!exportMode)
       if (GetBooleanPreference("Prune Empty Directories"))
           args.add_option("-P");

   if (forceHead)
       args.add_option("-f");

   args.add_option(module);
   glue.SetProgressCaption(Printf(_("Checking out in %s"),
                                  wxTextCStr(myDirectoryGroups.mySingleAbsolute)));

   glue.Command(myDirectoryGroups.mySingleAbsolute, args);

   if (edit)
   {
       MakeArgs args;
       args.add_option("edit");
       args.add_option("-z");
       if (features.SupportsExclusiveEdit())
       {
           if (GetIntegerPreference("Edit policy") == TortoisePreferences::EditPolicyExclusiveACL)
               args.add_option("-A");
           if (GetIntegerPreference("Edit policy") >= TortoisePreferences::EditPolicyExclusive)
               args.add_option("-x");
           else if (GetIntegerPreference("Edit policy") == TortoisePreferences::EditPolicyConcurrentExclusive)
               args.add_option("-c");
       }
       if (!bugNumber.empty())
       { 
           args.add_option("-b"); 
           args.add_option(bugNumber); 
       }
       std::string root = myDirectoryGroups.mySingleAbsolute;
       std::string dir;
       if (alternateDirectory.empty())
           dir = module;
       else
           dir = alternateDirectory;
       std::string firstDir = GetFirstPart(dir);
       if (firstDir.empty())
           root += dir;
       else
           root += firstDir;
       glue.Command(root, args);
   }
}

void TortoiseAct::PerformTortoiseCheckout()
{
   std::string project = "tortoisecvs";
   std::string module = "TortoiseCVS";
   
   // Only allow one directory
   if (myDirectoryGroups.mySingleAbsolute.empty())
      return;
   if (!IsDirectory(myDirectoryGroups.mySingleAbsolute.c_str()))
      return;
   
   CVSAction glue(0);
   glue.SetProgressFinishedCaption(Printf(_("Finished checkout in %s"), 
                                          wxText(myDirectoryGroups.mySingleAbsolute).c_str()));
   CVSRoot cvsRoot;
   cvsRoot.SetProtocol("pserver");
   cvsRoot.SetUser("anonymous");
   cvsRoot.SetServer("tortoisecvs.cvs.sourceforge.net");
   cvsRoot.SetDirectory("/cvsroot/" + project);
   glue.SetCVSRoot(cvsRoot);
   // Password is empty string for Sourceforge
   MakeArgs args;
   args.add_option("-z3");
   args.add_option("checkout");
   args.add_option("-P");
   args.add_arg(module);
   glue.SetProgressCaption(Printf(_("Checking out in %s"), wxText(myDirectoryGroups.mySingleAbsolute).c_str()));
   if (glue.Command(myDirectoryGroups.mySingleAbsolute, args))
   {
      wxString message(_("You can now compile the TortoiseCVS code."));
      message += wxT("\n\n");
      message += _("Create a patch file: If you fix some bugs or add some new \
features, please submit the changes for other users.  Right click \
on the changed files or folders and choose Make Patch from the CVS submenu.");
      glue.GetProgressDialog()->NewText(message);
   }
}

// Not your normal CVS import - creates just the top level dir,
// and does it "in-place" (or at least gives the illusion of doing so).
void TortoiseAct::PerformMakeModuleMenu()
{
   // Only allow one directory, which isn't in CVS already
   if (myDirectoryGroups.mySingleAbsolute.empty())
      return;
   if (!IsDirectory(myDirectoryGroups.mySingleAbsolute.c_str()))
      return;
   if (CVSDirectoryHere(myDirectoryGroups.mySingleAbsolute))
      return;
   if (IsRootPath(myDirectoryGroups.mySingleAbsolute))
   {
       DoMessageDialog(0, _("You should not create a module in the root folder of a drive."));
       return;
   }
   
   // Fetch parameters
   std::string module = ExtractLastPart(myDirectoryGroups.mySingleAbsolute);
   
   CVSRoot cvsRoot;
   bool watchon;
   std::string comment;
   if (!DoMakeModuleDialog(0, cvsRoot, module, watchon, comment))
      return;

   // Get temporary dir to work in
   std::string tempDir = GetTemporaryPath();
   tempDir = EnsureTrailingDelimiter(tempDir);
   tempDir += "TortoiseCVS make new module temp";
   tempDir = EnsureTrailingDelimiter(tempDir);
   DeleteDirectoryRecursively(tempDir);
   CreateDirectoryA(tempDir.c_str(), NULL);
   
   // Import that directory
   bool ok;
   CVSAction glue(0);
   glue.SetProgressFinishedCaption(Printf(_("Finished make new module in %s"), 
                                          wxText(myDirectoryGroups.mySingleAbsolute).c_str()));
   glue.SetCVSRoot(cvsRoot);

   // Get client features
   DirectoryGroup& group = myDirectoryGroups[0];
   if (!group.myCVSClientFeatures.Initialize())
      TortoiseFatalError(_("Could not connect to client"));

   {
      MakeArgs args;
      args.add_option("-d");
      args.add_option(cvsRoot.GetCVSROOT());
      args.add_option("import");
      args.add_option("-m");
      args.add_option(comment);
      args.add_option(module);
      args.add_option("tcvs-vendor");
      args.add_option("tcvs-release");
      glue.SetProgressCaption(_("Importing"));
      ok = glue.Command(tempDir, args);
   }
   
   // Now we can remove it
   RemoveDirectoryA(tempDir.c_str());
   
   // Give up, if we failed the import
   if (!ok)
      goto Cleanup;
   
   // Check-out on top of the original
   glue.SetCVSRoot(cvsRoot);
   {
      MakeArgs args;
      args.add_option("checkout");
      args.add_option("-d");
      args.add_option(ExtractLastPart(myDirectoryGroups.mySingleAbsolute));
      args.add_option(module);

      glue.SetProgressCaption(_("Checking out"));
      if (!glue.Command(GetDirectoryAbove(myDirectoryGroups.mySingleAbsolute), args))
         goto Cleanup;
   }

   if (watchon)
   {
      // Set 'watch on' for the new module.
      // This will apply to all files added in the future.
      MakeArgs args;
      args.add_option("watch");
      args.add_option("on");
      glue.SetProgressCaption(_("Enabling watch"));
      if (!glue.Command(myDirectoryGroups.mySingleAbsolute, args))
         goto Cleanup;
   }
   
   glue.GetProgressDialog()->NewText(_("Tortoise Tip: The top-level folder has now been created in CVS.\n\
To add files and subfolders, invoke the CVS Add Contents command."),
                                     ProgressDialog::TTTip);

Cleanup:
   RefreshExplorerWindow();
}


bool TortoiseAct::PerformCommit(bool messageIfNoAction)
{
   TDEBUG_ENTER("TortoiseAct::PerformCommit");

   // Get a list of all the modified/added/removed files
   FilesWithBugnumbers modified, modifiedStatic, added, removed, renamed;
   for (unsigned int i = 0; i < myAllFiles.size(); ++i)
       CVSStatus::RecursiveScan(myAllFiles[i], &modified, &modifiedStatic, &added, &removed, &renamed, 0, 0, true);

   TDEBUG_TRACE("Modified " << modified.size() << " added " << added.size()
                << " removed " << removed.size());
   
   // Sort them into order
   std::sort(modified.begin(), modified.end());
   std::sort(modifiedStatic.begin(), modifiedStatic.end());
   std::sort(added.begin(), added.end());
   std::sort(removed.begin(), removed.end());

   bool autoUnedit = (GetIntegerPreference("Edit policy") != TortoisePreferences::EditPolicyNone)
      && GetBooleanPreference("Automatic unedit");
   CVSAction* pglue = 0;

   // Check there are some files
   if (modified.empty() && modifiedStatic.empty() && added.empty() && removed.empty() && renamed.empty())
   {
      // If automatic unedit is enabled, check if any files are edited
      if (autoUnedit)
      {
         if (DoAutoUnedit(pglue))
            return true;
      }

      if (messageIfNoAction)
         DoMessageDialog(0, _("There is nothing to commit.\n\n\
All the files and folders that you selected are unchanged since you last performed a commit."));
      return false;
   }
   
   bool ok = true;

   // Try to determine if watch is off for this module
   bool isWatchOn = autoUnedit;
   std::vector<std::string> dirs;
   if (!modified.empty() || !modifiedStatic.empty() || !added.empty() || !removed.empty() || !renamed.empty())
      ok = DoCommit(&modified, &modifiedStatic, &added, &removed, &renamed, pglue, &isWatchOn);
   if (autoUnedit && !isWatchOn)
      autoUnedit = false;

   if (ok && autoUnedit)
      DoAutoUnedit(pglue);
   
   // Clear the Explorer window (unless it was all directories being
   // updated, in which case no point)
   if (FileSelected() && pglue && pglue->AtLeastOneSuccess())
      RefreshExplorerWindow();

   delete pglue;
   return true;
}


void TortoiseAct::PerformDiffMenu()
{
   TDEBUG_ENTER("TortoiseAct::PerformDiffMenu");
   // Forget the diff program if the user holds down Control,
   // so they can choose a new diff program
   bool forceQuery = IsControlDown();

   std::string rev1, rev2;
   if (myParameters.GetValueCount("-r") > 0)
   {
      // Not just diffing against head revision
      rev1 = myParameters.GetValue("-r", 0);
      rev2 = myParameters.GetValue("-r", 1);
   }

   DoDiff(myDirectoryGroups, rev1, rev2, forceQuery);
}

void TortoiseAct::PerformLogMenu()
{
   CVSAction glue(0);
   glue.SetProgressFinishedCaption(Printf(_("Finished log in %s"), GetDirList().c_str()));
   
   glue.SetClose(CVSAction::CloseNever);
   
   for (unsigned int j = 0; j < myDirectoryGroups.size(); ++j)
   {
      DirectoryGroup& group = myDirectoryGroups[j];
      
      MakeArgs args;
      args.add_option("log");
      for (unsigned int i = 0; i < group.size(); ++i)
      {
         CVSStatus::FileStatus status = CVSStatus::GetFileStatus(group[i].myAbsoluteFile);
         if (status == CVSStatus::STATUS_INCVS_RO ||
             status == CVSStatus::STATUS_INCVS_RW ||
             status == CVSStatus::STATUS_ADDED ||
             status == CVSStatus::STATUS_CHANGED ||
             status == CVSStatus::STATUS_CONFLICT)
         {
            args.add_arg(group[i].myRelativeFile);
         }
      }
      glue.SetProgressCaption(Printf(_("Logging in %s"), wxText(group.myDirectory).c_str()));
      if (!glue.Command(group.myDirectory, args))
         return;
   }
}

void TortoiseAct::PerformReleaseMenu()
{
   if (DoReleaseDialog(0, myAllFiles))
      RefreshExplorerWindow(&myAllFiles);
}
  
void TortoiseAct::PerformSwitchMenu()
{
   if (DoSwitchDialog(0, myAllFiles))
      RefreshExplorerWindow(&myAllFiles);
}
  
bool IsFileRemovable(const std::string& file)
{
   CVSStatus::FileStatus status = CVSStatus::GetFileStatus(file);
   if (status != CVSStatus::STATUS_NOTINCVS &&
       status != CVSStatus::STATUS_NOWHERENEAR_CVS &&
       status != CVSStatus::STATUS_OUTERDIRECTORY)
   {
      // Can't yet directly remove dirs with other stuff under them
      if (!CVSStatus::InCVSHasSubdirs(file))
         return true;
   }
   return false;
}

void TortoiseAct::PerformRemoveMenu()
{
   TDEBUG_ENTER("TortoiseAct::PerformRemoveMenu");

   // Redo the preprocessing into groups
   myDirectoryGroups.myDoNotGroupDirectories = true;
   PreprocessFileList();

   int items = 0;
   std::string name;
   bool directory = false;
   for (int j = 0; j < static_cast<int>(myDirectoryGroups.size()); ++j)
   {
      DirectoryGroup& group = myDirectoryGroups[j];
      for (size_t i = 0; i < group.size(); ++i)
      {
         ++items;
         name = group[i].myRelativeFile;
         if (name.empty())
         {
            name = group.myDirectory;
            directory = true;
         }
      }
   }
   wxString question;
   if (items == 1)
   {
      if (directory)
         question = Printf(_("Are you sure that you want to CVS Remove all files in the folder '%s'?"),
                           wxText(name).c_str());
      else
         question = Printf(_("Are you sure that you want to CVS Remove '%s'?"), wxTextCStr(name));
   }
   else
      question = Printf(_("Are you sure that you want to CVS Remove these %d items?"), items);

   if (!DoYesNoDialog(0, question, true))
      return;

   SHQUERYRBINFO SHQueryRBInfo;
   __int64 recycleBinSize = 0;
   {
       wxBusyCursor busy;
       // Get current size of recycle bin
       SHQueryRBInfo.cbSize = sizeof(SHQueryRBInfo);
       if (SHQueryRecycleBinA(myDirectoryGroups[0].myDirectory.c_str(), &SHQueryRBInfo) == S_OK)
           recycleBinSize = SHQueryRBInfo.i64Size;
       TDEBUG_TRACE("Recycle Bin size: " << recycleBinSize);
   
       std::vector<std::string> newFiles;

       for (size_t j = 0; j < myDirectoryGroups.size(); ++j)
       {
           DirectoryGroup& group = myDirectoryGroups[j];
           for (size_t i = 0; i < group.size(); ++i)
           {
               if (IsDirectory(group[i].myAbsoluteFile.c_str()))
               {
                   TDEBUG_TRACE("Directory: " << group[i].myAbsoluteFile);
                   std::vector<std::string> added, modified, unmodified;
                   CVSStatus::RecursiveScan(group[i].myAbsoluteFile,
                                            &modified, 
                                            &added,
                                            0,
                                            &modified, // renamed
                                            &unmodified,
                                            &modified); // conflict
                   newFiles.insert(newFiles.end(), modified.begin(), modified.end());
                   newFiles.insert(newFiles.end(), added.begin(), added.end());
                   newFiles.insert(newFiles.end(), unmodified.begin(), unmodified.end());
               }
               else
               {
                   TDEBUG_TRACE("File: " << group[i].myAbsoluteFile);
                   newFiles.push_back(group[i].myAbsoluteFile);
               }
           }
       }

       typedef std::set<std::string, less_nocase> DupSet;
       DupSet mySet;
       // Remove duplicate entries
       for (size_t i = 0; i < newFiles.size(); ++i)
       {
           TDEBUG_TRACE("file: " << newFiles[i]);
           mySet.insert(newFiles[i]);
       }

       DupSet::iterator it = mySet.begin();
       myAllFiles.clear();
       TDEBUG_TRACE("Readd files");
       while (it != mySet.end())
       {
           TDEBUG_TRACE("File: " << *it);
           myAllFiles.push_back(*it);
           ++it;
       }
       mySet.clear();
   }
   
   if (myAllFiles.empty())
   {
      DoMessageDialog(0,
                      wxString(_("No files found that could be removed.\n")) +
                      wxString(_("Note that CVS does not keep track of folders explicitly; \
if you wish to remove a folder, remove the files inside it, \
and then do a CVS Update. This requires that the preference \
setting 'Prune empty folders' is enabled.")));
      return;
   }

   CVSAction glue(0);
   glue.SetProgressFinishedCaption(Printf(_("Finished remove in %s"), 
                                          GetDirList().c_str()));
   PreprocessFileList();

   bool result = true;
   int totalFileCount = 0;
   for (int j = 0; j < static_cast<int>(myDirectoryGroups.size()); ++j)
   {
      DirectoryGroup& group = myDirectoryGroups[j];
      TDEBUG_TRACE("Group: " << group.myDirectory);
      
      if (!group.myCVSServerFeatures.Initialize())
         TortoiseFatalError(_("Could not connect to server"));

      bool readonly = false;
      MakeArgs texteditargs;
      texteditargs.add_option("edit");
      texteditargs.add_option("-z");
      bool doText = false;
      MakeArgs binaryeditargs;
      binaryeditargs.add_option("edit");
#ifdef MARCH_HARE_BUILD
      binaryeditargs.add_option("-z");
#else
      binaryeditargs.add_option("-zc");
      if (group.myCVSServerFeatures.SupportsExclusiveEdit())
          binaryeditargs.add_option("-x");
#endif
      bool doBinary = false;
      
      // Do a "cvs edit" on any read-only files
      std::vector<std::string> changedFiles;
      for (size_t i = 0; i < group.size(); ++i)
      {
         if (IsFileRemovable(group[i].myAbsoluteFile))
         {
            // Check if the file has changed
            CVSStatus::FileStatus status = CVSStatus::GetFileStatus(group[i].myAbsoluteFile);
            if ((status == CVSStatus::STATUS_CHANGED) ||
                (status == CVSStatus::STATUS_CONFLICT))
               changedFiles.push_back(group[i].myAbsoluteFile);
            std::string file = group[i].myAbsoluteFile;
            if (IsReadOnly(file.c_str()))
            {
               readonly = true;
               if (CVSStatus::IsBinary(file))
               {
                  doBinary = true;
                  binaryeditargs.add_arg(group[i].myRelativeFile);
               }
               else
               {
                  doText = true;
                  texteditargs.add_arg(group[i].myRelativeFile);
               }
            }
         }
      }
      if (!changedFiles.empty())
      {
         std::stringstream changedFilesList;
         std::ostream_iterator<std::string> it(changedFilesList, "\n");
         std::copy(changedFiles.begin(), changedFiles.end(), it);
         if (changedFiles.size() == 1)
         {
             if (!DoYesNoDialog(glue.GetProgressDialog(), 
                                Printf(_("The file\n\n%s\nis modified. Are you sure that you want to remove it and thus lose your changes?"), 
                                wxText(changedFilesList.str()).c_str()), false))
                 break;
         }
         else
         {
             if (!DoYesNoDialog(glue.GetProgressDialog(), 
                                Printf(_("The files\n\n%s\nare modified. Are you sure that you want to remove them and thus lose your changes?"), 
                                wxText(changedFilesList.str()).c_str()), false))
                 break;
         }
      }
      if (readonly)
      {
         // Do 'cvs edit' first
         glue.SetProgressCaption(Printf(_("Editing in %s"), wxText(group.myDirectory).c_str()));
         if (doText && !glue.Command(group.myDirectory, texteditargs))
         {
            if (glue.AtLeastOneSuccess())
               RefreshExplorerWindow();
            return;
         }
         if (doBinary && !glue.Command(group.myDirectory, binaryeditargs))
         {
            if (glue.AtLeastOneSuccess())
               RefreshExplorerWindow();
            return;
         }
      }

      // Construct command, and move all the files out of the way
      TDEBUG_TRACE("Removing files");
      glue.SetProgressCaption(Printf(_("Removing in %s"), wxText(group.myDirectory).c_str()));
      MakeArgs args;
      args.add_option("remove");
      int fileCount = 0;
      for (size_t i = 0; i < group.size(); ++i)
      {
         if (IsDirectory(group[i].myAbsoluteFile.c_str()))
            continue;
         TDEBUG_TRACE("File: " << group[i].myAbsoluteFile);
         if (IsFileRemovable(group[i].myAbsoluteFile))
         {
            std::string file = group[i].myAbsoluteFile;
            std::string fileMoved = file + ".tortoise.removed";

            // Move the file out of the way
            if (!MoveFileA(file.c_str(), fileMoved.c_str()))
            {
               TDEBUG_TRACE("Error moving file " << file);
               DoMessageDialog(glue.GetProgressDialog(), Printf(_("Couldn't rename file:\n%s to %s"), 
                               wxText(file).c_str(), wxText(fileMoved).c_str()));
               result = false;
               break;
            }
            fileCount++;
            totalFileCount++;
            args.add_arg(group[i].myRelativeFile);
         }
      }

      if (!result)
         return;

      if (fileCount == 0)
      {
         continue;
      }

      result = glue.Command(group.myDirectory, args);
      if (!result)
         return;

      // Check for conflicts caused by a file having been removed by second party
      const std::list<std::string>& lines = glue.GetStdOutList();
      std::vector<std::string> conflictFiles;
      ParseConflicts(group, lines, conflictFiles);
      for (size_t i = 0; i < conflictFiles.size(); ++i)
      {
         // Rename the file back
         std::string file = conflictFiles[i];
         std::string fileMoved = file + ".tortoise.removed";
         if (!MoveFileA(fileMoved.c_str(), file.c_str()))
         {
            TDEBUG_TRACE("Error moving file back " << file);
            DoMessageDialog(glue.GetProgressDialog(), Printf(_("Couldn't rename file back:\n%s to %s"),
                                                             wxText(fileMoved).c_str(), wxText(file).c_str()));
         }
      }

      CVSStatus::InvalidateCache();
      TDEBUG_TRACE("Recycling files");
      std::ostringstream ssFiles;
      for (size_t i = 0; i < group.size(); i++)
      {
         if (IsDirectory(group[i].myAbsoluteFile.c_str()))
            continue;
         
         TDEBUG_TRACE("File: " << group[i].myAbsoluteFile);
         CVSStatus::FileStatus status = CVSStatus::GetFileStatus(group[i].myAbsoluteFile);

         // Rename the file back
         std::string file = group[i].myAbsoluteFile;
         std::string fileMoved = file + ".tortoise.removed";
         if (!MoveFileA(fileMoved.c_str(), file.c_str()))
         {
            TDEBUG_TRACE("Error moving file " << file);
            DoMessageDialog(glue.GetProgressDialog(), Printf(_("Couldn't rename file back:\n%s to %s"),
                                                               wxText(fileMoved).c_str(), wxText(file).c_str()));
         }

         if (status == CVSStatus::STATUS_REMOVED)
         {
            ssFiles << file << '\n';
         }
      }

      if (ssFiles.str().length() > 0)
      {
         // "cvs remove" was succesful. Move the file to recycle bin
         SHFILEOPSTRUCTA fileop;
         fileop.hwnd     = myRemoteHandle; 
         fileop.wFunc    = FO_DELETE; 
         size_t len = ssFiles.str().length();
         char *p = (char*) malloc(len + 1);
         memcpy(p, ssFiles.str().c_str(), len + 1);
         for (size_t i = 0; i < len; i++)
         {
            if (p[i] == '\n')
               p[i] = '\0';
         }
         fileop.pFrom  = p; 
         fileop.pTo    = 0L; 
         fileop.fFlags = FOF_ALLOWUNDO | FOF_SILENT | FOF_NOCONFIRMATION;
         if (SHFileOperationA(&fileop) != 0)
            DoMessageDialog(glue.GetProgressDialog(), _("Couldn't recycle"));
         free(p);
      }
   }

   if (totalFileCount == 0)
   {
      glue.GetProgressDialog()->NewText(_("No removable files found"));
   }


   bool redoremove = false;
   if (GetIntegerPreference("Automatic commit") > 1)
   {
      // Get a list of removed files
      FilesWithBugnumbers removed;
      for (size_t i = 0; i < myDirectoryGroups.size(); ++i)
         CVSStatus::RecursiveScan(myDirectoryGroups[i].myDirectory, 0, 0, 0, &removed, 0, 0, 0);

      CVSAction* pglue = &glue;
      DoCommit(0, 0, 0, &removed, 0, pglue);
   }
   
   // If Recycle Bin is enabled (i.e. if its size has changed), show tip
   if ((SHQueryRecycleBinA(myDirectoryGroups[0].myDirectory.c_str(), &SHQueryRBInfo) == S_OK) &&
       (SHQueryRBInfo.i64Size != recycleBinSize))
   {
      glue.GetProgressDialog()->NewText(
          wxString(_("Tortoise Tip: Any removed files have been moved to the Recycle Bin.\n\
To undo the remove: Choose CVS Commit, right-click the file, and click Restore.\n")) +
          wxString(_("Note that CVS does not keep track of folders explicitly; \
if you wish to remove a folder, remove the files inside it, \
then do a CVS Update. This requires that the preference \
setting 'Prune empty folders' is enabled.")),
         ProgressDialog::TTTip);
   }
   else
   {
      TDEBUG_TRACE("Recycling didn't work");
   }

   if (glue.AtLeastOneSuccess())
      RefreshExplorerWindow();
}

void TortoiseAct::PerformRenameMenu()
{
   TDEBUG_ENTER("TortoiseAct::PerformRenameMenu");

   // Only allow one file
   if (myDirectoryGroups.mySingleAbsolute.empty())
      return;
   if (IsDirectory(myDirectoryGroups.mySingleAbsolute.c_str()))
      return;

   std::string newname;
   if (!DoRenameDialog(0, myDirectoryGroups.mySingleAbsolute, newname))
      return;

   if (newname == "..")
   {
       DoMessageDialog(0, _("Rename does not currently support moving a file to a different folder"));
       return;
   }
   
   CVSAction glue(0);
   glue.SetProgressFinishedCaption(Printf(_("Finished rename in %s"), GetDirList().c_str()));
   MakeArgs args;
   args.add_option("rename");
   args.add_arg(ExtractLastPart(myDirectoryGroups.mySingleAbsolute));
   args.add_arg(newname);
   glue.Command(StripLastPart(myDirectoryGroups.mySingleAbsolute), args);
   
   if (glue.AtLeastOneSuccess())
      RefreshExplorerWindow();
}



void TortoiseAct::PerformRestoreMenu()
{
   CVSAction glue(0);
   bool ok = true;
   unsigned int j;
   FileTree* tree = 0;
   
   glue.SetProgressFinishedCaption(Printf(_("Finished restoring in %s"), GetDirList().c_str()));

   
   for (j = 0; j < myDirectoryGroups.size(); ++j)
   {
      DirectoryGroup& group = myDirectoryGroups[j];

      // Parse list into a tree
      tree = group.BuildFileTree<DirectoryGroup::EntryTreeData>(0);

      // Get a list of all directory-nodes in the tree
      FileTree::NodeList dirNodes;
      dirNodes.push_back(tree->GetRoot());
      tree->GetRoot()->GetSubNodes(dirNodes, FileTree::TYPE_DIR);

      // Process each directory
      FileTree::NodeList::iterator itDirNodes = dirNodes.begin();
      FileTree::DirNode* dirNode;
      FileTree::Node* node;
      DirectoryGroup::EntryTreeData* entryTreeData;
      while (itDirNodes != dirNodes.end())
      {
         dirNode = (FileTree::DirNode*) *itDirNodes;
         
         std::vector<std::string> files;

         // Group files
         FileTree::NodeIterator itNodes = dirNode->Begin();
         while (itNodes != dirNode->End())
         {
            node = itNodes->second;
            entryTreeData = (DirectoryGroup::EntryTreeData*) node->GetUserData();
            if (entryTreeData)
            {
               if (node->GetNodeType() != FileTree::TYPE_DIR)
               {
                  files.push_back(node->GetName());
               }
            }
            itNodes++;
         }

         std::string dir = dirNode->GetFullName();
         glue.SetProgressCaption(Printf(_("Restoring in %s"), wxText(dir).c_str()));
         // Restore files 
         if (!files.empty())
         {
            MakeArgs args;
            args.add_option("add");

            std::vector<std::string>::iterator itFiles = files.begin();
            while (itFiles != files.end())
            {
               args.add_arg(*itFiles);
               itFiles++;
            }

            ok = glue.Command(dir, args);
            if (!ok)
               goto Cleanup;


            // TODO: Do we want to reuse an existing backup, in case the file has 
            // been modified? If so, we'd have to do an update, since the backups 
            // have wrong timestamps. But what if the file on the server has been
            // modified in the meantime? So better leave it, and let the user
            // decide whether he wants to use his existing backup by deleting the
            // file and renaming the backup ...
         }

         // On to the next node
         itDirNodes++;
      }

      delete tree;
      tree = 0;
   }

Cleanup:
   if (glue.AtLeastOneSuccess())
      RefreshExplorerWindow();
}



void TortoiseAct::PerformUpdateMenu(bool dialog)
{
   TDEBUG_ENTER("TortoiseAct::PerformUpdateMenu");
   bool resetsticky = false;
   std::string getrevision;
   std::string getdate;
   std::string getdirectory;
   bool clean = false;
   bool simulate = GetBooleanPreference("Simulate update");
   bool createdirs = GetBooleanPreference("Create Added Directories");
   bool forceHead = GetBooleanPreference("Force Head Revision");
   bool pruneEmptyDirectories = GetBooleanPreference("Prune Empty Directories");
   bool recurse = true;
   
   // bring up dialog with extra options, either if they choose
   // the menu option for it, or CTRL is held down
   if (dialog || IsControlDown())
   {
      bool query = DoUpdateDialog(0, getrevision, getdate, clean, 
                                  createdirs, simulate, forceHead, pruneEmptyDirectories,
                                  recurse,
                                  myDirectoryGroups.mySingleAbsolute.empty() ? 0 : &getdirectory,
                                  GetFetchButtonDirs(), GetFetchButtonTag()); 
      
      if (!query) // cancel entire update operation
         return;
   }

   if (getrevision == "HEAD")
   {
      resetsticky = true;
      getrevision = "";
   }
   
   // When CVSAction is constructed, the dialog appears
   CVSAction glue(0);
   glue.SetProgressFinishedCaption(Printf(_("Finished update in %s"), 
                                          GetDirList().c_str()));
   std::vector<std::string> conflictFiles;
   for (size_t j = 0; j < myDirectoryGroups.size() && !glue.Aborted(); ++j)
   {
      DirectoryGroup& group = myDirectoryGroups[j];
      
      MakeArgs args;
      if (simulate)
      {
         args.add_global_option("-n");
         glue.SetClose(CVSAction::CloseNever);
      }

      args.add_option("update");
      if (pruneEmptyDirectories)
         args.add_option("-P");
      if (resetsticky)
         args.add_option("-A");
      if (!getrevision.empty())
      {
         args.add_option("-r");
         args.add_option(getrevision);
      }
      if (!getdate.empty())
      {
         args.add_option("-D");
         args.add_option(getdate);
      }

      if (forceHead)
         args.add_option("-f");
      
      if (clean)
         args.add_option("-C");

      if (!recurse)
         args.add_option("-l");
      
      if (!getdirectory.empty())
      {
         // Update single folder
         args.add_option("-d");
         std::string arg(EnsureTrailingDelimiter(myDirectoryGroups.mySingleRelative));
         arg += getdirectory;
         args.add_arg(arg);
      }
      else
      {
         if (createdirs)
            args.add_option("-d"); // create missing directories that exist in the repository
         for (size_t i = 0; i < group.size(); ++i)
            args.add_arg(group[i].myRelativeFile);
      }
      glue.SetProgressCaption(Printf(_("Updating in %s"), wxText(group.myDirectory).c_str()));
      glue.Command(group.myDirectory, args);
      if (glue.Aborted())
         break;

      // Make a copy, so that the next update does not clear the conflict information
      std::list<std::string> out = glue.GetStdOutList();
      
      // identical files need another "cvs update"
      std::vector<std::string> identicalFiles;
      ParseIdentical(group, out, identicalFiles);

      // put them into a map
      if (!identicalFiles.empty())
      {
         MakeArgs args;
         args.add_option("update");
         std::vector<std::string>::const_iterator it = identicalFiles.begin();
         std::string s;
         while (it != identicalFiles.end())
         {
            s = it->substr(EnsureTrailingDelimiter(group.myDirectory).length());
            args.add_arg(s);
            it++;
         }
         glue.Command(group.myDirectory, args);
      }
      
      // Check for conflicts
      if (!simulate)
         ParseConflicts(group, out, conflictFiles);
   }
   
   // Edit conflict file if appropriate
   if (conflictFiles.size() > 0)
   {
      glue.LockProgressDialog(true);
      DoConflictListDialog(glue.GetProgressDialog(), conflictFiles);
      glue.LockProgressDialog(false);
   }
   
   if (glue.AtLeastOneSuccess())
      RefreshExplorerWindow();
}

void TortoiseAct::PerformUpdateDialogMenu()
{
   PerformUpdateMenu(true);
}

void TortoiseAct::PerformCleanUpdate()
{
   TDEBUG_ENTER("TortoiseAct::PerformCleanUpdate");
   // When CVSAction is constructed, the dialog appears
   CVSAction glue(0);
   glue.SetProgressFinishedCaption(Printf(_("Finished clean update in %s"), 
                                          GetDirList().c_str()));
   for (unsigned int j = 0; j < myDirectoryGroups.size() && !glue.Aborted(); ++j)
   {
      DirectoryGroup& group = myDirectoryGroups[j];
      
      MakeArgs args;
      args.add_option("update");
      args.add_option("-C");
      
      for (unsigned int i = 0; i < group.size(); ++i)
         args.add_arg(group[i].myRelativeFile);
      glue.SetProgressCaption(Printf(_("Doing clean update in %s"), wxText(group.myDirectory).c_str()));
      glue.Command(group.myDirectory, args);
      if (glue.Aborted())
         break;
   }
}

void TortoiseAct::PerformWebLogMenu()
{
   // Only allow one file or directory
   if (myDirectoryGroups.mySingleAbsolute.empty())
      return;
   
   // Find the path on the server
   std::string ambientPart = "/" + CVSStatus::CVSRepositoryForPath(myDirectoryGroups.mySingleAbsolute) + "/";
   if (!IsDirectory(myDirectoryGroups.mySingleAbsolute.c_str()))
      ambientPart += ExtractLastPart(myDirectoryGroups.mySingleAbsolute);

   // Find URL for this CVSROOT
   WebLog webLog(CVSStatus::CVSRootForPath(myDirectoryGroups.mySingleAbsolute), ambientPart);
   
   // Scan and/or query user to find out what URL to use
   std::string url;
   bool result = webLog.PerformSearchAndQuery(url);
   
   // See if we failed to find anything (e.g. user cancelled)
   if (!result)
      return;
   
   // Detect plain log - which just does "cvs log" in the progress window
   if (url == "plainlog")
   {
      PerformLogMenu();
      return;
   }
   
   // Launch the URL
   LaunchURL(url);
}

void TortoiseAct::PerformTagMenu()
{
   std::string tagname = TortoiseRegistry::ReadString("FailedTagName");
   bool checkunmodified;
   int action;
   if (DoTagDialog(0, tagname, checkunmodified, action, GetFetchButtonDirs()))
   {
      CVSAction glue(0);
      glue.SetProgressFinishedCaption(Printf(_("Finished tagging in %s"), 
                                             GetDirList().c_str()));
      for (unsigned int j = 0; j < myDirectoryGroups.size(); ++j)
      {
         DirectoryGroup& group = myDirectoryGroups[j];
         
         MakeArgs args;
         args.add_option("tag");
         if (checkunmodified)
            args.add_option("-c");
         switch (action)
         {
         case BranchTagDlg::TagActionCreate:
            break;
         case BranchTagDlg::TagActionMove:
            args.add_option("-F");
            break;
         case BranchTagDlg::TagActionDelete:
            args.add_option("-d");
            break;
         default:
            DoMessageDialog(glue.GetProgressDialog(), _("Internal error: Invalid action"));
            break;
         }
         args.add_option(tagname);
         for (unsigned int i = 0; i < group.size(); ++i)
            args.add_arg(group[i].myRelativeFile);
         glue.SetProgressCaption(Printf(_("Tagging in %s"), wxTextCStr(group.myDirectory)));
         if (!glue.Command(group.myDirectory, args))
         {
            TortoiseRegistry::WriteString("FailedTagName", tagname);
            return;
         }
         else
            TortoiseRegistry::WriteString("FailedTagName", "");
      }
   }
}


void TortoiseAct::PerformBranchMenu()
{
   std::string branchname;
   bool checkunmodified;
   int action;
   if (DoBranchDialog(0, branchname, checkunmodified, action, GetFetchButtonDirs()))
   {
      CVSAction glue(0);
      glue.SetProgressFinishedCaption(Printf(_("Finished branching in %s"),
                                             GetDirList().c_str()));
      for (unsigned int j = 0; j < myDirectoryGroups.size(); ++j)
      {
         DirectoryGroup& group = myDirectoryGroups[j];

         CVSStatus::CVSVersionInfo serverversion = CVSStatus::GetServerCVSVersion(group.myCVSRoot, &glue);
         MakeArgs args;
         args.add_option("tag");
         if (checkunmodified)
            args.add_option("-c");
         switch (action)
         {
         case BranchTagDlg::TagActionCreate:
            args.add_option("-b");
            break;
         case BranchTagDlg::TagActionMove:
            args.add_option("-b");
            args.add_option("-F");
            // -B was introduced in 1.11.1p2
            if (serverversion >= CVSStatus::CVSVersionInfo("1.11.1p2"))
               args.add_option("-B");
            break;
         case BranchTagDlg::TagActionDelete:
            args.add_option("-d");
            // -B was introduced in 1.11.1p2
            if (serverversion >= CVSStatus::CVSVersionInfo("1.11.1p2"))
               args.add_option("-B");
            break;
         default:
            DoMessageDialog(glue.GetProgressDialog(), _("Internal error: Invalid action"));
            return;
         }
         args.add_option(branchname);
         for (unsigned int i = 0; i < group.size(); ++i)
            args.add_arg(group[i].myRelativeFile);
         glue.SetProgressCaption(Printf(_("Branching in %s"), wxText(group.myDirectory).c_str()));
         if (!glue.Command(group.myDirectory, args))
         {
            return;
         }
      }
   }
}


void TortoiseAct::PerformMergeMenu()
{
   std::string fromtag;
   std::string rev1, rev2;
   unsigned int i;

   if (myParameters.GetValueCount("-r") > 0)
   {
      // Merging between two revisions
      rev1 = myParameters.GetValue("-r", 0);
      rev2 = myParameters.GetValue("-r", 1);
   }

   bool createdirs = false; // Does not make sense for single files
   bool suppressKeywordExpansion = false;
   
   std::string bugnumber = TortoiseRegistry::ReadString("Dialogs\\Commit\\Bug Number");

   if (!(TortoiseRegistry::ReadBoolean("Dialogs\\Commit\\Use Bug", false)))
       bugnumber.clear();
   
   if (rev1.empty())
   {
      // Single revision
      createdirs = GetBooleanPreference("Create Added Directories");

      // Determine if we have a directory argument
      bool gotFolder = false;
      unsigned int j;
      for (j = 0; !gotFolder && (j < myDirectoryGroups.size()); ++j)
      {
         DirectoryGroup& group = myDirectoryGroups[j];
         if (group.empty())
         {
            gotFolder = true;
            break;
         }
         for (unsigned int i = 0; i < group.size(); ++i)
         {
            if (IsDirectory(group[i].myAbsoluteFile.c_str()))
            {
               gotFolder = true;
               break;
            }
         }
      }
      if (!DoMergeDialog(0, rev1, rev2, bugnumber, GetFetchButtonDirs(), gotFolder, createdirs, suppressKeywordExpansion))
         return;
   }
   
   std::vector<std::string> conflictFiles;
    
   CVSAction glue(0);
   unsigned int j;
   for (j = 0; j < myDirectoryGroups.size(); ++j)
   {
      std::vector<std::string> binFiles;
      std::vector<std::string> textFiles;

      // If any files are read-only, do a CVS Edit on them
      DirectoryGroup& group = myDirectoryGroups[j];
      glue.SetProgressCaption(Printf(_("Editing before merge in %s"), 
                                     wxText(group.myDirectory).c_str()));

      // Sort into text/binary first
      // TODO: Does it make sense to try to merge binary files?
      for (unsigned int i = 0; i < group.size(); ++i)
      {
         std::string absoluteFile = group[i].myAbsoluteFile;
            
         binFiles.clear();
         textFiles.clear();
         if (!IsDirectory(absoluteFile.c_str()) && IsReadOnly(absoluteFile.c_str()))
         {
            if (CVSStatus::IsBinary(absoluteFile))
               binFiles.push_back(group[i].myRelativeFile);
            else
               textFiles.push_back(group[i].myRelativeFile);
         }
      }

      // For binary files, we do a 'cvs update' first, followed by 'cvs edit -c'
      if (!binFiles.empty())
      {
          if (!group.myCVSServerFeatures.Initialize())
              TortoiseFatalError(_("Could not connect to server"));

         MakeArgs argsUpdate;
         MakeArgs argsEdit;
         argsUpdate.add_option("update");
         argsEdit.add_option("edit");
         argsEdit.add_option("-z");
         if (group.myCVSServerFeatures.SupportsExclusiveEdit())
         {
             if (GetIntegerPreference("Edit policy") == TortoisePreferences::EditPolicyExclusiveACL)
                 argsEdit.add_option("-A");
             if (GetIntegerPreference("Edit policy") >= TortoisePreferences::EditPolicyExclusive)
                 argsEdit.add_option("-x");
             else if (GetIntegerPreference("Edit policy") == TortoisePreferences::EditPolicyConcurrentExclusive)
                 argsEdit.add_option("-c");
         }
         for (unsigned int i = 0; i < binFiles.size(); ++i)
         {
            argsUpdate.add_arg(binFiles[i]);
            argsEdit.add_arg(binFiles[i]);
         }
         if (!glue.Command(group.myDirectory, argsUpdate))
            return;
         if (!glue.Command(group.myDirectory, argsEdit))
            return;
      }

      // For text files, we just do 'cvs edit'
      if (!textFiles.empty())
      {
          if (!group.myCVSServerFeatures.Initialize())
              TortoiseFatalError(_("Could not connect to server"));

         // Build the argument lists
         MakeArgs argsEdit;
         argsEdit.add_option("edit");
         argsEdit.add_option("-z");
         if (group.myCVSServerFeatures.SupportsExclusiveEdit())
         {
             if (GetIntegerPreference("Edit policy") == TortoisePreferences::EditPolicyExclusiveACL)
                 argsEdit.add_option("-A");
             if (GetIntegerPreference("Edit policy") >= TortoisePreferences::EditPolicyExclusive)
                 argsEdit.add_option("-x");
             else if (GetIntegerPreference("Edit policy") == TortoisePreferences::EditPolicyConcurrentExclusive)
                 argsEdit.add_option("-c");
         }
         for (unsigned int i = 0; i < textFiles.size(); ++i)
            argsEdit.add_arg(textFiles[i]);
         if (!glue.Command(group.myDirectory, argsEdit))
            return;
      }
   }

   // Now for the actual merge
   for (j = 0; j < myDirectoryGroups.size(); ++j)
   {
      glue.SetProgressFinishedCaption(Printf(_("Finished merge in %s"),
                                             GetDirList().c_str()));
      DirectoryGroup& group = myDirectoryGroups[j];
            
      MakeArgs args;
      args.add_option("update");
      if (suppressKeywordExpansion)
          args.add_option("-kk");
      if (!bugnumber.empty())
      {
          args.add_option("-B");
          args.add_option(bugnumber);
      }
      args.add_option("-j");
      args.add_option(rev1);
      if (!rev2.empty())
      {
         args.add_option("-j");
         args.add_option(rev2);
      }
      if (createdirs)
         args.add_option("-d"); // create missing directories that exist in the repository
      for (i = 0; i < group.size(); ++i)
         args.add_arg(group[i].myRelativeFile);
      glue.SetProgressCaption(Printf(_("Merging in %s"), wxText(group.myDirectory).c_str()));
      if (!glue.Command(group.myDirectory, args))
         return;

      // Check for conflicts
      const std::list<std::string>& out = glue.GetStdOutList();
      ParseConflicts(group, out, conflictFiles);
      // Edit conflict file if appropriate
      if (conflictFiles.size() > 0)
      {
         glue.LockProgressDialog(true);
         DoConflictListDialog(glue.GetProgressDialog(), conflictFiles);
         glue.LockProgressDialog(false);
      }
   }

   if (FileSelected() && glue.AtLeastOneSuccess())
      RefreshExplorerWindow();
}



void TortoiseAct::PerformHelp()
{
   std::string url = GetTortoiseDirectory() + "Help.html";
   LaunchURL(url);
}

void TortoiseAct::PerformPrefs()
{
   std::string dir;
   if (!myDirectoryGroups.empty())
      dir = myDirectoryGroups[0][0].myAbsoluteFile;

   DoPreferencesDialog(0, dir);
}

void TortoiseAct::PerformAbout()
{
#if defined(_DEBUG)
   if (IsControlDown() && IsShiftDown())
   {
      MessageBoxA(0, "Testing crash dump in TortoiseAct", "Test", 0);
      int* p = 0;
      *p = 0;
   }
#endif

   std::string cvsclientversion = CVSStatus::GetClientCVSVersionString(true);
   std::string cvsserverversion;
   if (!myDirectoryGroups.empty())
   {
      std::string root = CVSStatus::CVSRootForPath(myDirectoryGroups[0][0].myAbsoluteFile);
      if (!root.empty())
         cvsserverversion = CVSStatus::GetServerCVSVersionString(root);
   }

   // Get SSH version
   std::string sshversion;
   std::string loginapp = Quote(GetStringPreference("External SSH Application")) + " -V";

   SECURITY_ATTRIBUTES sa;
   ZeroMemory(&sa, sizeof(sa));
   sa.nLength = sizeof(sa);
   sa.bInheritHandle = TRUE;

   std::string file = UniqueTemporaryFile();
   HANDLE h = CreateFileA(file.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, 
                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
   STARTUPINFOA si;
   PROCESS_INFORMATION pi;
   memset(&si, 0, sizeof(si));
   si.cb = sizeof(si);
   si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
   si.wShowWindow = SW_HIDE;
   si.hStdOutput = h;
   si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
   si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
   if (CreateProcessA(NULL, (LPSTR) loginapp.c_str(), &sa, &sa, TRUE, 0, 0, 0, &si, &pi))
   {
      CloseHandle(pi.hThread);
      WaitForSingleObject(pi.hProcess, INFINITE);
      CloseHandle(pi.hProcess);
      std::ifstream is(file.c_str());
      std::getline(is, sshversion, '\n');
   }
   wxString wideSSHVersion(wxText(sshversion));
   if (sshversion.empty())
   {
      wideSSHVersion = wxString(wxT("[")) + wxString(_("unknown")) + wxString(wxT("]"));
   }
   CloseHandle(h);
   DeleteFileA(file.c_str());

   DoAboutDialog(0, VersionString, cvsclientversion, cvsserverversion, wideSSHVersion);
}

void TortoiseAct::RefreshExplorerWindow(const std::vector<std::string>* files)
{
#if 1
    PostMessage(myRemoteHandle, WM_COMMAND, 41504, 0);
#else
   TDEBUG_ENTER("RefreshExplorerWindow");
   if (files)
   {
      std::set<std::string> dirs;
      for (std::vector<std::string>::const_iterator it = files->begin(); it != files->end(); ++it)
      {
         std::string dir = *it;
         if (!IsDirectory(dir.c_str()))
            dir = GetDirectoryPart(dir);
         TDEBUG_TRACE("Dir: " << dir);
         dirs.insert(dir);
         for (std::set<std::string>::iterator it = dirs.begin(); it != dirs.end(); ++it)
             SHChangeNotify(SHCNE_UPDATEDIR,
                            // The contents of an existing folder have changed, but the folder still exists and has not been renamed.
                            // SHCNF_IDLIST or SHCNF_PATH must be specified in uFlags.
                            // dwItem1 contains the folder that has changed.
                            SHCNF_PATHA,
                            it->c_str(),
                            0);
      }
   }
   else
   {
      const size_t n = myDirectoryGroups.size();
      for (size_t i = 0; i < n; ++i)
         SHChangeNotify(SHCNE_UPDATEDIR,
                        SHCNF_PATHA,
                        myDirectoryGroups[i].myDirectory.c_str(),
                        0);
   }
#endif
}


void TortoiseAct::RefreshExplorerWindow(const std::string& dir)
{
   std::vector<std::string> dirs;
   dirs.push_back(dir);
   RefreshExplorerWindow(&dirs);
}

// Returns true if they have selected at least one file
// Returns false if they have entirely selected directories
bool TortoiseAct::FileSelected()
{
   for (unsigned int j = 0; j < myDirectoryGroups.size(); ++j)
   {
      DirectoryGroup& group = myDirectoryGroups[j];
      
      for (unsigned int i = 0; i < group.size(); ++i)
         if (!IsDirectory(group[i].myAbsoluteFile.c_str()))
            return true;
   }
   
   return false;
}

void TortoiseAct::PerformMakePatchMenu()
{
   TDEBUG_ENTER("PerformMakePatchMenu");

   std::string selectedFile;
   if (myAllFiles.size() == 1)
       selectedFile = myDirectoryGroups.mySingleAbsolute;
   
   // Get a list of all the modified/added/removed files
   FilesWithBugnumbers modified, added, removed;
   for (unsigned int i = 0; i < myAllFiles.size(); ++i)
       CVSStatus::RecursiveScan(myAllFiles[i], &modified, &added, &removed, 0, 0, 0, 0);

   // Sort them into order
   std::sort(modified.begin(), modified.end());
   std::sort(added.begin(), added.end());
   std::sort(removed.begin(), removed.end());

   std::vector<std::string> userSelection;
   std::vector<std::string> additionalFiles;
   if (!DoMakePatchDialog(0, myDirectoryGroups, &modified, &added, &removed,
                          userSelection, additionalFiles))
      return;

   // Make patch file in temporary place
   std::string tempFilename = UniqueTemporaryFile();
   
   std::string patchFilename;
   if (!userSelection.empty())
   {
       // Redo the preprocessing into groups
       myAllFiles = userSelection;
       if (!PreprocessFileList())
       {
           // TODO: Better error code everywhere
           TortoiseFatalError(_("Failed to reprocess file list"));
       }

       CVSAction glue(0);
       glue.SetProgressFinishedCaption(Printf(_("Finished making patch in %s"),
                                              GetDirList().c_str()));
       glue.SetStreamFile(tempFilename);
       glue.SetCloseIfOK(true);
       for (unsigned int j = 0; j < myDirectoryGroups.size(); ++j)
       {
           DirectoryGroup& group = myDirectoryGroups[j];
           if (patchFilename.empty())
               patchFilename = group.myDirectory;
           else
           {
               patchFilename += ".";
               patchFilename += ExtractLastPart(group.myDirectory);
           }

           MakeArgs args;
           args.add_option("diff");
           args.add_option("-u");
           for (unsigned int i = 0; i < group.size(); ++i)
           {
               const std::string relFile = group[i].myRelativeFile;
               args.add_arg(relFile);
           }

           glue.SetProgressCaption(Printf(_("Making patch in %s"), 
                                          wxText(group.myDirectory).c_str()));
           if (!glue.Command(group.myDirectory, args))
               return;
       }

       if (!selectedFile.empty())
           patchFilename = selectedFile;

       patchFilename += ".patch";
   
       // See if any changes were made
       if (!FileExists(tempFilename.c_str()))
       {
           DoMessageDialog(0, _("There are no patches.\n\n\
All the files and folders that you selected are unchanged since you last performed a commit."));
           return;
       }
   }
   else if (additionalFiles.empty())
       return;

   // Query where to save the file
   wxString filter(_("Patch files"));
   filter += wxT(" (*.patch)|*.patch|");
   filter += _("Text files");
   filter += wxT(" (*.txt)|*.txt|");
   filter += _("All files");
   filter += wxT("|*.*");

   patchFilename = DoSaveFileDialog(0, _("Save patch file"), patchFilename, filter);
   if (patchFilename.empty())
      return;

   // Now append the patches for the added files
   std::ofstream os(tempFilename.c_str(), std::ios::app | std::ios::binary);
   for (size_t i = 0; i < additionalFiles.size(); ++i)
   {
      std::string filename = additionalFiles[i].substr(myDirectoryGroups.mySingleAbsolute.size());
      FindAndReplace(filename, std::string("\\"), std::string("/"));
      if (filename[0] == '/')
         filename = filename.substr(1);
      os << "--- " << filename << std::endl;
      os << "+++ " << filename << std::endl;
      // Read in file, counting # of lines
      int numberOfLines = 0;
      std::ifstream is(additionalFiles[i].c_str());
      std::string fileContents;
      while (1)
      {
         std::string line;
         if (!std::getline(is, line) && line.empty())
            break;
         ++numberOfLines;
         fileContents += "+";
         fileContents += line;
         fileContents += "\r\n";
      }
      os << "@@ -0,0 +1," << numberOfLines << " @@" << std::endl;
      os << fileContents << std::endl;
   }
   os.close();
   
   // Delete any existing file
   DeleteFileA(patchFilename.c_str());

   // Convert to DOS format so that Notepad can cope
   std::ifstream in(tempFilename.c_str());
   std::ofstream out(patchFilename.c_str());
   LineReader<std::string> lr(&in, LineReader<std::string>::CrlfStyleAutomatic);
   std::string line;
   while(lr.ReadLine(line))
      out << line << std::endl;
   DeleteFileA(tempFilename.c_str());
   
   // Launch it so the user can check it seems OK
   std::string command = GetTextEditorCommand(patchFilename.c_str());
   LaunchCommand(command, false);
}

void TortoiseAct::PerformApplyPatchMenu()
{
#if 0
    TDEBUG_ENTER("PerformApplyPatchMenu");
    HANDLE abortEvent = CreateEvent(0, TRUE, FALSE, 0);
    ProgressDialog* progressDialog = CreateProgressDialog(0,
                                                          abortEvent,
                                                          ProgressDialog::acDefaultNoClose);
    progressDialog->Update();
    ThreadSafeProgress* progress = ThreadSafeProgress::CreateThreadSafeProgress(progressDialog);
    HANDLE uiEvent = CreateEvent(0, TRUE, FALSE, 0);
    char buf[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, buf);
    SetCurrentDirectoryA(GetDirectoryPart(myDirectoryGroups.mySingleAbsolute).c_str());
    Patch(progress, myDirectoryGroups.mySingleAbsolute.c_str());
    while (1)
    {
        if (WaitForSingleObject(abortEvent, 200) == WAIT_OBJECT_0)
        {
            ResetEvent(abortEvent);
            break;
        }
        progress->Main(uiEvent);
        ResetEvent(uiEvent);
    }
    CloseHandle(uiEvent);
    CloseHandle(abortEvent);
    SetCurrentDirectoryA(buf);
#endif
}

void TortoiseAct::PerformEdit(bool exclusive)
{
   TDEBUG_ENTER("TortoiseAct::PerformEdit");
   myDirectoryGroups.myDoNotGroupDirectories = true;
   PreprocessFileList();
   CVSAction* glue = 0;

   int numberOfFiles = 0;
   for (unsigned int j = 0; j < myDirectoryGroups.size(); ++j)
   {
      DirectoryGroup& group = myDirectoryGroups[j];

      if (!group.myCVSServerFeatures.Initialize())
         TortoiseFatalError(_("Could not connect to server"));
      if (!group.myCVSClientFeatures.Initialize())
         TortoiseFatalError(_("Could not connect to client"));

      // Sort into text/binary first
      std::vector<std::string> binFiles;
      std::vector<std::string> textFiles;
      if (!group.size())
      {
         TDEBUG_TRACE("Dir '" << group.myDirectory << "'(1)");
         // This is a directory. To make sure we handle binary files correctly,
         // we have to add all unmodified files to our lists.
         std::vector<std::string> unmodified;
         CVSStatus::RecursiveScan(group.myDirectory, 0, 0, 0, 0, &unmodified, 0, false);
         group.myEntries.clear();
         std::string directory = group.myDirectory;
         directory = EnsureTrailingDelimiter(directory);
         for (std::vector<std::string>::const_iterator it = unmodified.begin(); 
              it != unmodified.end(); ++it)
         {
            TDEBUG_TRACE("Adding '" << *it << "'");
            group.myEntries.push_back(new DirectoryGroup::Entry(group, *it));
         }
         group.ShortenPaths();
      }

      std::vector<std::string> absFilesInSubdirs;
      std::vector<std::string> relFilesInSubdirs;
      for (unsigned int i = 0; i < group.size(); ++i)
      {
         std::string absoluteFile = group[i].myAbsoluteFile;
            
         if (IsDirectory(absoluteFile.c_str()))
         {
            TDEBUG_TRACE("Dir '" << absoluteFile << "' (2)");
            // This is a directory. To make sure we handle binary files correctly,
            // we have to add all unmodified files to our lists.
            std::vector<std::string> unmodified;
            CVSStatus::RecursiveScan(absoluteFile, 0, 0, 0, 0, &unmodified, 0, false);
            group.myEntries.clear();
            std::string directory = group.myDirectory;
            directory = EnsureTrailingDelimiter(directory);
            for (std::vector<std::string>::const_iterator it = unmodified.begin(); 
                 it != unmodified.end(); ++it)
            {
               TDEBUG_TRACE("Adding '" << *it << "'");
               if (CVSStatus::IsBinary(*it) ||
                   (CVSStatus::GetFileOptions(*it) & CVSStatus::foExclusive))
                  binFiles.push_back(it->substr(directory.length()));
               else
                  textFiles.push_back(it->substr(directory.length()));
            }
         }
         else
         {
            TDEBUG_TRACE("File: '" << absoluteFile << "'");
            // File
            if (CVSStatus::IsBinary(absoluteFile) ||
                (CVSStatus::GetFileOptions(absoluteFile) & CVSStatus::foExclusive))
               binFiles.push_back(group[i].myRelativeFile);
            else
               textFiles.push_back(group[i].myRelativeFile);
         }
      }

      // Show dialog to allow entering edit comment, if enabled
      bool usebug = false;
      std::string comment;
      std::string bugnumber;
      if ((!binFiles.empty() || !textFiles.empty()) &&
          GetBooleanPreference("Show edit options") &&
          group.myCVSServerFeatures.SupportsEditComment())
      {
         // Retrieve a comment about them
         if (!DoEditDialog(0, comment, bugnumber, usebug))
            return;
      }

      if (!glue)
         glue = new CVSAction(0);
      glue->SetProgressFinishedCaption(Printf(_("Finished edit in %s"),
                                             GetDirList().c_str()));
      glue->SetProgressCaption(Printf(_("Editing in %s"), 
                                     wxText(group.myDirectory).c_str()));
      
      // For binary files, we do a 'cvs update' first, followed by 'cvs edit -xc'
      if (!binFiles.empty())
      {
         MakeArgs argsUpdate;
         MakeArgs argsEdit;
         argsUpdate.add_option("update");
         argsEdit.add_option("edit");
         argsEdit.add_option("-z");
         if (group.myCVSServerFeatures.SupportsExclusiveEdit())
         {
             if (GetIntegerPreference("Edit policy") == TortoisePreferences::EditPolicyExclusiveACL)
                 argsEdit.add_option("-A");
             if (GetIntegerPreference("Edit policy") >= TortoisePreferences::EditPolicyExclusive)
                 argsEdit.add_option("-x");
             else if (GetIntegerPreference("Edit policy") == TortoisePreferences::EditPolicyConcurrentExclusive)
                 argsEdit.add_option("-c");
         }
             
         // Optional edit comment
         if (!comment.empty())
         {
            group.myCVSClientFeatures.AddOptionM(argsEdit);
            argsEdit.add_option(comment);
         }
         // Optional bug number
         if (usebug)
         { 
            argsEdit.add_option("-b"); 
            argsEdit.add_option(bugnumber); 
         }

         for (unsigned int i = 0; i < binFiles.size(); ++i)
         {
            TDEBUG_TRACE("Binary: '" << binFiles[i] << "'");
            argsUpdate.add_arg(binFiles[i]);
            argsEdit.add_arg(binFiles[i]);
         }
         if (!glue->Command(group.myDirectory, argsUpdate))
            goto Cleanup;
         if (!glue->Command(group.myDirectory, argsEdit))
            goto Cleanup;
         numberOfFiles += static_cast<int>(binFiles.size());
      }
   
      // For text files, we just do 'cvs edit', unless exclusive edit is requested
      if (!textFiles.empty())
      {
         // Build the argument lists
         MakeArgs argsEdit;
         argsEdit.add_option("edit");
         argsEdit.add_option("-z");
         if (exclusive ||
             group.myCVSServerFeatures.SupportsExclusiveEdit())
         {
             if (GetIntegerPreference("Edit policy") == TortoisePreferences::EditPolicyExclusiveACL)
                 argsEdit.add_option("-A");
             if (GetIntegerPreference("Edit policy") >= TortoisePreferences::EditPolicyExclusive)
                 argsEdit.add_option("-x");
             else if (GetIntegerPreference("Edit policy") == TortoisePreferences::EditPolicyConcurrentExclusive)
                 argsEdit.add_option("-c");
         }
         // Optional edit comment
         if (!comment.empty())
         {
            group.myCVSClientFeatures.AddOptionM(argsEdit);
            argsEdit.add_option(comment);
         }
         // Optional bug number
         if (usebug)
         { 
            argsEdit.add_option("-b"); 
            argsEdit.add_option(bugnumber); 
         }
         for (unsigned int i = 0; i < textFiles.size(); ++i)
         {
            TDEBUG_TRACE("Text: '" << textFiles[i] << "'");
            argsEdit.add_arg(textFiles[i]);
         }
         if (!glue->Command(group.myDirectory, argsEdit))
            break;
         numberOfFiles += static_cast<int>(textFiles.size());
      }
   }

Cleanup:
   if (glue && glue->AtLeastOneSuccess())
      RefreshExplorerWindow();
   delete glue;
}

void TortoiseAct::PerformUnedit()
{
   CVSAction glue(0);
   glue.SetProgressFinishedCaption(Printf(_("Finished unedit in %s"), GetDirList().c_str()));

   for (unsigned int j = 0; j < myDirectoryGroups.size(); ++j)
   {
      DirectoryGroup& group = myDirectoryGroups[j];
      
      MakeArgs args;
#ifdef MARCH_HARE_BUILD
      if (myDirectoryGroups.GetBooleanPreference("Checkout files Read Only"))
          args.add_global_option("-r");
#endif
      args.add_option("unedit");
      for (unsigned int i = 0; i < group.size(); ++i)
         args.add_arg(group[i].myRelativeFile);
      glue.SetProgressCaption(Printf(_("Unediting in %s"), wxText(group.myDirectory).c_str()));
      if (!glue.Command(group.myDirectory, args))
         goto Cleanup;
   }

Cleanup:   
   if (glue.AtLeastOneSuccess())
      RefreshExplorerWindow();
}

void TortoiseAct::PerformListEditors()
{
   bool result = true;
   EditorListDialog::EditedFileList editedfiles;
   // Make the progress dialog close before showing the Editors dialog
   {
      CVSAction glue(0);
      glue.SetProgressFinishedCaption(Printf(_("Finished list editors in %s"),
                                             GetDirList().c_str()));
      glue.SetClose(CVSAction::CloseIfNoErrors);
      
      for (unsigned int j = 0; result && (j < myDirectoryGroups.size()); ++j)
      {
         DirectoryGroup& group = myDirectoryGroups[j];
         result = GetEditors(glue, group, editedfiles);
      }
   }
   
   if (result)
   {
      // Now show the list of edited files in the dialog
      DoEditorListDialog(0, editedfiles);
   }
}

// Run 'cvs editors' on the files in 'group' and parse the output into 'editedfiles'.
// Each element of 'editedfiles' is a (file, editors) pair where 'editors' is a comma-separated
// list of users editing the file.
static bool GetEditors(CVSAction& glue,
                       const DirectoryGroup& group,
                       EditorListDialog::EditedFileList& editedfiles)
{
   MakeArgs args;
   args.add_option("editors");
   CVSServerFeatures features;
   features.SetCVSRoot(group.myCVSRoot);
   features.Initialize(&glue);
   if (features.SupportsVerboseEditors())
       args.add_option("-v");
   for (unsigned int i = 0; i < group.size(); ++i)
      args.add_arg(group[i].myRelativeFile);
   
   std::string streamfile = UniqueTemporaryFile();
   
   glue.SetStreamFile(streamfile);
   bool result = glue.Command(group.myDirectory, args);
   
   // Parse the log output
   std::ifstream ifs(streamfile.c_str());
   std::string workfile;
   std::vector<EditorListDialog::EditedFile::Editor> editors;
   
   while (result)
   {
      std::string line;
      if (!std::getline(ifs, line) && line.empty())
      {
         // End of output, store what we got
         if (!workfile.empty())
            editedfiles.push_back(EditorListDialog::EditedFile(EnsureTrailingDelimiter(group.myDirectory) + workfile,
                                                               editors));
         break;
      }
      
      // Skip lines starting with '?'
      if (line[0] == '?')
      {
         continue;
      }
      
      unsigned int i = 0;
      
      // Parse name. If this line starts with TAB, it applies to the file
      // listed previously.
      if (line[i] == '\t')
      {
         ++i;
      }
      else
      {
         i = 0;
         // New file, store info for previous
         if (!workfile.empty())
            editedfiles.push_back(EditorListDialog::EditedFile(EnsureTrailingDelimiter(group.myDirectory) + workfile,
                                                               editors));
         
         std::string name;
         while (i < line.size() && (line[i] != '\t'))
         {
            ++i;
         }
         if (!isspace(line[i]))
         {
            break;
         }
         name = line.substr(0, i);
         ++i;
         workfile = name;
         editors.clear();
      }
      
      // Parse name of editor
      int whoStart = i;
      while (i < line.size() && (line[i] != '\t'))
      {
         ++i;
      }
      if (!isspace(line[i]))
      {
         break;
      }
      int whoEnd = i;
      ++i;
      
      // Parse date
      int dateStart = i;
      while (i < line.size() && (line[i] != '\t'))
      {
         ++i;
      }
      if (!isspace(line[i]))
      {
         break;
      }
      int dateEnd = i - 4; // Remove "GMT"
      ++i;

      // Parse host
      int hostStart = i;
      while (i < line.size() && (line[i] != '\t'))
      {
         ++i;
      }
      if (!isspace(line[i]))
      {
         break;
      }
      int hostEnd = i;
      ++i;

      // "Parse" path
      int pathstart = i;
      int pathEnd = line.size();

      // Check for bug number
      std::string bugNumber;
      if (features.SupportsVerboseEditors())
      {
          int bugStart = pathstart+1;
          while (bugStart < pathEnd)
          {
              if (line[bugStart] == '\t')
              {
                  bugNumber = line.substr(bugStart+1);
                  pathEnd = bugStart;
                  break;
              }
              ++bugStart;
          }
      }
      
      time_t date = 0;
      asctime_to_time_t(line.substr(dateStart, dateEnd-dateStart).c_str(), &date);
      editors.push_back(EditorListDialog::EditedFile::Editor(line.substr(whoStart, whoEnd-whoStart),
                                                             date,
                                                             line.substr(hostStart, hostEnd-hostStart),
                                                             line.substr(pathstart, pathEnd-pathstart),
                                                             bugNumber));
   }
   
   // delete file
   DeleteFileA(streamfile.c_str());
   
   return result;
}

bool TortoiseAct::ParseConflicts(const DirectoryGroup& group,
                                 const std::list<std::string>& cvsOutput, 
                                 std::vector<std::string>& conflictFiles)
{
   // For each line starting with "C ", add the following filename 
   // to the list of conflicting files.
   CVSStatus::InvalidateCache();
   std::list<std::string>::const_iterator it;
   for (it = cvsOutput.begin(); it != cvsOutput.end(); it++)
   {
      const std::string& line = *it;
      
      // Ignore lines that are shorter than 2 chars
      if (line.length() < 2)
      {
         continue;
      }

      if ((line[0] == 'C') && (line[1] == ' '))
      {
         std::string file(group.myDirectory);
         file = EnsureTrailingDelimiter(file);
         int i = 2;
         while (isprint(line[i]))
            ++i;
         file += line.substr(2, i-2);
         FindAndReplace<std::string>(file, "/", "\\");
         // We only process non-binary files that need a merge
         if (CVSStatus::GetFileStatus(file) == CVSStatus::STATUS_CONFLICT)
         {
            // Do not edit conflict file if the file is binary
            if (!CVSStatus::IsBinary(file))
               conflictFiles.push_back(file);
         }
      }
   }
   
   return true;
}



bool TortoiseAct::ParseCommitted(const DirectoryGroup& group,
                                 const std::list<std::string>& cvsOutput, 
                                 std::vector<std::string>& committedFiles)
{
    TDEBUG_ENTER("ParseCommitted");
    // For each line containing "<--", add the following filename to the list of edited files.
    const unsigned int phraselengthCheckin = static_cast<unsigned int>(strlen(CVSPARSE(" <-- ")));
    const unsigned int phraselengthRemove = static_cast<unsigned int>(strlen(CVSPARSE("Removing ")));
    std::list<std::string>::const_iterator it;
    for (it = cvsOutput.begin(); it != cvsOutput.end(); it++)
    {
        const std::string& line = *it;

        // Ignore lines that are shorter than 4 chars
        if (line.length() < std::min(phraselengthCheckin, phraselengthRemove))
            continue;

        std::string::size_type pos = line.find(CVSPARSE(" <-- "));
        if (pos != std::string::npos)
        {
            TDEBUG_TRACE("Found committed: " << line);
            // Get filename after "<--"
            std::string file(line.substr(pos+6));
            if (!file.empty() && (file[file.size()-1] == '\r'))
                file = file.substr(0, file.size()-1);
            TDEBUG_TRACE("file: " << file);
            committedFiles.push_back(file);
        }
        else if ((line.substr(0, phraselengthRemove) == CVSPARSE("Removing ")))
        {
            std::string file(group.myDirectory);
            file = EnsureTrailingDelimiter(file);
            int i = phraselengthRemove + 1;
            while (line[i] != ';')
                ++i;
            file += line.substr(phraselengthRemove, i - phraselengthRemove);
            FindAndReplace<std::string>(file, "/", "\\");
            committedFiles.push_back(file);
        }
    }
   
    return true;
}



bool TortoiseAct::ParseIdentical(const DirectoryGroup& group,
                                 const std::list<std::string>& cvsOutput, 
                                 std::vector<std::string>& identicalFiles)
{
   std::string file;
   std::string::size_type p;

   std::list<std::string>::const_iterator it;
   for (it = cvsOutput.begin(); it != cvsOutput.end(); it++)
   {
      const std::string& line = *it;

      p = line.find(CVSPARSE(" already contains the differences between "));
      if (p != std::string::npos)
      {
         file = line.substr(0, p);
         file = EnsureTrailingDelimiter(group.myDirectory) + file;
         identicalFiles.push_back(file);
      }
   }
   
   return true;
}



bool TortoiseAct::EditConflictFile(const std::string& filename)
{
   // We can't edit binary files
   if (CVSStatus::IsBinary(filename))
   {
      DoMessageDialog(0, _("Merging changes in binary files is not allowed."));
      return false;
   }
#if 0
   return DoConflictDialog(0, filename);
#else

   std::string fileMine;
   std::string fileYours;
   std::string command;
   bool bContainsMarkers = false;
   bool forceQuery = false;
   bool bNestedConflicts;
   DWORD dwMsgBoxFlags;
   DWORD id;

   if (IsControlDown())
      forceQuery = true;

   std::string externalApp = GetExternalApplication("Merge",
                                                    myDirectoryGroups,
                                                    forceQuery);
   // If external app is empty, launch default text editor
   if (externalApp.empty())
   {
      std::string editor = GetTextEditorCommand(filename.c_str());
      LaunchCommand(editor, true);
      return true;
   }
   forceQuery = false;

   // Create new directory for merge
   std::string mergeDir = UniqueTemporaryDir();
   AutoDirectoryDeleter mergeDirDeleter(mergeDir);
   
   // Create names for merge files
   fileMine = MakeRevFilename(mergeDir + "\\" + ExtractLastPart(filename), "mine");
   fileYours = MakeRevFilename(mergeDir + "\\" + ExtractLastPart(filename), "yours");
   
   // Remove read-only flags so overwriting works in case files exists
   SetFileReadOnly(fileMine.c_str(), false);
   SetFileReadOnly(fileYours.c_str(), false);

   AutoFileDeleter fileMineDeleter(fileMine);
   AutoFileDeleter fileYoursDeleter(fileYours);
   
#if 0 // Not ready to use yet
   // Do we have a unicode file?
   bIsUnicode = CVSStatus::GetFileFormat(filename) == CVSStatus::fmtUnicode;
   if (bIsUnicode)
   {
      // Create files from conflict file
      ConflictParser<std::wstring> parser(filename, fileMine, fileYours);
      bContainsMarkers = parser.Parse();
   }
   else
   {
      // Create files from conflict file
      ConflictParser<std::string> parser(filename, fileMine, fileYours);
      bContainsMarkers = parser.Parse();
   }
#else

   bContainsMarkers = ConflictParser::ParseFile(filename, fileMine, fileYours, 
                                                bNestedConflicts);
#endif

   // if no conflict markers are in the file, exit
   if (!bContainsMarkers)
   {
      DoMessageDialog(0, _("The selected file doesn't contain any conflict markers"));
      RefreshExplorerWindow(filename);
      return true;
   }

   // Store write time
   FILETIME ftLastWriteTimeOld, ftLastWriteTimeNew;
   GetLastWriteTime(fileYours.c_str(), &ftLastWriteTimeOld);

   // Mark "mine" as read-only
   SetFileReadOnly(fileMine.c_str(), true);

   wxString message = Printf(_(
"File containing your changes: %s\n\
File containing repository changes: %s\n\
Please merge your changes into: %s"), 
                             wxText(ExtractLastPart(fileMine.c_str())).c_str(),
                             wxText(ExtractLastPart(fileYours.c_str())).c_str(),
                             wxText(ExtractLastPart(fileYours.c_str())).c_str());
   DoMessageDialog(0, message);

   // Perform merge, waiting for it to finish (so we can delete files)
   do
   {
      externalApp = GetExternalApplication("Merge",
                                           myDirectoryGroups,
                                           forceQuery);
      forceQuery = false;
      if (!externalApp.empty())
      {
         // Perform merge, waiting for it to finish (so we can delete file)
         forceQuery = !RunExternalMerge(fileMine,
                                        fileYours,
                                        myDirectoryGroups);
      }
      else
      {
          RefreshExplorerWindow(filename);
          return false;
      }
   } while (forceQuery);

   GetLastWriteTime(fileYours.c_str(), &ftLastWriteTimeNew);
   dwMsgBoxFlags = MB_YESNO | MB_ICONQUESTION;
   if (CompareFileTime(&ftLastWriteTimeOld, &ftLastWriteTimeNew) >= 0)
      dwMsgBoxFlags |= MB_DEFBUTTON2;

   // Ask the user whether he wants to apply the merged file
   message = Printf(_(
"Do you want to overwrite your working copy of \"%s\" with your merge results in \"%s\"?"), 
                      wxText(ExtractLastPart(filename)).c_str(), wxText(fileYours).c_str());

   id = MessageBox(myRemoteHandle, message.c_str(), _("Save merged file"), dwMsgBoxFlags);

   if (id == IDYES)
   {
      // Copy the file
      bool bReadOnly = IsReadOnly(filename.c_str());
      if (bReadOnly)
         SetFileReadOnly(filename.c_str(), false);

      if (!CopyFileA(fileYours.c_str(), filename.c_str(), false))
      {
         message = Printf(_("Failed to overwrite %s"), wxText(filename).c_str());
         DoMessageDialog(0, message);
         if (bReadOnly)
            SetFileReadOnly(filename.c_str(), true);
         RefreshExplorerWindow(filename);
         return false;
      }

      // Set timestamp
      TouchFile(filename.c_str());

      if (bReadOnly)
         SetFileReadOnly(filename.c_str(), true);
   }
   else if (id == IDCANCEL)
   {
       RefreshExplorerWindow(filename);
       return false;
   }

   // Delete temporary files
   RefreshExplorerWindow(filename);

   return true;
#endif
/*
   // Get temporary path
   std::string sTempPath = EnsureTrailingDelimiter(GetTemporaryPath());
   std::string sWorkingCopyName;
   std::string sNewRevisionName;
   std::stringstream pid;
   std::string externalApp;
   bool forceQuery = false;
   if (IsControlDown())
      forceQuery = true;
    
   pid << GetCurrentProcessId();
   if (originalFilename.empty())
   {
      sWorkingCopyName = sTempPath + "tortoise" + pid.str() + ".based_on_local." 
         + ExtractLastPart(filename);
      sNewRevisionName = sTempPath + "tortoise" + pid.str() + ".new_based_on.%s." 
         + ExtractLastPart(filename);
   }
   else
   {
      std::stringstream ss;
      ss << sTempPath + "tortoise" + pid.str() + ".based_on_local." << iNestingLevel << "."
         << ExtractLastPart(originalFilename);
      sWorkingCopyName = ss.str();
      sNewRevisionName = sTempPath + "tortoise" + pid.str() + ".new_based_on.%s." 
         + ExtractLastPart(originalFilename);
   }

   std::string command;
   bool bContainsMarkers = false;
   bool bResult = false;
   bool bConvertToAscii = false;
   std::string sOptions;
   bool IsUnicode = false;
   bool bUseUtf8 = false;
   bool bNestedConflicts = false;
   int id;
   DWORD dwMsgBoxFlags;

   // Parse conflict file
   bContainsMarkers = ConflictParser::ParseFile(filename, sWorkingCopyName,
                                                sNewRevisionName, bNestedConflicts);

   externalApp = GetExternalApplication("Merge", myDirectoryGroups, forceQuery);
   if (externalApp.empty())
   {
      // Launch text editor for manual resolution
      DeleteFile(sWorkingCopyName.c_str());
      DeleteFile(sNewRevisionName.c_str());
      std::string editor = GetTextEditorCommand(filename.c_str());
      LaunchCommand(editor, true);
      bResult = true;
      goto Cleanup;
   }

   if (!bContainsMarkers)
   {
      bResult = true;
      goto Cleanup;
   }

   if (bNestedConflicts)
   {
      std::string s = Printf(_("The file %s contains nested conflict sections. "
                               "You need to resolve them first before resolving the most recent "
                               "conflicts. Do you want to resolve the nested conflicts now?"), 
                             ExtractLastPart(filename).c_str());
      int i = MessageBox(myRemoteHandle, s.c_str(), _("Nested conflicts"), MB_ICONEXCLAMATION | MB_YESNOCANCEL);
      if (i == IDYES)
      {
         if (!EditConflictFile(sWorkingCopyName, filename, iNestingLevel + 1))
         {
            bResult = false;
            goto Cleanup;
         }
      }
      else if (i == IDNO)
      {
         bResult = true;
         goto Cleanup;
      }
      else
      {
         bResult = false;
         goto Cleanup;
      }

   }

   // Convert to unicode
   if (originalFilename.empty())
   {
      IsUnicode = CVSStatus::GetFileFormat(filename) == CVSStatus::formatUnicode;
   }
   else
   {
      IsUnicode = CVSStatus::GetFileFormat(originalFilename) == CVSStatus::formatUnicode;
   }
   if (IsUnicode)
   {
      std::string sTempFile = UniqueTemporaryFile();
      ConvertFileUtf8ToUcs2(sWorkingCopyName, sTempFile);
      CopyFile(sTempFile.c_str(), sWorkingCopyName.c_str(), false);
      ConvertFileUtf8ToUcs2(sNewRevisionName, sTempFile);
      CopyFile(sTempFile.c_str(), sNewRevisionName.c_str(), false);
      DeleteFile(sTempFile.c_str());

      bConvertToAscii = !GetBooleanPreference("DiffAsUnicode");
      if (bConvertToAscii)
      {
         if (!EncodeDiffFiles(sWorkingCopyName, sNewRevisionName, bUseUtf8))
         {
            bResult = true;
            goto Cleanup;
         }
      }
   }

   // Store write time
   FILETIME ftLastWriteTimeOld, ftLastWriteTimeNew;
   GetLastWriteTime(sNewRevisionName.c_str(), &ftLastWriteTimeOld);

   // Mark working copy as read-only
   SetFileReadOnly(sWorkingCopyName.c_str(), true);

   // Perform merge, waiting for it to finish (so we can delete file)
   do
   {
      externalApp = GetExternalApplication("Merge", myDirectoryGroups, forceQuery);
      forceQuery = false;
      if (externalApp.empty())
      {
         goto Cleanup;
      }
      command = "\"" + externalApp + "\" \"" + sWorkingCopyName + "\" \""
         + sNewRevisionName + "\"";
      if (!LaunchCommand(command, true))
      {
         std::string message = std::string(_("Failed to launch external merge application")) 
            + "\n" + command;
         DoMessageDialog(0, message);
         forceQuery = true;
      }
   } while (forceQuery);

   GetLastWriteTime(sNewRevisionName.c_str(), &ftLastWriteTimeNew);
   dwMsgBoxFlags = MB_YESNOCANCEL | MB_ICONQUESTION;
   if (CompareFileTime(&ftLastWriteTimeOld, &ftLastWriteTimeNew) >= 0)
   {
      dwMsgBoxFlags |= MB_DEFBUTTON2;
   }

   // Ask the user whether he wants to apply the merged file
   id = MessageBox(myRemoteHandle, _("Do you want to save your "
                                     "manually merged file (and overwrite the file "
                                     "containing the conflict markers)?"), 
                   _("Save merged file"), dwMsgBoxFlags);

   if (id == IDYES)
   {
      if (IsUnicode && bConvertToAscii)
      {
         DecodeDiffFiles(sNewRevisionName.c_str(), "", bUseUtf8);
      }

      // Copy the file
      bool bReadOnly = IsReadOnly(filename.c_str());
      if (bReadOnly)
         SetFileReadOnly(filename.c_str(), false);

      if (!CopyFile(sNewRevisionName.c_str(), filename.c_str(), false))
      {
         std::string message = Printf(_("Failed to overwrite %s"), filename.c_str());
         DoMessageDialog(0, message);
         if (bReadOnly)
            SetFileReadOnly(filename.c_str(), true);
         goto Cleanup;
      }

      // Set timestamp
      TouchFile(filename.c_str());

      if (bReadOnly)
         SetFileReadOnly(filename.c_str(), true);
   }
   else if (id == IDCANCEL)
   {
      goto Cleanup;
   }

   bResult = true;

Cleanup:
   // Delete temporary files
   SetFileReadOnly(sWorkingCopyName.c_str(), false);
   DeleteFile(sWorkingCopyName.c_str());
   DeleteFile(sNewRevisionName.c_str());
   RefreshExplorerWindow();

   return bResult;*/
}


// This does an update -r <ver/tag> command. Called from the History and Graph dialogs.
void TortoiseAct::PerformGet()
{
   std::string rev;
   const std::list<std::string>* out = 0;
   DirectoryGroup* myGroup = &myDirectoryGroups[0];
   
   rev = myParameters.GetValue("-r");
   
   CVSAction glue(0);
   std::vector<std::string> conflictFiles;
   
   MakeArgs args;
   args.add_option("update");
   if (rev == "HEAD")
   {
      args.add_option("-A");
   }
   else
   {
      args.add_option("-r");
      args.add_option(rev);
   }

   if (myParameters.Exists("-C"))
      args.add_option("-C");

   args.add_arg(myDirectoryGroups.mySingleRelative);
   
   if (!glue.Command(myDirectoryGroups.mySingleDirectory, args))
      goto Cleanup;

   // Check for conflicts
   out = &(glue.GetStdOutList());
   ParseConflicts(*myGroup, *out, conflictFiles);


   // Edit conflict file if appropriate
   if (conflictFiles.size() > 0)
   {
      glue.LockProgressDialog(true);
      DoConflictListDialog(glue.GetProgressDialog(), conflictFiles);
      glue.LockProgressDialog(false);
   }

Cleanup:
   if (FileSelected() && glue.AtLeastOneSuccess())
      RefreshExplorerWindow(myDirectoryGroups.mySingleDirectory);
}

// This does an update -r <ver/tag> -p > file command. Called from the History and Graph dialogs.
void TortoiseAct::PerformSaveAs()
{
   std::string rev;
   std::string sFile;
   bool unixSandbox = IsUnixSandbox(StripLastPart(myDirectoryGroups.mySingleAbsolute));

   rev = myParameters.GetValue("-r");

   // Query where to save the file
   std::string filename = MakeRevFilename(myDirectoryGroups.mySingleAbsolute, rev);
   filename = DoSaveFileDialog(0, _("Save file revision"), filename, 
                               wxString(_("All files")) + wxString(wxT("|*.*")));
   if (filename.empty())
      return;
   // Ensure that file is writable
   SetFileReadOnly(filename.c_str(), false);
   
   
   CVSAction glue(0);
   glue.SetCVSRoot(CVSRoot(CVSStatus::CVSRootForPath(myDirectoryGroups.mySingleDirectory)));

   MakeArgs args; 
   std::string sTempDir = UniqueTemporaryDir();
   glue.SetCloseIfOK(true);
   args.add_option("checkout");
   if (unixSandbox)
   {
       CVSServerFeatures features;
       features.Initialize(&glue);
       features.AddUnixLineEndingsFlag(args);
   }
   if (rev != "HEAD")
   {
      args.add_option("-r");
      args.add_option(rev);
   }
   args.add_option("-d");
   args.add_option("temp");
   std::string s = EnsureTrailingUnixDelimiter(CVSStatus::CVSRepositoryForPath(myDirectoryGroups.mySingleAbsolute))
      + ExtractLastPart(myDirectoryGroups.mySingleAbsolute);
   args.add_arg(s);
      
   if (!glue.Command(sTempDir, args))
      goto Cleanup;

   // Copy file to destination
   sFile = EnsureTrailingDelimiter(sTempDir) + "temp\\" 
      + ExtractLastPart(myDirectoryGroups.mySingleAbsolute);
   CopyFileA(sFile.c_str(), filename.c_str(), false);

   // Erase temporary directory
   DeleteDirectoryRec(sTempDir);

Cleanup:   
   if (FileSelected() && glue.AtLeastOneSuccess())
      RefreshExplorerWindow(myDirectoryGroups.mySingleDirectory);
}

void TortoiseAct::PerformHistory()
{
   // Only allow one file
   if (myDirectoryGroups.mySingleAbsolute.empty())
      return;
   if (IsDirectory(myDirectoryGroups.mySingleAbsolute.c_str()))
      return;
   
   DoHistoryDialog(0, myDirectoryGroups.mySingleAbsolute);
}

void TortoiseAct::PerformRevisionGraph()
{
   // Only allow one file
   if (myDirectoryGroups.mySingleAbsolute.empty())
      return;
   if (IsDirectory(myDirectoryGroups.mySingleAbsolute.c_str()))
      return;
   
   DoGraphDialog(0, myDirectoryGroups.mySingleAbsolute);
}



void TortoiseAct::PerformResolveConflicts()
{
   TDEBUG_ENTER("TortoiseAct::PerformResolveConflicts");
   // if we selected multiple 
   std::vector<std::string> files;
   unsigned int j;
   for (j = 0; j < myDirectoryGroups.size(); ++j)
   {
      DirectoryGroup& group = myDirectoryGroups[j];
      unsigned int i;
      for (i = 0; i < group.size(); ++i)
      {
         if (IsDirectory(group[i].myAbsoluteFile.c_str()))
         {
            CVSStatus::RecursiveScan(group[i].myAbsoluteFile, 0, 0, 0, 0, 0, &files);
         }
         else
         {
            TDEBUG_TRACE("file: " << group[i].myAbsoluteFile);
            if (CVSStatus::GetFileStatus(group[i].myAbsoluteFile) == CVSStatus::STATUS_CONFLICT)
            {
               files.push_back(group[i].myAbsoluteFile);
            }
         }
      }
   }

   if (files.size() == 0)
   {
      DoMessageDialog(0, _("There are no conflicts to resolve"));
      return;
   }
/*   else if (files.size() == 1)
   {
      EditConflictFile(myDirectoryGroups.mySingleAbsolute);
   }*/
   else
   {
      DoConflictListDialog(0, files);
   }
}



void TortoiseAct::PerformMergeConflicts()
{
   TDEBUG_ENTER("TortoiseAct::PerformResolveConflicts");
#if 0 // Don't use this part yet
   DoConflictDialog(0, myDirectoryGroups.mySingleAbsolute);
#else
   EditConflictFile(myDirectoryGroups.mySingleAbsolute);
#endif
}



void TortoiseAct::PerformUrl()
{
   HandleURL(myParameters.GetValue("-u"));
}


void TortoiseAct::PerformRebuildIcons()
{
   if (!RebuildIcons())
   {
      DoMessageDialog(0, _("Error rebuilding icons. You may not have the required permissions."));
   }
}


void TortoiseAct::PerformAnnotate()
{
    // Only allow one file
    if (myDirectoryGroups.mySingleAbsolute.empty())
        return;
    if (IsDirectory(myDirectoryGroups.mySingleAbsolute.c_str()))
        return;

    std::string rev;
    if (myParameters.GetValueCount("-r") > 0)
    {
        // Annotating specific revision
        rev = myParameters.GetValue("-r", 0);
    }
    
    DoAnnotateDialog(0, myDirectoryGroups.mySingleAbsolute, rev);
}


void TortoiseAct::PerformRefreshStatus()
{
   TDEBUG_ENTER("TortoiseAct::PerformRefreshStatus");
   unsigned int i, j;
   for (j = 0; j < myDirectoryGroups.size(); ++j)
   {
      DirectoryGroup& group = myDirectoryGroups[j];
      for (i = 0; i < group.size(); i++)
      {
         CVSStatus::GetDirectoryStatus(group[i].myAbsoluteFile, 0, 0, -1);
      }
   }
}


void TortoiseAct::PerformViewMenu()
{
   TDEBUG_ENTER("TortoiseAct::PerformViewMenu");

   std::string rev;
   if (myParameters.GetValueCount("-r") > 0)
   {
      // Not viewing head revision
      rev = myParameters.GetValue("-r", 0);
   }
   
   DoView(myDirectoryGroups, rev);
}


void TortoiseAct::PerformCommand()
{
   TDEBUG_ENTER("TortoiseAct::PerformCommand");

   std::string command;
   if (!DoCommandDialog(0, myAllFiles, command) || command.empty())
      return;

   // Convert space-separated list into vector
   std::vector<std::string> commandArgs;
   while (1)
   {
      std::string commandArg = CutFirstToken(command, " ");
      if (commandArg.empty())
         break;
      commandArgs.push_back(commandArg);
   }
   
   CVSAction glue(0);
   glue.SetProgressFinishedCaption(Printf(_("Finished '%s' in %s"),
                                          wxTextCStr(command), GetDirList().c_str()));
   for (unsigned int j = 0; j < myDirectoryGroups.size(); ++j)
   {
      DirectoryGroup& group = myDirectoryGroups[j];
      MakeArgs args;
      for (unsigned int i = 0; i < commandArgs.size(); ++i)
         args.add_option(commandArgs[i]);
      for (unsigned int i = 0; i < group.size(); ++i)
         args.add_arg(group[i].myRelativeFile);
      glue.Command(group.myDirectory, args);
   }
   
   if (glue.AtLeastOneSuccess())
      RefreshExplorerWindow();
}



// CVS URLs have the form
// cvs://<cvsroot>:<module>[#tag]
// e.g. cvs://:pserver:anonymous@cvs.tortoisecvs.sourceforge.net:/cvsroot/tortoisecvs:tortoisecvs#tortoisecvs-1-0-4
void TortoiseAct::HandleURL(std::string url)
{
   // Validate URL
   if (url.substr(0, 4) != std::string("cvs:"))
      TortoiseFatalError(_("Invalid cvs:// URL"));
   // Remove protocol specifier
   url = url.substr(4);
   if (url.substr(0, 1) != std::string("//"))
      url = url.substr(2);
   CVSRoot root;
   std::string tag;
   std::string module;
   std::string::size_type modpos = url.find_last_of(':');
   if ((modpos == std::string::npos) || (modpos == url.size()-1))
      TortoiseFatalError(_("Invalid cvs:// URL: no module specified"));
   std::string::size_type hashpos = url.find('#');
   if (hashpos != std::string::npos)
   {
      // Tag specified
      tag = url.substr(hashpos+1);
      url = url.substr(0, hashpos);
   }
   module = url.substr(modpos+1);
   url = url.substr(0, modpos);
   
   root.SetCVSROOT(url);
   if (!root.Valid())
      TortoiseFatalError(Printf(_("Invalid CVSROOT %s specified in cvs:// URL"),
                                wxTextCStr(root.GetCVSROOT())));

   // Show the preset checkout dialog
   std::string date, alternateDirectory;
   bool exportMode, unixLineEndings;
   bool forceHead = GetBooleanPreference("Force Most Recent Revision");
#ifdef MARCH_HARE_BUILD
   bool fileReadOnly = false;
#endif
   bool edit = false;
   std::string bugNumber;

   if (!DoCheckoutDialogPreset(0, root,
                               module,
                               tag,
                               myDirectoryGroups.mySingleAbsolute,
                               date,
                               exportMode,
                               alternateDirectory,
                               unixLineEndings,
#ifdef MARCH_HARE_BUILD
                               fileReadOnly,
#endif
                               forceHead, edit, bugNumber))
      return;

   // Ask user for destination directory
   std::string folder = DoSaveFileDialog(0, _("Choose destination folder for CVS checkout"),
                                         "", wxString(_("Folders")) + wxString(wxT("|*.*")));
   if (folder.empty())
      return;
   CreateDirectoryA(folder.c_str(), 0);
   
   CVSAction glue(0);
   glue.SetProgressFinishedCaption(Printf(_("Finished checkout in %s"), wxText(folder).c_str()));
   glue.SetCVSRoot(root);
   
   MakeArgs args;
   args.add_option("-d");
   args.add_option(root.GetCVSROOT());
   if (exportMode)
      args.add_option("export");
   else
      args.add_option("checkout");
   if (!tag.empty() || (exportMode && date.empty()))
   {
      args.add_option("-r");
      args.add_option(tag.empty() ? std::string("HEAD") : tag);
   }
   if (!date.empty())
   {
      args.add_option("-D");
      args.add_option(date);
   }
   
   if (!alternateDirectory.empty())
   {
      args.add_option("-d");
      args.add_option(alternateDirectory);
   }
   CVSServerFeatures features;
   if (unixLineEndings || edit)
   {
       features.Initialize(&glue);
       if (unixLineEndings)
           features.AddUnixLineEndingsFlag(args);
   }
#ifdef MARCH_HARE_BUILD
   if (fileReadOnly)
       args.add_global_option("-r");
#endif
   
   if (!exportMode)
   {
      if (GetBooleanPreference("Prune Empty Directories"))
         args.add_option("-P");
   }

   args.add_option(module);
   glue.SetProgressCaption(Printf(_("Checking out in %s"), wxText(folder).c_str()));
   if (exportMode && tag.empty())
      glue.GetProgressDialog()->NewText(wxString(_("No tag specified for Export, using HEAD"))
                                        + wxString(wxT("\n\n")), ProgressDialog::TTWarning);
   glue.Command(folder, args);

   if (edit)
   {
       MakeArgs args;
       args.add_option("edit");
       args.add_option("-z");
       if (features.SupportsExclusiveEdit())
       {
           if (GetIntegerPreference("Edit policy") == TortoisePreferences::EditPolicyExclusiveACL)
               args.add_option("-A");
           if (GetIntegerPreference("Edit policy") >= TortoisePreferences::EditPolicyExclusive)
               args.add_option("-x");
           else if (GetIntegerPreference("Edit policy") == TortoisePreferences::EditPolicyConcurrentExclusive)
               args.add_option("-c");
       }
       if (!bugNumber.empty())
       { 
           args.add_option("-b"); 
           args.add_option(bugNumber); 
       }
       std::string root = myDirectoryGroups.mySingleAbsolute;
       std::string dir;
       if (alternateDirectory.empty())
           dir = module;
       else
           dir = alternateDirectory;
       std::string firstDir = GetFirstPart(dir);
       if (firstDir.empty())
           root += dir;
       else
           root += firstDir;
       glue.Command(root, args);
   }
}


std::vector<std::string> TortoiseAct::GetFetchButtonDirs()
{
   std::vector<std::string> result;
   std::set<std::string> setDirs;
   unsigned int i, j;

   if (myDirectoryGroups.size() <= 0)
      return result;

   for (i = 0; i < myDirectoryGroups.size(); i++)
   {
      DirectoryGroup& group = myDirectoryGroups[i];

      // Nothing in group
      for (j = 0; j < group.size(); j++)
      {
         const std::string& file = group[j].myAbsoluteFile;
         if (IsDirectory(file.c_str()))
         {
            setDirs.insert(file);
         }
         else
         {
            setDirs.insert(GetDirectoryPart(file)); 
         }
      }
   }

   // transform set to vector
   std::set<std::string>::iterator it = setDirs.begin();
   while (it != setDirs.end())
   {
      result.push_back(*it);
      it++;
   }

   return result;
}

std::string TortoiseAct::GetFetchButtonTag()
{
   std::string result;
   std::string tag;
   bool bResultSet = false;
   std::set<std::string> setDirs;
   unsigned int i, j;

   if (myDirectoryGroups.size() <= 0)
      return result;

   for (i = 0; i < myDirectoryGroups.size(); i++)
   {
      DirectoryGroup& group = myDirectoryGroups[i];

      // Nothing in group
      for (j = 0; j < group.size(); j++)
      {
         tag = "";
         if (CVSStatus::HasStickyTag(group[j].myAbsoluteFile))
            tag = CVSStatus::GetStickyTag(group[j].myAbsoluteFile);
         if (bResultSet && tag != result)
            return "";
         bResultSet = true;
         result = tag;
      }
   }

   return result;
}

void TortoiseAct::AddToIgnoreList(const std::string& directory, const std::string& file)
{
    TDEBUG_ENTER("AddToIgnoreList");
    TDEBUG_TRACE("Dir " << directory << " File " << file);
    std::string ignorefile = directory;
    ignorefile = EnsureTrailingDelimiter(ignorefile);
    ignorefile += ".cvsignore";

   // Escape blanks
   std::string myfile = file;
   FindAndReplace(myfile, std::string(" "), std::string("\\ "));
   FindAndReplace(myfile, std::string("["), std::string("\\["));
   FindAndReplace(myfile, std::string("]"), std::string("\\]"));

   // Special case to support files/folders starting with '!'. A single backslash is not sufficient
   // as CVS decodes escape sequences before calling fnmatch().
   FindAndReplace(myfile, std::string("!"), std::string("\\\\!"));
   
   std::ofstream os(ignorefile.c_str(), std::ios::app);

   // Make sure there's a newline before the filename
   if (FileExists(ignorefile.c_str()))
   {
      std::ifstream is(ignorefile.c_str());
      std::string line;
      while (!is.eof())
      {
         std::getline(is, line);
      }
      line = Trim(line);
      is.close();
      if (!line.empty())
      {
         // Add a newline *before* the filename 
         os << std::endl;
      }
   }
   os << myfile;
}



// Encode diff files
bool TortoiseAct::EncodeDiffFiles(const std::string& sFilename1, 
                                  const std::string& sFilename2,
                                  bool& bUseUtf8)
{
   bool bAsciiSuccess = true;
   std::string tempFile1 = UniqueTemporaryFile();
   std::string tempFile2 = UniqueTemporaryFile();
   AutoFileDeleter tempFile1Deleter(tempFile1);
   AutoFileDeleter tempFile2Deleter(tempFile2);
   bUseUtf8 = false;

   if (bAsciiSuccess && FileExists(sFilename1.c_str()))
      // Convert file 1
      ConvertFileUcs2ToAscii(sFilename1, tempFile1, true, &bAsciiSuccess);

   if (bAsciiSuccess && FileExists(sFilename2.c_str()))
      // Convert file 2
      ConvertFileUcs2ToAscii(sFilename2, tempFile2, true, &bAsciiSuccess);

   if (!bAsciiSuccess)
   {
       if (MessageBox(myRemoteHandle, _(
"The files you want to compare contain characters that cannot be represented in the current codepage.\n\
To prevent loss of information, the files will be temporarily converted to UTF-8."),
                      _("UTF-8 conversion"),
                      MB_ICONINFORMATION | MB_OKCANCEL) == IDCANCEL)
           return false;
   }

   // if conversion failed, use utf8
   if (!bAsciiSuccess)
   {
      bUseUtf8 = true;
      // Convert file 1
      ConvertFileUcs2ToUtf8(sFilename1, tempFile1, false);
      // Convert file 2
      ConvertFileUcs2ToUtf8(sFilename2, tempFile2, false);
   }

   CopyFileA(tempFile1.c_str(), sFilename1.c_str(), false);
   CopyFileA(tempFile2.c_str(), sFilename2.c_str(), false);

   return true;
}



// Decode diff files
void TortoiseAct::DecodeDiffFiles(const std::string& sFilename1, 
                                  const std::string& sFilename2,
                                  bool bUseUtf8)
{
   std::string tempFile = UniqueTemporaryFile();
   
   if (bUseUtf8)
   {
      // Convert file 1
      if (FileExists(sFilename1.c_str()))
      {
         ConvertFileUtf8ToUcs2(sFilename1, tempFile);
         CopyFileA(tempFile.c_str(), sFilename1.c_str(), false);
      }

      // Convert file 2
      if (FileExists(sFilename2.c_str()))
      {
         ConvertFileUtf8ToUcs2(sFilename2, tempFile);
         CopyFileA(tempFile.c_str(), sFilename2.c_str(), false);
      }
   }
   else
   {
      // Convert file 1
      if (FileExists(sFilename1.c_str()))
      {
         ConvertFileAsciiToUcs2(sFilename1, tempFile);
         CopyFileA(tempFile.c_str(), sFilename1.c_str(), false);
      }

      // Convert file 2
      if (FileExists(sFilename2.c_str()))
      {
         ConvertFileAsciiToUcs2(sFilename2, tempFile);
         CopyFileA(tempFile.c_str(), sFilename2.c_str(), false);
      }
   }
   DeleteFileA(tempFile.c_str());
}

static bool nocase_compare(char c1, char c2)
{
    return toupper(c1) == toupper(c2);
}

// Do committing
bool TortoiseAct::DoCommit(const FilesWithBugnumbers* modified,
                           const FilesWithBugnumbers* modifiedStatic,
                           const FilesWithBugnumbers* added,
                           const FilesWithBugnumbers* removed,
                           const FilesWithBugnumbers* renamed,
                           CVSAction*& pglue,
                           bool* isWatchOn)
{
   TDEBUG_ENTER("TortoiseAct::DoCommit");
   bool ok = true;
   bool usebug = false;
   bool markbug = false;
   std::string comment;

   std::string bugnumber = TortoiseRegistry::ReadString("Dialogs\\Commit\\Bug Number");
   if (!(TortoiseRegistry::ReadBoolean("Dialogs\\Commit\\Use Bug", false)))
       bugnumber.clear();
   
   CommitDialog::SelectedFiles userSelectionStatic;
   while (1)
   {
       // Retrieve a comment about them
       comment = TortoiseRegistry::ReadString("FailedCommitMessage");
       // If there is no comment from the previous commit try to read it now from "CVS/Template"
       // It is not quite common but you can have different Template files in the subdirs.
       // For me it is enough to read the first one I find.
       if (comment.empty())
           comment = CVSStatus::ReadTemplateFile(myDirectoryGroups[0].myDirectory);

       // Ensure that max_command_line_len is set
       MakeArgs args;
       CommitDialog::SelectedFiles userSelection;
       bool result = DoCommitDialog(0, myDirectoryGroups,
                                    modified, modifiedStatic, added, removed, renamed, comment,
                                    bugnumber, usebug, markbug,
                                    userSelection, userSelectionStatic);
       if (!result || (userSelection.empty() && userSelectionStatic.empty()))
           return false;

       if (!usebug && !markbug)
           bugnumber.clear();
      
       if (!ComputeCommitSelection(userSelection, modified, added, removed, renamed, myAllFiles))
       {
           DoMessageDialog(0, _("You have tried to commit one or more renamed files without committing all changed\n\
(modified, added, or removed) files in the same folder.\n\
CVSNT does not currently support this. Please try again."));
           continue;
       }
       break;
   }

   // Redo the preprocessing into groups
   if (!PreprocessFileList())
   {
       // TODO: Better error code everywhere
       TortoiseFatalError(_("Failed to reprocess file list"));
   }

   if (!myAllFiles.empty() || !userSelectionStatic.empty())
   {
       if (!pglue)
           pglue = new CVSAction(0);
       pglue->SetProgressFinishedCaption(Printf(_("Finished commit in %s"), GetDirList().c_str()));

       // Include bug number in comment if appropriate
       if (usebug || markbug)
       {
           // Try to find bug number in comment
           bool has_bug_comment = false;

           // First, 'bug XXX'
           std::string pattern = "bug ";
           std::string::iterator pos = std::search(comment.begin(), comment.end(),
                                                   pattern.begin(), pattern.end(),
                                                   nocase_compare); // comparison criterion
           if (pos != comment.end())
               has_bug_comment = true;

           // Second, 'Bug Number:'
           pattern = "Bug Number:";
           if (!has_bug_comment)
           {
               pos = std::search(comment.begin(), comment.end(),
                                 pattern.begin(), pattern.end(),
                                 nocase_compare);
               if (pos != comment.end())
                   has_bug_comment = true;
           }
           // Then, '#XXX'
           if (!has_bug_comment)
           {
               if (comment.find("#") != std::string::npos)
                   has_bug_comment=true;
           }

           // Include bug number in comment if not found
           if (!has_bug_comment)
           {
               comment += " (bug ";
               comment += bugnumber;
               comment += ")";
           }
       }
       
       // Now commit all the files in groups (by repository)
       for (unsigned int j = 0; j < myDirectoryGroups.size(); ++j)
       {
           DirectoryGroup& group = myDirectoryGroups[j];
         
           if (!DoCommitDir(group, pglue, comment, usebug, markbug, bugnumber))
               ok = false;
       }

       if (!userSelectionStatic.empty())
       {
           myAllFiles.clear();
           for (CommitDialog::SelectedFiles::iterator it = userSelectionStatic.begin(); it != userSelectionStatic.end(); ++it)
               myAllFiles.push_back(it->first);
           if (!PreprocessFileList())
           {
               // TODO: Better error code everywhere
               TortoiseFatalError(_("Failed to reprocess file list"));
           }

           for (unsigned int j = 0; j < myDirectoryGroups.size(); ++j)
           {
               DirectoryGroup& group = myDirectoryGroups[j];

               if (!DoCommitDir(group, pglue, comment, usebug, markbug, bugnumber, true, true))
                   ok = false;
           }
       }

       // Remember comment from failed commit
       if (!ok)
           TortoiseRegistry::WriteString("FailedCommitMessage", comment);
       else
       {
           TortoiseRegistry::WriteString("FailedCommitMessage", "");
           if (comment.length() > 2000)
           {
               comment = comment.substr(0, 2000) + "...";
           }
           TDEBUG_TRACE("Push comment '" << comment << "'");
           TortoiseRegistry::InsertFrontVector("History\\Comments", comment, 50);
       }
   }
   return ok;
}


bool TortoiseAct::DoCommitDir(DirectoryGroup& group, CVSAction* pglue,
                              const std::string& comment,
                              bool usebug, bool markbug, const std::string& bugnumber,
                              bool recurse,
                              bool staticFiles)
{
    TDEBUG_ENTER("DoCommitDir");

    MakeArgs args;
#ifdef MARCH_HARE_BUILD
    if (myDirectoryGroups.GetBooleanPreference("Checkout files Read Only"))
        args.add_global_option("-r");
#endif
    args.add_option("commit");
    if (!recurse)
        args.add_option("-l");
    if (staticFiles)
        args.add_option("-f");

    // Get client features
    if (!group.myCVSClientFeatures.Initialize())
       TortoiseFatalError(_("Could not connect to client"));

    bool unixSandbox = IsUnixSandbox(StripLastPart(group.myDirectory));
    
    group.myCVSClientFeatures.AddOptionM(args);
    args.add_option(comment);
    if (usebug)
    {
        args.add_option("-b");
        args.add_option(bugnumber);
    }
    if (markbug)
    {
        args.add_option("-B");
        args.add_option(bugnumber);
    }
    for (unsigned int i = 0; i < group.size(); ++i)
        args.add_arg(group[i].myRelativeFile);
         
    if (unixSandbox)
    {
        CVSServerFeatures features;
        features.Initialize(pglue);
        features.AddUnixLineEndingsFlag(args);
    }

    pglue->SetProgressCaption(Printf(_("Committing in %s"), 
                                     wxText(group.myDirectory).c_str()));
    if (!pglue->Command(group.myDirectory, args))
        return false;

    // Get all committed files
    const std::list<std::string>& out = pglue->GetStdOutList();
    std::vector<std::string> committedFiles;
    std::vector<std::string> notCommittedFiles;
    std::map<std::string, int> map;
    ParseCommitted(group, out, committedFiles);

    // Put them into a map
    std::vector<std::string>::const_iterator it = committedFiles.begin();
    std::string s;
    while (it != committedFiles.end())
    {
        s = *it;
        MakeLowerCase(s);
        map[s] = 1;
        it++;
    }

    // look for the ones that haven't been committed
    for (unsigned int i = 0; i < group.size(); ++i)
    {
        s = GetLastPart(group[i].myAbsoluteFile);
        TDEBUG_TRACE("Check " << s);
        MakeLowerCase(s);
        if (map.find(s) == map.end())
        {
            TDEBUG_TRACE("Not found");
            notCommittedFiles.push_back(group[i].myRelativeFile);
        }
    }
    if (notCommittedFiles.size() > 0)
    {
        TDEBUG_TRACE("notCommittedFiles: " << notCommittedFiles.size());
        MakeArgs args;
        pglue->SetHideStdout(); // Don't show output from 'cvs stat'
        args.add_option("stat");
        for (unsigned int i = 0; i < notCommittedFiles.size(); i++)
            args.add_arg(notCommittedFiles[i]);

        if (!pglue->Command(group.myDirectory, args))
            return false;
        
        bool autoUnedit = (GetIntegerPreference("Edit policy") != TortoisePreferences::EditPolicyNone)
           && GetBooleanPreference("Automatic unedit");
        if (autoUnedit)
        {
            MakeArgs args;
#ifdef MARCH_HARE_BUILD
            if (myDirectoryGroups.GetBooleanPreference("Checkout files Read Only"))
                args.add_global_option("-r");
#endif
            args.add_option("unedit");
            for (unsigned int i = 0; i < notCommittedFiles.size(); i++)
            {
                args.add_arg(notCommittedFiles[i]);
            }
            return pglue->Command(group.myDirectory, args);
        }
    }
    return true;
}


bool TortoiseAct::DoAutoUnedit(CVSAction* pglue)
{
   bool ok = true;
   bool gotAny = false;
   std::vector<std::string> allEditedFiles;

   // Run "cvs editors"
   for (size_t i = 0; i < myDirectoryGroups.size(); ++i)
   {
      DirectoryGroup& group = myDirectoryGroups[i];
      if (group.empty() || (group[0].myRelativeFile == std::string(".")))
      {
         // This is a directory
         if (!pglue)
            pglue = new CVSAction(0);
         EditorListDialog::EditedFileList editedfiles;
         ok = ok && GetEditors(*pglue, group, editedfiles);
         if (!editedfiles.empty())
         {
            gotAny = true;
            for (size_t j = 0; j < editedfiles.size(); ++j)
               allEditedFiles.push_back(editedfiles[j].myFilename);
         }
      }
   }

   myAllFiles = allEditedFiles;
   PreprocessFileList();

   for (size_t i = 0; i < myDirectoryGroups.size(); ++i)
   {
      DirectoryGroup& group = myDirectoryGroups[i];
      MakeArgs args;
      args.add_option("unedit");
      for (size_t j = 0; j < group.size(); ++j)
         args.add_arg(group[j].myRelativeFile);
      ok = ok && pglue->Command(group.myDirectory, args);
   }
   return ok && gotAny;
}

// Return list of processing directories
wxString TortoiseAct::GetDirList()
{
   if (myDirectoryGroups.size() != 1)
   {
      return _("multiple folders");
   }
   else 
   {
      DirectoryGroup& group = myDirectoryGroups[0];
      return wxText(group.myDirectory);
   }
}


void TortoiseAct::GetAddableFiles(const DirectoryGroups& dirGroups, bool includeFolders)
{
   TDEBUG_ENTER("GetAddableFiles");
   std::vector<std::string> files;
   std::map<std::string, bool> selected;
   ::GetAddableFiles(dirGroups, includeFolders, myAllFiles, files, selected);
   myAllFiles = files;
   if (files.empty())
      return;

   // Preprocess the files
   PreprocessFileList(true);

   // Create AddFiles data for each selected file
   for (size_t i = 0; i < myDirectoryGroups.size(); i++)
   {
      DirectoryGroup& group = myDirectoryGroups[i];
      for (size_t j = 0; j < group.size(); j++)
      {
         DirectoryGroup::AddFilesData* data = new DirectoryGroup::AddFilesData(group[j]);
         group[j].myUserData = data;
         data->mySelected = selected[group[j].myAbsoluteFile];
      }
   }
}


bool TortoiseAct::GetBooleanPreference(const std::string& name)
{
   return myDirectoryGroups.GetBooleanPreference(name);
}

std::string TortoiseAct::GetStringPreference(const std::string& name)
{
   return myDirectoryGroups.GetStringPreference(name);
}

int TortoiseAct::GetIntegerPreference(const std::string& name)
{
   return myDirectoryGroups.GetIntegerPreference(name);
}
