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
 * Author : Alexandre Parenteau <aubonbeurre@hotmail.com> --- June 1998
 */

/*
 * CvsIgnore.cpp --- parse .cvsignore
 */

#include "StdAfx.h"
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string>
#include <time.h>
#include "CvsIgnore.h"
#include <Utils/PathUtils.h>
#include <Utils/TortoiseRegistry.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/TortoiseDebug.h>
#include <Utils/SyncUtils.h>
#include <Utils/Preference.h>


static DWORD defIgnoreListTimeStamp = 0;
static std::vector<std::string> defIgnoredList;
static DWORD dwUpdateIgnoredListInterval = GetIntegerPreference("IgnoredList update interval");
static CriticalSection myCriticalSection;
static FileChangeParams fcpCvsignore;


// 8-bit safe isspace()
bool myisspace(char c)
{
    return (c == ' ') || (c == '\t') || (c == '\n') || (c == '\r');
}


/* Parse a line of space-separated wildcards and add them to the list. */
/* mostly duplicated from cvs/src/ignore.c */
static void ign_add(const char* ign, std::vector<std::string>& ignlist)
{
   if (!ign || !*ign)
      return;

   int ign_hold = ignlist.empty() ? -1 : (int) ignlist.size();
   bool escape = false;

   for (; *ign; ign++)
   {
      /* ignore whitespace before the token */
      if (myisspace(*ign))
         continue;

      /*
       * if we find a single character !, we must re-set the ignore list
       * (saving it if necessary).  We also catch * as a special case in a
       * global ignore file as an optimization
       */
      char next = *(ign+1);
      if ((!*(ign+1) || myisspace(*(ign+1))) && (*ign == '!' || *ign == '*'))
      {
         /* temporarily reset the ignore list */
         if (ign_hold >= 0)
         {
            ignlist.erase(ignlist.begin(), ignlist.begin() + ign_hold);
            ign_hold = -1;
         }
         continue;
      }

      // Find the end of this token, treating "\ " as an escaped space
      const char* mark;
      escape = false;
      for (mark = ign; *mark && (escape || !myisspace(*mark)); mark++)
      {
         escape = false;
         if (*mark == '\\')
            escape = true;
      }

      std::string token(ign, mark-ign);
      FindAndReplace<std::string>(token, "\\\\", "\\");
      ignlist.push_back(token);

      if (*mark)
         ign = mark;
      else
         ign = mark - 1;
   }
}


// Read .cvsignore file and append entries to ignlist
static bool ReadIgnoredFile(const std::string& fileName, std::vector<std::string> & ignlist)
{
   std::ifstream in(fileName.c_str());
   if (!in)
      return false;
   while (1)
   {
      std::string line;
      std::getline(in, line);
      if (!in)
         break;
      ign_add(line.c_str(), ignlist);
   }

   return true;
}


void BuildIgnoredList(std::vector<std::string> & ignlist, const std::string& path,
                      DWORD *timeStamp, FileChangeParams *fcp)
{
   TDEBUG_ENTER("BuildIgnoredList");
   bool doUpdate = false;

   std::string ignPath(path);
   ignPath = EnsureTrailingDelimiter(ignPath);
   ignPath += ".cvsignore";
   FileChangeParams myFcp;
   if (fcp)
      myFcp = GetFileChangeParams(ignPath);

   // Update default ignored list
   BuildDefaultIgnoredList();
   if (timeStamp)
   {
      if ((*timeStamp != defIgnoreListTimeStamp) || (*timeStamp == 0))
         doUpdate = true;
   }
   else
   {
      doUpdate = true;
   }

   // check directory ignored file
   if (fcp)
   {
      if ((fcp->IsNull()) || (*fcp != myFcp))
      {
         doUpdate = true;
      }
   }
   else
   {
      doUpdate = true;
   }

   // Do we have to update
   if (doUpdate)
   {
      ignlist.clear();
      ignlist = defIgnoredList;

      // TODO: read $CVSROOT/CVSROOT/cvsignore
      
      // Read .cvsignore from current directory
      ReadIgnoredFile(ignPath, ignlist);
      if (fcp)
         *fcp = myFcp;
      if (timeStamp)
         *timeStamp = defIgnoreListTimeStamp;
   }
}


static bool fnmatch(const char *pattern, const char *string)
{
   const char *p = pattern, *n = string;
   char c;

   while ((c = *p++) != '\0')
   {
      switch (c)
      {
      case '?':
         if (*n == '\0')
            return false;
         break;
     
      case '\\':
         c = *p++;
         if (*n != c)
            return false;
         break;
     
      case '*':
         for (c = *p++; c == '?' || c == '*'; c = *p++, ++n)
            ;
     
         if (c == '\0')
            return true;
     
         {
            char c1 = (c == '\\') ? *p : c;
            for (--p; *n != '\0'; ++n)
               if ((c == '[' || *n == c1) && fnmatch(p, n))
                  return true;
            return false;
         }
     
      case '[':
      {
         /* Nonzero if the sense of the character class is inverted.  */
         register int inverted;
       
         if (*n == '\0')
            return false;
       
         inverted = (*p == '!' || *p == '^');
         if (inverted)
            ++p;
       
         c = *p++;
         for (;;)
         {
            char cstart = c, cend = c;
      
            if (c == '\\')
               cstart = cend = *p++;
      
            if (c == '\0')
               /* [ (unterminated) loses.  */
               return false;
      
            c = *p++;
      
            if (c == '-' && *p != ']')
            {
               cend = *p++;
               if (cend == '\\')
                  cend = *p++;
               if (cend == '\0')
                  return false;
               c = *p++;
            }
      
            if (*n >= cstart && *n <= cend)
               goto matched;
      
            if (c == ']')
               break;
         }
         if (!inverted)
            return false;
         break;
       
      matched:
         /* Skip the rest of the [...] that already matched.  */
         while (c != ']')
         {
            if (c == '\0')
               /* [... (unterminated) loses.  */
               return false;
      
            c = *p++;
            if (c == '\\')
               /* 1003.2d11 is unclear if this is right.  %%% */
               ++p;
         }
         if (inverted)
            return false;
      }
      break;
     
      default:
         if (c != *n)
            return false;
      }
      
      ++n;
   }

   if (*n == '\0')
      return true;
    
   return false;
}


bool MatchIgnoredList(const char * filename, const std::vector<std::string> & ignlist)
{
   std::vector<std::string>::const_iterator i;
   for (i = ignlist.begin(); i != ignlist.end(); ++i)
   {
      if (fnmatch((*i).c_str(), filename))
         return true;
   }
   return false;
}


bool MatchIgnoredList(const char * filename)
{
   return MatchIgnoredList(filename, defIgnoredList);
}

// Invalidate cached ignoredlist
void InvalidateIgnoredCache()
{
   defIgnoreListTimeStamp = 0;
}


// Do update ignored list
void DoUpdateIgnoredList()
{
   TDEBUG_ENTER("DoUpdateIgnoredList");
   defIgnoredList.clear();
   defIgnoreListTimeStamp = GetTickCount();

   // Default ignored list from cvsnt/src/ignore.c
   static const char* ign_default =
      ". .. RCSLOG tags TAGS RCS SCCS .make.state "
      ".nse_depinfo #* .#* cvslog.* ,* CVS CVS.adm .del-* *.a *.olb *.o *.obj "
      "*.so *.Z *~ *.old *.elc *.ln *.bak *.orig *.rej _$* *$";

   ign_add(ign_default, defIgnoredList);

   // .cvsignore from homedir
   std::string homeDir;
   bool GotHomeDir = GetHomeDirectory(homeDir);

   if (GotHomeDir)
   {
      // Read .cvsignore file from HOME directory.
      homeDir = EnsureTrailingDelimiter(homeDir);
      homeDir += ".cvsignore";
      ReadIgnoredFile(homeDir, defIgnoredList);
   }
}



// Build default ignored list
void BuildDefaultIgnoredList()
{
   TDEBUG_ENTER("BuildDefaultIgnoredList");
   CSHelper csHelper(myCriticalSection, true);
   std::string userCvsIgnoreFile;
   GetHomeDirectory(userCvsIgnoreFile);
   userCvsIgnoreFile = EnsureTrailingDelimiter(userCvsIgnoreFile) + ".cvsignore";
   FileChangeParams myFcp = GetFileChangeParams(userCvsIgnoreFile);

   // Update every dwUpdateIgnoredListInterval seconds
   if (GetTickCount() > defIgnoreListTimeStamp + 1000 * dwUpdateIgnoredListInterval)
   {
      DoUpdateIgnoredList();
      fcpCvsignore = myFcp;
   }
   // Update if .cvsignore in home dir has changed
   else if (fcpCvsignore != myFcp)
   {
      DoUpdateIgnoredList();
      fcpCvsignore = myFcp;
   }
}
