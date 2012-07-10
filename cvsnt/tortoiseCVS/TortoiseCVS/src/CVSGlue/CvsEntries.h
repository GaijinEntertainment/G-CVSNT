/*
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 1, or (at your option)
** any later version.

** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.

** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
 * Author : Alexandre Parenteau <aubonbeurre@hotmail.com> --- April 1998
 */

/*
 * CvsEntries.h --- adaptation from cvs/src/entries.c
 */

#ifndef CVSENTRIES_H
#define CVSENTRIES_H


#include "../Utils/FixCompilerBugs.h"

#include "Stat.h"
#include <map>
#include <locale>
#include <string>
#include <vector>
#include <ctype.h>

#include <wx/string.h>

#include "CvsIgnore.h"

typedef enum
{
   ENT_FILE,
   ENT_SUBDIR
} ent_type;

class EntnodeData
{
public:
   EntnodeData(ent_type atype)
      : ref(1),
        type(atype),
        missing(false),
        visited(false),
        unknown(false),
        unmodified(true),
        added(false),
        needsMerge(false),
        ignored(false),
        locked(false),
        removed(false),
        renamed(false),
        readonly(false)
   {
   }
   
   virtual ~EntnodeData();

   enum
   {
      kName = 0,
      kStatus = 3,
      kVN = 1,
      kTS = 5,
      kOption = 2,
      kTag = 4,
      kConflict = 6,
      kTagOnly = 7,
      kDateOnly = 8,
      kBugOnly = 9
   };

   inline ent_type GetType(void) const {return type;}
   inline static int Compare(const EntnodeData& node1, const EntnodeData& node2)
   {
      return stricmp(node1.user.c_str(), node2.user.c_str());
   }
   inline EntnodeData* Ref(void) {++ref; return this;}
   inline EntnodeData* UnRef(void)
   {
      if (--ref == 0)
      {
         delete this;
         return 0;
      }
      return this;
   }

   virtual const char* GetVN() const
   {
       return 0;
   }
    
   virtual const char* GetTS() const
   {
       return 0;
   }
    
   virtual const char* GetOption() const
   {
       return 0;
   }
    
   virtual const char* GetTag() const
   {
       return 0;
   }
    
   virtual const char* GetConflict() const
   {
       return 0;
   }
    
   virtual const char* GetTagOnly() const
   {
       return 0;
   }
    
   virtual const char* GetDateOnly() const
   {
       return 0;
   }
    
   virtual const char* GetBug() const
   {
       return 0;
   }
    
   virtual const char* GetName() const
   {
       return user.c_str();
   }
   
   virtual const wxChar* GetStatus() const
   {
       return desc.c_str();
   }

   inline void SetMissing(bool state)           { missing = state; }
   inline bool IsMissing(void) const            { return missing; }
   inline void SetVisited(bool state)           { visited = state; }
   inline bool IsVisited(void) const            { return visited; }
   inline void SetUnknown(bool state)           { unknown = state; }
   inline bool IsUnknown(void) const            { return unknown; }
   inline void SetIgnored(bool state)           { ignored = state; }
   inline bool IsIgnored(void) const            { return ignored; }
   inline void SetLocked(bool state)            { locked = state; }
   inline bool IsLocked(void) const             { return locked; }
   inline void SetRemoved(bool state)           { removed = state; }
   inline bool IsRemoved(void) const            { return removed; }
   inline void SetRenamed(bool state)           { renamed = state; }
   inline bool IsRenamed(void) const            { return renamed; }
   void SetDesc(const wxChar* newdesc);
   inline const wxChar* GetDesc(void) const    { return desc.c_str(); }
   inline void SetUnmodified(bool state)        { unmodified = state; }
   inline bool IsUnmodified(void) const         { return unmodified; }
   inline void SetAdded(bool state)             { added = state; }
   inline bool IsAdded(void) const              { return added; }
   inline void SetNeedsMerge(bool state)        { needsMerge = state; }
   inline bool NeedsMerge(void) const           { return needsMerge; }
   inline int IsBugNumber(std::string inbug) const           { return inbug.compare(bugnumber); }
   inline void SetReadOnly(bool state)          { readonly = state; }
   inline bool IsReadOnly(void) const           { return readonly; }

protected:
   int ref;
   ent_type type;
   std::string user;
   wxString desc;
   std::string bugnumber;
   bool missing;
   bool visited;
   bool unknown;
   bool unmodified;
   bool added;
   bool needsMerge;
   bool ignored;
   bool locked;
   bool removed;
   bool renamed;
   bool readonly;
};

class EntnodeFile : public EntnodeData
{
public:
   EntnodeFile(const char* newfullpath, const char* newuser, const char* newvn = 0,
               const char* newts = 0, const char* newoptions = 0,
               const char* newtag = 0, const char* newdate = 0,
               const char* newts_conflict = 0,
               bool renamed = false,
               const std::string& newbugno = "");

   virtual ~EntnodeFile() {}

    virtual const char* GetVN() const
    {
        return vn.c_str();
    }
    
    virtual const char* GetTS() const
    {
        return ts.c_str();
    }
    
    virtual const char* GetOption() const
    {
        return option.c_str();
    }
    
    virtual const char* GetTag() const
    {
        return tag.empty() ? date.c_str() : tag.c_str();
    }
    
    virtual const char* GetConflict() const
    {
        return ts_conflict.c_str();
    }
    
    virtual const char* GetTagOnly() const
    {
        return tag.c_str();
    }
    
    virtual const char* GetDateOnly() const
    {
        return date.c_str();
    }
    
    virtual const char* GetBug() const
    {
        return bugnumber.c_str();
    }
    

protected:
   std::string vn;
   std::string ts;
   std::string option;
   std::string tag; // can be nil
   std::string date; // can be nil
   std::string ts_conflict; // can be nil
};

class Tagnode
{
public:
    Tagnode(const char* newfullpath, const char* newuser,
            const std::string& newbugno = "");

    virtual ~Tagnode();

    virtual const char* GetTag() const;
    
    virtual const char* GetTagOnly() const;
    
    virtual const char* GetDateOnly() const;
    

protected:
    // Load cache
    void PrepareGet() const;

    std::string user;
    std::string fullpath;

    mutable bool alreadyChecked;
    mutable std::string tag; // from CVS/Tag - can be nil
    mutable std::string date; // from CVS/Tag - can be nil
};

class EntnodeDir : public EntnodeData
{
public:
   EntnodeDir(const char* newfullpath, const char* newuser, const char* newvn = 0,
              const char* newts = 0, const char* newoptions = 0, const char* newtag = 0,
              const char* newdate = 0, const char* newts_conflict = 0);

   virtual ~EntnodeDir()
   {
      delete tagnode;
   }

    virtual const char* EntnodeDir::GetTag() const;
    
    virtual const char* EntnodeDir::GetTagOnly() const;

    virtual const char* EntnodeDir::GetDateOnly() const;

protected:
    Tagnode* tagnode; // for sticky tag info
};

class EntnodeOuterDir : public EntnodeDir
{
public:
   EntnodeOuterDir(const char* newfullpath, const char* newuser, const char* newvn = 0,
                   const char* newts = 0, const char* newoptions = 0, const char* newtag = 0,
                   const char* newdate = 0, const char* newts_conflict = 0);
};

// ENTNODE gets a node file or a node dir
// and reference it.
class ENTNODE
{
public:
   ENTNODE() : shareData(0) {}
   ENTNODE(EntnodeData* data) : shareData(data->Ref()) {}
   ENTNODE(EntnodeData& data) : shareData(data.Ref()) {}
   ENTNODE(const ENTNODE& anode) : shareData(0)
   {
      *this = anode;
   }
   ~ENTNODE()
   {
      if (shareData)
         shareData->UnRef();
   }

   inline static int Compare(const ENTNODE& node1, const ENTNODE& node2)
   {
      return EntnodeData::Compare(*node1.shareData, *node2.shareData);
   }

   inline ENTNODE& operator=(const ENTNODE& anode)
   {
      if (shareData)
      {
         shareData->UnRef();
         shareData = 0;
      }
      if (anode.shareData)
         shareData = ((ENTNODE*)& anode)->shareData->Ref();
      return *this;
   }

   inline EntnodeData* Data(void) {return shareData;}
protected:
   EntnodeData* shareData;
};

// Function object to compare strings in a case-insensitive manner
class StringNoCaseCmp
{
public:
    bool operator() (const std::string& s1, const std::string& s2) const
    {
       return std::lexicographical_compare(s1.begin(), s1.end(),
                                           s2.begin(), s2.end(),
                                           nocase_compare);
    }

private:
    // Auxiliary function to compare case insensitive
    static bool nocase_compare(char c1, char c2)
    {
        return toupper(c1) < toupper(c2);
    }
};

typedef std::map<std::string, ENTNODE, StringNoCaseCmp> EntnodeMap;
typedef std::map<std::string, EntnodeData*, StringNoCaseCmp> EntnodeDataMap;

// Returns false if no CVS/Entries
bool Entries_Open(EntnodeMap& entries, const char* fullpath,
                  FileChangeParams* fcp = 0);

// Fill an ENTNODE when the file appears on the disk: Sets
// some flags like "visited", "unknown", "ignored"... and returns
// a reference to the node.
EntnodeData* Entries_SetVisited(const char* path, EntnodeMap& entries, const char* name,
                                const struct stat& finfo, bool isDir, bool isReadOnly,
                                bool isMissing, const std::vector<std::string>* ignlist = 0);

// Returns false if no CVS/Tag
bool Tag_Open(std::string& tag, const char* fullpath, char* cmd = 0);

#endif /* CVSENTRIES_H */
