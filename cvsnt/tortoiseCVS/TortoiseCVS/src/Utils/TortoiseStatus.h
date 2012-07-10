// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2003 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - October 2003

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

#ifndef TORTOISE_STATUS_H
#define TORTOISE_STATUS_H

#include "../CVSGlue/CVSStatus.h"
#include <string>
#include <map>


// Class to store TortoiseCVS directory status info
class TortoiseDirectoryStatus
{
public:
   // Constructor
   TortoiseDirectoryStatus(const std::string& directory);

   // Load the status file
   void Load();

   // Save the status file
   void Save();

   // Set directory status
   inline void SetDirStatus(CVSStatus::FileStatus status)
   {
      m_DirStatus = status;
   }

   // Get directory status
   inline CVSStatus::FileStatus GetDirStatus()
   {
      return m_DirStatus;
   }

   // Set recursive directory status
   inline void SetDirStatusRec(CVSStatus::FileStatus status)
   {
      m_DirStatusRec = status;
   }

   // Get recursive directory status
   inline CVSStatus::FileStatus GetDirStatusRec()
   {
      return m_DirStatusRec;
   }

private:
   // Directory name
   std::string m_Dirname;

   // Directory status
   CVSStatus::FileStatus m_DirStatus;

   // Recursive directory status
   CVSStatus::FileStatus m_DirStatusRec;

   // Convert text to filestatus
   static CVSStatus::FileStatus TextToFileStatus(const char* text, 
      CVSStatus::FileStatus defStatus);

   // Convert filestatus to text
   static const char* FileStatusToText(CVSStatus::FileStatus status);

};


// Class to store TortoiseCVS commit selection info
class TortoiseCommitStatus
{
public:
   // Constructor
   TortoiseCommitStatus(const std::string& dirname);

   // Load the status file
   void Load();

   // Save the status file
   void Save();

   // Add deselected file
   void AddDeselectedFile(const char* filename);

   // Check if file is deselected
   bool IsFileDeselected(const char* filename);
private:
   // Directory name
   std::string m_Dirname;
   // map of files
   std::map<std::string, std::string> m_DeselectedFiles;
};

#endif
