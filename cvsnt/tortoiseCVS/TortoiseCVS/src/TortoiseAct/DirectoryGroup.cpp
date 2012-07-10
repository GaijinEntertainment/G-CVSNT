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
#include <CVSGlue/CVSRoot.h>
#include <Utils/TortoiseRegistry.h>
#include <Utils/Preference.h>
#include "DirectoryGroup.h"


// Sort into groups of files which are in the same sandbox.  Each cluster
// can then be passed into CVS together.  Files from different directories
// can be operated on at once from the Find In Files dialog in Explorer.
bool DirectoryGroups::PreprocessFileList(std::vector<std::string>& allFiles, bool groupByTopDir)
{
    TDEBUG_ENTER("DirectoryGroups::PreprocessFileList");
    // Break into chunks of the same checked out sandbox 
    // For each directory, store a vector of files and other data in the
    // DirectoryGroup.  The group has relative and absolute filenames
    // because CVS requires relative, but it is convenient for TortoiseCVS
    // to use the absolute ones when it needs to.
    // (to be precise, only NT server _requires_ relative paths to a
    // current directory.  Unix CVS server can cope with absolute ones.
    // However, it is good practice anyway.)

    clear();

    if (groupByTopDir)
        return PreprocessFileListGrouped(allFiles);
    
   // First group by CVSROOT
   
   typedef std::multimap<std::string, DirectoryGroup*> DirectoryMap;
   DirectoryMap directoryMap;
   for (unsigned int i = 0; i < allFiles.size(); ++i)
   {
      std::string file = allFiles[i];
      TDEBUG_TRACE("File: " << file);
      
      std::string directory = GetDirectoryPart(file);
      TDEBUG_TRACE("Dir: " << directory);

      std::string cvsroot = CVSStatus::CVSRootForPath(directory);
      while (cvsroot.empty())
      {
          std::string directoryAbove = GetDirectoryAbove(directory);
          if (directoryAbove == directory)
              break;
          directory = directoryAbove;
          cvsroot = CVSStatus::CVSRootForPath(directory);
      }
      TDEBUG_TRACE("CVSROOT: " << cvsroot);

      DirectoryMap::iterator it = directoryMap.find(cvsroot);
      DirectoryGroup* group = 0;
      bool newGroup = false;
      if (it == directoryMap.end())
          newGroup = true;
      else
      {
          // The CVSROOT matches, but be careful: If two modules are located
          // inside a non-CVS folder, we could end up running CVS in that folder.
          if (IsDirectory(file.c_str()) && !CVSDirectoryAbove(it->second->myDirectory))
              newGroup = true;
      }
      if (newGroup)
      {
          TDEBUG_TRACE("New group");
          group = new DirectoryGroup;
          group->myDirectory = directory;
          directoryMap.insert(std::make_pair(cvsroot, group));
          group->myCVSRoot = cvsroot;
          group->myCVSServerFeatures.SetCVSRoot(group->myCVSRoot);
      }
      else
          group = it->second;
      group->myEntries.push_back(new DirectoryGroup::Entry(*group, file));
   }

   // For each group, compute common path
   DirectoryMap::iterator it;
   for (it = directoryMap.begin(); it != directoryMap.end(); ++it)
       it->second->ShortenPaths();

   // Copy into the vector for easier access by looping
   for (it = directoryMap.begin(); it != directoryMap.end(); ++it)
      add(it->second);

   // Compute relative names
   for (std::vector<DirectoryGroup*>::iterator it = myGroups.begin(); it != myGroups.end(); ++it)
   {
       DirectoryGroup& group = **it;
       size_t dirsize = group.myDirectory.size();
       for (size_t i = 0; i < group.size(); ++i)
       {
           size_t strip = dirsize;
           if (strip < group[i].myAbsoluteFile.size())
               ++strip;
           group[i].myRelativeFile = group[i].myAbsoluteFile.substr(strip);
       }
   }
   
   // Cache single values, so commands like Diff can look them up easily
   if (allFiles.size() == 1)
   {
      mySingleAbsolute = allFiles[0];
      mySingleDirectory = GetDirectoryPart(mySingleAbsolute);
      std::string::size_type s = mySingleDirectory.size();
      if ((s < mySingleAbsolute.size()) && (mySingleAbsolute[s] == '\\'))
          ++s;
      mySingleRelative = mySingleAbsolute.substr(s);
      TDEBUG_TRACE("mySingleAbsolute: " << mySingleAbsolute);
   }
   
   myPreprocessCache.clear();
   return true;
}


bool DirectoryGroups::PreprocessFileListGrouped(std::vector<std::string>& allFiles)      
{    
    TDEBUG_ENTER("DirectoryGroups::PreprocessFileListGrouped");      
         
    typedef std::map<std::string, DirectoryGroup*> DirectoryMap;     
    DirectoryMap directoryMap;   
    for (unsigned int i = 0; i < allFiles.size(); ++i)   
    {    
        std::string file = allFiles[i];      
        TDEBUG_TRACE("File: " << file);      
         
        // Find directory and relative filename      
        std::string directory;   
        std::string relativeFile;    
        SplitFileName(file, directory, relativeFile);    
        TDEBUG_TRACE("Dir: " << directory);      
        TDEBUG_TRACE("Relative file: " << relativeFile);     
         
        std::string key = directory;     
        // Normal case   
        //   In e:\cvswork\test: cvs update subdir      // Put in map    
        DirectoryMap::iterator it = directoryMap.find(directory);    
        DirectoryGroup* group = 0;   
        if (it == directoryMap.end())    
        {    
            group = new DirectoryGroup();    
            directoryMap[directory] = group;     
            group->myDirectory = directory;      
            group->myCVSRoot = CVSStatus::CVSRootForPath(directory);     
            group->myCVSServerFeatures.SetCVSRoot(group->myCVSRoot);     
        }    
        else     
        {    
            group = it->second;      
        }    
        group->myEntries.push_back(new DirectoryGroup::Entry(*group, file, relativeFile, 0));    
    }    
         
    // Copy into the vector for easier access by looping     
    DirectoryMap::iterator it;   
    for (it = directoryMap.begin(); it != directoryMap.end(); ++it)      
        add(it->second);     
         
    myPreprocessCache.clear();   
    return true;     
}    
     

void DirectoryGroup::ShortenPaths()
{
    TDEBUG_ENTER("ShortenPaths");
    if (myEntries.empty())
        return;

    myDirectory = RemoveTrailingDelimiter(FindCommonStub());

    TDEBUG_TRACE("Directory " << myDirectory);

    std::vector<Entry*>::iterator it = myEntries.begin();
    while (it != myEntries.end())
    {
        std::string::size_type s = myDirectory.size();
        if ((*it)->myAbsoluteFile[s] == '\\')
            ++s;
        (*it)->myRelativeFile = (*it)->myAbsoluteFile.substr(s);
        TDEBUG_TRACE("Relative " << (*it)->myRelativeFile);
        ++it;
    }

    // Special case for directory
    if ((myEntries.size() == 1) && IsDirectory(myEntries[0]->myAbsoluteFile.c_str()))
    {
        myDirectory = EnsureTrailingDelimiter(myDirectory) + myEntries[0]->myRelativeFile;
        myEntries[0]->myRelativeFile.clear();
    }
}


std::string DirectoryGroup::FindCommonStub()
{
    std::string stub;
    bool repeat = true;
    std::string::size_type offset = 0;
    while (repeat)
    {
        std::string firstPart;
        for (size_t i = 0; i < myEntries.size(); ++i)
        {
            std::string nextFirstPart = GetFirstPart(myEntries[i]->myAbsoluteFile.substr(offset));
            if (nextFirstPart.empty())
                return stub;
                        
            // If a first chunk of the path doesn't match, we 
            // give up stripping any more
            if (!firstPart.empty() && firstPart != nextFirstPart)
                return stub;
            firstPart = nextFirstPart;
                        
            // And we need something left as well!
            if (StripFirstPart(myEntries[i]->myAbsoluteFile.substr(offset)).empty())
                return stub;
        }
                
        // Remember the stub
        stub += GetFirstPart(myEntries[0]->myAbsoluteFile.substr(offset));

        // Right, we now know we can strip the front off everything
        offset = stub.size();
    }

    return stub;
}


std::string DirectoryGroups::GetToplevelDir(const std::string& directory,
                                            bool validCvsroot, 
                                            const std::string& cvsroot) const
{
   // Check the cache
   PreprocessCache::iterator it = myPreprocessCache.find(directory);
   if (it != myPreprocessCache.end())
   {
      return it->second;
   }

   std::string result;

   // Get parent dir
   std::string parentdir = GetDirectoryAbove(directory);
   if (parentdir == directory)
      result = directory;
   else
   {
      // Get CVSROOT for current dir
      std::string mycvsroot;
      if (validCvsroot)
      {
         mycvsroot = cvsroot;
      }
      else if (CVSDirectoryHere(directory))
      {
         mycvsroot = CVSStatus::CVSRootForPath(directory);
      }

      // if we don't have a CVSROOT, return parent dir
      if (mycvsroot.empty())
      {
         result = GetToplevelDir(parentdir);
      }
      else
      {
         // Get CVSROOT for parent dir
         std::string parentcvsroot;
         if (CVSDirectoryHere(parentdir))
         {
            parentcvsroot = CVSStatus::CVSRootForPath(parentdir);
         }
      
         // if parent dir has different (or no) CVSROOT, we've reached the toplevel
         if (parentcvsroot != mycvsroot)
         {
            result = directory;
         }
         else
         {
            result = GetToplevelDir(parentdir, true, parentcvsroot);
         }
      }
   }

   // Store dir in cache
   myPreprocessCache[directory] = result;

   return result;
}


void DirectoryGroups::SplitFileName(const std::string& file,
                                    std::string& directory,
                                    std::string& relativeFile) const
{
   std::string fileDir = GetDirectoryPart(file);
   
   if (myDoNotGroupDirectories)
      directory = fileDir;
   else
   {
      // Scan for the top level CVS directory
      // - we do everything relative to that
      directory = GetToplevelDir(fileDir);
   }
   
   // Get the relative filename
   if (stricmp(directory.c_str(), file.c_str()) == 0)
   {
      relativeFile = ".";
   }
   else
   {
      std::string::size_type p = directory.length();
      while (file[p] == '\\')
         p++;
      relativeFile = file.substr(p);
   }
}


bool DirectoryGroups::GetBooleanPreference(const std::string& name) const
{
   return GetIntegerPreference(name) ? true : false;
}

std::string DirectoryGroups::GetStringPreference(const std::string& name) const
{
    std::string rootModule = GetPreferenceRootModule();
    return GetStringPreferenceRootModule(name, rootModule);
}

void DirectoryGroups::SetStringPreference(const std::string& name, const std::string& value) const
{
    std::string rootModule = GetPreferenceRootModule();
    SetStringPreferenceRootModule(name, value, rootModule);
}

int DirectoryGroups::GetIntegerPreference(const std::string& name) const
{
    std::string rootModule = GetPreferenceRootModule();
    return GetIntegerPreferenceRootModule(name, rootModule);
}

std::string DirectoryGroups::GetPreferenceRootModule() const
{
    std::string rootModule;
    if (!myGroups.empty())
    {
        std::string root = myGroups[0]->myCVSRoot;
        std::string module = CVSStatus::CVSRepositoryForPath(myGroups[0]->myDirectory);
        std::string::size_type slashPos = module.find("/");
        if (slashPos != std::string::npos)
            module = module.substr(0, slashPos);
        rootModule += root + "/" + module;
    }
    return rootModule;
}
