// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - May 2000

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
#include "CVSStatus.h"
#include "CVSAction.h"
#include "CVSRoot.h"
#include "MakeArgs.h"
#include "CvsEntries.h"
#include "CVSFeatures.h"
#include <Utils/ShellUtils.h>
#include <Utils/PathUtils.h>
#include <Utils/TortoiseDebug.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/StringUtils.h>
#include <Utils/TimeUtils.h>
#include <Utils/Translate.h>
#include <Utils/TortoiseStatus.h>
#include <Utils/Wildcard.h>
#include <Utils/Preference.h>
#include <TortoiseAct/TortoisePreferences.h>
#include "Stat.h"
#include <time.h>
#include <fstream>
#include <sstream>
#include <memory>
#include <shlwapi.h>


// Cache size CVS server version
#define CACHE_SIZE_SERVER_VERSION 10
// Entry expiration time in hours
#define CACHE_EXPIRATION_SERVER_VERSION 5 * 24 // 5 days

// Cache size
#define RECURSIVE_STATUS_CACHE_SIZE 20
// Entry expiration time in ms
#define RECURSIVE_STATUS_CACHE_EXPIRATION 1 * 1000 // 1 second

// Cache size
#define ENTNODE_DIR_CACHE_SIZE 10
// Entry expiration time in ms
#define ENTNODE_DIR_CACHE_EXPIRATION 2 * 1000 // 2 seconds

// Cache size
#define ENTNODE_DATA_CACHE_SIZE 200
// Entry expiration time in ms
#define ENTNODE_DATA_CACHE_EXPIRATION 1 * 1000 // 1 second


TortoiseMemoryCache<std::string, CVSStatus::EntnodeDirData> 
   CVSStatus::ourEntnodeDirCache(ENTNODE_DIR_CACHE_SIZE);
CriticalSection CVSStatus::ourEntnodeDirCS;


// Cache for recursive directory status
TortoiseMemoryCache<std::string, CVSStatus::OurRecStatusCacheData> 
   CVSStatus::ourRecStatusCache(RECURSIVE_STATUS_CACHE_SIZE);
CriticalSection CVSStatus::ourRecStatusCS;

// thread-safe class for string comparison
StringNoCaseCmp         CVSStatus::ourStringNoCaseCmp;

// Mask for CVSStatus bits in FileStatus
const int CVSStatus::STATUS_CVSSTATUS_MASK = 0xFF;

// Normal keyword options
const int CVSStatus::keyNormal = CVSStatus::keyNames | CVSStatus::keyValues;

#ifdef __GNUC__
typedef unsigned short uint16_t;
#else // MSVC
typedef unsigned __int16 uint16_t;
#endif


CVSStatus::EntnodeDirData::EntnodeDirData()
   : timeStamp(0), timeStampDefIgnList(0), entnodeCache(ENTNODE_DATA_CACHE_SIZE) 
{
}


// Constructor
CVSStatus::CVSVersionInfo::CVSVersionInfo(const std::string version)
   : m_IsCVSNT(false), m_IsValid(false), m_VersionMajor(0), m_VersionMinor(0),
     m_VersionRelease(0), m_VersionPatch(0)
{
   if (!version.empty())
   {
      ParseVersionString(version);
   }
}


// Convert to string
std::string CVSStatus::CVSVersionInfo::AsString()
{
   std::string str;
   if (!m_IsValid)
      return str;
   if (m_IsCVSNT)
      str += "CVSNT ";
   else
      str += "CVS ";

   char buf[100];
   // Write major and minor version
   sprintf(buf, "%d.%d", m_VersionMajor, m_VersionMinor);
   str += buf;

   if (m_VersionRelease >= 0)
   {
      sprintf(buf, ".%d", m_VersionRelease);
      str += buf;
   }

   if (m_VersionPatch >= 0)
   {
      sprintf(buf, "p%d", m_VersionPatch);
      str += buf;
   }

   return str;
}


// Parse a version string
void CVSStatus::CVSVersionInfo::ParseVersionString(const std::string& str)
{
   m_IsValid = false;
   m_VersionMajor = -1;
   m_VersionMinor = -1;
   m_VersionRelease = -1;
   m_VersionPatch = -1;

   // Check if it's CVSNT
   m_IsCVSNT = (str.find("CVSNT") != std::string::npos);

   // find the first digit of the version number
   std::string::size_type p = str.find_first_of("0123456789");
   if (p == std::string::npos)
      return;

   // read the major version number
   std::string::size_type p0 = p;
   while (isdigit(str[p]))
   {
      p++;
   }
   if (p == p0)
      return;
   m_VersionMajor = atoi(str.substr(p0, p - p0).c_str());
   if (str[p] != '.')
      return;

   // read the minor version number
   p++;
   p0 = p;
   while (isdigit(str[p]))
   {
      p++;
   }
   if (p == p0)
      return;
   m_VersionMinor = atoi(str.substr(p0, p - p0).c_str());
   m_IsValid = true;
   if (str[p] != '.')
      return;

   // read release version number
   p++;
   p0 = p;
   while (isdigit(str[p]))
   {
      p++;
   }
   if (p == p0)
      return;
   m_VersionRelease = atoi(str.substr(p0, p - p0).c_str());
   // Check if format is X.Y.Z.pP, X.Y.Z.P, or X.Y.Z (AAA) Build P
   if (str[p] == 'p')
      ++p;
   if (str[p] == '.')
      ++p;
   else
   {
      // Look for 'Build XXXX'
      p = str.find("Build ");
      if (p == std::string::npos)
         return;
      p += 6;
   }

   // read patch version number
   p0 = p;
   while (isdigit(str[p]))
   {
      p++;
   }
   if (p == p0)
      return;
   m_VersionPatch = atoi(str.substr(p0, p - p0).c_str());
}



// Set version as unknown (0.0.0.0)
void CVSStatus::CVSVersionInfo::SetUnknown()
{
   m_IsCVSNT = false;
   m_IsValid = true;
   m_VersionMajor = 0;
   m_VersionMinor = 0;
   m_VersionRelease = 0;
   m_VersionPatch = -1;
}


// Operator "greater than"
bool CVSStatus::CVSVersionInfo::operator>(const CVSVersionInfo& op) const
{
   if (m_VersionMajor > op.VersionMajor())
      return true;
   if (m_VersionMajor == op.VersionMajor() 
      && m_VersionMinor > op.VersionMinor())
      return true;
   if (m_VersionMajor == op.VersionMajor() 
      && m_VersionMinor == op.VersionMinor()
      && m_VersionRelease > op.VersionRelease())
      return true;
   if (m_VersionMajor == op.VersionMajor() 
      && m_VersionMinor == op.VersionMinor()
      && m_VersionRelease == op.VersionRelease()
      && m_VersionPatch > op.VersionPatch())
      return true;

   return false;
}


// Operator "equals"
bool CVSStatus::CVSVersionInfo::operator==(const CVSVersionInfo& op) const
{
   return (m_VersionMajor == op.VersionMajor() && m_VersionMinor == op.VersionMinor()
      && m_VersionRelease == op.VersionRelease() && m_VersionPatch == op.VersionPatch());
}


CVSStatus::EntnodeDirData* CVSStatus::GetEntnodeDirData(const std::string& directory,
                                                        CSHelper &csDir)
{
   TDEBUG_ENTER("CVSStatus::GetEntnodeDirData");

   if (!CVSDirectoryHere(directory))
   {
      return 0;
   }

   std::string justdirname = ExtractLastPart(directory);
   if (stricmp(justdirname.c_str(), "CVS") == 0)
      return 0;

   TDEBUG_TRACE("Looking for " << directory);
   // Check if it is in the cache
   EntnodeDirData *pData = 0;
   bool bUpdated = false;
   DWORD tmpTimeStamp;
   FileChangeParams tmpFcp;
   csDir.Attach(ourEntnodeDirCS);
   csDir.Enter();

   pData = ourEntnodeDirCache.Find(directory);
   if (pData)
   {
      TDEBUG_TRACE("Found " << directory);
      // Check if cache value is not too old
      if (GetTickCount() - pData->timeStamp < (ENTNODE_DIR_CACHE_EXPIRATION))
      {
         TDEBUG_TRACE("And it's not outdated");
         return pData;
      }
   }

   // Fetch new data
   if (!pData)
   {
      pData = ourEntnodeDirCache.Insert(directory);
      bUpdated = true;
   }

   CSHelper cs1(pData->entnodeCacheCS, true);
   // New ignored list
   tmpTimeStamp = pData->timeStampDefIgnList;
   tmpFcp = pData->fcpCvsIgnore;
   BuildIgnoredList(pData->ignoreList, directory, &(pData->timeStampDefIgnList), 
                    &(pData->fcpCvsIgnore));
   bUpdated |= ((tmpFcp != pData->fcpCvsIgnore) 
                || (tmpTimeStamp != pData->timeStampDefIgnList));

   tmpFcp = pData->fcpEntries;
   if (bUpdated)
   {
      pData->fcpEntries.SetNull();
   }
   // Open entries
   Entries_Open(pData->entries, directory.c_str(), &(pData->fcpEntries));
   bUpdated |= (tmpFcp != pData->fcpEntries);

   // Clear data map
   if (bUpdated)
      pData->entnodeCache.Clear();

   // Time stamp
   pData->timeStamp = GetTickCount();

   return pData;
}



// This calls the GetEntnodeData (below) which actually does the work.
// The caching speeds up directory listing display in Explorer in TortoiseCVS.
EntnodeData* CVSStatus::GetEntnodeData(const std::string& filename,
                                       CSHelper &csNode)
{
   TDEBUG_ENTER("CVSStatus::GetEntnodeData");
   TDEBUG_TRACE("Looking for " << filename);
   std::string directory = GetDirectoryAbove(filename);

   // get filename from cache
   CSHelper cs;
   EntnodeDirData* dirData = GetEntnodeDirData(directory, cs);
   if (!dirData)
      return 0;

   return GetEntnodeData(filename, dirData, csNode);
}


EntnodeData* CVSStatus::GetEntnodeData(const std::string& filename,
                                       EntnodeDirData *dirData,
                                       CSHelper &csNode)
{
   TDEBUG_ENTER("CVSStatus::GetEntnodeData");
   std::string justfilename = ExtractLastPart(filename);
   TDEBUG_TRACE("Is " << justfilename << " in the cache?" );

   // Check if it is in the cache
   EntnodeCacheData* pData = 0;
   csNode.Attach(dirData->entnodeCacheCS);
   csNode.Enter();
   pData = dirData->entnodeCache.Find(justfilename);
   if (pData)
   {
      TDEBUG_TRACE("Found " << justfilename);
      // Check if cache value is not too old
      if (GetTickCount() - pData->timeStamp < (ENTNODE_DATA_CACHE_EXPIRATION))
      {
         TDEBUG_TRACE("And it's not outdated");
         return pData->data;
      }
   }

   // Fetch new value
   if (!pData)
   {
      TDEBUG_TRACE("Insert " << justfilename << " into cache");
      pData = dirData->entnodeCache.Insert(justfilename);
   }
   pData->data = GetEntnodeDataUncached(filename, dirData);
   pData->timeStamp = GetTickCount();
   return pData->data;
}

// This stuff is a bit fiddly - I've tried to write it without
// breaking into the CvsEntries file too much.  That class was
// designed for scanning a whole directory, rather than individual
// files like we do
EntnodeData* CVSStatus::GetEntnodeDataUncached(const std::string& filename,
                                               EntnodeDirData* dirData)
{
   TDEBUG_ENTER("CVSStatus::GetEntnodeDataUncached");
   std::string justfilename = ExtractLastPart(filename);
   std::string directory = GetDirectoryAbove(filename);

   if (stricmp(justfilename.c_str(), "CVS") == 0)
      return 0;
   
   struct stat sb;
   memset(&sb, 0, sizeof(sb));
   int statReturnCode = stat(filename.c_str(), &sb);

   bool bIsDir = (sb.st_mode & _S_IFDIR) != 0;
   bool bIsReadOnly = (sb.st_mode & (_S_IWRITE + (_S_IWRITE >> 3)
                                     + (_S_IWRITE >> 6))) == 0;


   EntnodeData* data = Entries_SetVisited(directory.c_str(), dirData->entries, justfilename.c_str(),
                                          sb, bIsDir, bIsReadOnly,
                                          statReturnCode != 0, &(dirData->ignoreList));
   
   return data;
}


std::string CVSStatus::GetBugNumber(const std::string& filename)
{
   TDEBUG_ENTER("CVSStatus::GetBugNumber");
   TDEBUG_TRACE(filename);
   std::string result;

   // Check for special case of the outer directory
   if (IsOuterDirectory(filename))
   {
      TDEBUG_TRACE("OuterDir");
      // No bug number
   }
   else
   {
      TDEBUG_TRACE("Initialize CSHelper");
      CSHelper cs;
      TDEBUG_TRACE("Before GetEntnodeData");
      EntnodeData* data = GetEntnodeData(filename, cs);
      TDEBUG_TRACE("Before GetBugNumber");
      result = GetBugNumber(data);
   }
   
   return result;
}

CVSStatus::FileStatus CVSStatus::GetFileStatusRecursive(const std::string& filename, FileStatus status, 
                                                        int levels)
{
   TDEBUG_ENTER("CVSStatus::GetFileStatusRecursive");
   // Check if it is in the cache
   OurRecStatusCacheData *pData = 0;
   CSHelper cs(ourRecStatusCS, true);
   pData = ourRecStatusCache.Find(filename);
   if (pData)
   {
      // Check if cache value is not too old
      if (GetTickCount() - pData->timeStamp < (RECURSIVE_STATUS_CACHE_EXPIRATION))
         return pData->status;
   }

   // Fetch new value
   if (!pData)
      pData = ourRecStatusCache.Insert(filename);

   pData->status = GetFileStatusRecursiveUncached(filename, status, levels);
   pData->timeStamp = GetTickCount();

   return pData->status;
}



// Recursively scans directories to see if they have changed files in them
CVSStatus::FileStatus CVSStatus::GetFileStatusRecursiveUncached(const std::string& filename, 
                                                                FileStatus status, int levels)
{
    TDEBUG_ENTER("CVSStatus::GetFileStatusRecursiveUncached");
    if (status < 0)
      status = GetFileStatus(filename);

    // TODO: Make this a scoped preference?
    int overlays = GetIntegerPreference("Recursive folder overlays");
    TDEBUG_TRACE("Overlay status: " << overlays);
    if (overlays == 0) 
    {
       TDEBUG_TRACE("Don't show folder contents in overlays: Status " << status);
       return status;
    }

    if ((status == STATUS_OUTERDIRECTORY || 
         status == STATUS_INCVS_RO || 
         status == STATUS_INCVS_RW || 
         status == STATUS_ADDED) &&
        IsDirectory(filename.c_str()))
    {
       // TODO: Make this a scoped preference?
       bool autoload = GetBooleanPreference("Autoload folder overlays");

       if (autoload)
       {
          if (overlays == 1)
          { 
             // Refresh given number of levels
             TDEBUG_TRACE("Autoload dir status");
             GetDirectoryStatus(filename, &status, 0, 0, false);
          }
          else if (overlays == 2)
          {
             TDEBUG_TRACE("Autoload recursive status");
             GetDirectoryStatus(filename, 0, &status, levels, false);
          }
       }
       else
       {
          TortoiseDirectoryStatus dirStatus(filename);
          dirStatus.SetDirStatus(status);
          dirStatus.SetDirStatusRec(status);
          if (overlays == 1)
          { 
             // Refresh given number of levels
             TDEBUG_TRACE("Load dir status");
             if (levels >= 0)
                GetDirectoryStatus(filename, 0, 0, 0);
             dirStatus.Load();
             status = dirStatus.GetDirStatus();
          }
          else if (overlays == 2)
          {
             TDEBUG_TRACE("Load recursive status");
             if (levels >= 0)
                GetDirectoryStatus(filename, 0, 0, levels);
             dirStatus.Load();
             status = dirStatus.GetDirStatusRec();
          }
       }
    }

    TDEBUG_TRACE("Status: " << status);
    return status;
}


CVSStatus::FileStatus CVSStatus::GetFileStatus(const std::string& filename)
{
   TDEBUG_ENTER("CVSStatus::GetFileStatus");
   TDEBUG_TRACE(filename);
   CVSStatus::FileStatus status = STATUS_INCVS_RW;

   // Check for special case of the outer directory
   if (IsOuterDirectory(filename))
   {
      TDEBUG_TRACE("OuterDir");
      status = STATUS_OUTERDIRECTORY;
   }
   else
   {
      TDEBUG_TRACE("Initialize CSHelper");
      CSHelper cs;
      TDEBUG_TRACE("Before GetEntnodeData");
      EntnodeData* data = GetEntnodeData(filename, cs);
      TDEBUG_TRACE("Before GetFileStatus");
      status = GetFileStatus(data, GetEditStatus(filename));
   }
   
   return status;
}



CVSStatus::FileStatus CVSStatus::GetFileStatus(EntnodeData* data, bool edit)
{
   TDEBUG_ENTER("CVSStatus::GetFileStatus");

   CVSStatus::FileStatus status = STATUS_INCVS_RW;
   if (!data)
   {
      TDEBUG_TRACE("Data empty");
      status = STATUS_NOWHERENEAR_CVS;
   }
   else
   {
      TDEBUG_TRACE("Data not empty");
      if (data->IsIgnored())
      {
         TDEBUG_TRACE("Ignored");
         status = STATUS_IGNORED;
      }
      else if (data->IsUnknown())
      {
         TDEBUG_TRACE("Not in CVS");
         status = STATUS_NOTINCVS;
      }
      else if (data->IsRemoved())
      {
         TDEBUG_TRACE("Removed");
         status = STATUS_REMOVED;
      }
      else if (data->IsRenamed())
      {
         TDEBUG_TRACE("Renamed");
         status = STATUS_RENAMED;
      }
      else if (data->IsMissing())
      {
         TDEBUG_TRACE("Missing");
         status = STATUS_MISSING;
      }
      else
      {
          if (data->IsUnmodified())
          {
              if (data->IsReadOnly())
              {
                  TDEBUG_TRACE("In CVS read only, edit " << edit);
                  status = edit ? STATUS_LOCKED_RO : STATUS_INCVS_RO;
              }
              else
              {
                  TDEBUG_TRACE("In CVS, edit " << edit);
                  status = edit ? STATUS_LOCKED : STATUS_INCVS_RW;
              }
          }
          else if (data->NeedsMerge())
          {
              TDEBUG_TRACE("Conflict");
              status = STATUS_CONFLICT;
          }
          else if (data->IsAdded())
          {
              TDEBUG_TRACE("Added");
              status = STATUS_ADDED;
          }
          else
          {
              TDEBUG_TRACE("Changed");
              status = STATUS_CHANGED;
          }
          const char* options = data->GetOption();
          if (options && strchr(options, 's'))
              status = STATUS_STATIC;
      }
   }
   return status;
}

bool CVSStatus::GetEditStatus(const std::string& filename)
{
   // Determine Edit by checking existence of CVS/Base/<filename>[.gz]
   std::string basefile = GetDirectoryPart(filename);
   basefile += "/CVS/Base/";
   basefile += GetLastPart(filename);
   bool edit = false;
   if (FileExists(basefile.c_str()))
      edit = true;
   else
   {
      basefile += ".gz";
      if (FileExists(basefile.c_str()))
         edit = true;
   }
   return edit;
}  

bool CVSStatus::InCVSHasSubdirs(const std::string& filename)
{
    TDEBUG_ENTER("CVSStatus::InCVSHasSubdirs");
    CSHelper cs;

    if (IsOuterDirectory(filename))
       return false;

    EntnodeData* data = GetEntnodeData(filename, cs);
    if (!data)
    {
        return false;
    }
    return data->GetType() == ENT_SUBDIR;
}


std::string CVSStatus::ReadTemplateFile(const std::string& path)
{
   std::string template_file(path);

   template_file = GetDirectoryPart(template_file);
   template_file = EnsureTrailingDelimiter(template_file);
   if (!CVSDirectoryHere(template_file))
      return "";
   
   template_file.append("CVS/Template");
   std::ifstream in(template_file.c_str(), std::ios::in);
   if (!in.good())
      return "";

   std::stringstream res;  
   res << in.rdbuf();
   in.close();
   return res.str();
}


bool CVSStatus::ResetFileTimestamp(const std::string& filename)
{
   bool bResult = false;
   std::string ascTimestamp;
   SYSTEMTIME st;
   FILETIME ft;
   bool bIsReadOnly = false;
   HANDLE hFile = INVALID_HANDLE_VALUE;
 
   // Do we have a timestamp?
   if (!HasTimestamp(filename))
      goto Cleanup;

   // Convert timestamp to Windows format
   ascTimestamp = GetTimestamp(filename);
   if (!asctime_to_SYSTEMTIME(ascTimestamp.c_str(), &st))
      goto Cleanup;

   if (!SystemTimeToFileTime(&st, &ft))
      goto Cleanup;

   // Remove read-only flag
   bIsReadOnly = IsReadOnly(filename.c_str());
   if (bIsReadOnly)
      SetFileReadOnly(filename.c_str(), false);
   
   // Get file handle
   hFile = CreateFileA(filename.c_str(), GENERIC_WRITE, 
                       FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

   if (hFile == INVALID_HANDLE_VALUE)
      goto Cleanup;

   // Set file time
   if (!SetFileTime(hFile, NULL, NULL, &ft))
      goto Cleanup;

   bResult = true;

Cleanup:
   if (hFile != INVALID_HANDLE_VALUE)
      CloseHandle(hFile);
   if (bIsReadOnly)
      SetFileReadOnly(filename.c_str(), true);

   return bResult;
}



void CVSStatus::ScanEntries(const std::string& directory, 
                            std::vector<ScanData>& scandata, bool recurse)
{
   TDEBUG_ENTER("CVSStatus::ScanEntries");
   TDEBUG_TRACE("ScanEntries(" << directory << ")");
   
   CSHelper csDir;
   std::vector<std::string> dirs;
   std::vector<std::string>::const_iterator it;
   EntnodeMap::const_iterator ite;

   EntnodeDirData* dirData = GetEntnodeDirData(directory, csDir);
   if (!dirData)
      return;
   
   for (ite = dirData->entries.begin();
        ite != dirData->entries.end(); ite++)
   {
      std::string filename = EnsureTrailingDelimiter(directory) + ite->first;
      
      if (IsDirectory(filename.c_str()) && recurse)
      {
         dirs.push_back(filename);
      }
      EntnodeData* nodeData = GetEntnodeDataUncached(filename, dirData);
      const char* p = nodeData->GetBug();
      scandata.push_back(ScanData(filename, GetFileStatus(nodeData), p ? p : ""));
   }

   csDir.Leave();
   
   for (it = dirs.begin(); it != dirs.end(); it++)
   {
      ScanEntries(*it, scandata, recurse);
   }
}

void CVSStatus::RecursiveNeedsAdding(const std::string& scanDirectory, 
                                     std::vector<std::string>& files, 
                                     std::vector<bool>& ignored,
                                     bool includeFolders,
                                     bool dirIsIgnored)
{
   TDEBUG_ENTER("CVSStatus::RecursiveNeedsAdding");
   TDEBUG_TRACE("Dir '" << scanDirectory << "'");

   bool includeIgnored = GetBooleanPreference("Add Ignored Files", scanDirectory);

   std::vector<std::string>* ignList = 0;
   std::vector<std::string> children;
   std::vector<std::string> dirs;
   std::vector<bool> dirsIgnored;
   std::vector<std::string>::const_iterator itDirs;
   std::vector<bool>::const_iterator itDirsIgnored;
   unsigned int i;
   EntnodeDirData* dirData = 0;
   FileStatus status;
   CSHelper csDir;
   
   // First make sure that we do not ever add a "CVS" directory!
   std::string lastPart = ExtractLastPart(scanDirectory);
   if (!stricmp(lastPart.c_str(), "CVS"))
      goto Cleanup;

   // add ourself
   status = GetFileStatus(scanDirectory);

   // We need to add even if nowhere near CVS, as directories inside directories
   // come under that category. 
   if (status == STATUS_NOTINCVS || 
       status == STATUS_NOWHERENEAR_CVS ||
       status == STATUS_IGNORED)
   {
      if (includeFolders && (includeIgnored || (status != STATUS_IGNORED)))
      {
         files.push_back(scanDirectory);
         ignored.push_back(status == STATUS_IGNORED || dirIsIgnored);
      }
      if ((status == STATUS_IGNORED) && !includeIgnored)
          return;
   }
   
   if (!IsDirectory(scanDirectory.c_str()))
   {
      TDEBUG_TRACE("Not a folder");
      goto Cleanup;
   }
   
   dirData = GetEntnodeDirData(scanDirectory, csDir);
   
   // add our child files, or call recursively for child directories
   GetDirectoryContents(scanDirectory, &children, &children);
   for (i = 0; i < children.size(); ++i)
   {
      std::string filename = EnsureTrailingDelimiter(scanDirectory) + children[i];
      std::string justfilename = ExtractLastPart(filename);
      std::string directory = GetDirectoryAbove(filename);

      if (dirData)
      {
         // This directory is in CVS
         EntnodeData* data = GetEntnodeDataUncached(filename, dirData);
      
         if (!data)
            continue;

         if (stricmp(justfilename.c_str(), "CVS") == 0)
            continue;
      
         if (IsDirectory(filename.c_str()))
         {
            dirs.push_back(filename);
            dirsIgnored.push_back(data->IsIgnored());
         }
         else if (!IsSystemFile(filename.c_str()))
         {
            if (data->IsUnknown())
            {
               files.push_back(filename);
               ignored.push_back(data->IsIgnored());
            }
         }
      }
      else
      {
         if (ignList == 0)
         {
            ignList = new std::vector<std::string>;
            BuildIgnoredList(*ignList, scanDirectory, 0, 0);
         }
         // The parent directory is not in CVS, add everything except files matching .cvsignore
         bool matchIgnored = MatchIgnoredList(justfilename.c_str(), *ignList);
         if (includeIgnored || !matchIgnored)
         {
            if (IsDirectory(filename.c_str()))
            {
               dirs.push_back(filename);
               dirsIgnored.push_back(matchIgnored);
            }
            else if (!IsSystemFile(filename.c_str()))
            {
               files.push_back(filename);
               ignored.push_back(matchIgnored);
            }
         }
      }
   }

   csDir.Leave();
   

   itDirs = dirs.begin();
   itDirsIgnored = dirsIgnored.begin();
   while (itDirs != dirs.end() && itDirsIgnored != dirsIgnored.end())
   {
      RecursiveNeedsAdding(*itDirs, files, ignored, includeFolders, *itDirsIgnored);
      itDirs++;
      itDirsIgnored++;
   }

Cleanup:
   if (ignList)
      delete ignList;
}

bool CVSStatus::RecursiveHasChangedFiles(std::string scanDirectory)
{
   if (!IsDirectory(scanDirectory.c_str()))
      return false;
   
   scanDirectory = EnsureTrailingDelimiter(scanDirectory);
   EntnodeMap entries;
   
   bool result = Entries_Open(entries, scanDirectory.c_str());
   if (!result)
      return false;
    
   for (EntnodeMap::const_iterator it = entries.begin(); it != entries.end(); it++)
   {
      std::string filename = scanDirectory + it->first;

      if (IsDirectory(filename.c_str()))
      {
         if (RecursiveHasChangedFiles(filename))
            return true;
      }
      else
      {
         std::string justfilename = ExtractLastPart(filename);
         std::string directory = GetDirectoryAbove(filename);
         struct stat sb;
         memset(&sb, 0, sizeof(sb));
         int statReturnCode = stat(filename.c_str(), &sb);
         bool notThere = (statReturnCode != 0);
         
         EntnodeData* data = Entries_SetVisited(directory.c_str(), entries, justfilename.c_str(),
                                                sb, IsDirectory(filename.c_str()), 0, 0 /* !! */);
         
         if (!data->IsUnmodified())
         {
            if (data->IsAdded())
               return true;
            else if (!notThere)
               return true;
         }
      
         if (data->IsRemoved())
            return true;
      }
   }
   return false;
}


std::string CVSStatus::RecursiveFindRoot(std::string scanDirectory,
                                         const std::string& server)
{
   TDEBUG_ENTER("CVSStatus::RecursiveFindRoot");
   TDEBUG_TRACE("scanDirectory: " << scanDirectory << "  server: " << server);
   
   if (!IsDirectory(scanDirectory.c_str()))
   {
      // Should not happen
      TDEBUG_TRACE("Oops, nondirectory");
      return "";
   }

   std::string thisroot = CVSRootForPath(scanDirectory);
   std::string thisserver = CVSRoot(thisroot).GetServer();
   TDEBUG_TRACE("thisserver: " << thisserver);
   if (thisserver == server)
      return scanDirectory;

   scanDirectory = EnsureTrailingDelimiter(scanDirectory);
   
   EntnodeMap entries;
   
   bool result = Entries_Open(entries, scanDirectory.c_str());
   if (!result)
      return "";
    
   for (EntnodeMap::const_iterator it = entries.begin();
        it != entries.end(); it++)
   {
      std::string filename = scanDirectory + it->first;
      
      if (IsDirectory(filename.c_str()))
      {
         std::string result = RecursiveFindRoot(filename, server);
         if (!result.empty())
            return result;
      }
   }
   return "";
}


bool CVSStatus::HasRevisionNumber(const std::string& filename)
{
   CSHelper cs;
   EntnodeData* data = GetEntnodeData(filename, cs);
   return HasRevisionNumber(data);
}


bool CVSStatus::HasRevisionNumber(EntnodeData* data)
{
   const char* p = data ? data->GetVN() : 0;
   return (p && strlen(p));
}


bool CVSStatus::HasTimestamp(const std::string& filename)
{
   CSHelper cs;
   EntnodeData* data = GetEntnodeData(filename, cs);
   return HasTimestamp(data);
}


bool CVSStatus::HasTimestamp(EntnodeData* data)
{
   const char* p = 0;
   if (data)
   {
      p = data->GetConflict();
      if (p == 0)
         p = data->GetTS();
   }

   return (p && strlen(p));
}


bool CVSStatus::HasStickyTag(const std::string& filename)
{
   if (IsOuterDirectory(filename))
   {
      std::string tagFile = EnsureTrailingDelimiter(filename) + "CVS/Tag";
      std::string tag;
      char cmd;
      Tag_Open(tag, tagFile.c_str(), &cmd);
      return cmd == 'T' || cmd == 'N';
   }  
   else
   {
      CSHelper cs;
      EntnodeData* data = GetEntnodeData(filename, cs);
      return HasStickyTag(data);
   }
}


bool CVSStatus::HasStickyTag(EntnodeData* data)
{
   const char* p = data ? data->GetTagOnly() : 0;
   return (p && strlen(p));
}


bool CVSStatus::HasStickyDate(const std::string& filename)
{
   if (IsOuterDirectory(filename))
   {
      std::string tagFile = EnsureTrailingDelimiter(filename) + "CVS/Tag";
      std::string tag;
      char cmd;
      Tag_Open(tag, tagFile.c_str(), &cmd);
      return cmd == 'D';
   }  
   else
   {
      CSHelper cs;
      EntnodeData* data = GetEntnodeData(filename, cs);
      return HasStickyDate(data);
   }
}


bool CVSStatus::HasStickyDate(EntnodeData* data)
{
   const char* p = data ? data->GetDateOnly() : 0;
   return (p && strlen(p));
}


bool CVSStatus::HasOptions(const std::string& filename)
{
   CSHelper cs;
   EntnodeData* data = GetEntnodeData(filename, cs);
   return HasOptions(data);
}


bool CVSStatus::HasOptions(EntnodeData* data)
{
   const char* p = data ? data->GetOption() : 0;
   return (p && strlen(p));
}


std::string CVSStatus::GetRevisionNumber(const std::string& filename)
{
   CSHelper cs;
   EntnodeData* data = GetEntnodeData(filename, cs);
   return GetRevisionNumber(data);
}


std::string CVSStatus::GetRevisionNumber(EntnodeData* data)
{
   return data->GetVN();
}


std::string CVSStatus::GetTimestamp(const std::string& filename)
{
   CSHelper cs;
   EntnodeData* data = GetEntnodeData(filename, cs);
   return GetTimestamp(data);
}


std::string CVSStatus::GetTimestamp(EntnodeData* data)
{
   const char* p = data->GetConflict();
   if (p == 0)
      p = data->GetTS();
   return p;
}


std::string CVSStatus::GetStickyTag(const std::string& filename)
{
   if (IsOuterDirectory(filename))
   {
      std::string tagFile = EnsureTrailingDelimiter(filename) + "CVS/Tag";
      std::string tag;
      char cmd;
      Tag_Open(tag, tagFile.c_str(), &cmd);
      return tag;
   }  
   else
   {
      CSHelper cs;
      EntnodeData* data = GetEntnodeData(filename, cs);
      return GetStickyTag(data);
   }
}


std::string CVSStatus::GetStickyTag(EntnodeData* data)
{
   return data->GetTagOnly();
}


std::string CVSStatus::GetStickyDate(const std::string& filename)
{
   if (IsOuterDirectory(filename))
   {
      std::string tagFile = EnsureTrailingDelimiter(filename) + "CVS/Tag";
      std::string tag;
      char cmd;
      Tag_Open(tag, tagFile.c_str(), &cmd);
      return tag;
   }  
   else
   {
      CSHelper cs;
      EntnodeData* data = GetEntnodeData(filename, cs);
      return GetStickyDate(data);
   }
}


std::string CVSStatus::GetStickyDate(EntnodeData* data)
{
   return data->GetDateOnly();
}


std::string CVSStatus::GetOptions(const std::string& filename)
{
   CSHelper cs;
   EntnodeData* data = GetEntnodeData(filename, cs);
   return GetOptions(data);
}


std::string CVSStatus::GetOptions(EntnodeData* data)
{
   if (data == 0)
      return "";
   if (!data->GetOption())
      return "";
   return data->GetOption();
}

std::string CVSStatus::GetBugNumber(EntnodeData* data)
{
   if (data == 0)
      return "";
   if (!data->GetBug())
      return "";
   return data->GetBug();
}

void CVSStatus::ParseOptions(const std::string& options, 
                             FileFormat* fileFormat,
                             KeywordOptions* keywordOptions,
                             FileOptions* fileOptions)
{
   FileFormat fmt = fmtUnknown;
   KeywordOptions ko = 0;
   FileOptions fo = 0;
   int index = 0;
   char c;
   int len = static_cast<int>(options.length());

   // Default 
   if (!options.empty())
   {
      // if string starts with "-k", ignore this
      if (options.substr(0, 2) == "-k")
         index += 2;
   }

   // get format specifier
   if (index < len)
   {
      c = options[index];
      // look for file format specifier
      switch (c)
      {
         case 'u':
         {
            fmt = fmtUnicode;
            index++;
            break;
         }

         case 't':
         {
            fmt = fmtASCII;
            index++;
            break;
         }

         case 'b':
         {
            fmt = fmtBinary;
            index++;
            break;
         }

         case 'B':
         {
            fmt = fmtBinary;
            fo |= foBinaryDiff;
            index++;
            break;
         }

         case 'x':
         {
            fo |= foExclusive;
            break;
         }

         default:
         {
            fmt = fmtASCII;
            break;
         }
      }
   }
   else
   {
      fmt = fmtASCII;
   }

   // if there are no more options, default to "kv"
   if (index >= len && fmt != fmtBinary)
   {
      ko = keyNormal;
   }

   // parse other options
   while (index < len)
   {
      c = options[index];
      switch (c)
      {
         case 'k':
            ko |= keyNames;
            break;

         case 'v':
            ko |= keyValues;
            break;

         case 'l':
            ko |= keyLocker;
            break;

         case 'o':
            ko |= keyDisabled;
            break;

         case 'z':
            fo |= foCompressed;
            break;

         case 'L':
            fo |= foUnixLines;
            break;

         case 's':
            fo |= foStaticFile;
            break;
      }
      index++;
   }

   if (fileFormat)
      *fileFormat = fmt;
   if (keywordOptions)
      *keywordOptions = ko;
   if (fileOptions)
      *fileOptions = fo;
}

void CVSStatus::GetParsedOptions(const std::string& filename, 
                                 FileFormat* fileFormat,
                                 KeywordOptions* keywordOptions,
                                 FileOptions* fileOptions)
{
   ParseOptions(GetOptions(filename), fileFormat, keywordOptions, 
      fileOptions);
}

void CVSStatus::GetParsedOptions(EntnodeData* data, 
                                 FileFormat* fileFormat,
                                 KeywordOptions* keywordOptions,
                                 FileOptions* fileOptions)
{
   ParseOptions(GetOptions(data), fileFormat, keywordOptions, fileOptions);
}




CVSStatus::FileFormat CVSStatus::GetFileFormat(const std::string& filename)
{
   CSHelper cs;
   EntnodeData* data = GetEntnodeData(filename, cs);
   return GetFileFormat(data);
}


CVSStatus::FileFormat CVSStatus::GetFileFormat(EntnodeData* data)
{
   std::string sOptions = GetOptions(data);
   FileFormat fmt;

   if (sOptions.length() < 2)
   {
      fmt = fmtASCII;
   }
   else
   {
      ParseOptions(GetOptions(data), &fmt, 0, 0);
   }
   return fmt;
}


CVSStatus::KeywordOptions CVSStatus::GetKeywordOptions(const std::string& filename)
{
   CSHelper cs;
   EntnodeData* data = GetEntnodeData(filename, cs);
   return GetKeywordOptions(data);
}


CVSStatus::KeywordOptions CVSStatus::GetKeywordOptions(EntnodeData* data)
{
   std::string sOptions = GetOptions(data);
   KeywordOptions ko = 0;

   if (sOptions.length() < 2)
   {
      ko = keyNames | keyValues;
   }
   else
   {
      ParseOptions(GetOptions(data), 0, &ko, 0);
   }
   return ko;
}


CVSStatus::FileOptions CVSStatus::GetFileOptions(const std::string& filename)
{
   CSHelper cs;
   EntnodeData* data = GetEntnodeData(filename, cs);
   return GetFileOptions(data);
}


CVSStatus::FileOptions CVSStatus::GetFileOptions(EntnodeData* data)
{
   std::string sOptions = GetOptions(data);
   FileOptions so = 0;

   if (sOptions.length() < 2)
   {
      so = 0;
   }
   else
   {
      ParseOptions(GetOptions(data), 0, 0, &so);
   }
   return so;
}


wxString CVSStatus::KeywordOptionsString(KeywordOptions keywordOptions)
{
   wxString result;

   switch (keywordOptions)
   {
      case keyNames | keyValues:
      {
         result = _("Normal");
         break;
      }

      case keyNames:
      {
         result = _("Only keyword names");
         break;
      }

      case keyValues:
      {
         result = _("Only keyword values");
         break;
      }

      case keyNames | keyValues | keyLocker:
      {
         result = _("Include locker");
         break;
      }

      case keyDisabled:
      {
         result = _("Disabled");
         break;
      }

      case 0:
      {
         result = _("None");
         break;
      }

      default:
      {
         result = _("Custom");
         break;
      }
   }

   // Add flags
   if (keywordOptions != 0)
   {
      result += wxT(" (");
      if (keywordOptions & keyNames)
         result += wxT("k");
      if (keywordOptions & keyValues)
         result += wxT("v");
      if (keywordOptions & keyLocker)
         result += wxT("l");
      if (keywordOptions & keyDisabled)
         result += wxT("o");
      result += wxT(")");
   }

   return result;
}


wxString CVSStatus::FileFormatString(FileFormat fileFormat)
{
   wxString result;
   switch (fileFormat)
   {
      case fmtUnknown:
      {
         result = _("Unknown");
         break;
      }

      case fmtASCII:
      {
         result = _("Text/ASCII");
         break;
      }

      case fmtUnicode:
      {
         result = _("Text/Unicode");
         break;
      }

      case fmtBinary:
      {
         result = _("Binary");
         break;
      }
   }
   
   return result;
}



wxString CVSStatus::FileOptionString(FileOption fileOption)
{
   ASSERT(fileOption != 0);
   wxString result;
   switch (fileOption)
   {
      case foBinaryDiff:
         result = _("Binary diff");
         break;

      case foCompressed:
         result = _("Compressed");
         break;

      case foUnixLines:
         result = _("UNIX line endings");
         break;

      case foStaticFile:
         result = _("Static file");
         break;

      default:
         ASSERT(0);
   }

   return result;
}


wxString CVSStatus::FileOptionsString(FileOptions fileOptions)
{
   wxString result;

   FileOption options[] = { foBinaryDiff, foCompressed, foUnixLines, foStaticFile };
   unsigned int i;

   for (i = 0; i < sizeof(options) / sizeof(*options); i++)
   {
      if (fileOptions & options[i])
      {
         if (!result.empty())
            result += wxT(", ");
         result += FileOptionString(options[i]);
      }
   }

   return result;
}


wxString CVSStatus::FileStatusString(FileStatus fileStatus)
{
   TDEBUG_ENTER("FileStatusString");
   wxString result;

   switch (fileStatus)
   {
      case STATUS_NOWHERENEAR_CVS:
      {
         result = _("Not in CVS");
         break;
      }

      case STATUS_INCVS_RO:
      {
         result = _("Unmodified r/o");
         break;
      }

      case STATUS_INCVS_RW:
      {
         result = _("Unmodified");
         break;
      }

      case STATUS_LOCKED:
      {
         result = _("Edited");
         break;
      }

      case STATUS_LOCKED_RO:
      {
         result = _("Edited r/o");
         break;
      }

      case STATUS_CHANGED:
      {
         result = _("Modified");
         break;
      }

      case STATUS_STATIC:
      {
         result = _("Static");
         break;
      }

      case STATUS_ADDED:
      {
         result = _("Added");
         break;
      }

      case STATUS_CONFLICT:
      {
         result = _("Needs merge");
         break;
      }

      case STATUS_NOTINCVS:
      {
         result = _("Not in CVS");
         break;
      }

      case STATUS_OUTERDIRECTORY:
      {
         result = _("Module");
         break;
      }

      case STATUS_IGNORED:
      {
         result = _("Ignored");
         break;
      }

      case STATUS_REMOVED:
      {
         result = _("Removed");
         break;
      }

      case STATUS_RENAMED:
      {
         result = _("Renamed");
         break;
      }

      case STATUS_MISSING:
      {
         result = _("Missing");
         break;
      }
   }

   TDEBUG_TRACE("In: " << fileStatus << " Out: " << result);
   return result;
}


#ifndef NOTORTOISELIB

// Return true if this file's extension is one of the known 'binary' types.
static bool IsBinaryExtension(const std::string& filename, CVSStatus::FileOptions &fo)
{
   struct BinExt
   {
     const char *ext;
     int cmp;
   };
   static const BinExt apchExtensions[] =
   {
      {"lib", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"exe", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"dll", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"DDS", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"dds", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"tga" , CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"dag" , CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"dag_ser", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"dag_vc" , CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"dag_gf2", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"png", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"hdr", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"bmp", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"dlu", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"wav", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"WAV", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"ttf", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"max", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"pm", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"lm", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"l2d", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"rt", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"scn", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"sob", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"ps", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"tr", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"a2d", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"cam", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"vid", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"doc", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"gz", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"path", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"pdl", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"pdb", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"dtx", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"sfo", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"xdb", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"xex", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"elf", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"self", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"r16", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"r32", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"height", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"res", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"xls", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"jar", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"class", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"frt", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"exp", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"drv", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"fev", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"yup", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"psd", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"tm", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"rel", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"pcm", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"zz", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"cof", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"vad", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"aif", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"tf2", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"dphys", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"cached", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"fla", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"mll", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"rff", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"sff", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"pdf", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"tmd", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"m", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"pll", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"3dd", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"nd", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"sd", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"kd", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"gdp", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"mma", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"mms", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"mmt", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},

      {"dat", CVSStatus::foBinaryDiff},
      {"DAT", CVSStatus::foBinaryDiff},
      {"fsb", CVSStatus::foBinaryDiff},
      {"sdat", CVSStatus::foBinaryDiff},
      {"ddsx", CVSStatus::foBinaryDiff},
      {"DDSX", CVSStatus::foBinaryDiff},
      {"bin", CVSStatus::foBinaryDiff},
      {"grp", CVSStatus::foBinaryDiff},
      {"jpg", CVSStatus::foBinaryDiff},
      {"JPG", CVSStatus::foBinaryDiff},
      {"jpeg", CVSStatus::foBinaryDiff},
      {"gif", CVSStatus::foBinaryDiff},
      {"avi", CVSStatus::foBinaryDiff},
      {"AVI", CVSStatus::foBinaryDiff},
      {"ogg", CVSStatus::foBinaryDiff},
      {"OGG", CVSStatus::foBinaryDiff},
      {"ogv", CVSStatus::foBinaryDiff},
      {"ogm", CVSStatus::foBinaryDiff},
      {"OGM", CVSStatus::foBinaryDiff},
      {"pam", CVSStatus::foBinaryDiff},
      {"wmv", CVSStatus::foBinaryDiff},
      {"PAM", CVSStatus::foBinaryDiff},
      {"WMV", CVSStatus::foBinaryDiff},
      {"TIF", CVSStatus::foBinaryDiff},
      {"tif", CVSStatus::foBinaryDiff},
      {"zip", CVSStatus::foBinaryDiff},
      {"rar", CVSStatus::foBinaryDiff},
      {"chm", CVSStatus::foBinaryDiff},
      {"xlsx", CVSStatus::foBinaryDiff},
      {"docx", CVSStatus::foBinaryDiff},
      {"ods", CVSStatus::foBinaryDiff},
      {"odt", CVSStatus::foBinaryDiff},
      {"swf", CVSStatus::foBinaryDiff},
      {"unrt", CVSStatus::foBinaryDiff},
      {"nxrt", CVSStatus::foBinaryDiff},
      {"ptv", CVSStatus::foBinaryDiff},
      {"pk", CVSStatus::foBinaryDiff},
      {"ico", 0},

      {"a", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                      // UNIX archive (static library)
      {"ai", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                     // Adobe Illustrator
      {"au", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                     // UNIX sound file
      {"bin", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Binary data
      {"bmp", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Windows bitmap
      {"bpl", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Borland C++ Builder
      {"chi", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Microsoft Compressed HTML index
      {"class", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                  // Java byte code
      {"com", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // DOS executable
      {"dat", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Data
      {"db", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                     // Paradox or Berkeley database
      {"dbf", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"dcu", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Delphi unit
      {"dcp", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Delphi
      {"doc", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Microsoft Word document
      {"dot", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Microsoft Word template
      {"dsn", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"eml", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Microsoft Outlook Express email message
      {"gif", CVSStatus::foBinaryDiff},                    // GIF
      {"gz", CVSStatus::foBinaryDiff},                     // Gzipped file
      {"hlp", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Windows help file
      {"ico", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Icon
      {"ide", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Borland C project
      {"iwz", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Install Shield project
      {"jar", CVSStatus::foBinaryDiff},                    // Java archive
      {"jfif", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"jpe", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // JPEG graphics file
      {"jpeg", CVSStatus::foBinaryDiff},                   // JPEG graphics file
      {"jpg", CVSStatus::foBinaryDiff},                    // JPEG graphics file
      {"lnk", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Shell link
      {"mpp", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Microsoft Project file
      {"o", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                      // Object file
      {"obj", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Windows object file
      {"ool", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Object Outline configuration file
      {"pcx", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // PCX graphics file
      {"pdf", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Adobe Portable Document Format
      {"pif", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Windows PIF
      {"png", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Portable Network Graphics file
      {"ppt", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Microsoft PowerPoint presentation
      {"rar", CVSStatus::foBinaryDiff},                    // RAR archive
      {"rtf", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Rich Text Format
      {"snd", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Sound file
      {"so", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                     // UNIX dynamic library
      {"ssl", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},
      {"sys", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Windows device driver
      {"tar", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // UNIX tar archive
      {"tgz", CVSStatus::foBinaryDiff},                    // UNIX gzipped tar archive
      {"tif", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // TIFF graphics file
      {"tiff", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                   // TIFF graphics file
      {"vsd", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // (Microsoft) Visio drawing
      {"wav", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Windows sound file
      {"wmf", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Windows Meta File
      {"wpj", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Project file
      {"wri", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Write document
      {"wsp", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Workspace file
      {"xls", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                    // Microsoft Excel spreadsheet
      {"z", CVSStatus::foCompressed|CVSStatus::foBinaryDiff},                      // Unix 'compress'
      {"zip", CVSStatus::foBinaryDiff},                    // Zip/PKZIP file
      {0,0}
   };

   std::string::size_type lastdot = filename.find_last_of("."); 
   if (lastdot != std::string::npos)
   {
      // The file has an extension
      std::string szExt = filename.substr(lastdot+1);
      for (int i = 0; apchExtensions[i].ext; ++i)
      {
         if (!stricmp(apchExtensions[i].ext, szExt.c_str()))
         {
            fo = apchExtensions[i].cmp;
            return true;
         }
      }
   } else {
     if (stricmp(filename.c_str(), "EBOOT.BIN") == 0 || 
         stricmp(filename.c_str(), "PARAM.SFO") == 0)
     {
       fo = 0;
       return true;
     }

   }
   return false;
}

// get first token
static std::string CutFirstToken(std::string& string)
{
   int state = 0;
   unsigned int i = 0;
   std::string token;
   token.reserve(100);
   char c;

   while ((i < string.length()) && (state >= 0))
   {
      c = string[i];
      i++;
        
      switch(state)
      {
      case 0:  // Initial state
         if (c == '#')
         {
            state = -1;
            i = static_cast<unsigned int>(string.length());
         }
         else if (c <= 32) 
         {
            // do nothing
         }
         else if (c == '"')
         {
            state = 1;
         }
         else if (c == '\\')
         {
            state = 4;
         }
         else
         {
            state = 3;
            token = token + c;
         }
         break;

      case 1: // reading quoted token
         if (c == '\\')
         {
            state = 2;
         }
         else if (c == '"')
         {
            state = -1;
         }
         else
         {
            token = token + c;
         }
         break;

      case 2: // literal mode after backslash
         token = token + c;
         state = 1;
         break;

      case 3: // unquoted token
         if (c <= 32)
         {
            state = -1;
         }
         else if (c == '#')
         {
            state = -1;
            i = static_cast<unsigned int>(string.length());
         }
         else if (c == '"')
         {
            i--;
            state = -1;
         }
         else if (c == '\\')
         {
            state = 5;
         }
         else
         {
            token = token + c;
         }
         break;

      case 4: // literal mode after backslash
         token = token + c;
         state = 0;
         break;

      case 5: // literal mode after backslash
         token = token + c;
         state = 5;
         break;
      }
   }

   string = string.substr(i, string.length());

   return token;
}


// static variables
bool                            CVSStatus::ourUserFileTypesRead = false;
CVSStatus::UserFileTypesVector  CVSStatus::ourUserFileTypes;
CriticalSection                 CVSStatus::ourUserFileTypesCS;



// Read user filetypes
void CVSStatus::ReadUserFileTypes()
{
   TDEBUG_ENTER("CVSStatus::ReadUserFileTypes");
   CSHelper csHelper(ourUserFileTypesCS, true);
   if (!ourUserFileTypesRead)
   {
      ourUserFileTypesRead = true;

      // Open config file
      std::string homeDir;
      if (!GetHomeDirectory(homeDir))
         return;

      std::string configFile = EnsureTrailingDelimiter(homeDir) 
         + "TortoiseCVS.Filetypes";
      std::ifstream types(configFile.c_str());
      if (!types)
      {
         TDEBUG_TRACE("Could not open " << configFile);
         return;
      }

      std::string line, type;
      UserFileType uft;
      const size_t BUFSIZE = 500;
      char buf[BUFSIZE];
      bool bValid;

      // Read lines
      types.getline(buf, sizeof(buf));
      line = buf;
      while (true)
      {
         TDEBUG_TRACE("Read line " << line);
         uft.wildcard = CutFirstToken(line);
         if (!uft.wildcard.empty())
         {
            type = CutFirstToken(line);
            
            if (!type.empty())
            {
               bValid = false;
               MakeLowerCase(type);
               uft.fileformat = fmtUnknown;

               // check for the file type
               if (type == "binary")
               {
                  TDEBUG_TRACE(uft.wildcard << " is binary");
                  uft.fileformat = fmtBinary;
                  bValid = true;
               }
               else if (type == "text")
               {
                  TDEBUG_TRACE(uft.wildcard << " is text");
                  uft.fileformat = fmtASCII;
                  bValid = true;
               }
               else if (type == "unicode")
               {
                  TDEBUG_TRACE(uft.wildcard << " is unicode");
                  uft.fileformat = fmtUnicode;
                  bValid = true;
               }
               else if (type == "auto")
               {
                  TDEBUG_TRACE(uft.wildcard << " is auto-detect");
                  uft.fileformat = fmtUnknown;
                  bValid = true;
               }
               else if (type == "dll")
               {
                  uft.fileformat = fmtUnknown;
                  uft.dllname = CutFirstToken(line);
                  uft.functionname = CutFirstToken(line);
                  TDEBUG_TRACE(uft.wildcard << " is detected by " 
                     << uft.functionname << " in " << uft.dllname);
                  bValid = true;
               }

               if (bValid)
               {
                  MakeLowerCase(uft.wildcard);
                  ourUserFileTypes.push_back(uft);
                }
            }
         }

         if (types.eof())
         {
            return;
         }
         types.getline(buf, sizeof(buf));
         line = buf;
      }
   }
}


typedef int (__stdcall *UserTypeProc) (const char*);

// Return true if the type for this file's extension has been defined by the user
bool CVSStatus::IsUserDefinedType(const std::string& filename, CVSStatus::FileFormat& myFormat)
{
   TDEBUG_ENTER("CVSStatus::IsUserDefinedType");
   TDEBUG_TRACE("Check " << filename);

   // Rread the config file
   ReadUserFileTypes();

   std::string lfilename = filename;

   // On Windows filenames are case insensitive
   MakeLowerCase(lfilename);

   CSHelper syncHelper(ourUserFileTypesCS, true);
   // find userdefined file type
   UserFileTypesVector::const_iterator p = ourUserFileTypes.begin();
   while (p != ourUserFileTypes.end())
   {
      // see if we match the filename
      if (wc_match(p->wildcard.c_str(), lfilename.c_str()) == 1)
      {
         TDEBUG_TRACE("Match");
         if (p->fileformat != fmtUnknown)
         {
            TDEBUG_TRACE("Format " << FileFormatString(p->fileformat));
            myFormat = p->fileformat;
         }
         else
         {
            if (!(p->dllname.empty()))
            {
              TDEBUG_TRACE("Call " << p->functionname << " in " << p->dllname);
              HMODULE hDll = LoadLibraryA(p->dllname.c_str());
              if (hDll == 0)
              {
                 TDEBUG_TRACE("Dll not found");
                 return false;
              }
              UserTypeProc proc;
              proc = (UserTypeProc) GetProcAddress(hDll, p->functionname.c_str());
              if (proc == 0)
              {
                 TDEBUG_TRACE("Function not found");
                 FreeLibrary(hDll);
                 return false;
              }
              int i = (proc)(filename.c_str());
              TDEBUG_TRACE("Function returned " << i);
              switch(i)
              {
              case 0:
                 myFormat = fmtUnknown;
                 break;
                 
              case 1:
                 myFormat = fmtASCII;
                 break;

              case 2:
                 myFormat = fmtUnicode;
                 break;

              case 3:
                 myFormat = fmtBinary;
                 break;

              default:
                 myFormat = fmtUnknown;
                 break;
              }
              FreeLibrary(hDll);
            }
            else
            {
              TDEBUG_TRACE("Format " << FileFormatString(p->fileformat));
              myFormat = p->fileformat;
            }
         }
         return true;
      }
      ++p;
   }
   return false;
}



// Return true if this file's extension is one of the known 'text' types.
static bool IsTextExtension(const std::string& filename)
{
   static const char * apchExtensions[] =
   {
      "c",                      // C source
      "cpp",                    // C++ source
      "cvsignore",              // .cvsignore file
      "h",                      // C/C++ header
      "hpp",                    // C++ header
      "txt",                    // Text file
      "html",                   // HTML
      "shtml",                  // HTML (server-side include)
      "phtml",                  // PHP
      "php",                    // PHP
      "php3",                   // PHP3
      "php4",                   // PHP4
      "sql",                    // SQL
      0
   };
   
   std::string::size_type lastdot = filename.find_last_of("."); 
   bool fRes = false;
   if (lastdot != std::string::npos)
   {
      // The file has an extension
      std::string szExt = filename.substr(lastdot+1);
      for (int i = 0; apchExtensions[i]; ++i)
      {
         if (!stricmp(apchExtensions[i], szExt.c_str()))
         {
            fRes = true;
            break;
         }
      }
   }
   return fRes;
}

// The following code is swiped from the file ascmagic.c, which is part of the
// file(1) command implementation available from ftp.astron.com.
// License:
/*
* This software is not subject to any license of the American Telephone
* and Telegraph Company or of the Regents of the University of California.
*
* Permission is granted to anyone to use this software for any purpose on
* any computer system, and to alter it and redistribute it freely, subject
* to the following restrictions:
*
* 1. The author is not responsible for the consequences of use of this
*    software, no matter how awful, even if they arise from flaws in it.
*
* 2. The origin of this software must not be misrepresented, either by
*    explicit claim or by omission.  Since few users ever read sources,
*    credits must appear in the documentation.
*
* 3. Altered versions must be plainly marked as such, and must not be
*    misrepresented as being the original software.  Since few users
*    ever read sources, credits must appear in the documentation.
*
* 4. This notice may not be removed or altered.
*/


#define F 0   /* character never appears in text */
#define T 1   /* character appears in plain ASCII text */
#define I 2   /* character appears in ISO-8859 text */
#define X 3   /* character appears in non-ISO extended ASCII (Mac, IBM PC) */

static char text_chars[256] =
{
    /*                  BEL BS HT LF    FF CR    */
    F, F, F, F, F, F, F, T, T, T, T, F, T, T, F, F,  /* 0x0X */
      /*                              ESC          */
      F, F, F, F, F, F, F, F, F, F, F, T, F, F, F, F,  /* 0x1X */
      T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x2X */
      T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x3X */
      T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x4X */
      T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x5X */
      T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x6X */
      T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, F,  /* 0x7X */
      /*            NEL                            */
      X, X, X, X, X, T, X, X, X, X, X, X, X, X, X, X,  /* 0x8X */
      X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,  /* 0x9X */
      I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xaX */
      I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xbX */
      I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xcX */
      I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xdX */
      I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xeX */
      I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I   /* 0xfX */
};

static int looks_ascii(const unsigned char *buf, int nbytes)
{
   for (int i = 0; i < nbytes; i++)
   {
      int t = text_chars[buf[i]];
      
      if (t != T)
         return 0;
    }
   
    return 1;
}

static int looks_latin1(const unsigned char *buf, int nbytes)
{
   for (int i = 0; i < nbytes; i++)
   {
      int t = text_chars[buf[i]];
      
      if (t != T && t != I)
         return 0;
   }
   
   return 1;
}

static int looks_extended(const unsigned char *buf, int nbytes)
{
   for (int i = 0; i < nbytes; i++)
   {
      int t = text_chars[buf[i]];
      
      if (t != T && t != I && t != X)
         return 0;
   }
   
   return 1;
}

static int looks_utf8(const unsigned char *buf, int nbytes)
{
    int i, n;
    uint16_t c;
    int gotone = 0;
   
    for (i = 0; i < nbytes; i++)
    {
        if ((buf[i] & 0x80) == 0)
        {
         /* 0xxxxxxx is plain ASCII */
         /*
            * Even if the whole file is valid UTF-8 sequences,
            * still reject it if it uses weird control characters.
            */
         
         if (text_chars[buf[i]] != T)
            return 0;
        }
        else if ((buf[i] & 0x40) == 0)
        {
         /* 10xxxxxx never 1st byte */
         return 0;
        }
        else
        {
         /* 11xxxxxx begins UTF-8 */
         int following;
         
         if ((buf[i] & 0x20) == 0) {      /* 110xxxxx */
            c = buf[i] & 0x1f;
            following = 1;
         } else if ((buf[i] & 0x10) == 0) {   /* 1110xxxx */
            c = buf[i] & 0x0f;
            following = 2;
         } else if ((buf[i] & 0x08) == 0) {   /* 11110xxx */
            c = buf[i] & 0x07;
            following = 3;
         } else if ((buf[i] & 0x04) == 0) {   /* 111110xx */
            c = buf[i] & 0x03;
            following = 4;
         } else if ((buf[i] & 0x02) == 0) {   /* 1111110x */
            c = buf[i] & 0x01;
            following = 5;
         } else
            return 0;
         
         for (n = 0; n < following; n++)
         {
            i++;
            if (i >= nbytes)
               goto done;
            
            if ((buf[i] & 0x80) == 0 || (buf[i] & 0x40))
               return 0;
            
            c = (c << 6) + (buf[i] & 0x3f);
         }
         gotone = 1;
        }
    }
done:
    return gotone;   /* don't claim it's UTF-8 if it's all 7-bit */
}

static int looks_unicode(const unsigned char *buf, int nbytes)
{
   int bigend;
   int i;
   
   if (nbytes < 2)
      return 0;
   
   if (buf[0] == 0xff && buf[1] == 0xfe)
      bigend = 0;
   else if (buf[0] == 0xfe && buf[1] == 0xff)
      bigend = 1;
   else
      return 0;
   
   for (i = 2; i + 1 < nbytes; i += 2)
   {
      /* XXX fix to properly handle chars > 65536 */
      
      uint16_t u;
      if (bigend)
         u = buf[i + 1] + 256 * buf[i];
      else
         u = buf[i] + 256 * buf[i + 1];
      
        if (u == 0xfffe)
            return 0;
        if (u < 128 && text_chars[u] != T)
            return 0;
    }
   
    return 1;
}

#undef F
#undef T
#undef I
#undef X


static bool IsTextExtension(const std::string& filename);
static bool IsBinaryExtension(const std::string& filename, CVSStatus::FileOptions &fo);

static int looks_ascii(const unsigned char *buf, int nbytes);
static int looks_latin1(const unsigned char *buf, int nbytes);
static int looks_extended(const unsigned char *buf, int nbytes);
static int looks_unicode(const unsigned char *buf, int nbytes);
static int looks_utf8(const unsigned char *buf, int nbytes);

#define MAX_TEST_SIZE 4000

bool CVSStatus::GuessFileFormat(const std::string& filename, CVSStatus::FileFormat &fmt, CVSStatus::FileOptions &fo)
{
   fmt = fmtUnknown;
   fo = 0;
   if (IsDirectory(filename.c_str()))
      return false;

   FileFormat &result = fmt;

   if (IsUserDefinedType(filename, result))
   {
      if (result != fmtUnknown)
         return true;
   }
   
   // First check for binary extension.
   FileOptions binfo;
   if (IsBinaryExtension(filename, binfo))
   {
      result = fmtBinary;
      fo = binfo;
      return true;
    }
   
   // Check if this ought to be text, regardless of contents.
   bool is_text = IsTextExtension(filename);
   
   // Read a chunk of the file
   std::auto_ptr<unsigned char> buf(new unsigned char[MAX_TEST_SIZE]);
   unsigned char* p = buf.get();
    
   FILE *samp = fopen(filename.c_str(), "rb");
   if (!samp)
   {
     result = fmtUnknown;
     return false;
   }
   
   int BufSize = static_cast<int>(fread(p, sizeof(char), MAX_TEST_SIZE, samp));
   fclose(samp);
   if (BufSize <= 0)
   {
      // File has zero length. If the extension indicates text, assume ASCII.
      result = is_text ? fmtASCII : fmtUnknown;
      return true;
   }
   
   if (looks_ascii(p, BufSize))
      result = fmtASCII;
   else if (looks_utf8(p, BufSize))
      result = fmtASCII;
   else if (looks_unicode(p, BufSize))
      result = fmtUnicode;
   else if (looks_latin1(p, BufSize) ||
            looks_extended(p, BufSize))
      result = fmtASCII;
   else if (is_text)
      result = fmtASCII;  // Hmm...
   else
   {
     result = fmtBinary;
     fo = foBinaryDiff;
   }
    
   return true;
}



std::string GetServerVersionCacheKey(const std::string& cvsroot)
{
   CVSRoot root;
   root.SetCVSROOT(cvsroot);
   std::string key = root.GetServer() + ":" + root.GetPort();
   return key;
}


int GetTimeStamp()
{
   SYSTEMTIME st;
   FILETIME ft;
   __int64 *pi = reinterpret_cast<__int64*>(&ft);
   GetSystemTime(&st);
   SystemTimeToFileTime(&st, &ft);
   *pi = *pi / 10000000 / 3600; // convert 100ns to hours
   return static_cast<int>(*pi);
}

#ifndef TORTOISESHELL

CVSStatus::CVSVersionInfo CVSStatus::GetServerCVSVersion(const std::string& cvsroot,
                                                         CVSAction* defglue)
{
   TDEBUG_ENTER("CVSStatus::GetServerCVSVersion");

   TortoiseRegistryCache trc("Cache\\ServerVersions", CACHE_SIZE_SERVER_VERSION);
   bool bExists = false;
   std::string sCacheKey = GetServerVersionCacheKey(cvsroot);
   std::string sData = trc.ReadString(sCacheKey, "Data", "", &bExists);

   CVSVersionInfo cvsVersionInfo;
   int iNow = GetTimeStamp();
   if (bExists)
   {
      // verify timestamp
      std::string::size_type p = sData.find(":");
      if (p != std::string::npos)
      {
         std::string sTimeStamp = sData.substr(0, p);
         int iTimeStamp = atoi(sTimeStamp.c_str());

         if (iNow - iTimeStamp < CACHE_EXPIRATION_SERVER_VERSION)
         {
            cvsVersionInfo.ParseVersionString(sData.substr(p + 1));
         }
      }
   }

   if (!cvsVersionInfo.IsValid())
   {
      cvsVersionInfo = GetServerCVSVersionUncached(cvsroot, defglue);
      if (cvsVersionInfo.IsValid())
      {
         char buf[200];
         sprintf(buf, "%d", iNow);
         sData = buf;
         sData += ":" + cvsVersionInfo.AsString();
         trc.WriteString(sCacheKey, "Data", sData);
      }
      else
          cvsVersionInfo.SetUnknown();
   }

   return cvsVersionInfo;
}


std::string CVSStatus::GetServerCVSVersionString(const std::string& cvsroot,
                                                 CVSAction* defglue)
{
   TDEBUG_ENTER("CVSStatus::GetServerCVSVersionString");

   CVSRoot root;
   root.SetCVSROOT(cvsroot);

   CVSAction* glue = defglue;
   CVSAction::close_t oldCloseSetting = CVSAction::CloseIfNoErrors;
   bool oldHideStdout = false;
   int oldErrors = 0;
   int oldUses = 0;
   CVSRoot oldCVSRoot;
   if (!defglue)
   {
      glue = new CVSAction(0);
   }
   else
   {
      oldCloseSetting = glue->GetClose();
      oldHideStdout = glue->GetHideStdout();
      oldCVSRoot = glue->GetCVSRoot();
      oldErrors = glue->myErrors;
      oldUses = glue->myUses;
   }

   glue->SetCloseIfOK(true);
   glue->SetHideStdout();
   if (root.Valid())
       glue->SetCVSRoot(root);
   else
       root = oldCVSRoot;
   
   MakeArgs args;
   args.add_option("version");
   glue->Command(GetTemporaryPath(), args); // directory doesn't matter
   
   std::string out = glue->GetStdOutText(true);
   
   if (!defglue)
   {
      delete(glue);
   }
   else
   {
      glue->SetClose(oldCloseSetting);
      glue->SetHideStdout(oldHideStdout);
      glue->SetCVSRoot(oldCVSRoot);
      glue->myErrors = oldErrors;
      glue->myUses = oldUses;
   }

   TDEBUG_TRACE("Protocol: " << root.GetProtocol());
   if (root.GetProtocol() != "local")
   {
      // Ouput contains:
      // Client: xxx
      // Server: xxx
      TDEBUG_TRACE("Output from 'cvs version': " << out);
      std::string::size_type pos = out.find("Server:");
      if (pos != std::string::npos)
      {
         out = out.substr(pos + 8);
      }
   }
   // Remove "(client/server)" part to make the About dialog less confusing
   std::string::size_type endpos = out.find(" (client");
   if (endpos == std::string::npos)
      endpos = out.find_first_of("\r\n");
   out = out.substr(0, endpos);
   return out;
}

CVSStatus::CVSVersionInfo CVSStatus::GetServerCVSVersionUncached(const std::string& cvsroot,
                                                                 CVSAction* defglue)
{
   TDEBUG_ENTER("CVSStatus::GetServerCVSVersionUncached");

   CVSVersionInfo cvsVersionInfo;
   cvsVersionInfo.SetUnknown();

   std::string out = GetServerCVSVersionString(cvsroot, defglue);
   cvsVersionInfo.ParseVersionString(out);
   if (!cvsVersionInfo.IsValid())
   {
      TDEBUG_TRACE("GetServerCVSVersionUncached failed to retrieve server version");
   }
   return cvsVersionInfo;
}

CVSStatus::CVSVersionInfo CVSStatus::GetClientCVSVersion(const std::string& cvsroot,
                                                         CVSAction* defglue)
{
   TDEBUG_ENTER("CVSStatus::GetClientCVSVersion");

   std::string out = GetClientCVSVersionString();
   CVSVersionInfo cvsVersionInfo;
   cvsVersionInfo.ParseVersionString(out);
   if (!cvsVersionInfo.IsValid())
   {
      TDEBUG_TRACE("GetClientCVSVersion failed to retrieve client version");
   }
   return cvsVersionInfo;
}

std::string CVSStatus::GetClientCVSVersionString(bool silentOnError)
{
   TDEBUG_ENTER("CVSStatus::GetClientCVSVersion");
   CVSAction glue(0, CVSAction::PIPE_STDOUT | CVSAction::HIDE_PROGRESS |
                  (silentOnError ? CVSAction::SILENT_ON_ERROR : 0));
   glue.SetCloseIfOK(true);
   
   MakeArgs args;
   args.add_option("--version");
   if (!glue.Command(GetTemporaryPath(), args)) // directory doesn't matter
      return "[unknown]";

   std::string out = glue.GetStdOutText();
   
   // Ouput can be:
   // Concurrent Versions System (CVS) x.y.z blah blah [(client/server)]
   // Concurrent Versions System (CVSNT) x.y.z blah blah [(client/server)]
   // Server: xxx
   TDEBUG_TRACE("Output from 'cvs --version': " << out);
   std::string::size_type pos = out.find("Concurrent");
   if (pos != std::string::npos)
   {
      // Find end of line
      std::string::size_type endpos = out.find(" (client", pos);
      if (endpos == std::string::npos)
         endpos = out.find_first_of("\r\n", pos);
      return Trim(out.substr(pos, endpos - pos));
   }
   TDEBUG_TRACE("GetClientCVSVersion failed to retrieve CVS version");
   return "?";
}


std::string CVSStatus::BuildAddFlags(const std::string& dir,
                                     FileFormat fileFormat, 
                                     KeywordOptions keywordOptions,
                                     FileOptions fileOptions,
                                     const CVSServerFeatures& serverFeatures)
{
   std::string result;

   // is it k, ku, kb or kB
   switch (fileFormat)
   {
      case fmtUnicode:
      {
         result += "u";
         break;
      }

      case fmtBinary:
      {
         // Exclusive edit/commit
         if (fileOptions & foBinaryDiff)
            result += "B";
         else
            result += "b";
         break;
      }

      case fmtASCII:
         break;

      default:
         ASSERT(0);
   }

   if (serverFeatures.SupportsExclusiveEdit())
   {
       if (GetIntegerPreference("Edit policy", dir) >= TortoisePreferences::EditPolicyExclusive)
           result += "x";
       else if (GetIntegerPreference("Edit policy", dir) == TortoisePreferences::EditPolicyConcurrentExclusive)
           result += "c";
   }

   if (fileFormat != fmtBinary)
   {
      if (keywordOptions != keyNormal || fileOptions != 0)
      {
         if (keywordOptions & keyNames)
            result += "k";
         if (keywordOptions & keyValues)
            result += "v";
         if (keywordOptions & keyLocker)
            result += "l";
         if (keywordOptions & keyDisabled)
            result += "o";
         if (fileOptions & foUnixLines)
            result += "L";
         if (fileOptions & foStaticFile)
            result += "s";
      }
      else
          result += "kv";
   }
   else
   {
      // Binary doesn't allow keyword options
      ASSERT(keywordOptions == 0);
      // Binary doesn't allow Unix lines
      ASSERT((fileOptions & foUnixLines) == 0);
   }

   if (fileOptions & foCompressed)
   {
      result += "z";
   }

   return result;
}

#endif

void CVSStatus::InvalidateCache()
{
   CSHelper cs(ourEntnodeDirCS, true);
   ourEntnodeDirCache.Clear();
}



// Refresh TCVS status for this directory recursively
void CVSStatus::GetDirectoryStatus(const std::string& dirname, 
                                   FileStatus *dirStatus,
                                   FileStatus *dirStatusRec,
                                   int levels, bool persist)
{
   TDEBUG_ENTER("CVSStatus::GetDirectoryStatus");
   // Iterate through all directories
   WIN32_FIND_DATAA fd;
   std::string myDir;
   std::string myEntry;
   FileStatus myDirStatus = STATUS_INCVS_RW;
   FileStatus myDirStatusRec = STATUS_INCVS_RW;
   FileStatus entryStatus;
   FileStatus entryStatusRec;
   EntnodeDirData* dirData = 0;
   EntnodeData* nodeData = 0;
   CSHelper csDirData;
   CSHelper csNode;
   EntnodeMap::const_iterator ite;
   std::vector<std::string> dirs, files;
   std::vector<FileStatus> vDirStatus;
   std::vector<FileStatus>::const_iterator its;
   std::vector<std::string>::const_iterator it;
   HANDLE hFind = INVALID_HANDLE_VALUE;

   TDEBUG_TRACE("Scan directory " << dirname);
   myDir = EnsureTrailingDelimiter(dirname) + "*.*";
   hFind = FindFirstFileA(myDir.c_str(), &fd);
   
   if (hFind == INVALID_HANDLE_VALUE)
      goto Cleanup;

   do
   {
      // ignore directories ".", ".." and "CVS"
      if ((stricmp(fd.cFileName, ".") == 0) || (stricmp(fd.cFileName, "..") == 0) ||
          (stricmp(fd.cFileName, "CVS") == 0))
      {
         continue;
      }

      myEntry = EnsureTrailingDelimiter(dirname) + fd.cFileName;

      if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      {
         dirs.push_back(myEntry);
      }
      else
      {
         files.push_back(myEntry);
      }
   } while (FindNextFileA(hFind, &fd));
   FindClose(hFind);
   hFind = INVALID_HANDLE_VALUE;

   // Get dir data
   dirData = GetEntnodeDirData(dirname, csDirData);
   if (dirData == 0)
   {
      goto Cleanup;
   }

   csNode.Attach(dirData->entnodeCacheCS);
   csNode.Enter();

   // Process files
   TDEBUG_TRACE("Process files");
   for (it = files.begin(); it < files.end(); it++)
   {
      TDEBUG_TRACE("File: " << *it);
      nodeData = GetEntnodeData(*it, dirData, csNode);
      entryStatus = GetFileStatus(nodeData);
      // Combine with current directory status
      myDirStatus = GetCombinedOverlayStatus(myDirStatus, entryStatus);
   }

   // Process Entries file
   TDEBUG_TRACE("Process entries");
   for (ite = dirData->entries.begin();
        ite != dirData->entries.end(); ite++)
   {
      std::string filename = EnsureTrailingDelimiter(dirname) + ite->first;
      TDEBUG_TRACE("Entry: " << filename);
      EntnodeData* nodeData = GetEntnodeData(filename, dirData, csNode);
      myDirStatus = GetCombinedOverlayStatus(myDirStatus, 
                                             GetFileStatus(nodeData));
   }

   myDirStatusRec = myDirStatus;
   
   // Process directories
   TDEBUG_TRACE("Process directories");
   for (it = dirs.begin(); it < dirs.end(); it++)
   {
      TDEBUG_TRACE("Directory: " << *it);
      nodeData = GetEntnodeData(*it, dirData, csNode);
      entryStatus = GetFileStatus(nodeData);
      vDirStatus.push_back(entryStatus);
   }
   csNode.Leave();
   csDirData.Leave();

   for (it = dirs.begin(), its = vDirStatus.begin(); 
        it < dirs.end() && its < vDirStatus.end(); ++it, ++its)
   {
      entryStatus = *its;
      entryStatusRec = *its;

      if (*its == STATUS_INCVS_RW || *its == STATUS_INCVS_RO || 
          *its == STATUS_OUTERDIRECTORY)
      {
         if (levels != 0)
         {
            GetDirectoryStatus(*it, 0, &entryStatusRec, levels > 0 ? levels - 1 : -1, persist);
         }
      }
      myDirStatus = GetCombinedOverlayStatus(myDirStatus, entryStatus);
      myDirStatusRec = GetCombinedOverlayStatus(myDirStatusRec, entryStatusRec);
   }

   if (persist)
   {
      FileStatus oldDirStatus;
      FileStatus oldDirStatusRec;
      TortoiseDirectoryStatus dirStatus(dirname);

      // Save directory status in status file
      dirStatus.Load();
      oldDirStatus = dirStatus.GetDirStatus();
      oldDirStatusRec = dirStatus.GetDirStatusRec();
      TDEBUG_TRACE("Old status: " << oldDirStatus << ", Rec: " << oldDirStatusRec);
      TDEBUG_TRACE("New status: " << myDirStatus << ", Rec: " << myDirStatusRec);

      dirStatus.SetDirStatus(myDirStatus);
      dirStatus.SetDirStatusRec(myDirStatusRec);
      dirStatus.Save();

      // Notify the shell
//      if (oldDirStatusRec != myDirStatusRec)
      ShellNotifyUpdateFile(dirname);
   }

Cleanup:
   if (hFind != INVALID_HANDLE_VALUE)
      FindClose(hFind);
   if (dirStatus)
      *dirStatus = myDirStatus;
   if (dirStatusRec)
      *dirStatusRec = myDirStatusRec;
}


CVSStatus::FileStatus CVSStatus::GetCombinedOverlayStatus(FileStatus s1, FileStatus s2)
{
   TDEBUG_ENTER("CVSStatus::GetCombinedOverlayStatus");
   TDEBUG_TRACE("Status: " << s1 << ", " << s2);
   int p1, p2;
   FileStatus res;

   p1 = GetFileStatusPriority(s1);
   p2 = GetFileStatusPriority(s2);

   if (p1 > p2)
   {
      res = s1;
   }
   else
   {
      res = s2;
   }

   TDEBUG_TRACE("New status: " << res);
   return res;
}


int CVSStatus::GetFileStatusPriority(FileStatus status)
{
   TDEBUG_ENTER("CVSStatus::GetFileStatusPriority");
   int res = 0;
   switch (status)
   {
      case STATUS_NOWHERENEAR_CVS:
         res = 0;
         break;

      case STATUS_OUTERDIRECTORY:
         res = 10;
         break;
   
      case STATUS_IGNORED:
         res = 20;
         break;

      case STATUS_INCVS_RO:
         res = 30;
         break;

      case STATUS_INCVS_RW:
         res = 40;
         break;

      case STATUS_REMOVED:
         res = 50;
         break;

      case STATUS_CHANGED:
      case STATUS_STATIC:
         res = 60;
         break;

      case STATUS_MISSING:
         res = 70;
         break;

      // Disable this as most users don't seem to like it
      case STATUS_NOTINCVS:
         res = 0;
         break;

      case STATUS_ADDED:
         res = 80;
         break;

      case STATUS_CONFLICT:
         res = 90;
         break;

      default:
         res = 0;
         break;
   }
   TDEBUG_TRACE("Priority: " << res);
   return res;
}


// Scan for files
void CVSStatus::RecursiveScan(const std::string& directory, 
                              std::vector<std::string>* modified, 
                              std::vector<std::string>* added, 
                              std::vector<std::string>* removed, 
                              std::vector<std::string>* renamed,
                              std::vector<std::string>* unmodified,
                              std::vector<std::string>* conflict,
                              bool includeWritable)
{
    std::vector<ScanData> scandata;
    std::vector<ScanData>::iterator it;

    // Get all entries in Entries file
    if (IsDirectory(directory.c_str()))
        ScanEntries(directory, scandata, true);
    else
        scandata.push_back(ScanData(directory, GetFileStatus(directory)));

    for (it = scandata.begin(); it < scandata.end(); it++)
    {
        // Modified files
        if (modified)
            if (it->status == STATUS_CHANGED) 
                modified->push_back(it->filename);
      
        // Added files
        if (added)
            if (it->status == STATUS_ADDED)
                added->push_back(it->filename);

        // Removed files
        if (removed)
            if (it->status == STATUS_REMOVED)
                removed->push_back(it->filename);

        // Renamed files
        if (renamed)
            if (it->status == STATUS_RENAMED)
                renamed->push_back(it->filename);
      
        // Unmodified files
        if (unmodified)
            if ((it->status == STATUS_INCVS_RO) || 
                ((it->status == STATUS_INCVS_RW) && includeWritable))
                unmodified->push_back(it->filename);

        if (conflict)
            if (it->status == STATUS_CONFLICT) 
                conflict->push_back(it->filename);
    }
}

void CVSStatus::RecursiveScan(const std::string& directory, 
                              std::vector< std::pair<std::string, std::string> >* modified, 
                              std::vector< std::pair<std::string, std::string> >* modifiedStatic, 
                              std::vector< std::pair<std::string, std::string> >* added,
                              std::vector< std::pair<std::string, std::string> >* removed,
                              std::vector< std::pair<std::string, std::string> >* renamed,
                              std::vector< std::pair<std::string, std::string> >* unmodified, 
                              std::vector< std::pair<std::string, std::string> >* conflict,
                              bool includeWritable)
{
    std::vector<ScanData> scandata;
    std::vector<ScanData>::iterator it;

    // Get all entries in Entries file
    if (IsDirectory(directory.c_str()))
        ScanEntries(directory, scandata, true);
    else
        scandata.push_back(ScanData(directory, GetFileStatus(directory), GetBugNumber(directory)));

    for (it = scandata.begin(); it < scandata.end(); it++)
    {
        // Modified files
        if (modified)
            if (it->status == STATUS_CHANGED)
                modified->push_back(make_pair(it->filename, it->bugnumber));

        // Modified static files
        if (modifiedStatic)
            if ((directory == it->filename) && (GetFileOptions(directory) & foStaticFile))
                modifiedStatic->push_back(make_pair(it->filename, it->bugnumber));

        // Added files
        if (added)
            if (it->status == STATUS_ADDED)
                added->push_back(make_pair(it->filename, it->bugnumber));

        // Removed files
        if (removed)
            if (it->status == STATUS_REMOVED)
                removed->push_back(make_pair(it->filename, it->bugnumber));

        // Renamed files
        if (renamed)
            if (it->status == STATUS_RENAMED)
                renamed->push_back(make_pair(it->filename, it->bugnumber));
      
        // Unmodified files
        if (unmodified)
            if ((it->status == STATUS_INCVS_RO) || 
                ((it->status == STATUS_INCVS_RW) && includeWritable))
                unmodified->push_back(make_pair(it->filename, it->bugnumber));

        if (conflict)
            if (it->status == STATUS_CONFLICT) 
                conflict->push_back(make_pair(it->filename, it->bugnumber));
    }
}


bool CVSStatus::IsDisabledPath(const std::string& path, bool isDirectory)
{
   TDEBUG_ENTER("CVSStatus::IsDisabledPath");
   bool res = false;
   if (!GetBooleanPreference("Allow Network Drives")
       && PathIsNetworkPathA(path.c_str()))
   {
      TDEBUG_TRACE("Network drive path not allowed");
      res = true;
   }
   else if (!GetBooleanPreference("Allow Removable Drives")
            && IsRemovableDrivePath(path.c_str()))
   {
      TDEBUG_TRACE("Removable drive path not allowed");
      res = true;
   }
   else if (!isDirectory && IsSystemFile(path.c_str()))
   {
      TDEBUG_TRACE("System files not allowed");
      res = true;
   }
   else
   {
       // Check positive/negative list
       std::vector<std::string> negativelist = GetStringListPreference("Excluded Directories");
       bool match = false;
       if (!negativelist.empty())
           match = isDirectory ? DirectoryMatchesDirList(path, negativelist) : FilenameMatchesDirList(path, negativelist);
       if (match)
       {
           TDEBUG_TRACE("neg match");
           res = true;
       }
       else
       {
           std::vector<std::string> positivelist = GetStringListPreference("Included Directories");
           if (!positivelist.empty())
           {
               match = isDirectory ? DirectoryMatchesDirList(path, positivelist) : FilenameMatchesDirList(path, positivelist);
               if (!match)
               {
                   TDEBUG_TRACE("no pos match");
                   res = true;
               }
           }
       }
   }

   return res;
}
   


bool CVSStatus::IsOuterDirectory(const std::string& path)
{
   TDEBUG_ENTER("CVSStatus::IsOuterDirectory");
   return IsDirectory(path.c_str()) && CVSDirectoryHere(path)
      && !CVSDirectoryAbove(path);
}

bool CVSStatus::IsBinary(const std::string& file)
{
   return GetFileFormat(file) == fmtBinary;
}


bool CVSStatus::IsDirInCVS(const std::string& dir)
{
   FileStatus status = GetFileStatus(dir);
   return (status == STATUS_INCVS_RO) || (status == STATUS_INCVS_RW) 
      || (status == STATUS_OUTERDIRECTORY);
}

#endif
