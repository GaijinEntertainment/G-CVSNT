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



// Migrates .cvspass file from home directory to registry
// .cvspass cached the :pserver: protocol passwords in older
// versions of CVS.  CVSNT stores them in the registry.

// TortoiseCVS version of PostInst executable from CVSNT
// (uses the TortoiseCVS home directory instead of $HOME)

#include "StdAfx.h"
#include <string>
#include <iomanip>
#include <limits>

#include <Utils/TortoiseRegistry.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/StringUtils.h>
#include <Utils/PathUtils.h>

using namespace std;

static void MigrateCvsPass();
static void MigrateRegVectors();
static void MigratePreferences();

int APIENTRY WinMain(HINSTANCE /* hInstance */,
                     HINSTANCE /* hPrevInstance */,
                     LPSTR     /* lpCmdLine */,
                     int       /* nCmdShow */)
{
   MigrateCvsPass();
   MigrateRegVectors();
   MigratePreferences();
   return 0;
}

static bool WriteRegistryKey(LPCSTR key, LPCSTR value, LPCSTR buffer)
{
   HKEY hKey,hSubKey;
   DWORD dwLen;

   if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Cvsnt", 0, KEY_READ, &hKey) &&
       RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Cvsnt", 0, NULL, 0, KEY_READ, NULL, &hKey, NULL))
   {
      return false; // Couldn't open or create key
   }
   
   if (key)
   {
      if (RegOpenKeyExA(hKey, key, 0, KEY_WRITE, &hSubKey) &&
          RegCreateKeyExA(hKey, key, 0, NULL, 0, KEY_WRITE, NULL, &hSubKey, NULL))
      {
         RegCloseKey(hKey);
         return false; // Couldn't open or create key
      }
      RegCloseKey(hKey);
      hKey = hSubKey;
   }

   if (!buffer)
   {
      RegDeleteValueA(hKey, value);
   }
   else
   {
      dwLen = strlen(buffer);
      if (RegSetValueExA(hKey, value, 0, REG_SZ, (LPBYTE) buffer, dwLen+1))
      {
         RegCloseKey(hKey);
         return false;
      }
   }
   RegCloseKey(hKey);

   return true;
}

void MigrateCvsPass()
{
   std::string path;
   bool GotHomeDir(GetCalculatedHomeDirectory(path));

   if (!GotHomeDir || path.empty())
      return; /* Nothing to migrate */

   path = EnsureTrailingDelimiter(path);
   path += ".cvspass";
   FILE *f = fopen(path.c_str(),"r");
   if (!f)
      return; /* No .cvspass file */

   char line[1024], *key = 0, *pw = 0;
   while (fgets(line, 1024, f) > 0)
   {
      line[strlen(line)-1] = '\0';
      // Prevent strtok() crash on empty string
      if (!strlen(line))
         break;
      key = strtok(line," ");
      if (key[0] == '/') /* This was added in 1.11.1.  Not sure why. */
         key = strtok(NULL, " ");
      if (key)
         pw = key + strlen(key) + 1;
      if (key && pw)
         WriteRegistryKey("cvspass", key, pw);
   }

   fclose(f);

   /* Should we delete here?  At the moment I'm assuming people are mixing
      legacy cvs with cvsnt.. perhaps later */
}


// Read registry vector in old format
static bool ReadOldVector(std::string const& basekey,
                          std::vector<std::string>& strings)
{
   int i = 0;
   while (i < 999)
   {
      std::stringstream ss;
      ss << basekey;
      ss << std::setw(4) << std::setfill('0') << i << std::ends;
      bool exists;
      std::string str = TortoiseRegistry::ReadString(ss.str(), "", &exists);
      if (!exists)
      {
          if (!i)
              return false;
          break;
      }
      strings.push_back(str);
      ++i;
   }
   return true;
}


// Read registry vector in old format
static bool ReadOldVector(std::string const& basekey,
                          std::vector<int>& ints)
{
   int i = 0;
   int def = std::numeric_limits<int>::min();
   while (i < 999)
   {
      std::stringstream ss;
      ss << basekey;
      ss << std::setw(4) << std::setfill('0') << i << std::ends;
      int value = 0;
      if (!TortoiseRegistry::ReadInteger(ss.str(), value))
      {
          if (!i)
              return false;
          break;
      }
      ints.push_back(value);
      ++i;
   }
   return true;
}


static void MigrateVectorStr(const std::string& name_old, const std::string& name_new)
{
   std::vector<std::string> v;
   // Read new vector
   if (!TortoiseRegistry::ReadVector(name_new, v))
   {
       if (ReadOldVector(name_old, v))
           TortoiseRegistry::WriteVector(name_new, v);
   }
}


static void MigrateVectorInt(const std::string& name_old, const std::string& name_new)
{
   std::vector<int> v;
   // Read new vector
   if (!TortoiseRegistry::ReadVector(name_new, v))
   {
       if (ReadOldVector(name_old, v))
           TortoiseRegistry::WriteVector(name_new, v);
   }
}


static void MigrateRegVectors()
{
   MigrateVectorStr("Checkout CVSROOT History ", "History\\Checkout CVSROOT");
   MigrateVectorStr("Checkout CVSROOT Module History ", "History\\Checkout CVSROOT Module");
   MigrateVectorInt("HistoryColumnWidth ", "Dialogs\\History\\Column Widths");

   MigrateVectorStr("Checkout Directory History ", "History\\Checkout Directory");
   MigrateVectorStr("Checkout Server History ", "History\\Checkout Server");
   MigrateVectorStr("Checkout Port History ", "History\\Checkout Port");
   MigrateVectorStr("Checkout Module History ", "History\\Checkout Module");
   MigrateVectorStr("Checkout Username History ", "History\\Checkout Username");
   MigrateVectorInt("Checkout Protocol History ", "History\\Checkout Protocol");
   
   MigrateVectorStr("Revision Date History ", "History\\Revision Date");

   MigrateVectorStr("Comment History ", "History\\Comments");

   MigrateVectorStr("Preferences\\Included Directories\\Dir ", "Prefs\\Included Directories\\_");
   MigrateVectorStr("Preferences\\Excluded Directories\\Dir ", "Prefs\\Excluded Directories\\_");

   MigrateVectorStr("Icons ", "Icons");
}

struct Pre1912Preference
{
   const char* name;
   bool        numeric;
};

static const Pre1912Preference Pre1912Preferences[] =
{
   { "External Merge Application",      false },
   { "External Diff Application",       false },
   { "Close Automatically",             true  },
   { "Quietness",                       true  },
   { "Always Recalculate Home",         true  },
   { "HOME",                            false },
   { "External Diff2 Params",           false },
   { "External Merge2 Params",          false },
   { "External SSH Application",        false },
   { "External SSH Params",             false },
   { "Recursive folder overlays",       true  },
   { "Autoload folder overlays",        true  },
   { "GraphMaxTags",                    true  },
   { "SandboxType",                     true  },
   { "Prune Empty Directories",         true  },
   { "Create Added Directories",        true  },
   { "Force Head Revision",             true  },
   { "Simulate update",                 true  },
   { "Automatic unedit",                true  },
   { "Automatic commit",                true  },
   { "Allow Network Drives",            true  },
   { "Allow Removable Drives",          true  },
   { "Add Ignored Files",               true  },
   { "Compression Level",               true  },
   { "LanguageIso",                     false },
   { "ContextIcons",                    true  },
   { 0,  false },
};

static void MigratePreferences()
{
   // 1.9.1
   if (TortoiseRegistry::ReadBoolean("Automatic unedit", false))
      TortoiseRegistry::WriteInteger("Edit policy", 2);
   // 1.9.12 - global and local preferences
   const Pre1912Preference* p = Pre1912Preferences;
   while (p->name)
   {
      std::string key("Prefs\\");
      key += p->name;
      key += "\\_";
      if (p->numeric)
      {
         int val = 0;
         if (TortoiseRegistry::ReadInteger(p->name, val))
             TortoiseRegistry::WriteInteger(key, val);
      }
      else
      {
          bool exists = false;
          std::string val = TortoiseRegistry::ReadString(p->name, "", &exists);
          if (exists)
              TortoiseRegistry::WriteString(key, val);
      }
      TortoiseRegistry::EraseValue(p->name);
      ++p;
   }
}
      
