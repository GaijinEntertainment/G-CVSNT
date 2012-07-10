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
#ifndef CACHE_UTILS_H
#define CACHE_UTILS_H

#include "FixCompilerBugs.h"

#include <map>
#include <list>
#include <string>
#include <vector>
#include <windows.h>
#include "FixWinDefs.h"


template <class Key, class Value> class TortoiseMemoryCache
{
public:
   // Constructor
   TortoiseMemoryCache(unsigned int maxSize = 20) 
      : m_MaxSize(maxSize), m_Size(0) {};

   // Destructor
   ~TortoiseMemoryCache() { Clear(); }

   // Insert key / value pair
   Value* Insert(const Key &key);

   // Find a cache element
   Value* Find(const Key &key);

   // Remove specific element from cache
   void Remove(const Key &key);

   // Clear the cache
   void Clear();

private:
   class Entry;
   typedef typename std::map<Key, Entry*> CacheMap;
   typedef typename CacheMap::value_type CacheValue;
   typedef typename std::map<Key, Entry*>::iterator CacheMapIterator;
   typedef typename std::list<Entry*> CacheStack;
   // Cache map entry
   class Entry
   {
   public:
      Entry() : m_Value(0) {}
      // Reference to value
      Value* m_Value;
      // Reference to map
      typename CacheMap::iterator m_ItMap;
      // Reference to stack
      typename CacheStack::iterator m_ItStack;
   };

   // Shrink cache size
   void Shrink();

   // Remove specific entry from map and stack
   void RemoveEntry(Entry *e);

   // Mark entry as used
   void MarkAsUsed(Entry *e);

   // map
   CacheMap m_Map;

   // lru stack
   CacheStack m_Stack;

   // size
   unsigned int m_MaxSize;
   unsigned int m_Size;
};


class TortoiseRegistryCache
{
public:
    // Constructor
    TortoiseRegistryCache(const std::string& storagePath, int maxSize);

    // Write a vector
    void WriteVector(const std::string& key, const std::string& prefix, 
                     const std::vector<std::string>& values);

    // Write a string
    void WriteString(const std::string& key, const std::string& name, 
                     const std::string& value);

    // Read a vector
    void ReadVector(const std::string& key, const std::string& prefix, 
                     std::vector<std::string>& values);

    // Read a string
    std::string ReadString(const std::string& key, const std::string& name, 
                           const std::string& defaultValue = "", bool *exists = 0);

    // Shrink cache
    void Shrink();

private:
   // Update LRU list
   void MarkAsUsed(const std::string& key);
   
   // storage path
   std::string m_StoragePath;

   // max size
   unsigned int m_MaxSize;
};



// Insert key / value pair
template <class Key, class Value> 
Value* TortoiseMemoryCache<Key, Value>::Insert(const Key &key)
{
   Entry *e;
   // See if element with specified key already exists
   CacheMapIterator it = m_Map.find(key);
   if (it != m_Map.end())
   {
      e = it->second;  

      // mark entry as used
      MarkAsUsed(e);

      delete e->m_Value;
      e->m_Value = new Value();
      return e->m_Value;
   }
   
   // Element not in cache => remove old elements if cache is full
   while (m_Size >= std::max(m_MaxSize, (unsigned int) 1))
   {
      Shrink();
   }

   // Create new entry
   e = new Entry();
   e->m_Value = new Value();

   e->m_ItMap = m_Map.insert(CacheValue(key, e)).first;
   e->m_ItStack = m_Stack.insert(m_Stack.begin(), e);
   m_Size++;
   return e->m_Value;
}



// Clear the cache
template <class Key, class Value> 
void TortoiseMemoryCache<Key, Value>::Clear()
{
   CacheMapIterator it = m_Map.begin();
   while (it != m_Map.end())
   {
      delete it->second->m_Value;
      delete it->second;
      it++;
   }

   m_Map.clear();
   m_Stack.clear();
   m_Size = 0;
}



// Shrink cache size
template <class Key, class Value> 
void TortoiseMemoryCache<Key, Value>::Shrink()
{
   Entry *e;
   if (m_Size == 0)
      return;

   // remove elements from end of stack and from map
   while (m_Size >= m_MaxSize)
   {
      e = m_Stack.back();
      RemoveEntry(e);
   }
}



// Remove specific element from cache
template <class Key, class Value> 
void TortoiseMemoryCache<Key, Value>::Remove(const Key &key)
{
   CacheMapIterator it = m_Map.find(key);
   if (it != m_Map.end())
   {
      RemoveEntry(it->second);
   }
}



// Find a cache element
template <class Key, class Value> 
Value* TortoiseMemoryCache<Key, Value>::Find(const Key &key)
{
   CacheMapIterator it = m_Map.find(key);
   if (it == m_Map.end())
   {
      return 0;
   }
   else
   {
      MarkAsUsed(it->second);
      return it->second->m_Value;
   }
}



// Remove specific entry from map and stack
template <class Key, class Value> 
void TortoiseMemoryCache<Key, Value>::RemoveEntry(Entry *e)
{
   if (e)
   {
      delete e->m_Value;
      m_Map.erase(e->m_ItMap);
      m_Stack.erase(e->m_ItStack);
      delete e;
      m_Size--;
   }
}



// Mark entry as used
template <class Key, class Value> 
void TortoiseMemoryCache<Key, Value>::MarkAsUsed(Entry *e)
{
   if (e)
   {
      m_Stack.erase(e->m_ItStack);
      e->m_ItStack = m_Stack.insert(m_Stack.begin(), e);
   }
}



#endif
