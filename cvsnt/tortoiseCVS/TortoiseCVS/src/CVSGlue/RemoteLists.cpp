// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2002 - Francis Irving
// <francis@flourish.org> - June 2002

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
#include "../Utils/TortoiseDebug.h"
#include "../Utils/Translate.h"

#include "RemoteLists.h"
#include <algorithm>
#include <set>
#include <errno.h>
#include "CVSRoot.h"
#include "CVSStatus.h"
#include "CVSFeatures.h"
#include "../Utils/TortoiseUtils.h"
#include "../Utils/Cache.h"
#include "../DialogsWxw/ProgressDialog.h"
#include "../DialogsWxw/MessageDialog.h"
#include "../TortoiseAct/WebLog.h"
#include "../Utils/PathUtils.h"
#include "../Utils/StringUtils.h"
#include "../Utils/TortoiseRegistry.h"
#include "../Utils/Translate.h"

#define CACHE_SIZE_BRANCHTAGS 40
#define CACHE_SIZE_MODULES 20

std::string GetCacheKey(CVSRoot cvsroot, std::string module = "")
{
   std::string key = cvsroot.GetCVSROOT();
   // remove protocol
   std::string::size_type pos = key.find_first_of(":");
   pos = key.find_first_of(":", pos + 1);
   std::string::size_type pos1 = key.find_first_of("@:", pos + 1);
   if (pos1 != std::string::npos)
   {
      if (key[pos1] == '@')
      {
         pos = pos1;
      }
   }
   key = key.substr(pos + 1);
   if (!module.empty())
   {
      key += ":" + module;
   }
   return key;
}

void RemoteLists::GetModuleList(wxWindow *parent, std::vector<std::string>& modules, 
                                const CVSRoot& cvsroot, const std::string& ofwhat)
{
   CVSAction glue(parent);
   modules.clear();
   glue.SetCloseIfOK(true);
   // First try using "cvs co -c" to get CVSROOT/modules list
   glue.SetCVSRoot(cvsroot);
   MakeArgs args;
   args.add_global_option("-Q");
   args.add_option("co");
   args.add_option("-c");
   glue.Command(GetTemporaryPath(), args); // directory doesn't matter
   glue.SetCVSRoot(cvsroot);
      
   ParseModuleNames(glue.GetStdOutList(), modules, false);

   // Stop if the user aborted
   if (glue.GetProgressDialog()->UserAborted())
   {
       modules.clear();
       return;
   }

   // Try "cvs ls" (CVSNT emulates this in the client if the server does not support it)
   glue.SetCVSRoot(cvsroot);
   glue.SetSilent(true);
   MakeArgs lsargs;
   lsargs.add_global_option("-Q");
   lsargs.add_option("ls");
   lsargs.add_option("-q");
   lsargs.add_option(ofwhat);
   glue.Command(GetTemporaryPath(), lsargs); // directory doesn't matter
   
   ParseModuleNames(glue.GetStdOutList(), modules, true);

   // Stop if we got something, or if the user aborted
   if (glue.GetProgressDialog()->UserAborted())
   {
       modules.clear();
       return;
   }
   if (modules.empty())
   {
       // Last resort: Do some evil hairy weblog parsing to extract
       // the data from viewcvs or cvsweb
       WebLog webLog(cvsroot.GetCVSROOT(), "/", true, glue.GetProgressDialog());
       std::string url;
       bool result = webLog.PerformSearch(url);
       if (result)
       {
           std::string out = webLog.GetReturnedHTML();
      
           // search for text like HREF="modulename/"
           unsigned int lastpos = 0;
           while (true)
           {
               unsigned int pos = static_cast<unsigned int>(out.find("HREF=\"", lastpos));
               if (pos == std::string::npos)
                   pos = static_cast<unsigned int>(out.find("href=\"", lastpos));
               if (pos == std::string::npos)
                   break;

               // Get URL
               std::string linkName;
               for (lastpos = pos + 6; lastpos < out.size() && out[lastpos] != '\"'; ++lastpos)
                   linkName += out[lastpos];
               // Remove trailing slash
               if (linkName[linkName.size() - 1] == '/')
                   linkName = linkName.substr(0, linkName.size() - 1);
               std::string::size_type slashpos = linkName.find_last_of("/");
               if (slashpos != std::string::npos)
               {
                   linkName = linkName.substr(slashpos+1);
                   FindAndReplace<std::string>(linkName, "%20", " ");
                   if (!linkName.empty() &&
                       (linkName[0] != '#') &&
                       (linkName[0] != '?'))
                       modules.push_back(linkName);
               }
           }
       }
   }
   
   // Remove duplicates
   std::sort(modules.begin(), modules.end());
   modules.erase(std::unique(modules.begin(), modules.end()), modules.end());

   // Cache list
   TortoiseRegistryCache trc("Cache\\Modules", CACHE_SIZE_MODULES);
   trc.WriteVector(GetCacheKey(cvsroot), "Modules", modules);
   trc.Shrink();
}

// Return the next real Entries line.  On end of file, returns 0.
// On error, prints an error message and returns 0.
static EntnodeData* fgetentent(std::list<std::string>::const_iterator* itLines,
                               const std::list<std::string>& text,
                               const char *fullpath)
{
   EntnodeData* ent = 0;
   
   char nothing[] = "\0";
   size_t line_length = 0;

   while (*itLines != text.end())
   {
      std::string line = *(*itLines);
      const char* l = line.c_str();
      (*itLines)++;
      line_length = line.length();

      ent_type type = ENT_FILE;

      if (l[0] == 'D')
      {
         type = ENT_SUBDIR;
         ++l;
         /* An empty D line is permitted; it is a signal that this
         Entries file lists all known subdirectories.  */
      }

      if (l[0] != '/')
         continue;

      const char* user = l + 1;
      const char* cp = strchr(user, '/');
      if (!cp)
         continue;
      // finish creating 'user'
      line[cp++-line.c_str()] = '\0';

      const char* vn = cp;
      if ((cp = strchr(vn, '/')) == 0)
         continue;
      line[cp++-line.c_str()] = '\0';
      const char* ts = cp;
      if ((cp = strchr(ts, '/')) == 0)
         continue;
      line[cp++-line.c_str()] = '\0';
      const char* options = cp;
      if ((cp = strchr(options, '/')) == 0)
         continue;
      line[cp++-line.c_str()] = '\0';
      const char* tag = &nothing[0];
      const char* date = &nothing[0];
      const char* ts_conflict=&nothing[0];

      if (type == ENT_SUBDIR)
         ent = new EntnodeDir(fullpath, user);
      else
      {
         ent = new EntnodeFile(fullpath, user, vn, ts, options,
                               tag, date, ts_conflict);
      }
      if (ent == 0)
         errno = ENOMEM;
      break;

   }

   _ASSERT(line_length >= 0);

   return ent;
}

// Parse text for module names
void RemoteLists::ParseEntries(const std::list<std::string>& text,
                               EntnodeMap& entries,
                               const std::string& fullpath,
                               bool bIsLs)
{
   EntnodeData* ent;
   std::list<std::string>::const_iterator itLines = text.begin();
   while ((ent = fgetentent(&itLines, text, fullpath.c_str())) != 0)
   {
      ENTNODE newnode(ent);
      ent->UnRef();

      std::string name = newnode.Data()->GetName();
      EntnodeMap::iterator it = entries.find(name);
      _ASSERT(it == entries.end());
      entries[name] = newnode;
   }
}

void RemoteLists::GetEntriesList(wxWindow *parent, EntnodeMap& entries, 
                                 const CVSRoot& cvsroot, const std::string& ofwhat)
{
   entries.clear();
   CVSAction glue(parent);
   glue.SetCloseIfOK(true);
   glue.SetCloseAnyway(true);

   glue.SetCVSRoot(cvsroot);
   glue.SetSilent(true);
   MakeArgs args;
   args.add_global_option("-Q");
   args.add_option("ls");
   args.add_option("-q");
   args.add_option("-e");
   args.add_option(ofwhat);
   glue.Command(GetTemporaryPath(), args); // directory doesn't matter
   // Stop if the user aborted
   if (glue.GetProgressDialog()->UserAborted())
      return;
      
   const std::list<std::string>& out = glue.GetStdOutList();
   ParseEntries(out, entries, ofwhat, true);
}


// Get module list from cache
void RemoteLists::GetCachedModuleList(std::vector<std::string>& modules, const CVSRoot& cvsroot)
{
   TortoiseRegistryCache trc("Cache\\Modules", CACHE_SIZE_MODULES);
   trc.ReadVector(GetCacheKey(cvsroot), "Modules", modules);
}


// Parse text for module names
void RemoteLists::ParseModuleNames(const std::list<std::string>& text, std::vector<std::string>& modules,
                                   bool bIsLs)
{
   bool bVirtualModules = false;

   if (!bIsLs)
      bVirtualModules = true;

   bool inQuestion = false;
   std::list<std::string>::const_iterator itLines = text.begin();
   while (itLines != text.end())
   {
      std::string line = *itLines++;

      // Skip any questions from CVS
      const char* questionString = CVSPARSE("Question: ");
      const char* promptString = CVSPARSE("Enter: ");
      if (!inQuestion && (line.substr(0, strlen(questionString)) == std::string(questionString)))
         inQuestion = true;
      else if (inQuestion && (line.substr(0, strlen(promptString)) == std::string(promptString)))
      {
         inQuestion = false;
         continue;
      }

      if (inQuestion)
         continue;
      
      std::string moduleName;

      if (!bVirtualModules)
      {
         moduleName = Trim(line);
      }
      else if (!isspace(*line.begin()))
      {
         line = Trim(line);
         // Get first word
         std::string::iterator it = line.begin();
         while (it != line.end())
         {
            if (isspace(*it))
               break;
            it++;
         }
         moduleName.assign(line.begin(), it);
      }

      if (!moduleName.empty())
      {
         modules.push_back(moduleName);
      }
   }
}



// Get list of tags and branches
void RemoteLists::GetTagBranchList(wxWindow* parent, std::vector<std::string>& tags, 
                                   std::vector<std::string>& branches, 
                                   const std::vector<std::string>& dirs, 
                                   bool recursive)
{
   // use "rlog" when scanning local dir only
   GetTagBranchListRlog(parent, tags, branches, dirs, "", "", recursive);
}



// Get cached list of branches and tags for cvsroots and modules
void RemoteLists::GetCachedTagBranchList(std::vector<std::string>& tags, 
                                         std::vector<std::string>& branches, 
                                         const std::string& cvsroot, 
                                         const std::string& module)
{
   TortoiseRegistryCache trc("Cache\\BranchesTags", CACHE_SIZE_BRANCHTAGS);
   trc.ReadVector(GetCacheKey(cvsroot, module), "Tags", tags);
   trc.ReadVector(GetCacheKey(cvsroot, module), "Branches", branches);
}



// Get cached list of branches and tags for directories
void RemoteLists::GetCachedTagBranchList(std::vector<std::string>& tags, 
                                         std::vector<std::string>& branches, 
                                         const std::vector<std::string>& dirs)
{
   std::string cvsroot, module, repository;

   std::vector<std::string>::const_iterator it = dirs.begin();
   while (it != dirs.end())
   {
      cvsroot = CVSStatus::CVSRootForPath(*it);
      repository = CVSStatus::CVSRepositoryForPath(*it);
      if (repository == "CVSROOT/Emptydir")
      {
         // Enumerate all subdirs
         std::vector<std::string> subdirs;
         std::vector<std::string> cvssubdirs;
         GetDirectoryContents(*it, 0, &subdirs);
         std::vector<std::string>::const_iterator itsub = subdirs.begin();
         while (itsub != subdirs.end())
         {
            if (CVSStatus::IsDirInCVS(EnsureTrailingDelimiter(*it) + *itsub))
            {
               cvssubdirs.push_back(EnsureTrailingDelimiter(*it) + *itsub);
            }
            itsub++;
         }
         if (!cvssubdirs.empty())
         {
            GetCachedTagBranchList(tags, branches, cvssubdirs);
         }
      }

      module = CutFirstToken(repository, "/");
      GetCachedTagBranchList(tags, branches, cvsroot, module);
      it++;
   }

   // Remove duplicates
   std::sort(tags.begin(), tags.end());
   tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
   std::sort(branches.begin(), branches.end());
   branches.erase(std::unique(branches.begin(), branches.end()), branches.end());
}



// Get tags using "cvs rlog"
void RemoteLists::GetTagBranchListRlog(wxWindow* parent, std::vector<std::string>& tags, 
                                       std::vector<std::string>& branches,
                                       const std::vector<std::string>& dirs, 
                                       const std::string& cvsRoot,
                                       const std::string& module, bool recursive,
                                       CVSAction *glue)
{
   CVSAction *myglue = 0;
   if (glue)
      myglue = glue;
   else
   {
      myglue = new CVSAction(parent);
      myglue->SetCloseIfOK(true);
   }
      
   std::set<std::string> myTags;
   std::set<std::string> myBranches;

   std::vector<std::string>::const_iterator it1 = dirs.begin();
   while (it1 != dirs.end() && !myglue->Aborted())
   {
      std::set<std::string> dirTags;
      std::set<std::string> dirBranches;
      MakeArgs args;
      CVSRoot myCvsRoot;
      std::string myRepository;
      if (cvsRoot.empty())
      {
         myRepository = CVSStatus::CVSRepositoryForPath(*it1);
         myCvsRoot.SetCVSROOT(CVSStatus::CVSRootForPath(*it1));
      }
      else
      {
         myRepository = module;
         myCvsRoot.SetCVSROOT(cvsRoot);
         myglue->SetCVSRoot(myCvsRoot);
      }

      if (myRepository == "CVSROOT/Emptydir")
      {
         // Enumerate all subdirs
         std::vector<std::string> subdirs;
         std::vector<std::string> cvssubdirs;
         GetDirectoryContents(*it1, 0, &subdirs);
         std::vector<std::string>::const_iterator itsub = subdirs.begin();
         while (itsub != subdirs.end())
         {
            if (CVSStatus::IsDirInCVS(EnsureTrailingDelimiter(*it1) + *itsub))
            {
               cvssubdirs.push_back(EnsureTrailingDelimiter(*it1) + *itsub);
            }
            itsub++;
         }
         if (!cvssubdirs.empty())
         {
            std::vector<std::string> subDirTags, subDirBranches;
            GetTagBranchListRlog(parent, subDirTags, subDirBranches, cvssubdirs, "", "", recursive, myglue);
            // Insert vector elements into set
            std::vector<std::string>::const_iterator it2;
            for (it2 = subDirTags.begin(); it2 != subDirTags.end(); it2++)
               myTags.insert(*it2);
            for (it2 = subDirBranches.begin(); it2 != subDirBranches.end(); it2++)
               myBranches.insert(*it2);
         }
      }
      else
      {
         args.add_option("-Q");
         args.add_option("rlog");
         args.add_option("-h");
         if (!recursive)
         {
            args.add_option("-l");
         }
         args.add_arg(myRepository);

         myglue->SetHideStdout(); // removed the screeds of output, meaningless to the user ...
         myglue->Command(*it1, args);

         // ... but helpful fodder for us to parse
         const std::list<std::string>& out = myglue->GetStdOutList();
         std::list<std::string>::const_iterator itLine;
         std::string currentLine;
         std::string symbol;
         std::string number;
         std::string::reverse_iterator rit;
         bool symbolic_section = false;
         for (itLine = out.begin(); itLine != out.end(); itLine++)
         {
            const std::string &currentLine = *itLine;
            // are we in the symolic names section?
            if (symbolic_section)
            {
               // examine current line
               if (isspace(currentLine[0]))
               {
                  // examine current line
                  std::string::size_type p = currentLine.find(":");
                  if (p != std::string::npos)
                  {
                     symbol = Trim(currentLine.substr(0, p));
                     number = Trim(currentLine.substr(p + 1));

                     // if number ends with 0.x, it's a branch
                     rit = number.rbegin();
                     while ((rit != number.rend()) && isdigit(*rit))
                     {
                        rit++;
                     }
                     if (*(rit++) == '.' && *(rit++) == '0' && *(rit++) == '.')
                     {
                        // we have a branch
                        myBranches.insert(symbol);
                        dirBranches.insert(symbol);
                     }
                     else
                     {
                        // we have a tag
                        myTags.insert(symbol);
                        dirTags.insert(symbol);
                     }
                  }
               }
               else
               {
                  symbolic_section = false;
               }
            }
            else
            {
               symbolic_section = StartsWith(currentLine, "symbolic names:");
            }
         }

         // Cache list
         std::string cvsroot = myCvsRoot.GetCVSROOT();
         std::string repository = myRepository;
         std::string module = CutFirstToken(repository, "/");

         // Insert set elements into vector
         std::vector<std::string> vTags(dirTags.size());
         std::copy(dirTags.begin(), dirTags.end(), vTags.begin());
         std::sort(vTags.begin(), vTags.end());

         std::vector<std::string> vBranches(dirBranches.size());
         std::copy(dirBranches.begin(), dirBranches.end(), vBranches.begin());
         std::sort(vBranches.begin(), vBranches.end());


         TortoiseRegistryCache trc("Cache\\BranchesTags", CACHE_SIZE_BRANCHTAGS);
         trc.WriteVector(GetCacheKey(cvsroot, module), "Tags", vTags);
         trc.WriteVector(GetCacheKey(cvsroot, module), "Branches", vBranches);
         trc.Shrink();
      }
      it1++;
   }

   std::vector<std::string>::size_type oldSize;

   // copy branches from set to vector
   oldSize = branches.size();
   branches.resize(oldSize + myBranches.size());
   std::copy(myBranches.begin(), myBranches.end(), branches.begin() + oldSize);
   std::sort(branches.begin(), branches.end());
   branches.erase(std::unique(branches.begin(), branches.end()), branches.end());

   // copy tags from set to vector
   oldSize = tags.size();
   tags.resize(oldSize + myTags.size());
   std::copy(myTags.begin(), myTags.end(), tags.begin() + oldSize);
   std::sort(tags.begin(), tags.end());
   tags.erase(std::unique(tags.begin(), tags.end()), tags.end());

   if (glue == 0)
      delete myglue;
}
