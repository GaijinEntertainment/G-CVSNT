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

#ifndef TORTOISE_REGISTRY_H
#define TORTOISE_REGISTRY_H


#include "FixCompilerBugs.h"

#include <windows.h>
#include "FixWinDefs.h"
#include <string>
#include <vector>

class TortoiseRegistry
{
public:
    static const int MAX_TORTOISE_REGISTRY_VECTOR_SIZE = 999;


    static void Init();

    // If you don't specify a default, ReadString() returns "" if entry not found.
    static std::string ReadString(const std::string& name, const std::string& defaultValue = "", bool* exists = 0);

#ifndef POSTINST
    static wxString ReadWxString(const std::string& name, const wxString& defaultValue = wxT(""), bool* exists = 0);
#endif

    static void WriteString(const std::string& name, const std::string& value);
#if wxUSE_UNICODE
    static void WriteString(const std::string& name, const wxString& value);
#endif
    
   static bool ReadInteger(const std::string& name, int& value);
   static void WriteInteger(const std::string& name, int value);

   static bool ReadBoolean(const std::string& name, bool defaultValue);
   static bool ReadBoolean(const std::string& name, bool defaultValue, bool& exists);
   static void WriteBoolean(const std::string& name, bool value);

   static bool ReadVector(const std::string& name,
                          std::vector<std::string>& strings);

   static void WriteVector(std::string const& name,
                           std::vector<std::string> const& strings,
                           int maxsize = MAX_TORTOISE_REGISTRY_VECTOR_SIZE);
   static void PushBackVector(std::string const& name,
                              const std::string& value);
   static void InsertFrontVector(std::string const& name,
                                 const std::string& value,
                                 unsigned int maxsize);

   static bool ReadVector(std::string const& name,
                          std::vector<int>& ints);
   static void WriteVector(std::string const& name,
                           std::vector<int> const& ints);
   
   static bool ReadSize(std::string const& name, int* width, int* height);

   static void WriteSize(std::string const& name, int width, int height);

   static void EraseValue(std::string const& basekey);

   static void ReadValueNames(const std::string& basekey, std::vector<std::string>& names);

#ifndef POSTINST
   static void ReadValueNames(const std::string& basekey, std::vector<wxString>& names);
#endif
    
   static void ReadKeys(const std::string& basekey, std::vector<std::string>& keys);
   
   static void EraseKey(const std::string& name);

   static bool ParseRegistryPath(const std::string& path, HKEY*& rootkeys, 
                                 int* count, std::string& key, std::string& name);

   // WinCVS compatibility
   static std::string ReadWinCVSString(const std::string& name, const std::string& defaultValue = "");

private:
    static DWORD ourFlag64;
};

#endif
