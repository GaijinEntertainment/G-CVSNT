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
#include "TortoiseUtils.h"
#include "TortoiseRegistry.h"
#include "PathUtils.h"
#include "ShellUtils.h"
#include "StringUtils.h"
#include "Translate.h"
#include "TortoiseDebug.h"
#include <windows.h>
#include "FixWinDefs.h"
#include <fstream>
#include <stdio.h>


std::string GetTortoiseDirectory()
{
   return TortoiseRegistry::ReadString("RootDir");
}

// Get environment variable
std::string GetEnvVar(const std::string& name)
{
   DWORD dwSize = 0;
   char *buf = 0;
   dwSize = GetEnvironmentVariableA(name.c_str(), buf, dwSize);
   if (dwSize != 0)
   {
      buf = new char[dwSize];
      GetEnvironmentVariableA(name.c_str(), buf, dwSize);
      std::string s(buf);
      delete buf;
      return s;
   }
   else
   {
      return "";
   }
}


// Set environment variable
void SetEnvVar(const std::string& name, const std::string& value)
{
   SetEnvironmentVariableA(name.c_str(), value.c_str());
}


// Schedule a command for execution
bool ScheduleCommand(const std::string& sName, const std::string& sCommand)
{
   TDEBUG_ENTER("ScheduleCommand");
   bool result = true;
   HKEY key = 0;
   if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows"
                       "\\CurrentVersion\\RunOnce", 0, 0, 0, KEY_READ | KEY_WRITE, 0, &key, 0) != ERROR_SUCCESS)
   {
      TDEBUG_TRACE("Failed to open RunOnce key");
      result = false;
   }
   else
   {
      if (RegSetValueExA(key, sName.c_str(), 0, REG_SZ, 
                         (LPBYTE) sCommand.c_str(), static_cast<DWORD>(sCommand.length() + 1)) != ERROR_SUCCESS)
      {
         TDEBUG_TRACE("Failed to store command");
         result = false;
      }
   }
   if (key)
      RegCloseKey(key);
   return result;
}



// Get default text editor command
std::string GetTextEditorCommand(const char* filename)
{
   std::string editor = 
      TortoiseRegistry::ReadString("\\HKEY_CLASSES_ROOT\\txtfile\\shell\\open\\command\\");
   
   if (editor.find("%1") != std::string::npos)
   {
      FindAndReplace<std::string>(editor, "%1", Quote(filename));
   }
   return editor;
}


bool GetCalculatedHomeDirectory(std::string& HomeDirectory)
{
   TDEBUG_ENTER("GetCalculatedHomeDirectory");
   std::string home("");

   // Try home environment variable
   const char* got = getenv("HOME");
   if (got)
      home = got;
   else
   {
      got = getenv("HOMEDRIVE");
      if (got)
      {
         home = got;
         got = getenv("HOMEPATH");
         if (got)
            home += got;
      }
   }

   if (home.empty() || !IsDirectory(home.c_str()))
   {
      // Try WinCVS's home directory
      home = TortoiseRegistry::ReadWinCVSString("P_Home");
   }
   if (home.empty() || !IsDirectory(home.c_str()))
   {
      // Try My Documents
      home = GetSpecialFolder(CSIDL_PERSONAL);
   }

   // Cope with Win98 trouble with C:\ style paths by stripping slash
   // (that top level drive letter paths are special annoys me)
   if (home.size() == 3)
   {
      if (home[1] == ':' && (home[2] == '/' || home[2] == '\\'))
         home = home.substr(0, 2);
   }

   TDEBUG_TRACE("Calculated home directory is " << home);
 
   bool SuccessValue(!home.empty() && IsDirectory(home.c_str()));
   if (SuccessValue)
      HomeDirectory = home;

   return SuccessValue;
}
