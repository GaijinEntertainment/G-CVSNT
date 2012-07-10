//  Copyright (C) 2005 - March Hare Software Ltd
//  <arthur.barrett@march-hare.com> - July 2005

//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 1, or (at your option)
//  any later version.

//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.

//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


#ifndef CVSREPO_H
#define CVSREPO_H

#ifdef _DEBUG
#   define qCvsDebug 1
#else
#   define qCvsDebug 0
#endif

#include "../CvsGlue/CvsEntries.h"

#include <vector>
#include <time.h>
#include <stdio.h>

#include <iostream>

// classes used to build a tree off the cvs ls output

enum kRepoNode
{
   kNodeModule,
   kNodeDirectory,
   kNodeFile,
   kNodeRubbish,
};

class CRepoNode;

class CRepoNodeData
{
public:
   CRepoNodeData(CRepoNode* node) { fNode = node; }
   virtual ~CRepoNodeData() {}

   inline CRepoNode* Node(void) { return fNode; }

protected:
   CRepoNode* fNode;
};

class CRepoNode
{
public:
   CRepoNode(CRepoNode*  root = 0);
   virtual ~CRepoNode();

   virtual kRepoNode GetType(void) const = 0;

   typedef std::vector<CRepoNode*> ChildList;
   
   inline const ChildList& Childs(void) const { return childs; }
   inline ChildList& Childs(void) { return childs; }

   inline const CRepoNode* Root(void) const { return root; }
   inline CRepoNode*& Root(void) { return root; }

   inline const CRepoNode*  Next(void) const { return next; }
   inline CRepoNode*& Next(void) { return next; }

   inline void SetUserData(CRepoNodeData* data) { user = data; }
   inline CRepoNodeData* GetUserData(void) { return user; }

protected:
   ChildList childs;
   CRepoNode* root;
   CRepoNode* next;
   CRepoNodeData* user;
};

class CRepoNodeModule : public CRepoNode
{
public:
   CRepoNodeModule(const EntnodeDir& m, CRepoNode* r = 0)
      : CRepoNode(r),
        module(m)
   {
   }

   virtual kRepoNode GetType(void) const { return kNodeModule; }

   inline const EntnodeDir& operator*() const { return module; }
   inline EntnodeDir& operator*() { return module; }

   inline const char* c_str() const { return module.GetName(); }

protected:
   EntnodeDir module;
};

class CRepoNodeRubbish : public CRepoNode
{
public:
   CRepoNodeRubbish(CRepoNode* r = 0)
      : CRepoNode(r)
   {
   }

   virtual kRepoNode GetType(void) const { return kNodeRubbish; }

   inline const char* c_str() const { return "rubbish\0"; }

};

class CRepoNodeDirectory : public CRepoNode
{
public:
   CRepoNodeDirectory(const EntnodeDir& d, CRepoNode* r = 0)
      : CRepoNode(r),
        directory(d)
   {
   }

   virtual kRepoNode GetType(void) const { return kNodeDirectory; }

   inline const EntnodeDir& operator*() const { return directory; }
   inline EntnodeDir& operator*() { return directory; }

   inline const char* name_c_str() const { return directory.GetName(); }
   inline const char* c_str() const { return fullname.c_str(); }
   inline const std::string& str() const { return fullname; }
   inline void SetFullname(std::string& myfullname) { fullname = myfullname; }
   
protected:
   EntnodeDir     directory;
   std::string    fullname;
};

class CRepoNodeFile : public CRepoNode
{
public:
   CRepoNodeFile(const EntnodeFile& f, CRepoNode* r = 0)
      : CRepoNode(r),
        file(f)
   {
   }

   virtual kRepoNode GetType(void) const { return kNodeFile; }

   inline const EntnodeFile& operator*() const { return file; }
   inline EntnodeFile& operator*() { return file; }

   inline const char* name_c_str() const { return file.GetName(); }
   inline const char* c_str() const { return fullname.c_str(); }
   inline const std::string& str() const { return fullname; }
   inline void SetFullname(std::string& myfullname) { fullname = myfullname; }
   
protected:
   EntnodeFile file;
   std::string    fullname;
};

// parse an "cvs ls -e" and return the set of CVS Entries
std::vector<EntnodeData>& CvsRepoParse(FILE* file);

// free the memory used
void CvsRepoReset(void);

struct CvsRepoParameters
{
   CvsRepoParameters(const std::string& path)
      : myPath(path)
   {
   }

   const std::string& myPath;
};

// build a tree (i.e. graph) of the directory listing...
CRepoNode* CvsRepoGraph(struct RepoParserData& repoParserData,
                        CvsRepoParameters* settings = 0, CRepoNode* header = 0);

#endif
