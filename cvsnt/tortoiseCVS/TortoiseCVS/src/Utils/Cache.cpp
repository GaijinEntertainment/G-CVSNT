// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2003 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - March 2003

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
#include "Cache.h"
#include "TortoiseRegistry.h"
#include "PathUtils.h"
#include "StringUtils.h"


////////////////////////////////////////////////////////////////////////
//
// TortoiseRegistryCache


// Constructor
TortoiseRegistryCache::TortoiseRegistryCache(const std::string& storagePath, int maxSize)
   : m_StoragePath(storagePath), m_MaxSize(maxSize)
{
   // Nothing else to do here
}



// Write a vector
void TortoiseRegistryCache::WriteVector(const std::string& key, const std::string& prefix, 
                                        const std::vector<std::string>& values)
{
   // Update LRU list
   MarkAsUsed(key);

   std::string myKey = key;
   MakeLowerCase(myKey);
   FindAndReplace<std::string>(myKey, "\\", "/");

   // Write values
   std::string regPath = EnsureTrailingDelimiter(m_StoragePath) 
      + EnsureTrailingDelimiter(myKey) + prefix;
   TortoiseRegistry::WriteVector(regPath, values);
}



// Write a string
void TortoiseRegistryCache::WriteString(const std::string& key, const std::string& name, 
                                        const std::string& value)
{
   // Update LRU list
   MarkAsUsed(key);

   std::string myKey = key;
   MakeLowerCase(myKey);
   FindAndReplace<std::string>(myKey, "\\", "/");

   // Write values
   std::string regPath = EnsureTrailingDelimiter(m_StoragePath) 
      + EnsureTrailingDelimiter(myKey) + name;
   TortoiseRegistry::WriteString(regPath, value);
}




// Read a vector
void TortoiseRegistryCache::ReadVector(const std::string& key, const std::string& prefix, 
                                       std::vector<std::string>& values)
{
   std::string myKey = key;
   MakeLowerCase(myKey);
   FindAndReplace<std::string>(myKey, "\\", "/");

   std::string regPath = EnsureTrailingDelimiter(m_StoragePath) 
      + EnsureTrailingDelimiter(myKey) + prefix;
   TortoiseRegistry::ReadVector(regPath, values);
}



// Read a string
std::string TortoiseRegistryCache::ReadString(const std::string& key, const std::string& name, 
                                              const std::string& defaultValue, bool *exists)
{
   std::string myKey = key;
   MakeLowerCase(myKey);
   FindAndReplace<std::string>(myKey, "\\", "/");

   std::string regPath = EnsureTrailingDelimiter(m_StoragePath) 
      + EnsureTrailingDelimiter(myKey) + name;
   return TortoiseRegistry::ReadString(regPath, defaultValue, exists);
}



// Shrink cache
void TortoiseRegistryCache::Shrink()
{
   std::string regPath = EnsureTrailingDelimiter(m_StoragePath) + "LRU";
   std::vector<std::string> lruKeys, regKeys;
   TortoiseRegistry::ReadVector(regPath, lruKeys);

   if (lruKeys.size() <= m_MaxSize)
      return;

   // Remove keys from LRU list
   while (lruKeys.size() > m_MaxSize)
   {
      lruKeys.erase(lruKeys.end() - 1);
   }

   // Store keys in map for faster access
   std::vector<std::string>::iterator it = lruKeys.begin();
   std::map<std::string, bool> lruMap;
   while (it != lruKeys.end())
   {
      lruMap.insert(std::pair<std::string, bool>(*it, true));
      it++;
   }


   // Remove invalid keys
   TortoiseRegistry::ReadKeys(m_StoragePath, regKeys);
   std::string myKey;
   while (it != regKeys.end())
   {
      myKey = *it;
      MakeLowerCase(myKey);
      if (lruMap.find(myKey) == lruMap.end())
      {
         TortoiseRegistry::EraseKey(myKey);
      }
      it++;
   }
}



// Update LRU list
void TortoiseRegistryCache::MarkAsUsed(const std::string& key)
{
   std::string myKey = key;
   MakeLowerCase(myKey);
   FindAndReplace<std::string>(myKey, "\\", "/");

   std::string regPath = EnsureTrailingDelimiter(m_StoragePath) + "LRU";
   std::vector<std::string> keys;
   TortoiseRegistry::ReadVector(regPath, keys);

   std::vector<std::string>::iterator it = keys.begin();
   while (it != keys.end())
   {
      if (*it == myKey)
      {
         break;
      }
      it++;
   }

   if (it != keys.end())
   {
      keys.erase(it);
   }
   
   keys.insert(keys.begin(), myKey);

   TortoiseRegistry::WriteVector(regPath, keys);
}
