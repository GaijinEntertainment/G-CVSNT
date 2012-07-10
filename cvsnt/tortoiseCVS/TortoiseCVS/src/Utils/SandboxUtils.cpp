// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2008 - Torsten Martinsen
// <torsten@bullestock.net> - January 2008

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

#include <Utils/Preference.h>
#include <Utils/PathUtils.h>
#include <Utils/SandboxUtils.h>


#define FF_UNKNOWN 0
#define FF_UNIX 1
#define FF_DOS 2

static int GetTextFileFormat(const char* file);


// Check if CVS/Root has Unix line endings
bool IsUnixSandbox(const std::string& dir)
{
    TDEBUG_ENTER("IsUnixSandbox");
    int sandboxType = GetIntegerPreference("SandboxType", dir);
    TDEBUG_TRACE("sandboxType " << sandboxType);
    
    if (sandboxType == 2)  // DOS
        return false;
    if (sandboxType == 3)  // UNIX
        return true;

    // try autodetection
    if (CVSDirectoryHere(dir))
    {
        TDEBUG_TRACE("Checking " << dir);
        std::string mydir = EnsureTrailingDelimiter(dir);
        std::string rootFile = mydir + "CVS\\Root";
        if (FileExists(rootFile.c_str()))
        {
            int fileFormat = GetTextFileFormat(rootFile.c_str());
            TDEBUG_TRACE("fileFormat " << fileFormat);
            if (fileFormat == FF_UNIX)
                return true;

            if (fileFormat == FF_DOS)
                return false;
        }
    }

    if (sandboxType == 0)  // auto / DOS
        return false;

    if (sandboxType == 1)  // auto / UNIX
        return true;

    return false;
}


static int GetTextFileFormat(const char* file)
{
   char buf[8192];
   DWORD dwRead;
   int result = FF_UNKNOWN;
   unsigned int i;
   // open file
   HANDLE hFile = CreateFileA(file, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 
                              0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

   if (hFile == INVALID_HANDLE_VALUE)
      return result;
   
   while (ReadFile(hFile, buf, sizeof(buf), &dwRead, 0))
   {
      for (i = 0; i < dwRead; i++)
      {
         if (buf[i] == '\n')
         {
            result = FF_UNIX;
            break;
         }
         if (buf[i] == '\r')
         {
            result = FF_DOS;
            break;
         }
      }

      if (result != FF_UNKNOWN)
         break;

      if (dwRead == 0)
         break;
   }

   if (hFile != INVALID_HANDLE_VALUE)
      CloseHandle(hFile);

   return result;
}
