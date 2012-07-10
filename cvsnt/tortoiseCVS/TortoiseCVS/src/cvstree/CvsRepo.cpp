/*
** Copyright (C) 2005 - March Hare Software Ltd
** <arthur.barrett@march-hare.com> - July 2005

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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits>
#include <sstream>

#include "CvsRepo.h"
#include "../cvstree/common.h"
#include "../Utils/TortoiseDebug.h"
#include "../Utils/TimeUtils.h"

//
// tree nodes
//

CRepoNode::CRepoNode(CRepoNode* r)
{
   root = r;
   next = 0;
   user = 0;
}

CRepoNode::~CRepoNode()
{
   delete Next();
   std::vector<CRepoNode *>::iterator i;
   for(i = Childs().begin(); i != Childs().end(); ++i)
   {
      delete *i;
      *i = 0;
   }
   delete user;
}

static bool insertDirectory(CRepoNode *node, const std::string& modulename, EntnodeDir& direct)
{
   // TODO
   const int maxDirs = 200;
   
   std::vector<CRepoNode*>::iterator i;
   if ((node->GetType() == kNodeModule) || (node->GetType() == kNodeDirectory))
   {
      std::string myname;
      switch (node->GetType())
      {
      case kNodeModule:
         myname = ((CRepoNodeModule*) (node))->c_str();
         break;
      case kNodeDirectory:
         myname = ((CRepoNodeDirectory*) (node))->c_str();
         break;
      default:
         break;
      }
      if(myname == modulename)
      {
         int directoryCount = 0;
         for(i = node->Childs().begin(); i != node->Childs().end(); ++i)
         {
            if( (*i)->GetType() == kNodeDirectory )
               directoryCount ++;
         }

         if(maxDirs == -1 || directoryCount < maxDirs)
         {
            // Insert the directory as a child of the node
            CRepoNodeDirectory *newdirectory = new CRepoNodeDirectory(direct, node);

            std::string name(myname);
            name += newdirectory->name_c_str();
            newdirectory->SetFullname(name);
            node->Childs().push_back(newdirectory);
         }

         return true;
      }
   }

   return false;
}

static bool insertFile(CRepoNode* node, const std::string& directoryname, EntnodeFile& file)
{
   const int maxDirs = 200;
   
   std::vector<CRepoNode*>::iterator i;
   if ((node->GetType() == kNodeModule) || (node->GetType() == kNodeDirectory))
   {
      std::string myname;
      switch (node->GetType())
      {
      case kNodeModule:
         myname = ((CRepoNodeModule*) (node))->c_str();
         break;
      case kNodeDirectory:
         myname = ((CRepoNodeDirectory*) (node))->str();
         break;
      default:
         break;
      }
      if (myname == directoryname)
      {
         int fileCount = 0;
         for(i = node->Childs().begin(); i != node->Childs().end(); ++i)
         {
            if( (*i)->GetType() == kNodeFile )
               fileCount ++;
         }

         if(maxDirs == -1 || fileCount < maxDirs)
         {
            // Insert the directory as a child of the node
            CRepoNodeFile *newfile = new CRepoNodeFile(file, node);
            std::string name(myname);
            name += newfile->name_c_str();
            newfile->SetFullname(name);
            node->Childs().push_back(newfile);
         }

         return true;
      }
   }

   return false;
}

// creates each of the nodes using the parserData
CRepoNode* CvsRepoGraph(RepoParserData& repoParserData,
                        CvsRepoParameters* settings, CRepoNode* header)
{
   TDEBUG_ENTER("CvsRepoGraph");
   if (repoParserData.entries.empty())
       return 0;

   EntnodeDir *myentnodedir = new EntnodeDir("/", "mydir");
   CRepoNodeModule* myheader = new CRepoNodeModule(*myentnodedir);
   if (!header)
      header = myheader;

   // Append the entries to the tree
   EntnodeMap::iterator i;
   for (i = repoParserData.entries.begin(); i != repoParserData.entries.end(); ++i)
   {
      EntnodeData* ent = i->second.Data();
      
      if (ent->GetType() == ENT_SUBDIR)
      {
         CRepoNodeDirectory* ndir = new CRepoNodeDirectory(*reinterpret_cast<EntnodeDir*>(ent), header);
         header->Childs().push_back(ndir);
      }
      else
      {
         CRepoNodeFile* nfil = new CRepoNodeFile(*reinterpret_cast<EntnodeFile*>(ent), header);
         header->Childs().push_back(nfil);
      }
   }

   return header;
}

