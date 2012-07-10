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

#include "StdAfx.h"
#include <algorithm>
#include "TortoiseActUtils.h"
#include "DirectoryGroup.h"
#include <CVSGlue/CVSAction.h>
#include <DialogsWxw/ChooseFile.h>
#include <DialogsWxw/MessageDialog.h>
#include <Utils/AutoDirectoryDeleter.h>
#include <Utils/AutoFileDeleter.h>
#include <Utils/SandboxUtils.h>
#include <Utils/ShellUtils.h>
#include <Utils/TortoiseDebug.h>
#include <Utils/Preference.h>


std::string GetExternalApplication(const char* appType,
                                   const DirectoryGroups& dirGroups,
                                   bool forceQuery)
{
   TDEBUG_ENTER("GetExternalApplication");
   std::string regName;
   wxString dialogTitle;
   if (stricmp(appType, "Diff") == 0)
   {
      regName = "External Diff Application";
      dialogTitle = _("Choose external diff application");
   }
   else if (stricmp(appType, "Merge") == 0)
   {
      regName = "External Merge Application";
      dialogTitle = _("Choose external merge application");
   }
   else
   {
      return "";
   }
      
   TDEBUG_TRACE("RegKey: " << regName);
   std::string externalApp = dirGroups.GetStringPreference(regName);
   if (externalApp.empty() || (externalApp == "?"))
       // Module preference not set, use global
       externalApp = GetStringPreference(regName);
   
   TDEBUG_TRACE("App: " << externalApp);

   // Only ask the user once for external diff application
   if (externalApp.empty() && !forceQuery)
      return externalApp;

   // A question mark signals to ask the user
   if (externalApp == "?" || forceQuery)
   {
      do
      {
         if (externalApp == "?")
            externalApp = "";

         externalApp = DoOpenFileDialog(0, dialogTitle, externalApp, 
                                        wxString(_("Executables")) + wxString(wxT(" (*.exe)|*.exe")));
      }
      while (!(externalApp.empty() || FileExists(externalApp.c_str())));
      // Always set global preference
      SetStringPreferenceRootModule(regName, externalApp, "");
   }
   return externalApp;
}

bool DoDiff(DirectoryGroups& dirGroups,
            std::string rev1,
            std::string rev2,
            bool forceQuery)
{
   std::string dir = UniqueTemporaryDir();
   AutoDirectoryDeleter dirDeleter(dir);
   std::string diffFile = dir;
   std::string diffFile2 = diffFile;
   bool ok = true;
   CVSAction glue(0);

   if (rev1.empty())
      rev1 = CVSStatus::GetRevisionNumber(dirGroups.mySingleAbsolute);

   // Should we do a text diff only?
   std::string externalApp = GetExternalApplication("Diff",
                                                    dirGroups,
                                                    forceQuery);
   forceQuery = false;
   if (externalApp.empty())
   {
      // Perform a textual diff
      glue.SetProgressFinishedCaption(Printf(_("Finished diff in %s"), 
                                             wxText(dirGroups.mySingleDirectory).c_str()));
      glue.SetProgressCaption(Printf(_("Diffing in %s"), wxText(dirGroups.mySingleDirectory).c_str()));
      MakeArgs args;
      args.add_option("diff");
      args.add_option("-u");
      args.add_option("-r");
      args.add_option(rev1);
      if (!rev2.empty())
      {
         args.add_option("-r");
         args.add_option(rev2);
      }
      args.add_arg(dirGroups.mySingleRelative);

      return glue.Command(dirGroups.mySingleDirectory, args);
   }
   
   bool unixSandbox = IsUnixSandbox(StripLastPart(dirGroups.mySingleAbsolute));

   diffFile += "\\" + MakeRevFilename(ExtractLastPart(dirGroups.mySingleAbsolute), rev1);
   DeleteFileA(diffFile.c_str());
   AutoFileDeleter diffFileDeleter(diffFile);

   AutoFileDeleter diffFile2Deleter;
   if (!rev2.empty())
   {
      diffFile2 += "\\" + MakeRevFilename(ExtractLastPart(dirGroups.mySingleAbsolute), rev2);
      DeleteFileA(diffFile2.c_str());
      diffFile2Deleter.Attach(diffFile2);
   }

   glue.SetProgressFinishedCaption(Printf(_("Finished diff in %s"), 
                                          wxText(dirGroups.mySingleDirectory).c_str()));
   glue.SetProgressCaption(Printf(_("Diffing in %s"), wxText(dirGroups.mySingleDirectory).c_str()));
   std::string cvsroot = CVSStatus::CVSRootForPath(dirGroups.mySingleDirectory);

   CVSServerFeatures sf;
   if (unixSandbox)
       sf.Initialize(&glue);

   // Diffing two revisions
   if (!rev1.empty())
   {
       glue.SetCVSRoot(CVSRoot(cvsroot));
       MakeArgs args;
       std::string tempDir = UniqueTemporaryDir();
       glue.SetCloseIfOK(true);
       args.add_global_option("-f");
       args.add_option("checkout");
       if (rev1 != "HEAD")
       {
           args.add_option("-r");
           args.add_option(rev1);
       }
       args.add_option("-d");
       args.add_option("temp");
       if (unixSandbox)
           sf.AddUnixLineEndingsFlag(args);
       std::string s = EnsureTrailingUnixDelimiter(CVSStatus::CVSRepositoryForPath(dirGroups.mySingleAbsolute))
           + ExtractLastPart(dirGroups.mySingleAbsolute);
       args.add_arg(s);
      
       ok = glue.Command(tempDir, args);
       if (ok)
       {
           // Copy file to destination
           std::string file = EnsureTrailingDelimiter(tempDir) + "temp\\" + ExtractLastPart(dirGroups.mySingleAbsolute);
           SetFileReadOnly(file.c_str(), false);
           CopyFileA(file.c_str(), diffFile.c_str(), false);
           SetFileReadOnly(diffFile.c_str(), true);
       }

       // Erase temporary directory
       DeleteDirectoryRec(tempDir);
     
       if (!ok)
           return false;
       glue.CloseConsoleOutput();
   }
     
   if (!rev2.empty())
   {
       glue.SetCVSRoot(CVSRoot(cvsroot));
       MakeArgs args; 

       std::string tempDir = UniqueTemporaryDir();
       glue.SetCloseIfOK(true);
       args.add_global_option("-f");
       args.add_option("checkout");
       args.add_option("-r");
       args.add_option(rev2);
       args.add_option("-d");
       args.add_option("temp");
       if (unixSandbox)
           sf.AddUnixLineEndingsFlag(args);
       std::string s = EnsureTrailingUnixDelimiter(CVSStatus::CVSRepositoryForPath(dirGroups.mySingleAbsolute))
           + ExtractLastPart(dirGroups.mySingleAbsolute);
       args.add_arg(s);
      
       ok = glue.Command(tempDir, args);

       if (ok)
       {
           // Copy file to destination
           std::string file = EnsureTrailingDelimiter(tempDir) + "temp\\" 
               + ExtractLastPart(dirGroups.mySingleAbsolute);
           SetFileReadOnly(file.c_str(), false);
           CopyFileA(file.c_str(), diffFile2.c_str(), false);
           SetFileReadOnly(diffFile2.c_str(), true);
       }
     
       // Erase temporary directory
       DeleteDirectoryRec(tempDir);

       if (!ok)
           return false;
       glue.CloseConsoleOutput();
   }
    
   if (!FileExists(diffFile.c_str()))
   {
      DoMessageDialog(0, wxString(_("This file is new and has never been committed to the server or is an empty file on the server."))
                      + wxString(wxT("\n\n"))
                      + wxString(wxText(dirGroups.mySingleAbsolute)));
      return true;
   }
    
   do
   {
      externalApp = GetExternalApplication("Diff",
                                           dirGroups,
                                           forceQuery);
      forceQuery = false;
      if (externalApp.empty())
      {
          return false;
      }
      // Perform diff, waiting for it to finish (so we can delete file)
      glue.LockProgressDialog(true);
      if (rev2.empty())
          forceQuery = !RunExternalDiff(diffFile, dirGroups.mySingleAbsolute, dirGroups);
      else
          forceQuery = !RunExternalDiff(diffFile, diffFile2, dirGroups);
      glue.LockProgressDialog(false);
   }
   while (forceQuery);

   return true;
}


// View a revision
bool DoView(DirectoryGroups& dirGroups, std::string rev)
{
   std::string dir = UniqueTemporaryDir();
   std::string viewFile = dir;
   bool ok = true;
   CVSAction glue(0);
   std::string cvsroot;
   bool unixSandbox = IsUnixSandbox(StripLastPart(dirGroups.mySingleAbsolute));

   if (!rev.empty())
   {
      viewFile += "\\" + MakeRevFilename(ExtractLastPart(dirGroups.mySingleAbsolute), rev);
      DeleteFileA(viewFile.c_str());
   }

   glue.SetProgressFinishedCaption(Printf(_("Viewing %s"), 
                                          wxText(dirGroups.mySingleAbsolute).c_str()));
   glue.SetProgressCaption(Printf(_("Viewing %s"), wxText(dirGroups.mySingleAbsolute).c_str()));
   cvsroot = CVSStatus::CVSRootForPath(dirGroups.mySingleDirectory);

   // Viewing revision
   if (!rev.empty())
   {
      glue.SetCVSRoot(CVSRoot(cvsroot));
      MakeArgs args; 
      std::string sTempDir = UniqueTemporaryDir();
      glue.SetCloseIfOK(true);
      args.add_option("checkout");
      if (unixSandbox)
      {
          CVSServerFeatures sf;
          sf.Initialize(&glue);
          sf.AddUnixLineEndingsFlag(args);
      }
      if (rev != "HEAD")
      {
         args.add_option("-r");
         args.add_option(rev);
      }
      args.add_option("-d");
      args.add_option("temp");
      std::string s = EnsureTrailingUnixDelimiter(CVSStatus::CVSRepositoryForPath(dirGroups.mySingleAbsolute))
         + ExtractLastPart(dirGroups.mySingleAbsolute);
      args.add_arg(s);
      
      ok = glue.Command(sTempDir, args);
      if (ok)
      {
         // Copy file to destination
         std::string file = EnsureTrailingDelimiter(sTempDir) + "temp\\" +
            ExtractLastPart(dirGroups.mySingleAbsolute);
         SetFileReadOnly(file.c_str(), false);
         CopyFileA(file.c_str(), viewFile.c_str(), false);
         SetFileReadOnly(viewFile.c_str(), true);
      }

      // Erase temporary directory
      DeleteDirectoryRec(sTempDir);

      if (!ok)
         goto Cleanup;
   }
     
   if (!FileExists(viewFile.c_str()))
   {
      DoMessageDialog(0, wxString(_("This file is new and has never been committed to the server or is an empty file on the server."))
                      + wxString(wxT("\n\n")) 
                      + wxString(wxText(dirGroups.mySingleAbsolute)));
      return true;
   }
   
   // Perform view, waiting for it to finish (so we can delete file)
   glue.LockProgressDialog(true);
   if (rev.empty())
      LaunchFile(dirGroups.mySingleAbsolute, true);
   else
      LaunchFile(viewFile, true);
   glue.LockProgressDialog(false);
    
Cleanup:
   // Clean up
   if (!rev.empty())
   {
      SetFileReadOnly(viewFile.c_str(), false);
      DeleteFileA(viewFile.c_str());
   }
   DeleteDirectoryRec(dir);

   return ok;
}



bool RunExternalDiff(std::string filename1,
                     std::string filename2,
                     DirectoryGroups& dirGroups,
                     std::string filename3)
{
   TDEBUG_ENTER("RunExternalDiff");
   TDEBUG_TRACE("File 1: " << filename1);
   TDEBUG_TRACE("File 2: " << filename2);
   bool bResult = true;
   bool again = false;
   std::string command;
   std::string externalApp;
   std::string externalParams;

   // Perform diff, waiting for it to finish 
   if (filename3.empty())
   {
      externalParams = dirGroups.GetStringPreference("External Diff2 Params");
      std::map<std::string, std::string> params;
      params["1"] = filename1;
      params["2"] = filename2;
      externalParams = ReplaceParams(externalParams, params);
   }
   else
      externalParams = dirGroups.GetStringPreference("External Diff3 Params");

   do
   {
      externalApp = GetExternalApplication("Diff",
                                           dirGroups,
                                           again);
      again = false;
      if (externalApp.empty())
          return false;

      command = "\"" + externalApp + "\" " + externalParams;
      if (!LaunchCommand(command, true))
      {
         DoMessageDialog(0, wxString(_("Failed to launch external diff application"))
                         + wxString(wxT("\n"))
                         + wxString(wxText(command)));
         again = true;
      }
   } while (again);

   return true;
}


bool RunExternalMerge(std::string fileMine,
                      std::string fileYours,
                      DirectoryGroups& dirGroups,
                      std::string fileOlder)
{
   TDEBUG_ENTER("RunExternalMerge");
   TDEBUG_TRACE("File mine: " << fileMine);
   TDEBUG_TRACE("File yours: " << fileYours);
   bool bResult = true;
   bool again = false;
   std::string command;
   std::string externalApp;
   std::string externalParams;


   // Perform merge, waiting for it to finish 
   if (fileOlder.empty())
   {
       externalParams = dirGroups.GetStringPreference("External Merge2 Params");
       std::map<std::string, std::string> params;
       params["mine"] = fileMine;
       params["yours"] = fileYours;
       externalParams = ReplaceParams(externalParams, params);
   }
   else
       externalParams = dirGroups.GetStringPreference("External Merge3 Params");

   do
   {
      externalApp = GetExternalApplication("Merge",
                                           dirGroups,
                                           again);

      again = false;
      if (externalApp.empty())
      {
         goto Cleanup;
      }
      command = "\"" + externalApp + "\" " + externalParams;
      if (!LaunchCommand(command, true))
      {
         DoMessageDialog(0, wxString(_("Failed to launch external merge application")) 
                         + wxString(wxT("\n"))
                         + wxString(wxText(command)));
         again = true;
      }
   } while (again);

   bResult = true;

Cleanup:
   return bResult;
}

void GetAddableFiles(const DirectoryGroups& dirGroups,
                     bool includeFolders,
                     const std::vector<std::string>& allFiles,
                     std::vector<std::string>& addableFiles,
                     std::map<std::string, bool>& dupMap)
{
   TDEBUG_ENTER("GetAddableFiles");
   std::vector<bool> ignored;
   for (size_t i = 0; i < allFiles.size(); ++i)
      CVSStatus::RecursiveNeedsAdding(allFiles[i], addableFiles, ignored, includeFolders);

   bool includeIgnored = dirGroups.GetBooleanPreference("Add Ignored Files");

   // Remove duplicate entries
   for (size_t i = 0; i < addableFiles.size(); ++i)
   {
      TDEBUG_TRACE("file: " << addableFiles[i] << " " << ignored[i]);
      if (includeIgnored || !ignored[i])
         dupMap[addableFiles[i]] = !ignored[i];
   }

   std::map<std::string, bool>::iterator it = dupMap.begin();
   addableFiles.clear();
   TDEBUG_TRACE("Readd files");
   while (it != dupMap.end())
   {
      TDEBUG_TRACE("file: " << it->first);
      addableFiles.push_back(it->first);
      ++it;
   }
}


class equal_to_first
{
public:
    equal_to_first(const std::string& value)
        : myValue(value)
    {
    }

    bool operator()(const std::pair<std::string, CVSStatus::FileStatus>& pair)
    {
        return pair.first == myValue;
    }

private:
    const std::string& myValue;
};

bool Includes(const CommitDialog::SelectedFiles& list1, const std::vector<std::string>& list2)
{
    for (std::vector<std::string>::const_iterator it = list2.begin(); it != list2.end(); ++it)
        if (find_if(list1.begin(), list1.end(), equal_to_first(*it)) == list1.end())
            return false;
    return true;
}


bool Includes(const std::vector<std::string>& list1, const std::vector<std::string>& list2)
{
    for (std::vector<std::string>::const_iterator it = list2.begin(); it != list2.end(); ++it)
        if (find(list1.begin(), list1.end(), *it) == list1.end())
            return false;
    return true;
}


std::set<std::string> GetTopLevelDirs(const std::vector<std::string>& files)
{
    assert(false);
    std::set<std::string> result;
    for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it)
    {
        // TODO
    }
    return result;
}


bool ComputeCommitSelection(const CommitDialog::SelectedFiles& userSelection,
                            const FilesWithBugnumbers* modified,
                            const FilesWithBugnumbers* added,
                            const FilesWithBugnumbers* removed,
                            const FilesWithBugnumbers* renamed,
                            std::vector<std::string>& result)
{
#if 0
    // Optimization: If all files are selected, we can simply do a 'cvs commit' in the top-level directories
    if (Includes(userSelection, modified) &&
        Includes(userSelection, added) &&
        Includes(userSelection, removed) &&
        Includes(userSelection, renamed))
    {
        std::set<std::string> modifiedDirs;
        if (modified)
            modifiedDirs = GetTopLevelDirs(*modified);
        std::set<std::string> addedDirs;
        if (addedDirs)
            addedDirs = GetTopLevelDirs(*added);
        std::set<std::string> removedDirs;
        if (removedDirs)
            removedDirs = GetTopLevelDirs(*removed);
        std::set<std::string> renamedDirs;
        if (renamedDirs)
            renamedDirs = GetTopLevelDirs(*renamed);
        std::set<std::string> dirs1;
        std::set_union(modifiedDirs.begin(), modifiedDirs.end(), addedDirs.begin(), addedDirs.end(), std::inserter(dirs1, dirs1.begin()));
        std::set<std::string> dirs2;
        std::set_union(removedDirs.begin(), removedDirs.end(), renamedDirs.begin(), renamedDirs.end(), std::inserter(dirs2, dirs2.begin()));
        std::set<std::string> dirs;
        std::set_union(dirs1.begin(), dirs1.end(), dirs2.begin(), dirs2.end(), std::inserter(dirs, dirs.begin()));
        result.clear();
        for (std::set<std::string>::iterator it = dirs.begin(); it != dirs.end(); ++it)
            result.push_back(*it);
        return true;
    }
    else
#endif
    {
        // Get list of non-renamed files
        result.clear();
        for (CommitDialog::SelectedFiles::const_iterator it = userSelection.begin(); it != userSelection.end(); ++it)
            result.push_back(it->first);

        // Check if any renames are to be committed
        std::vector<std::string> renameDirs;
        for (CommitDialog::SelectedFiles::const_iterator it = userSelection.begin(); it != userSelection.end(); ++it)
            if (it->second == CVSStatus::STATUS_RENAMED)
                renameDirs.push_back(GetDirectoryPart(it->first));

        // For each directory containing renamed files
        for (std::vector<std::string>::iterator it = renameDirs.begin(); it != renameDirs.end(); ++it)
        {
            // Check if this directory contains any modified files that the user did not select
            std::vector<std::string> files;
            if (modified)
                files = GetFilesInFolder(*modified, *it);  
            for (std::vector<std::string>::iterator fileIter = files.begin(); fileIter != files.end(); ++fileIter)
                if (find(result.begin(), result.end(), *fileIter) == result.end())
                    // This file is not selected
                    return false;

            // Same for added
            files.clear();
            if (added)
                files = GetFilesInFolder(*added, *it);  
            for (std::vector<std::string>::iterator fileIter = files.begin(); fileIter != files.end(); ++fileIter)
                if (find(result.begin(), result.end(), *fileIter) == result.end())
                    // This file is not selected
                    return false;

            // Same for removed
            files.clear();
            if (removed)
                files = GetFilesInFolder(*removed, *it);  
            for (std::vector<std::string>::iterator fileIter = files.begin(); fileIter != files.end(); ++fileIter)
                if (find(result.begin(), result.end(), *fileIter) == result.end())
                    // This file is not selected
                    return false;

            // Same for renamed
            files.clear();
            if (renamed)
                files = GetFilesInFolder(*renamed, *it);  
            for (std::vector<std::string>::iterator fileIter = files.begin(); fileIter != files.end(); ++fileIter)
                if (find(result.begin(), result.end(), *fileIter) == result.end())
                    // This file is not selected
                    return false;
        }
        // Remove all files from the directories where renames are being committed
        result.clear();
        for (CommitDialog::SelectedFiles::const_iterator it = userSelection.begin(); it != userSelection.end(); ++it)
        {
            std::string dir = GetDirectoryPart(it->first);
            if (find(renameDirs.begin(), renameDirs.end(), dir) == renameDirs.end())
                result.push_back(it->first);
        }
        // Add directories where renames are being committed
        for (std::vector<std::string>::iterator it = renameDirs.begin(); it != renameDirs.end(); ++it)
            result.push_back(*it);
    }
    return true;
}


std::string GetPreferenceKey(const std::string& dir)
{
   CVSRoot root(CVSStatus::CVSRootForPath(dir));
   std::string module = CVSStatus::CVSRepositoryForPath(dir);
   std::string::size_type slashPos = module.find("/");
   if (slashPos != std::string::npos)
      module = module.substr(0, slashPos);
   return root.GetServer() + ":" + root.GetDirectory() + ":" + module;
}
