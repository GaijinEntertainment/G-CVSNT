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

#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <Windows.h>
#include "FixWinDefs.h"
// g++ cygwin compatibility
#include <stdlib.h>
#ifndef _MAX_PATH
   #define _MAX_PATH MAX_PATH
#endif

// Compatibility defines (formerly in FileTraversal.h)

#if !defined(S_ISDIR) && defined(S_IFDIR)
#if defined(S_IFMT)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#else
#define S_ISDIR(m) ((m) & S_IFDIR)
#endif
#endif

#if !defined(S_ISREG) && defined(S_IFREG)
#if defined(S_IFMT)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#else
#define S_ISREG(m) ((m) & S_IFREG)
#endif
#endif

#if !defined(S_ISLNK) && defined(S_IFLNK)
#if defined(S_IFMT)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#else
#define S_ISLNK(m) ((m) & S_IFLNK)
#endif
#endif

#ifdef WIN32
#include <direct.h>
#define STRUCT_DIRENT struct direct
#else /* !WIN32 */
#include <dirent.h>
#define STRUCT_DIRENT struct dirent
#endif /* !WIN32 */

#include <string>
#include <sstream>
#include <vector>

// List of (filename, bugnumber) pairs
typedef std::vector< std::pair<std::string, std::string> > FilesWithBugnumbers;

bool FileExists(const char* filename);
bool IsDirectory(const char* filename);
bool IsReadOnly(const char* filename);
bool IsSystemFile(const char* filename);

int FileSize(const char* filename);
bool GetLastWriteTime(const char* filename, FILETIME *pFileTime);
bool SetLastWriteTime(const char* filename, FILETIME *pFileTime);
bool SetFileReadOnly(const char* filename, bool setReadOnly);

std::string GetDirectoryAbove(const std::string& filename);
std::string GetDirectoryPart(const std::string& filename);

std::string RemoveTrailingDelimiter(const std::string& pathorfile);
std::string EnsureTrailingDelimiter(const std::string& pathorfile);
std::string EnsureTrailingUnixDelimiter(const std::string& pathorfile);

bool CVSDirectoryHere(const std::string& path);
bool CVSDirectoryAbove(const std::string& path);

std::string ExtractLastPart(const std::string& pathorfile);
std::string StripLastPart(const std::string& pathorfile);

std::string GetFirstPart(const std::string& pathorfile);
std::string GetLastPart(const std::string& pathorfile);
#if wxUSE_UNICODE
wxString GetLastPart(const wxString& pathorfile);
#endif
std::string StripFirstPart(const std::string& pathorfile);

std::string GetNameBody(const std::string& file);
std::string GetExtension(const std::string& file);
bool HasExtension(const std::string& file);
std::string CutFirstPathElement(std::string& file);

std::string MakeRevFilename(const std::string& file, const std::string& rev);

std::string GetTemporaryPath();
std::string UniqueTemporaryFile();
std::string UniqueTemporaryDir();

// Get a list of files and/or folders in the specified folder.
void GetDirectoryContents(std::string path,
                          // List of files returned here unless 0
                          std::vector<std::string>* files, 
                          // List of folders returned here unless 0
                          std::vector<std::string>* dirs);
void GetRecursiveContents(std::string path, std::vector<std::string>& files,
                          const std::string& wildcard, bool direcoriesFirst=true);
bool CopyDirectoryRecursively(std::string source, std::string dest);
void DeleteDirectoryRecursively(std::string directory);

bool IsRootPath(const std::string& PathName);
bool IsRemovableDrivePath(const std::string& PathName);

// Find common stub for all files in list
std::string FindCommonStub(std::vector<std::string> files);

// Strip shared bits from the front of all the paths
void ShortenPaths(std::vector<std::string>& files, std::string& stub);
void ShortenPaths(std::vector<std::string>& files, std::vector<std::string>& ignoredFiles, 
                  std::string& stub);

// Get the 8.3 representation of a path.
std::string GetShortPath(const std::string& longpath);

// Return true if 'filename' is located inside one of the directories
// in the list.
bool FilenameMatchesDirList(const std::string& filename,
                            const std::vector<std::string>& aList);

bool DirectoryMatchesDirList(std::string dirname,
                             const std::vector<std::string>& aList);

// Return all the files that are located directly in the specified folder.
std::vector<std::string> GetFilesInFolder(const FilesWithBugnumbers& files, const std::string& folder);

// Concatenate paths
std::string ConcatenatePaths(const std::string& Path1, const std::string& Path2);

// Strip common path from files in the same sandbox
bool StripCommonPath(std::vector<std::string>& files, std::string& sDir);

// Touch
void TouchFile(const std::string& filename);

// Delete directory recursively
void DeleteDirectoryRec(const std::string& directory);

// Fix path for drive root
std::string FixDriveRootPath(const std::string& directory);

// Find position where directory part starts
std::string::size_type FindDirStartPos(const std::string& directory);

class FileChangeParams
{
public:
   FileChangeParams()
      : dwFileSizeLow(0), dwFileSizeHigh(0) 
   {
      memset(&ftLastWriteTime, 0, sizeof(ftLastWriteTime));
   }
   FILETIME ftLastWriteTime;
   DWORD    dwFileSizeLow;
   DWORD    dwFileSizeHigh;
   bool operator!=(FileChangeParams fcp)
   {
      return ((ftLastWriteTime.dwLowDateTime != fcp.ftLastWriteTime.dwLowDateTime) 
         || (ftLastWriteTime.dwHighDateTime != fcp.ftLastWriteTime.dwHighDateTime) 
         || (dwFileSizeLow != fcp.dwFileSizeLow)
         || (dwFileSizeHigh != fcp.dwFileSizeHigh));
   }

   bool operator==(FileChangeParams fcp)
   {
      return ((ftLastWriteTime.dwLowDateTime == fcp.ftLastWriteTime.dwLowDateTime) 
         && (ftLastWriteTime.dwHighDateTime == fcp.ftLastWriteTime.dwHighDateTime) 
         && (dwFileSizeLow == fcp.dwFileSizeLow)
         && (dwFileSizeHigh == fcp.dwFileSizeHigh));
   }

   bool IsNull()
   {
      return ((ftLastWriteTime.dwLowDateTime == 0) 
         && (ftLastWriteTime.dwHighDateTime == 0) 
         && (dwFileSizeLow == 0)
         && (dwFileSizeHigh == 0));
   }

   void SetNull()
   {
      ftLastWriteTime.dwLowDateTime = 0; 
      ftLastWriteTime.dwHighDateTime = 0;
      dwFileSizeLow = 0;
      dwFileSizeHigh = 0;
   }

   std::string AsString()
   {
      std::ostringstream msg;
      SYSTEMTIME st;
      FileTimeToSystemTime(&ftLastWriteTime, &st);
      char buf[100];
      sprintf(buf, "%2.2d/%2.2d/%2.2d %2.2d:%2.2d:%2.2d", st.wDay, st.wMonth, st.wYear % 100,
              st.wHour, st.wMinute, st.wSecond);
      msg << "Time: " << buf << ", " 
          << ", Size: " << dwFileSizeLow;
      return msg.str();

   }
};

// Get parameters to detect file content change
FileChangeParams GetFileChangeParams(const std::string& filename);

#endif
