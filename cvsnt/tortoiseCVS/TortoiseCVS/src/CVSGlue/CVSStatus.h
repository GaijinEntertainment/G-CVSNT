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

#ifndef CVS_STATUS_H
#define CVS_STATUS_H

#include <string>
#include <vector>
#include <algorithm>
#include <time.h>
#ifdef __BORLANDC__
   // Types needed for time functions
   using std::time_t;
#endif
#include <wx/string.h>
#include "CvsEntries.h"
#include "../Utils/SyncUtils.h"
#include "../Utils/Cache.h"

class ProgressDialog;
class CVSAction;
class CInitCVSStatus;
class CVSServerFeatures;
class MakeArgs;


class CVSStatus
{
   friend class CInitCVSStatus;
public:
   enum FileStatusFlags
   {
      STATUS_NOWHERENEAR_CVS = 0,
      STATUS_INCVS_RO,       // In CVS, read-only, unmodified
      STATUS_INCVS_RW,       // In CVS, read-write, unmodified
      STATUS_LOCKED,         // In CVS, local user has an Edit on the file
      STATUS_LOCKED_RO,      // In CVS, local user has an Edit on the file, but file is read-only
      STATUS_CHANGED,        // In CVS, modified
      STATUS_STATIC,         // In CVS, static, may or may not be modified
      STATUS_ADDED,
      STATUS_CONFLICT,       // In CVS, conflict
      STATUS_NOTINCVS,
      STATUS_OUTERDIRECTORY, // Special for top level directories that contain stuff which is in CVS
      STATUS_IGNORED,
      STATUS_REMOVED,
      STATUS_RENAMED,
      STATUS_MISSING
   };

   static const int STATUS_CVSSTATUS_MASK;

   enum KeywordOption
   { 
       keyNames = 1,   // k
       keyValues = 2,  // v
       keyLocker = 4,  // l
       keyDisabled = 8 // o
   };

   static const int keyNormal;

   enum FileFormat 
   {
      fmtUnknown = 0,
      fmtASCII,       // empty or t
      fmtUnicode,     // u
      fmtBinary       // b or B
   };

   enum FileOption
   {
      foBinaryDiff = 1,  // B
      foCompressed = 2,  // z
      foExclusive = 4,   // x
      foUnixLines = 8,
      foStaticFile = 16  // s
   };

   typedef int FileOptions;
   typedef int KeywordOptions;

   // This is a combination of FileStatusFlags values
   typedef int FileStatus;

   class CVSVersionInfo
   {
   public:
      // Constructor
      CVSVersionInfo(const std::string version = "");
      // Convert to string
      std::string AsString();
      // Parse a version string
      void ParseVersionString(const std::string& str);
      // Set version as unknown (0.0.0.0)
      void SetUnknown();
      // Operator "greater than"
      bool operator> (const CVSVersionInfo& op) const;
      // Operator "equals"
      bool operator==(const CVSVersionInfo& op) const;
      // Operator "less than"
      inline bool operator<(const CVSVersionInfo& op) const 
         { return op > (*this);};
      // Operator "greater or equals"
      inline bool operator>=(const CVSVersionInfo& op) const
         { return (*this) > op || (*this) == op;};
      // Operator "less or equals"
      inline bool operator<=(const CVSVersionInfo& op) const
         { return (*this) < op || (*this) == op;};
      // Is valid
      inline bool IsValid() const { return m_IsValid; };
      // Is CVSNT
      inline bool IsCVSNT() const { return m_IsCVSNT; };
      // Major version
      inline int VersionMajor() const { return m_VersionMajor; };
      // Minor version
      inline int VersionMinor() const { return m_VersionMinor; };
      // Release version
      inline int VersionRelease() const { return m_VersionRelease; };
      // Patch version
      inline int VersionPatch() const { return m_VersionPatch; };

   private:
      bool m_IsCVSNT;
      bool m_IsValid;
      int m_VersionMajor;
      int m_VersionMinor;
      int m_VersionRelease;
      int m_VersionPatch;
   };

   struct ScanData
   {
      ScanData(const std::string& fn, FileStatus s, const std::string& bn = "")
         : filename(fn),
           bugnumber(bn),
           status(s)
      {
      }
      std::string filename;
      std::string bugnumber;
      FileStatus  status;
   };
   
   // Data for EntNode dir
   class EntnodeCacheData
   {
   public:
      EntnodeCacheData() : timeStamp(0), data(0) {}
      DWORD timeStamp;
      EntnodeData *data;
   };

   class EntnodeDirData
   {
   public:
      EntnodeDirData();

      DWORD                                         timeStamp;
      DWORD                                         timeStampDefIgnList; 
      FileChangeParams                              fcpCvsIgnore;
      std::vector<std::string>                      ignoreList;

      EntnodeMap                                    entries;
      FileChangeParams                              fcpEntries;
      TortoiseMemoryCache<std::string, 
                          EntnodeCacheData>         entnodeCache;
      CriticalSection                               entnodeCacheCS;
   };

   static FileStatus GetFileStatus(const std::string& filename);
   static FileStatus GetFileStatus(EntnodeData* data, bool edit = false);
   static std::string GetBugNumber(const std::string& filename);
   static std::string GetBugNumber(EntnodeData* data);
   static FileStatus GetFileStatusRecursive(const std::string& filename, FileStatus status = -1, 
                                            int levels = -1);
   static bool GetEditStatus(const std::string& filename);
   static void ScanEntries(const std::string& directory,
                           std::vector<ScanData>& scandata, bool recurse);
   static void RecursiveNeedsAdding(const std::string& directory, 
                                    std::vector<std::string>& files,
                                    std::vector<bool>& ignored,
                                    bool includeFolders = true,
                                    bool dirIsIgnored = false);

   // Scan for files
   static void RecursiveScan(const std::string& directory,
                             std::vector<std::string>* modified, 
                             std::vector<std::string>* added,
                             std::vector<std::string>* removed,
                             std::vector<std::string>* renamed,
                             std::vector<std::string>* unmodified, 
                             std::vector<std::string>* conflict,
                             // If false, exclude writable files from 'unmodified'
                             bool includeWritable = true);

    // Includes bugnumbers
    static void RecursiveScan(const std::string& directory,
                              std::vector< std::pair<std::string, std::string> >* modified, 
                              std::vector< std::pair<std::string, std::string> >* modifiedStatic, 
                              std::vector< std::pair<std::string, std::string> >* added,
                              std::vector< std::pair<std::string, std::string> >* removed,
                              std::vector< std::pair<std::string, std::string> >* renamed,
                              std::vector< std::pair<std::string, std::string> >* unmodified, 
                              std::vector< std::pair<std::string, std::string> >* conflict,
                              // If false, exclude writable files from 'unmodified'
                              bool includeWritable = true);
    
   static void GetDirectoryStatus(const std::string& dirname, FileStatus* dirStatus,
                                  FileStatus* dirStatusRec, int levels, bool persist = true);
   static FileStatus GetCombinedOverlayStatus(FileStatus s1, FileStatus s2);

   // Find the first directory that has a CVS/Root file referring the specified server
   static std::string RecursiveFindRoot(std::string scanDirectory,
                                                   const std::string& server);

   static void InvalidateCache();
   
   static bool InCVSHasSubdirs(const std::string& filename);
   
   static std::string CVSRootForPath(std::string path);
   static std::string CVSRepositoryForPath(std::string path);

   static CVSVersionInfo GetClientCVSVersion(const std::string& cvsroot, 
                                             CVSAction* defglue = 0);
   static std::string GetClientCVSVersionString(bool silentOnError = false);

   static CVSVersionInfo GetServerCVSVersion(const std::string& cvsroot, 
                                             CVSAction* defglue = 0);
   static std::string GetServerCVSVersionString(const std::string& cvsroot,
                                                CVSAction* defglue = 0);

   
   static bool HasRevisionNumber(const std::string& filename);
   static bool HasRevisionNumber(EntnodeData* data);
   static bool HasTimestamp(const std::string& filename);
   static bool HasTimestamp(EntnodeData* data);
   static bool HasStickyTag(const std::string& filename);
   static bool HasStickyTag(EntnodeData* data);
   static bool HasStickyDate(const std::string& filename);
   static bool HasStickyDate(EntnodeData* data);
   static bool HasOptions(const std::string& filename);
   static bool HasOptions(EntnodeData* data);
   
   static std::string GetRevisionNumber(const std::string& filename);
   static std::string GetRevisionNumber(EntnodeData* data);
   static std::string GetTimestamp(const std::string& filename);
   static std::string GetTimestamp(EntnodeData* data);
   static std::string GetStickyTag(const std::string& filename);
   static std::string GetStickyTag(EntnodeData* data);
   static std::string GetStickyDate(const std::string& filename);
   static std::string GetStickyDate(EntnodeData* data);
   static std::string GetOptions(const std::string& filename);
   static std::string GetOptions(EntnodeData* data);

   static void ParseOptions(const std::string& options, 
                            FileFormat* fileFormat,
                            KeywordOptions* keywordOptions,
                            FileOptions* fileOptions);

   static void GetParsedOptions(const std::string& filename, 
                                FileFormat* fileFormat,
                                KeywordOptions* keywordOptions,
                                FileOptions* fileOptions);

   static void GetParsedOptions(EntnodeData* data, 
                                FileFormat* fileFormat,
                                KeywordOptions* keywordOptions,
                                FileOptions* fileOptions);

   static std::string BuildAddFlags(const std::string& dir,
                                    FileFormat fileFormat, 
                                    KeywordOptions keywordOptions,
                                    FileOptions fileOptions,
                                    const CVSServerFeatures& serverFeatures);

   static FileFormat GetFileFormat(const std::string& filename);
   static FileFormat GetFileFormat(EntnodeData* data);
   static KeywordOptions GetKeywordOptions(const std::string& filename);
   static KeywordOptions GetKeywordOptions(EntnodeData* data);
   static FileOptions GetFileOptions(const std::string& filename);
   static FileOptions GetFileOptions(EntnodeData* data);

   static wxString KeywordOptionsString(KeywordOptions keywordOptions);
   static wxString FileFormatString(FileFormat fileFormat);
   static wxString FileOptionString(FileOption fileOption);
   static wxString FileOptionsString(FileOptions fileOptions);
   static wxString FileStatusString(FileStatus fileStatus);

   static EntnodeData* GetEntnodeData(const std::string& filename,
                                      CSHelper& csNode);
   static EntnodeData* GetEntnodeData(const std::string& filename,
                                      EntnodeDirData *dirData,
                                      CSHelper &csNode);

   static std::string ReadTemplateFile(const std::string& path);

   static bool ResetFileTimestamp(const std::string& filename);
   static bool IsDisabledPath(const std::string& path, bool isDirectory = false);
   static bool IsOuterDirectory(const std::string& path);
   static bool GuessFileFormat(const std::string& filename, FileFormat &fmt, FileOptions &fo);
   static bool IsBinary(const std::string& file);
   static bool IsDirInCVS(const std::string& dir);
   
private:

   static TortoiseMemoryCache<std::string, 
                              EntnodeDirData>       ourEntnodeDirCache;
   static CriticalSection ourEntnodeDirCS;
   
   static EntnodeDirData* GetEntnodeDirData(const std::string& directory,
                                            CSHelper &csDir);
   static EntnodeData* GetEntnodeDataUncached(const std::string& filename,
                                              EntnodeDirData* dirData);
   static CVSVersionInfo GetServerCVSVersionUncached(const std::string& cvsroot, 
                                                     CVSAction* defglue);
   
   
   // This is a thread-safe class
   static StringNoCaseCmp ourStringNoCaseCmp;
   
   // Cache for recursive file status
   class OurRecStatusCacheData
   {
   public:
      OurRecStatusCacheData() : timeStamp(0) {}
      FileStatus  status;
      DWORD       timeStamp;
   };
   static TortoiseMemoryCache<std::string, OurRecStatusCacheData> ourRecStatusCache;
   static CriticalSection ourRecStatusCS;

   
   static FileStatus GetFileStatusRecursiveUncached(const std::string& filename, FileStatus status,
                                                    int levels);
   
   struct UserFileType
   {
       std::string      wildcard;
       FileFormat       fileformat;
       std::string      dllname;
       std::string      functionname;
   };
   typedef std::vector<UserFileType> UserFileTypesVector;

   // Read user filetypes
   static bool                ourUserFileTypesRead;
   static UserFileTypesVector ourUserFileTypes;
   static CriticalSection     ourUserFileTypesCS;
   static void ReadUserFileTypes();
   static bool IsUserDefinedType(const std::string& filename, CVSStatus::FileFormat& myFormat);
    
   static bool RecursiveHasChangedFiles(std::string scanDirectory);
   static FileStatus CVSStatus::GetCombinedStatus(FileStatus s1, FileStatus s2);
   static int GetFileStatusPriority(FileStatus status);
};



#endif // CVS_STATUS_H
