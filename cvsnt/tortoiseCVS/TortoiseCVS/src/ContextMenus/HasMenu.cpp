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


#include <StdAfx.h>
#include "HasMenu.h"
#include "../CVSGlue/CVSStatus.h"
#include "../Utils/PathUtils.h"
#include "../Utils/ShellUtils.h"
#include "../Utils/TortoiseRegistry.h"
#include "../Utils/TortoiseDebug.h"
#include "../TortoiseAct/TortoisePreferences.h"
#include "../TortoiseAct/DirectoryGroup.h"
#include <iomanip>
#include <Shlwapi.h>


class HasMenuInfo 
{
public:
   HasMenuInfo()
      : m_Ignore(false),
        m_TimeStamp(0)
   {
   }
   bool m_Ignore;
   DWORD m_TimeStamp;
};


class FileInfo
{
public:
    FileInfo()
        : m_Ignore(false),
          m_IsDirectory(false),
          m_isStatic(false)
    {
    }
    bool m_Ignore;
    bool m_IsDirectory;
    bool m_isStatic;
    CVSStatus::FileStatus m_Status;
};


#define HAS_MENU_CACHE_SIZE 10
#define HAS_MENU_CACHE_TIMEOUT 1000

TortoiseMemoryCache<std::string, HasMenuInfo> g_HasMenuCache(HAS_MENU_CACHE_SIZE);
CriticalSection g_HasMenuCS;




bool HasMenu(int flags, const std::vector<std::string>& files)
{
   std::vector<bool> hasMenus(1);
   std::vector<int> menuFlags(1);
   menuFlags[0] = flags;
   HasMenus(menuFlags, files, hasMenus);
   return hasMenus[0];
}


void HasMenus(std::vector<int> menuFlags, const std::vector<std::string>& files, std::vector<bool>& hasMenus)
{
   TDEBUG_ENTER("HasMenus");

   // Set all of hasMenus to false
   for (size_t i = 0; i < hasMenus.size(); i++)
      hasMenus[i] = false;

   std::vector<FileInfo> fileInfos(files.size());
   
   // check cache for directory
   CSHelper cs(g_HasMenuCS, false);

   size_t i = 0;
   for (std::vector<std::string>::const_iterator it = files.begin(); 
        it != files.end(); 
        ++it, i++)
   {
      TDEBUG_TRACE("Checking cache for " << *it);
      fileInfos[i].m_IsDirectory = IsDirectory((*it).c_str());
      std::string dir;
      dir = *it;
      if (!fileInfos[i].m_IsDirectory)
         dir = StripLastPart(dir);
      bool getUncached = false;
      DWORD tc = GetTickCount();

      cs.Enter();
      TDEBUG_TRACE("Searching cache for " << dir);
      HasMenuInfo* hasMenuInfo = g_HasMenuCache.Find(dir);
      if (hasMenuInfo)
      {
         TDEBUG_TRACE("Found " << dir);
         if (tc - hasMenuInfo->m_TimeStamp < HAS_MENU_CACHE_TIMEOUT)
         {
            TDEBUG_TRACE("Not outdated :)");
            fileInfos[i].m_Ignore = hasMenuInfo->m_Ignore;
         }
         else
         {
            TDEBUG_TRACE("Outdated :(");
            getUncached = true;
         }
      }

      if (!hasMenuInfo)
      {
         TDEBUG_TRACE("Create new entry");
         hasMenuInfo = g_HasMenuCache.Insert(dir);
         getUncached = true;
      }

      if (getUncached)
      {
         hasMenuInfo->m_TimeStamp = tc;
         if (it->empty())
         {
             TDEBUG_TRACE("Empty");
             fileInfos[i].m_Ignore = true;
             hasMenuInfo->m_Ignore = true;
             continue;
         }

         if (CVSStatus::IsDisabledPath(*it, fileInfos[i].m_IsDirectory))
         {
             TDEBUG_TRACE("Disabled path");
             fileInfos[i].m_Ignore = true;
             hasMenuInfo->m_Ignore = true;
             continue;
         }
      }
      cs.Leave();
   }

   // Build file infos
   for (size_t i = 0; i < files.size(); i++)
   {
      TDEBUG_TRACE("Build file info for " << files[i]);
      if (fileInfos[i].m_Ignore)
      {
          TDEBUG_TRACE("Ignored");
          continue;
      }

      // Nothing matches if it is called "CVS"
      // (e.g. the CVS directory)
      std::string last = ExtractLastPart(files[i]);
      if (stricmp(last.c_str(), "cvs") == 0)
      {
          TDEBUG_TRACE("Skip CVS folder");
          fileInfos[i].m_Ignore = true;
          continue; 
      }

      bool skipStatus = false;
      if (!CVSDirectoryAbove(files[i]))
      {
          TDEBUG_TRACE("None above");
          if (!CVSDirectoryHere(files[i]))
          {
              TDEBUG_TRACE("None here");
              skipStatus = true;
          }
      }

      if (skipStatus)
      {
          TDEBUG_TRACE("NOWHERENEAR_CVS");
          fileInfos[i].m_Status = CVSStatus::STATUS_NOWHERENEAR_CVS;
      }
      else
      {
          fileInfos[i].m_Status = CVSStatus::GetFileStatus(files[i]);
          fileInfos[i].m_isStatic = (CVSStatus::GetFileOptions(files[i]) & CVSStatus::foStaticFile) ? true : false;
      }
      TDEBUG_TRACE("Status: " << fileInfos[i].m_Status << " static: " << fileInfos[i].m_isStatic);
   }

   for (size_t f = 0; f < menuFlags.size(); f++)
   {
      // Check there is exactly one
      if (menuFlags[f] & EXACTLY_ONE)
      {
         if (files.size() != 1)
         {
            hasMenus[f] = false;
            continue;
         }
      }

      if (menuFlags[f] & (HAS_EDIT | HAS_EXCL_EDIT))
      {
         int editPolicy = GetIntegerPreference("Edit policy", files[0]);
         if (editPolicy < TortoisePreferences::EditPolicyConcurrent)
         {
            hasMenus[f] = false;
            continue;
         }
         if ((menuFlags[f] & HAS_EXCL_EDIT) && (editPolicy != TortoisePreferences::EditPolicyConcurrentExclusive))
         {
            hasMenus[f] = false;
            continue;
         }
      }
      
      bool allMustMatch = (menuFlags[f] & ALL_MUST_MATCH_TYPE) ? true : false;
      bool match = allMustMatch;
   
      for (size_t i = 0; i < files.size(); ++i)
      {
         // Clever bit to see if ALL must match type,
         // or just at least one:

         // ALL: Stop if one breaks the rule
         if (allMustMatch && !match)
         {
            TDEBUG_TRACE("ALL: One breaks the rule");
            break;
         }

         // AT LEAST ONE: Stop if one meets the rule
         if (!allMustMatch && match)
         {
            TDEBUG_TRACE("AT LEAST ONE: One meets the rule");
            break;
         }

         // Determine whether this file meets the criteria
         match = false;

         if (fileInfos[i].m_Ignore && !(menuFlags[f] & ALWAYS_HAS))
            continue;

         // Check directory/file type
         if (menuFlags[f] & TYPE_DIRECTORY)
         {
            if (!fileInfos[i].m_IsDirectory)
               continue; // false
         }

         if (menuFlags[f] & TYPE_FILE)
         {
            if (fileInfos[i].m_IsDirectory)
               continue; // false
         }

         if (menuFlags[f] & ALWAYS_HAS)
         {
            match = true;
            continue;
         }

         // Check the ones that correspond to CVSStatus::STATUS_
         CVSStatus::FileStatus status = fileInfos[i].m_Status;
         match =
            (CVSStatus::STATUS_NOTINCVS == status && (TYPE_NOTINCVS & menuFlags[f])) ||
            (CVSStatus::STATUS_IGNORED == status && (TYPE_IGNORED & menuFlags[f])) ||
            (CVSStatus::STATUS_NOWHERENEAR_CVS == status && (TYPE_NOWHERENEAR_CVS & menuFlags[f])) ||
            (CVSStatus::STATUS_INCVS_RW == status && (TYPE_INCVS_RW & menuFlags[f])) ||
            (CVSStatus::STATUS_INCVS_RO == status && (TYPE_INCVS_RO & menuFlags[f])) ||
            (CVSStatus::STATUS_LOCKED == status && (TYPE_INCVS_LOCKED & menuFlags[f])) ||
            (CVSStatus::STATUS_LOCKED_RO == status && (TYPE_INCVS_LOCKED & menuFlags[f])) ||
            (CVSStatus::STATUS_ADDED == status && (TYPE_ADDED & menuFlags[f])) ||
            (CVSStatus::STATUS_CHANGED == status && (TYPE_CHANGED & menuFlags[f])) ||
            (CVSStatus::STATUS_RENAMED == status && (TYPE_RENAMED & menuFlags[f])) ||
            (CVSStatus::STATUS_CONFLICT == status && (TYPE_CONFLICT & menuFlags[f])) ||
            (CVSStatus::STATUS_OUTERDIRECTORY == status && (TYPE_OUTERDIRECTORY & menuFlags[f])) ||
             (fileInfos[i].m_isStatic && (TYPE_STATIC & menuFlags[f]));
      }

      hasMenus[f] = match;
   }
}
