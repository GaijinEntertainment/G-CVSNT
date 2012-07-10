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

#ifndef DIRECTORYGROUP_H
#define DIRECTORYGROUP_H

#include <string>
#include <vector>
#include "../CVSGlue/CVSStatus.h"
#include "../Utils/FileTree.h"
#include "../CVSGlue/CVSFeatures.h"

class DirectoryGroup
{
public:
   // Constructor
   DirectoryGroup()
   {
   }

   // Destructor
   ~DirectoryGroup()
   {
      for (std::vector<Entry*>::iterator it = myEntries.begin(); it != myEntries.end(); ++it)
         delete *it;
   }

   class Entry;
   
   // User defined data attached to entry
   class UserData 
   {
   public:
      // Constructor
      UserData(Entry& entry)
         : myEntry(entry)
      {
      }

      // Destructor
      virtual ~UserData() { }

      // Pointer to entry
      Entry& myEntry;
   };

   // User data for adding files
   class AddFilesData : public UserData
   {
   public:
      AddFilesData(Entry& entry)
         : UserData(entry)
      {
      }

      bool                      mySelected;
      CVSStatus::FileFormat     myFileFormat;
      CVSStatus::KeywordOptions myKeywordOptions;
      CVSStatus::FileOptions    myFileOptions;
   };


   // Entry in directory group
   class Entry 
   {
   public:
      // Constructor
      Entry(DirectoryGroup& group, 
            const std::string& absoluteFile = "", 
            UserData* userData = 0) 
         : myDirectoryGroup(group),
           myAbsoluteFile(absoluteFile), 
           myUserData(userData) 
      { 
      }

      Entry(DirectoryGroup& group, 
            const std::string& absoluteFile,
            const std::string& relativeFile,
            UserData* userData) 
         : myDirectoryGroup(group),
           myAbsoluteFile(absoluteFile),
           myRelativeFile(relativeFile),
           myUserData(userData)
      { 
      }
       
      // Destructor
      virtual ~Entry() 
      { 
         delete myUserData;
      }

      DirectoryGroup&   myDirectoryGroup;
      std::string       myAbsoluteFile;
      std::string       myRelativeFile;
      UserData*         myUserData;
   };

   // Entry data for file tree
   class EntryTreeData : public FileTree::UserData
   {
   public:
      EntryTreeData(FileTree::Node&, Entry &entry, void*) 
         : m_Entry(entry)
      {
      }
      Entry& m_Entry;
   };

   inline const Entry& operator[](size_t i) const
   {
      return *myEntries[i];
   }

   inline Entry& operator[](size_t i) 
   {
      return *myEntries[i];
   }

   void ShortenPaths();
    
   // Build a file tree
   template<class T> FileTree* BuildFileTree(void* data)
   {
      // Create new tree
      FileTree* fileTree = new FileTree();

      // Set root directory
      fileTree->GetRoot()->SetName(myDirectory);

      // Add files
      for (size_t j = 0; j < myEntries.size(); j++)
      {
         FileTree::NodeType type;
         DirectoryGroup::Entry& entry = *myEntries[j];
         ASSERT(&entry.myDirectoryGroup == this);

         if (!IsDirectory(entry.myAbsoluteFile.c_str()))
            type = FileTree::TYPE_FILE;
         else
            type = FileTree::TYPE_DIR;

         FileTree::Node* node = fileTree->AddNode(entry.myRelativeFile, type);
         node->SetUserData(new T(*node, entry, data));
      }

      // recalc ids
      fileTree->CalcNodeIds();
   
      return fileTree;
   }
   
   // STL container interface
   size_t size() const  { return myEntries.size(); }
   bool empty()        const  { return myEntries.empty(); }
   
   // Sort helper
   inline static bool SortEntriesByFileName(Entry* entry1, Entry* entry2)
   {
      // A "." alway comes first
      if (entry1->myRelativeFile.empty() && !entry2->myRelativeFile.empty())
         return true;
      if (entry2->myRelativeFile.empty())
         return false;

      return entry1->myRelativeFile < entry2->myRelativeFile;
   }

   std::string          myDirectory;
   std::string          myCVSRoot;
   std::vector<Entry*>  myEntries;
   CVSServerFeatures    myCVSServerFeatures;
   CVSClientFeatures    myCVSClientFeatures;

private:
    std::string FindCommonStub();
};



class DirectoryGroups
{
public:
   // Constructor
   DirectoryGroups()
      : myDoNotGroupDirectories(false)
   {
   }

   // Destructor
   ~DirectoryGroups()
   {
      clear();
   }

   // return size
   inline size_t size() 
   { 
      return myGroups.size(); 
   }

   inline bool empty() const
   { 
      return myGroups.empty(); 
   }
   
   // return reference
   inline DirectoryGroup& operator[](size_t i) 
   { 
      return *myGroups[i]; 
   }

   // clear all groups
   inline void clear()
   {
      for (std::vector<DirectoryGroup*>::iterator it = myGroups.begin(); it != myGroups.end(); ++it)
         delete *it;
      myGroups.clear();
   }

   // add element
   inline void add(DirectoryGroup* group)
   {
      myGroups.push_back(group);
   }

    bool GetBooleanPreference(const std::string& name) const;

    std::string GetStringPreference(const std::string& name) const;

    void SetStringPreference(const std::string& name, const std::string& value) const;

    int GetIntegerPreference(const std::string& name) const;

    bool PreprocessFileList(std::vector<std::string>& allFiles, bool groupByTopDir = false);

    // For commands that require a single value
    std::string                  mySingleAbsolute;
    std::string                  mySingleDirectory;
    std::string                  mySingleRelative;

    // If true, "myRelativeFile" will not contain a path.
    bool myDoNotGroupDirectories;

private:
    // Cache for preprocessing the file list
    typedef std::map<std::string, std::string, less_nocase> PreprocessCache;

    bool PreprocessFileListGrouped(std::vector<std::string>& allFiles);

    std::string GetToplevelDir(const std::string& directory,
                               bool validCvsroot = false,
                               const std::string& cvsroot = "") const;
    void SplitFileName(const std::string& file,
                       std::string& directory,
                       std::string& relativeFile) const;

    std::string GetPreferenceRootModule() const;

    std::vector<DirectoryGroup*> myGroups;
    mutable PreprocessCache      myPreprocessCache;
};

#endif
