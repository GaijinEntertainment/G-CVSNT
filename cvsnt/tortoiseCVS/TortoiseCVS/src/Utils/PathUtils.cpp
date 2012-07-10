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
#include "PathUtils.h"
#include <windows.h>
#include "FixWinDefs.h"
#include <sys/stat.h> // PathUtils.cpp doesn't need DST saving Stat.h
#include <sys/types.h>
#include "TortoiseDebug.h"
#include "TortoiseRegistry.h"
#include "TortoiseException.h"
#include <shlobj.h>
#include <Utils/Translate.h>
#include <Utils/StringUtils.h>
#include <fstream>
#include <sstream>
#include "ShellUtils.h"

bool FileExists(const char* filename)
{
   DWORD attributes = GetFileAttributesA(filename);
   return attributes == INVALID_FILE_ATTRIBUTES ?  false: true;
}

std::string ExtractLastPart(const std::string& pathorfile)
{
   std::string result = RemoveTrailingDelimiter(pathorfile);
        
   std::string::size_type pos = result.find_last_of("\\/");
   if (pos == std::string::npos)
      return result;
   return result.substr(pos + 1, pathorfile.size());
}

std::string StripLastPart(const std::string& pathorfile)
{
   std::string result = RemoveTrailingDelimiter(pathorfile);
        
   std::string::size_type pos = result.find_last_of("\\");
   if (pos == std::string::npos)
      return "";
   return result.substr(0, pos);
}

std::string GetNameBody(const std::string& file)
{
   std::string result = RemoveTrailingDelimiter(file);
   std::string::size_type pos = result.find_last_of("\\.");
   if (pos == std::string::npos || result[pos] == '\\')
      return result;
   return result.substr(0, pos);
}


bool HasExtension(const std::string& file)
{
   std::string result = RemoveTrailingDelimiter(file);
   std::string::size_type pos = result.find_last_of("\\.");
   if (pos == std::string::npos || result[pos] == '\\')
      return false;
   else
      return true;
}


std::string GetExtension(const std::string& file)
{
   std::string result = RemoveTrailingDelimiter(file);
   std::string::size_type pos = result.find_last_of("\\.");
   if (pos == std::string::npos || result[pos] == '\\')
      return "";
   return result.substr(pos + 1, file.size());
}


// Cut first element off the path
std::string CutFirstPathElement(std::string& file)
{
   std::string result;
   std::string::size_type p;
   if (file.substr(0, 2) == "\\\\")
   {
      // it's an UNC path, so consider everything up to the 4th backslash as drive 
      p = file.find_first_of("\\", 2);
      if (p == std::string::npos)
      {
         // Last element
         result = file;
         file = "";
      }
      else
      {
         p = file.find_first_of("\\", p);
         if (p == std::string::npos)
         {
            result = file;
            file = "";
         }
         else
         {
            result = file.substr(0, p);
            file = file.substr(p + 1);
         }
      }
   }
   else 
   {
      std::string::size_type p = file.find_first_of("\\");
      if (p == std::string::npos)
      {
         result = file;
         file = "";
      }
      else
      {
         result = file.substr(0, p);
         file = file.substr(p + 1);
      }
   }
   return result;
}

bool IsDirectory(const char* filename)
{
   DWORD attributes = GetFileAttributesA(filename);
   if (attributes == INVALID_FILE_ATTRIBUTES)
      return false;

   return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool IsReadOnly(const char* filename)
{
   // Check whether read-only
   DWORD attributes = GetFileAttributesA(filename);
   if (attributes == INVALID_FILE_ATTRIBUTES)
      return false;

   return (attributes & FILE_ATTRIBUTE_READONLY) != 0;
}

bool IsSystemFile(const char* filename)
{
   // Root paths are no system files
   if (IsRootPath(filename))
      return false;

   DWORD attributes = GetFileAttributesA(filename);
   if (attributes == INVALID_FILE_ATTRIBUTES)
      return false;

   return (attributes & FILE_ATTRIBUTE_SYSTEM) != 0;
}


std::string GetDirectoryAbove(const std::string& filename)
{
   if (filename.empty())
      return filename;
   std::string myfilename = RemoveTrailingDelimiter(filename);
        
   // Return the directory above this file/directory
   std::string::size_type dirstart = FindDirStartPos(myfilename);
   std::string::size_type pos = myfilename.find_last_of("\\");
   if (pos >= dirstart)
      myfilename = myfilename.substr(0, pos);

   // Ensure that we have a trailing slash if this is the root directory
   myfilename = FixDriveRootPath(myfilename);
        
   return myfilename;
}

std::string GetDirectoryPart(const std::string& filename)
{
   if (filename.empty())
      return "";

   std::string myfilename = RemoveTrailingDelimiter(filename);
     
   if (!IsDirectory(myfilename.c_str()))
   {
      // A file, so return the directory above it
      std::string::size_type dirstart = FindDirStartPos(myfilename);
      
      std::string::size_type pos = myfilename.find_last_of("\\");
      if (pos >= dirstart)
         myfilename = myfilename.substr(0, pos);
   }
   // Ensure that we have a trailing slash if this is the root directory
   return FixDriveRootPath(myfilename);
}

std::string RemoveTrailingDelimiter(const std::string& pathorfile)
{
   if (pathorfile.empty())
      return "";

   // Remove trailing slash
   if (pathorfile[pathorfile.size() - 1] == '\\'
       || pathorfile[pathorfile.size() - 1] == '/')
      return pathorfile.substr(0, pathorfile.size() - 1);

   return pathorfile;
}

std::string EnsureTrailingDelimiter(const std::string& pathorfile)
{
   if (pathorfile.empty())
      return "";
        
   if (pathorfile[pathorfile.size() - 1] == '/')
      return pathorfile.substr(0, pathorfile.size() - 1) + '\\';
   if (pathorfile[pathorfile.size() - 1] == '\\')
      return pathorfile;
        
   return pathorfile + "\\";
}

std::string EnsureTrailingUnixDelimiter(const std::string& pathorfile)
{
   if (pathorfile.empty())
      return "";
        
   if (pathorfile[pathorfile.size() - 1] == '\\')
      return pathorfile.substr(0, pathorfile.size() - 1) + '/';
   if (pathorfile[pathorfile.size() - 1] == '/')
      return pathorfile;
        
   return pathorfile + "/";
}

bool CVSDirectoryHere(const std::string& path)
{
   TDEBUG_ENTER("CVSDirectoryHere");
   TDEBUG_TRACE("Checking " << path);
   std::string mypath = RemoveTrailingDelimiter(path);
   std::string CVSDirectory = mypath + "\\CVS";
   // We also check for CVS/Repository, to minimise the chance of hitting
   // a directory called CVS that was made by the user or another program.
   std::string CVSDirectoryRepository = mypath + "\\CVS\\Repository";
   return FileExists(CVSDirectory.c_str()) && FileExists(CVSDirectoryRepository.c_str());
}


bool CVSDirectoryAbove(const std::string& path)
{
   TDEBUG_ENTER("CVSDirectoryAbove");
   TDEBUG_TRACE("Checking " << path);
   std::string mypath = RemoveTrailingDelimiter(path);
   std::string CVSDirectory = RemoveTrailingDelimiter(GetDirectoryAbove(mypath));
   TDEBUG_TRACE("Parent dir " << CVSDirectory);
   if (mypath == CVSDirectory)
   {
      // We're toplevel
      return false;
   }
   else
   {
      CVSDirectory += "\\CVS";
      // We also check for CVS/Repository, to minimise the chance of hitting
      // a directory called CVS that was made by the user or another program.
      std::string CVSDirectoryRepository = CVSDirectory + "\\Repository";
      return FileExists(CVSDirectory.c_str()) && FileExists(CVSDirectoryRepository.c_str());
   }
}

// Return true if PathName is a drive letter ("A:")
// a root path ("A:\") or a UNC share path, false otherwise.
bool IsRootPath(const std::string& PathName)
{
   bool Result = false;

   // Drive letter
   if ((PathName.length() >= 2) && (PathName[1] == ':'))
   {
      if ((PathName.length() == 2) ||
          ((PathName.length() == 3) && ((PathName[2] == '\\') || (PathName[2] == '/'))))
         Result = true;
   }
   // UNC
   else if ((PathName.length() >= 4) && (PathName[0] == '\\') 
      && (PathName[1] == '\\'))
   {
      // find next backslash
      std::string::size_type p = PathName.find('\\', 2);
      if (p != std::string::npos) 
      {
         p = PathName.find('\\', p + 1);
         if ((p == std::string::npos) || (p == PathName.length() + 1))
         {
            Result = true;
         }
      }
   }
        
   return Result;
}

// Fix path for drive root
std::string FixDriveRootPath(const std::string& directory)
{
   if (directory.size() == 2 && directory[1] == ':')
   {
      return directory + "\\";
   }
   else
   {
      return directory;
   }
}

// Find position where directory part starts
std::string::size_type FindDirStartPos(const std::string& directory)
{
   if (directory.size() >= 2)
   {
      if (directory[1] == ':')
         return 2;
      if (directory[0] == '\\' && directory[1] == '\\')
      {
         std::string::size_type pos = directory.find_first_of("\\", 2);
         if (pos == std::string::npos)
            return 0;
         pos = directory.find_first_of("\\", pos + 1);
         if (pos == std::string::npos)
            return directory.length();
         else
            return pos;
      }
   }
   return 0;
}

#ifndef POSTINST

std::string GetTemporaryPath()
{
   char tempDir[_MAX_PATH + 1];
   if (GetTempPathA(_MAX_PATH, tempDir) == 0)
      TortoiseFatalError(_("Failed to find temporary path"));
   return std::string(tempDir);
}



std::string UniqueTemporaryFile()
{
   std::string temppath = GetTemporaryPath();
   char tempFile[_MAX_PATH + 1];
   if (!GetTempFileNameA(temppath.c_str(), "TCV", 0, tempFile))
   {
      DWORD dwErr = GetLastError();
      wxString sMsg = Printf(_("Failed to generate temporary filename in '%s': error %d"), 
                             MultibyteToWide(temppath).c_str(), dwErr);
      sMsg += wxT("\n");
      sMsg += GetWin32ErrorMessage(dwErr) + wxT("\n");
      ULARGE_INTEGER liFreeSpace;
      ULARGE_INTEGER liTotalSpace;
      ULARGE_INTEGER liTotalFreeSpace;

      if (GetDiskFreeSpaceExA(temppath.c_str(), &liFreeSpace, &liTotalSpace, &liTotalFreeSpace))
      {
          TCHAR buf[256];
          NUMBERFMT nf;
          TCHAR decsep[10];
          TCHAR thosep[10];
          nf.NumDigits = 0;
          nf.LeadingZero = 0;
          nf.Grouping = 3;

          nf.NegativeOrder = 0;
          GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, decsep, sizeof(decsep)); 
          nf.lpDecimalSep = decsep;
          GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SMONTHOUSANDSEP, thosep, sizeof(thosep)); 
          nf.lpThousandSep = thosep;

          wxString s = Printf(wxT("%I64d"), liFreeSpace.QuadPart / 1024);
          if (GetNumberFormat(LOCALE_USER_DEFAULT, 0, s.c_str(), &nf, buf, sizeof(buf)))
              sMsg += Printf(_("Free space: %s KB"), buf) + wxT("\n");
          s = Printf(wxT("%I64d"), liTotalSpace.QuadPart / 1024);
          if (GetNumberFormat(LOCALE_USER_DEFAULT, 0, s.c_str(), &nf, buf, sizeof(buf)))
              sMsg += Printf(_("Total space: %s KB"), buf) + wxT("\n");
          s = Printf(wxT("%I64d"), liTotalFreeSpace.QuadPart / 1024);
          if (GetNumberFormat(LOCALE_USER_DEFAULT, 0, s.c_str(), &nf, buf, sizeof(buf)))
              sMsg += Printf(_("Total free space: %s KB"), buf);
      }

      TortoiseFatalError(sMsg);
   }

   return std::string(tempFile);
}


std::string MakeRevFilename(const std::string& file, const std::string& rev)
{
   std::string result = GetNameBody(file) + "." + rev;
   if (HasExtension(file))
   {
      result += "." + GetExtension(file);
   }
   return result;
}




int FileSize(const char* filename)
{
   struct stat buf;
   int i = stat( filename, &buf );
   if (i == -1)
      return -1;
   return buf.st_size;
}



bool GetLastWriteTime(const char* filename, FILETIME* pFileTime)
{
   bool bResult = false;
   HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
   if (hFile != INVALID_HANDLE_VALUE)
   {
      if (GetFileTime(hFile, NULL, NULL, pFileTime))
      {
         bResult = true;
      }
      CloseHandle(hFile);
   }
   return bResult;
}


bool SetLastWriteTime(const char* filename, FILETIME *pFileTime)
{
   bool bResult = false;
   HANDLE hFile = CreateFileA(filename, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
   if (hFile != INVALID_HANDLE_VALUE)
   {
      if (SetFileTime(hFile, NULL, NULL, pFileTime))
      {
         bResult = true;
      }
      CloseHandle(hFile);
   }
   return bResult;
}



bool SetFileReadOnly(const char* filename, bool setReadOnly)
{
   bool bResult = false;
   DWORD attr = GetFileAttributesA(filename);
   if (attr != INVALID_FILE_ATTRIBUTES)
   {
      if (setReadOnly)
      {
         attr |= FILE_ATTRIBUTE_READONLY;
      }
      else
      {
         attr &= !FILE_ATTRIBUTE_READONLY;
      }
      if (SetFileAttributesA(filename, attr))
      {
         bResult = true;
      }
   }
   return bResult;
}



std::string UniqueTemporaryDir()
{
    std::string result;
    std::string temppath = GetTemporaryPath();
    int retries = 0;
    bool success;

    WORD id = (WORD) GetTickCount();
    do
    {
        std::ostringstream ss;
        ss << EnsureTrailingDelimiter(temppath) << "TCV" << id << ".tmp";
        result = ss.str();
        success = CreateDirectoryA(result.c_str(), 0) != 0;
        ++id;
        ++retries;
    } while (!success && retries < 100);

    if (!success)
    {
        DWORD dwErr = GetLastError();
        wxString sMsg = Printf(_("Failed to generate temporary folder in '%s': error %d"), 
                               MultibyteToWide(temppath).c_str(), dwErr) + wxT("\n");
        sMsg += GetWin32ErrorMessage(dwErr) + wxT("\n");
        ULARGE_INTEGER liFreeSpace;
        ULARGE_INTEGER liTotalSpace;
        ULARGE_INTEGER liTotalFreeSpace;

        if (GetDiskFreeSpaceExA(temppath.c_str(), &liFreeSpace, &liTotalSpace, &liTotalFreeSpace))
        {
            TCHAR buf[256];
            NUMBERFMT nf;
            TCHAR decsep[10];
            TCHAR thosep[10];
            nf.NumDigits = 0;
            nf.LeadingZero = 0;
            nf.Grouping = 3;

            nf.NegativeOrder = 0;
            GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, decsep, sizeof(decsep)); 
            nf.lpDecimalSep = decsep;
            GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SMONTHOUSANDSEP, thosep, sizeof(thosep)); 
            nf.lpThousandSep = thosep;

            wxString s = Printf(wxT("%I64d"), liFreeSpace.QuadPart / 1024);
            if (GetNumberFormat(LOCALE_USER_DEFAULT, 0, s.c_str(), &nf, buf, sizeof(buf)))
                sMsg += Printf(_("Free space: %s KB"), buf) + wxT("\n");
            s = Printf(wxT("%I64d"), liTotalSpace.QuadPart / 1024);
            if (GetNumberFormat(LOCALE_USER_DEFAULT, 0, s.c_str(), &nf, buf, sizeof(buf)))
                sMsg += Printf(_("Total space: %s KB"), buf) + wxT("\n");
            s = Printf(wxT("%I64d"), liTotalFreeSpace.QuadPart / 1024);
            if (GetNumberFormat(LOCALE_USER_DEFAULT, 0, s.c_str(), &nf, buf, sizeof(buf)))
                sMsg += Printf(_("Total free space: %s KB"), buf);
        }

        TortoiseFatalError(sMsg);
    }
    return result;
}

void GetDirectoryContents(std::string path, std::vector<std::string>* files, 
                          std::vector<std::string>* dirs)
{
   path = EnsureTrailingDelimiter(path);
   std::string str = path + "*";

   WIN32_FIND_DATAA finddata;
   HANDLE h = FindFirstFileA(str.c_str(), &finddata);
   if (h == INVALID_HANDLE_VALUE)
      TortoiseFatalError(Printf(_("Couldn't scan folder %s"), wxText(path).c_str()));
   do
   {
      if (std::string(finddata.cFileName) != "." && std::string(finddata.cFileName) != "..")
      {
         if ((finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
         {
            if (dirs)
               dirs->push_back(finddata.cFileName);
         }
         else
         {
            if (files)
               files->push_back(finddata.cFileName);
         }
      }
   }
   while (FindNextFileA(h, &finddata));
   FindClose(h);
}

void GetRecursiveContents(std::string path, std::vector<std::string>& files,
                          const std::string& wildcard, bool directoriesFirst)
{
   path = EnsureTrailingDelimiter(path);
        
   std::string str = path + wildcard;
   WIN32_FIND_DATAA finddata;
   HANDLE h = FindFirstFileA(str.c_str(), &finddata);
   if ( h == INVALID_HANDLE_VALUE )
      TortoiseFatalError(Printf(_("Couldn't scan folder %s"), wxText(path).c_str()));
   do
   {
      if (std::string(finddata.cFileName) != "." && std::string(finddata.cFileName) != "..")
      {
         // Add files and directories
         if (directoriesFirst)
            files.push_back(path + finddata.cFileName);

         // Recurse into directories
         if ((finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            GetRecursiveContents(path + finddata.cFileName, files, wildcard, directoriesFirst);

         // Add files and directories
         if (!directoriesFirst)
            files.push_back(path + finddata.cFileName);
      }
   }
   while (FindNextFileA(h, &finddata));
   FindClose(h);
}

bool IsRemovableDrivePath(const std::string& PathName)
{
   TDEBUG_ENTER("IsRemovableDrivePath");
   bool Result = false;

   // Special case for UNC paths
   if ((PathName[0] == '\\') && (PathName[1] == '\\'))
      return false;
    
   // We use 3 as the slash is required for Win95/98 - otherwise
   // GetDriveType sometimes spuriously returns DRIVE_NO_ROOT_DIR
   if (PathName.length() >= 3)
   {
      std::string drive = PathName.substr(0, 3);
      DWORD drivetype = GetDriveTypeA(drive.c_str());
      TDEBUG_TRACE("GetDriveType(" << drive << "): " << drivetype);
      switch (drivetype)
      {
      case DRIVE_UNKNOWN:
      case DRIVE_FIXED:
      case DRIVE_RAMDISK:
      case DRIVE_REMOTE:
      default:
         // Extra check added: If this is a WebDrive (TM, I presume), also
         // disable icon overlays.
         DWORD MaximumComponentLength, Flags;
         char FileSystemName[80];   // Ought to be enough...
         if (GetVolumeInformationA(drive.c_str(),
                                   0, 0, 0,
                                   &MaximumComponentLength,
                                   &Flags,
                                   FileSystemName,
                                   sizeof(FileSystemName)) &&
             !strcmpi(FileSystemName, "webdrive"))
            Result = true;
         break;
                        
      case DRIVE_NO_ROOT_DIR:
      case DRIVE_REMOVABLE:
      case DRIVE_CDROM:
         Result = true;
         break;
      }
   }
    
   return Result;
}

std::string GetFirstPart(const std::string& pathorfile)
{
   std::string::size_type pos = pathorfile.find_first_of("\\");
   if (pos == std::string::npos)
      return "";
   return pathorfile.substr(0, pos + 1);
}

std::string GetLastPart(const std::string& pathorfile)
{
   std::string::size_type pos = pathorfile.find_last_of("\\");
   if (pos == std::string::npos)
      return "";
   return pathorfile.substr(pos + 1);
}

wxString GetLastPart(const wxString& pathorfile)
{
   wxString::size_type pos = pathorfile.find_last_of(wxT("\\"));
   if (pos == wxString::npos)
      return wxT("");
   return pathorfile.substr(pos + 1);
}


std::string StripFirstPart(const std::string& pathorfile)
{
   std::string::size_type pos = pathorfile.find_first_of("\\");
   if (pos == std::string::npos)
      return pathorfile;
   return pathorfile.substr(pos + 1, pathorfile.size());
}

bool CopyDirectoryRecursively(std::string source, std::string dest)
{
   // Double-null terminated strings required
   source += (char) 0;
   source += (char) 0;
   dest += (char) 0;
   dest += (char) 0;
        
   SHFILEOPSTRUCTA fileOp;
   fileOp.hwnd = 0;
   fileOp.wFunc = FO_COPY;
   fileOp.pFrom = source.c_str();
   fileOp.pTo = dest.c_str();
   fileOp.fFlags = FOF_NOCONFIRMMKDIR;

   if (SHFileOperationA(&fileOp))
      return false;
        
   return true;
}

void DeleteDirectoryRecursively(std::string directory)
{
   if (!FileExists(directory.c_str()))
      return;
        
   std::vector<std::string> files;
   GetRecursiveContents(directory, files, "*", false);
        
   for (unsigned int i = 0; i < files.size(); ++i)
   {
      std::string directory = GetDirectoryPart(files[i]);
      DeleteFileA(files[i].c_str());
      RemoveDirectoryA(directory.c_str());
   }
}



std::string FindCommonStub(std::vector<std::string> files)
{
   std::string stub;
   bool bRepeat = true;
   while (bRepeat)
   {
      std::string firstPart;
      for(unsigned int i = 0; i < files.size(); ++i)
      {
         std::string nextFirstPart = GetFirstPart(files[i]);
         if (nextFirstPart.empty())
            return stub;
                        
         // If a first chunk of the path doesn't match, we 
         // give up stripping any more
         if (firstPart != "" && firstPart != nextFirstPart)
            return stub;
         firstPart = nextFirstPart;
                        
         // And we need something left as well!
         if (StripFirstPart(files[i]).empty())
            return stub;
      }
                
      // Remember the stub
      stub += GetFirstPart(files[0]);
                
      // Right, we now know we can strip the front off everything
      for(unsigned int j = 0; j < files.size(); ++j)
      {
         files[j] = StripFirstPart(files[j]);
      }
   }

   return stub;
}


// Strip shared bits from the front of all the paths
void ShortenPaths(std::vector<std::string>& files, std::string& stub)
{
   if (files.empty())
      return;

   stub = FindCommonStub(files);

   std::vector< std::string >::iterator it = files.begin();
   while (it != files.end())
   {
      *it = it->substr(stub.length());
      it++;
   }
}

   
   
// Strip shared bits from the front of all the paths
void ShortenPaths(std::vector<std::string>& files, std::vector<std::string>& ignoredFiles,
                  std::string& stub)
{
   if (files.empty() && ignoredFiles.empty())
      return;

   std::vector<std::string> myFiles;

   myFiles.insert(myFiles.end(), files.begin(), files.end());
   myFiles.insert(myFiles.end(), ignoredFiles.begin(), ignoredFiles.end());

   stub = FindCommonStub(myFiles);

   std::vector< std::string >::iterator it = files.begin();
   while(it != files.end())
   {
      *it = it->substr(stub.length());
      it++;
   }

   it = ignoredFiles.begin();
   while(it != ignoredFiles.end())
   {
      *it = it->substr(stub.length());
      it++;
   }
}



std::string GetShortPath(const std::string& longpath)
{
   // Size of short path will never be larger than size of long path,
   // but remember to allow for the NUL character
   int size = static_cast<int>(longpath.size()) + 1;
   char* shortpath = new char[size];
   // Default to using the long path
   std::string retval(longpath);
   if (GetShortPathNameA(longpath.c_str(),
                         shortpath,
                         size))
   {
      // Success, return short path
      retval = shortpath;
   }
   delete[] shortpath;
   return retval;
}



bool FilenameMatchesDirList(const std::string& filename,
                            const std::vector<std::string>& aList)
{
   TDEBUG_ENTER("FilenameMatchesDirList");
   std::vector<std::string>::const_iterator it; 
   for (it = aList.begin(); it != aList.end(); ++it)
   {
      std::string entry(*it);
      entry = EnsureTrailingDelimiter(entry);
      TDEBUG_TRACE("FilenameMatchesDirList('" << filename << "'): " << entry);

      // file c:/user/tma/TortoiseCVS/src/ContextMenus/HasMenu.cpp
      // entry c:/user/tma/TortoiseCVS/src/

      if (!strnicmp(filename.c_str(), entry.c_str(), entry.size()))
      {
         TDEBUG_TRACE("  match!");
         return true;
      }
   }
   return false;
}

bool DirectoryMatchesDirList(std::string dirname,
                             const std::vector<std::string>& aList)
{
   TDEBUG_ENTER("DirectoryMatchesDirList");
   dirname = EnsureTrailingDelimiter(dirname);
   std::vector<std::string>::const_iterator it;
   for (it = aList.begin(); it != aList.end(); ++it)
   {
      std::string entry(*it);
      entry = EnsureTrailingDelimiter(entry);
      TDEBUG_TRACE("DirectoryMatchesDirList('" << dirname << "'): " << entry);

      // dir   c:/user/tma/TortoiseCVS/src/ContextMenus/
      // entry c:/user/tma/TortoiseCVS/
      // match

      // dir   c:/user/tma/TortoiseCVS/src/
      // entry c:/user/tma/TortoiseCVS/
      // match

      // dir   c:/user/tma/TortoiseCVS/
      // entry c:/user/tma/TortoiseCVS/
      // match

      // dir   c:/user/tma/TortoiseCVS-1-0/src/
      // entry c:/user/tma/TortoiseCVS/
      // no match
      
      if (!strnicmp(dirname.c_str(), entry.c_str(), entry.size()))
      {
         TDEBUG_TRACE("  match!");
         return true;
      }
   }
   return false;
}


std::vector<std::string> GetFilesInFolder(const FilesWithBugnumbers& files, const std::string& folder)
{
    std::vector<std::string> result;
    for (FilesWithBugnumbers::const_iterator fileIter = files.begin(); fileIter != files.end(); ++fileIter)
        if (GetDirectoryPart(fileIter->first) == folder)
            result.push_back(fileIter->first);
    return result;
}


// Concatenate paths
std::string ConcatenatePaths(const std::string& Path1, const std::string& Path2)
{
   // Concatenate paths starting with a dot
   if (Path2.substr(0, 2) == ".\\")
   {
      std::string s = Path1;
      s = EnsureTrailingDelimiter(s);
      return s + Path2.substr(2);
   };
   
   // Starting with backslash 
   if (Path2.substr(0, 1) == "\\")
   {
      return Path2;
   }

   std::string s = Path1;
   s = EnsureTrailingDelimiter(s);
   return s + Path2;
}



// Strip common path from files in the same sandbox
bool StripCommonPath(std::vector<std::string>& files, std::string& sDir)
{
   if (files.size() == 0) 
      return false;

   ShortenPaths(files, sDir);
   return !sDir.empty();
}



// Touch
void TouchFile(const std::string& filename)
{
   FILETIME ft;
   bool bIsReadOnly = false;
   HANDLE hFile = INVALID_HANDLE_VALUE;
 
   // Get current time
   GetSystemTimeAsFileTime(&ft);

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

Cleanup:
   if (hFile != INVALID_HANDLE_VALUE)
      CloseHandle(hFile);
   if (bIsReadOnly)
      SetFileReadOnly(filename.c_str(), true);
}



// Delete directory recursively
void DeleteDirectoryRec(const std::string& directory)
{
   // Find all files in the directory
   std::string sPath = EnsureTrailingDelimiter(directory);
   std::string s = sPath + "*.*";
   WIN32_FIND_DATAA fd;
   HANDLE h;
   h = FindFirstFileA(s.c_str(), &fd);
   bool bMore = (h != INVALID_HANDLE_VALUE);
   while (bMore)
   {
      std::string sFile = fd.cFileName;
      if ((sFile != ".") && (sFile != ".."))
      {
         sFile = sPath + sFile;

         // remove read-only flags
         if (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
            SetFileReadOnly(sFile.c_str(), false);

         // delete directories
         if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            DeleteDirectoryRec(sFile);
         else
            DeleteFileA(sFile.c_str());
      }
      if (!FindNextFileA(h, &fd))
         bMore = false;
   }
   FindClose(h);

   // remove directory itself
   RemoveDirectoryA(RemoveTrailingDelimiter(directory).c_str());
}





// Get parameters to detect file content change
FileChangeParams GetFileChangeParams(const std::string& filename)
{
   HANDLE hFile = INVALID_HANDLE_VALUE;
   FileChangeParams fcp;
   memset(&fcp, 0, sizeof(fcp));
   hFile = CreateFileA(filename.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
   if(hFile != INVALID_HANDLE_VALUE)
   {
      BY_HANDLE_FILE_INFORMATION bhfi;
      if (GetFileInformationByHandle(hFile, &bhfi))
      {
         fcp.ftLastWriteTime.dwHighDateTime = bhfi.ftLastWriteTime.dwHighDateTime;
         fcp.ftLastWriteTime.dwLowDateTime = bhfi.ftLastWriteTime.dwLowDateTime;
         fcp.dwFileSizeLow = bhfi.nFileSizeLow;
         fcp.dwFileSizeHigh = bhfi.nFileSizeHigh;
      }
      CloseHandle(hFile);
   }
   return fcp;
}

#endif
