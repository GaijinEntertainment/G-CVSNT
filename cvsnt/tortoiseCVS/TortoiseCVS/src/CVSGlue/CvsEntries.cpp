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
 * CvsEntries.cpp --- adaptation from cvs/src/entries.c
 */

#include "StdAfx.h"
#include "../Utils/TortoiseDebug.h"
#include "../Utils/Translate.h"

#include <stdlib.h>
#include <string.h>

#include "CvsEntries.h"
#include "CvsIgnore.h"
#include "getline.h"
#include <stdio.h>
#include <string>
#include "../Utils/PathUtils.h"
#include "../Utils/TimeUtils.h"

#if TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
#elif HAVE_SYS_TIME_H
#  include <sys/time.h>
#else
#  include <time.h>
#endif
#include <errno.h> // to get ENOMEM


EntnodeData::~EntnodeData()
{
   --ref;
}

void EntnodeData::SetDesc(const wxChar* newdesc)
{
   desc = newdesc;
}

EntnodeFile::EntnodeFile(const char* /* newfullpath */, const char* newuser, const char* newvn,
                         const char* newts, const char* newoptions, const char* newtag,
                         const char* newdate, const char* newts_conflict, bool renamed,
                         const std::string& newbugno)
   : EntnodeData(ENT_FILE)
{
   if (newuser && newuser[0])
      user = newuser;
   if (newvn && newvn[0])
      vn = newvn;
   if (newts && newts[0])
      ts = newts;
   if (newoptions && newoptions[0])
      option = newoptions;
   if (newtag && newtag[0])
      tag = newtag;
   if (newdate && newdate[0])
      date = newdate;
   if (newts_conflict && newts_conflict[0])
      ts_conflict = newts_conflict;
   bugnumber = newbugno;

   if (newvn && newvn[0] == '-')
      SetRemoved(true);

   SetRenamed(renamed);
}


Tagnode::Tagnode(const char* newfullpath, const char* newuser,
                 const std::string& newbugno)
{
   fullpath = newfullpath;
   user = newuser;
   alreadyChecked = false;
}

Tagnode::~Tagnode()
{
}

const char* Tagnode::GetTag() const
{
    PrepareGet();
    return tag.empty() ? date.c_str() : tag.c_str();
}
    
const char* Tagnode::GetTagOnly() const
{
    PrepareGet();
    return tag.c_str();
}

const char* Tagnode::GetDateOnly() const
{
    PrepareGet();
    return date.c_str();
}


void Tagnode::PrepareGet() const
{
   if (!alreadyChecked)
   {
      // don't check twice
      alreadyChecked = true;

      // look in Tags file
      std::string tagFile = fullpath;
      tagFile.append("/");
      tagFile.append(user);
      tagFile.append("/CVS/Tag");

      std::string tagString;
      char cmd;
      if (Tag_Open(tagString, tagFile.c_str(), &cmd) && !tagString.empty())
      {
         // figure out which sticky thing we have
         switch (cmd) {
         case 'T':
         case 'N':
            tag = tagString;
            break;
         case 'D':
            date = tagString;
            break;
         }
      }
   }
}

EntnodeDir::EntnodeDir(const char* newfullpath, const char* newuser, const char* newvn,
                       const char* newts, const char* newoptions, const char* newtag,
                       const char* newdate, const char* newts_conflict) : EntnodeData(ENT_SUBDIR)
{
   if (newuser && newuser[0])
      user = newuser;

   if (newvn && newvn[0] == '-')
      SetRemoved(true);

   tagnode = new Tagnode(newfullpath, user.c_str());
}


const char* EntnodeDir::GetTag() const
{
    return tagnode->GetTag();
}
    
const char* EntnodeDir::GetTagOnly() const
{
    return tagnode->GetTagOnly();
}

const char* EntnodeDir::GetDateOnly() const
{
    return tagnode->GetDateOnly();
}


EntnodeOuterDir::EntnodeOuterDir(const char* newfullpath, const char* newuser, const char* newvn,
                                       const char* newts, const char* newoptions, const char* newtag,
                                       const char* newdate, const char* newts_conflict)
    : EntnodeDir(newfullpath, newuser, newvn, newts, newoptions, newtag,
                    newdate, newts_conflict)
{
}

/* Return the next real Entries line.  On end of file, returns NULL.
   On error, prints an error message and returns NULL.  */

static EntnodeData* fgetentent(FILE* fpin, const char* extrabuf, const char* fullpath, char* cmd = 0,
                               unsigned long sizeextra = 0)
{
   EntnodeData *ent;
   char* line = 0;
   size_t line_chars_allocated = 0;
   const char* cp;
   ent_type type;
   const char* l, *user, *vn, *ts, *options;
   const char* tag_or_date, *tag, *date, *ts_conflict, *bugno;
   int line_length;
   size_t bugnolen;

   ent = 0;
   while ((line_length = getline (&line, &line_chars_allocated, fpin)) > 0)
   {
      l = line;

      /* If CMD is not NULL, we are reading an Entries.Log file.
      Each line in the Entries.Log file starts with a single
      character command followed by a space.  For backward
      compatibility, the absence of a space indicates an add
      command.  */
      bugno = 0;
      bugnolen = 0;
      if (cmd)
      {
         if (l[1] != ' ')
            *cmd = 'A';
         else
         {
            *cmd = l[0];
            l += 2;
         }
      }
      type = ENT_FILE;

      if (l[0] == 'D')
      {
         type = ENT_SUBDIR;
         ++l;
         /* An empty D line is permitted; it is a signal that this
         Entries file lists all known subdirectories.  */
      }

      if (l[0] != '/')
         continue;

      user = l + 1;
      if ((cp = strchr(user, '/')) == 0)
         continue;
      // Find the bug number using Entries.Extra
      if ((type != ENT_SUBDIR) && extrabuf)
      {
         char save = line[cp-line+1];
         line[cp-line+1] = '\0';
         const char* extrapos = strstr(extrabuf, l);
         line[cp-line+1] = save;
         if (extrapos)
         {
            // Count off eight slashes
            const char* posax = strchr(extrapos, '/');
            const char* posbx = 0;
            int slashcount = 8;
            while (posax && slashcount)
            {
                if (!--slashcount)
                    break;
                posbx = posax;
                posax = strchr(posax+1, '/');
            }
            if (posax && !slashcount && (posbx[1] != '/'))
            {
               bugno = posbx+1;
               bugnolen = posax - bugno;
            }
         }
      }
      // Finish creating 'user'
      line[cp++-line] = '\0';
      vn = cp;
      if ((cp = strchr(vn, '/')) == 0)
         continue;
      line[cp++-line] = '\0';
      ts = cp;
      if ((cp = strchr(ts, '/')) == 0)
         continue;
      line[cp++-line] = '\0';
      options = cp;
      if ((cp = strchr(options, '/')) == 0)
         continue;
      line[cp++-line] = '\0';
      tag_or_date = cp;
      if ((cp = strchr(tag_or_date, '\n')) == 0)
         continue;
      line[cp-line] = '\0';
      tag = 0;
      date = 0;
      if (*tag_or_date == 'T')
         tag = tag_or_date + 1;
      else if (*tag_or_date == 'D')
         date = tag_or_date + 1;

      if ((ts_conflict = strchr(ts, '+')) != 0)
         line[ts_conflict++-line] = '\0';

      if (type == ENT_SUBDIR)
         ent = new EntnodeDir(fullpath, user);
      else
      {
         std::string bugNoStr;
         if (bugno)
             bugNoStr.assign(bugno, bugnolen);
         ent = new EntnodeFile(fullpath, user, vn, ts, options, tag, date, ts_conflict, false, bugNoStr);
      }
      if (ent == 0)
         errno = ENOMEM;
      break;
   }

   if (line_length < 0 && !feof (fpin))
   {
      _ASSERT(false);
      // cvs_err("Cannot read entries file (error %d)\n", errno);
   }

   if (line)
      free(line);
   return ent;
}

/* Read the entries file into a list, hashing on the file name.

   UPDATE_DIR is the name of the current directory, for use in error
   messages, or NULL if not known (that is, noone has gotten around
   to updating the caller to pass in the information).  */
bool Entries_Open(EntnodeMap& entries,
                  const char* fullpath,
                  FileChangeParams* fcp)
{
   TDEBUG_ENTER("Entries_Open");
   std::string cvsdir(fullpath);
   cvsdir = EnsureTrailingDelimiter(cvsdir);
   cvsdir += "CVS";
   cvsdir = EnsureTrailingDelimiter(cvsdir);

   if (fcp)
   {
      FileChangeParams myFcp = GetFileChangeParams(cvsdir + "Entries");
      if ((!(fcp->IsNull())) && (myFcp == *fcp))
      {
         return true;
      }
      *fcp = myFcp;
   }

   entries.clear();
   unsigned long sizeextra = 0;
   FILE* fpinx = fopen((cvsdir + "Entries.Extra").c_str(), "r");
   if (fpinx)
   {
      FileChangeParams myFcp = GetFileChangeParams(cvsdir + "Entries.Extra");
      sizeextra = myFcp.dwFileSizeLow;
   }
   FILE* fpin = fopen((cvsdir + "Entries").c_str(), "r");
   if (!fpin)
   {
      if (fpinx)
         fclose (fpinx);
      return false;
   }

   // Read contents of CVS/Rename into a set for easy lookup
   std::ifstream cvsRename((cvsdir + "Rename").c_str());
   std::set<std::string> renameEntries;
   while (cvsRename.good())
   {
      std::string file;
      // Each entry is
      //    newname
      //    (blank)
      //    newname
      //    oldname
      // We are only interested in the new name.
      std::getline(cvsRename, file);
      file = ExtractLastPart(file);
      renameEntries.insert(file);
      for (int i = 0; i < 3; ++i)
         std::getline(cvsRename, file);
   }
   EntnodeData* ent;
   char* extrabuf = 0;
   size_t lenreadx;
   if (fpinx && (sizeextra > 0))
   {
      extrabuf = (char*) malloc((sizeextra*2)+10);
      fseek(fpinx, 0, SEEK_SET);
      lenreadx = fread(extrabuf, sizeof(char), (sizeextra*2)+9, fpinx);
      *(extrabuf+lenreadx) = '\0';
      if (!feof(fpinx))
      {
         // could not read the whole file for some reason...
         free(extrabuf);
         extrabuf = 0;
      }
   }

   while ((ent = fgetentent(fpin, extrabuf, fullpath, 0, sizeextra)) != 0)
   {
      ENTNODE newnode(ent);
      ent->UnRef();

      std::string name = newnode.Data()->GetName();
      EntnodeMap::iterator it = entries.find(name);
      if (it != entries.end())
      {
         _ASSERT(false);
         TDEBUG_TRACE("Warning : duplicated entry in the 'CVS/Entries' file in folder " << fullpath);
      }
      std::set<std::string>::iterator renameIter = renameEntries.find(name);
      ent->SetRenamed(renameIter != renameEntries.end());
      entries[name] = newnode;
   }

   fclose (fpin);
   if (fpinx)
      fclose (fpinx);
   if (extrabuf)
      free (extrabuf);

   fpin = fopen((cvsdir + "Entries.log").c_str(), "r");
   if (fpin)
   {
      char cmd;

      while ((ent = fgetentent(fpin, extrabuf, fullpath, &cmd, sizeextra)) != 0)
      {
         ENTNODE newnode(ent);
         ent->UnRef();

         std::string name = newnode.Data()->GetName();

         switch (cmd)
         {
         case 'A':
            entries[name] = newnode;
            break;
         case 'R':
            entries.erase(std::string(name));
            break;
         default:
            /* Ignore unrecognized commands.  */
            TDEBUG_TRACE("Warning: Unrecognized command '" << cmd << "'");
            break;
         }
      }
      fclose (fpin);
   }
   return true;
}

// mostly the cvs one...
static bool unmodified(const struct stat & sb, const char* ts)
{
   TDEBUG_ENTER("unmodified");
   struct tm tmbuf;
   char timebuf[30];
   char* cp;
   struct tm *tm_p;
   struct tm local_tm;
   /* We want to use the same timestamp format as is stored in the
   st_mtime.  For unix (and NT I think) this *must* be universal
   time (UT), so that files don't appear to be modified merely
   because the timezone has changed.  For VMS, or hopefully other
   systems where gmtime returns NULL, the modification time is
   stored in local time, and therefore it is not possible tcause
   st_mtime to be out of sync by changing the timezone.  */
   tm_p = gmtime_r(&sb.st_mtime, &tmbuf);
   if (tm_p)
   {
      memcpy (&local_tm, tm_p, sizeof (local_tm));
      cp = asctime_r(&local_tm, timebuf);  /* copy in the modify time */
   }
   else
      cp = ctime_r(&sb.st_mtime, timebuf);

   if (!cp)
      return true;

   cp[24] = 0;

#if defined(_MSC_VER) || defined(__GNUWIN32__)
   /* Work around non-standard asctime() and ctime() in MS VC++ and mingw
   These return '01' instead of ' 1' for the day of the month. */
   if (
      (strlen(ts) > 8) &&
      ((cp[8] == '0' && ts[8] == ' ') ||
      (cp[8] == ' ' && ts[8] == '0'))
   ) {
      cp[8] = ts[8];
   }
#endif
   
   TDEBUG_TRACE("Timestamp: " << ts);
   TDEBUG_TRACE("Filetime : " << cp);
   return strcmp(cp, ts) == 0;
}

EntnodeData *Entries_SetVisited(const char* path, EntnodeMap& entries, const char* name,
                                const struct stat& finfo, bool isDir, bool isReadOnly,
                                bool isMissing, const std::vector<std::string>* ignlist)
{
   TDEBUG_ENTER("Entries_SetVisited");
   bool isCvs = false;
   std::string lookupName;
   if (isDir)
   {
      TDEBUG_TRACE("Is dir");
      EntnodeDir *adata = new EntnodeDir(path, name);
      ENTNODE anode(adata);
      adata->UnRef();

      lookupName = anode.Data()->GetName();
      EntnodeMap::iterator it = entries.find(lookupName);
      isCvs = it != entries.end();
      if (!isCvs)
         entries[lookupName] = anode;
   }
   else
   {
      TDEBUG_TRACE("Is no dir");
      EntnodeFile *adata = new EntnodeFile(path, name);
      ENTNODE anode(adata);
      adata->UnRef();

      lookupName = anode.Data()->GetName();
      EntnodeMap::iterator it = entries.find(lookupName);
      isCvs = it != entries.end();
      if (!isCvs)
         entries[lookupName] = anode;
   }

   const ENTNODE & theNode = entries[lookupName];
   EntnodeData *data = ((ENTNODE *)&theNode)->Data();
   data->SetVisited(true);
   if (!isCvs)
   {
      data->SetUnknown(true);
      if (ignlist && MatchIgnoredList(name, *ignlist) || 
         finfo.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)
         data->SetIgnored(true);

      // the dir may have some cvs informations in it, despite the fact
      // that it is not referenced by the parent directory, so try
      // to figure it.

      if (!data->IsIgnored())
      {
         std::string cvsFile = path;
         cvsFile = EnsureTrailingDelimiter(cvsFile);
         cvsFile += name;
         cvsFile = EnsureTrailingDelimiter(cvsFile);
         cvsFile += "CVS";
         struct stat sb;
         TDEBUG_TRACE("Before stat");
         if (stat(cvsFile.c_str(), &sb) != -1 && S_ISDIR(sb.st_mode))
         {
            data->SetUnknown(false);
         }
         TDEBUG_TRACE("After stat");
      }
   }

   data->SetReadOnly(isReadOnly);
   data->SetMissing(isMissing);

   if (isDir)
   {
      if (data->IsIgnored())
         data->SetDesc(_("Ignored Folder"));
      else if (data->IsUnknown())
         data->SetDesc(_("Non-CVS Folder"));
      else
         data->SetDesc(_("Folder"));
   }
   else if (!isMissing)
   {
      const char* ts = data->GetTS();

     TDEBUG_TRACE("Timestamp: " << (ts == 0 ? "NULL" : ts));
      // Revision "0" means "added"
      if (ts == 0)
      {
         data->SetUnmodified(true);
      }
      else if (strcmp(data->GetVN(), "0") == 0)
      {
         data->SetAdded(true);
         // Added files are always modified
         data->SetUnmodified(false);
      }
      else
      {
         data->SetUnmodified(unmodified(finfo, ts));
      }

      const char* ts_conflict = data->GetConflict();
      if (ts_conflict == 0)
         data->SetNeedsMerge(false);
      else
         data->SetNeedsMerge(unmodified(finfo,ts_conflict));

      data->SetLocked((finfo.st_mode & S_IWRITE) == 0);
      
      const char* info = 0;
      if (data->IsIgnored())
      {
         data->SetDesc(_("Ignored"));
      }
      else if (data->IsUnknown())
      {
         data->SetDesc(_("Non-CVS File"));
      }
      else if (data->NeedsMerge())
      {
         data->SetDesc(_("Conflict"));
      }
      else if ((info = data->GetOption()) != 0 && strcmp(info, "-kb") == 0)
      {
         data->SetDesc(data->IsUnmodified() ? _("Binary") : _("Mod. Binary"));
      }
      else
      {
         data->SetDesc(data->IsUnmodified() ? _("File") : _("Mod. File"));
      }
   }

   return data;
}

bool Tag_Open(std::string & tag, const char* fullpath, char* cmd)
{
   tag = "";
   if (cmd)
      *cmd = '\0';

   FILE* fpin = fopen(fullpath, "r");
   if (fpin == 0)
      return false;

   char* line = 0;
   size_t line_chars_allocated = 0;
   int line_length;
   if ((line_length = getline (&line, &line_chars_allocated, fpin)) > 0)
   {
      char* tmp = strchr(line, '\n');
      if (tmp)
         *tmp = '\0';
      if (cmd)
      {
         tag = line + 1;
         *cmd = line[0];
      }
      else {
         if (line[0] == 'T')
            tag = line + 1;
      }
   }
   if (line)
      free(line);
   if (line_length < 0 && !feof (fpin))
   {
      fclose (fpin);
      _ASSERT(false);
      return false;
   }

   fclose (fpin);
   return !tag.empty();
}

