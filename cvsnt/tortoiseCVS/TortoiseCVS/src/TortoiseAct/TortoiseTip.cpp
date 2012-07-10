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

#include "StdAfx.h"
#include "TortoiseTip.h"
#include "../CVSGlue/CVSRoot.h"
#include <windows.h>
#include "../Utils/Translate.h"


#define MATCH_PARTIAL 0
#define MATCH_BEGIN_OF_LINE 1



bool ScanTextsContain(const std::vector<std::list<std::string>* >& store, const std::string& str, 
                      int matchType = MATCH_PARTIAL)
{
   std::vector<std::list<std::string>* >::const_iterator it1 = store.begin();
   while (it1 != store.end())
   {
      std::list<std::string>::const_iterator it = (*it1)->begin();
      while (it != (*it1)->end())
      {
         std::string::size_type findpos = it->find(str, 0);
         if (findpos != std::string::npos)
         {
            if (matchType == MATCH_PARTIAL)
            {
               return true;
            }
            else if (matchType == MATCH_BEGIN_OF_LINE && findpos == 0)
            {
               return true;
            }
         }
         it++;
      }
      it1++;
   }
   return false;
}



wxString TortoiseTip(const std::vector<std::list<std::string>*>& scanTexts)
{
   if (ScanTextsContain(scanTexts, CVSPARSE("Log message unchanged or not specified")))
   {
      // This message is obsolete now there is a commit dialog - it is still here
      // as the non-dialog version of commit is still available to someone who
      // edits TortoiseMenus.config
      return _(
"Tortoise Tip:  This error happens if you didn't enter a message at the top \
of the Notepad window when you performed a commit.  Please try again and \
enter some text, describing roughly what you changed.\n\n\
There is also a bug in CVS under Windows which can cause this to happen. \
If you save the file too quickly, CVS doesn't notice that it has changed.\n\
Try again, and wait a few seconds before saving the file!");
   }

   if (ScanTextsContain(scanTexts, CVSPARSE(" it is in the way")))
   {
      return _(
"Tortoise Tip:  CVS tried to create a new file and discovered that the file \
already existed.   There can be several causes for this:\n\
1) You have created the file locally, and someone else added a file of the same name to CVS.\n\
2) Your sandbox has become corrupted (possibly due to interference from antivirus programs).\n\
To solve the problem: Rename or delete the affected files, then perform a CVS Update.");
   }

   if (ScanTextsContain(scanTexts, CVSPARSE("C "), MATCH_BEGIN_OF_LINE))
   {
      return _(
"Tortoise Tip:  One or more of your files has a conflict.  This means that \
someone else has altered the same part of the file as you.  Look above for \
the files marked \"C \", and manually edit them to resolve the conflict.");
   }

   if (ScanTextsContain(scanTexts, CVSPARSE("No CVSROOT specified!")))
   {
      return _(
"Tortoise Tip:  This error can be caused by a folder called \"CVS\" which was \
made by you rather than by CVS.  If so, please rename the folder to something \
else such as \"cvswork\".");
   }

   if (ScanTextsContain(scanTexts, CVSPARSE("Up-to-date check failed for ")))
   {
      return _(
"Tortoise Tip:  Someone else has committed a new version of one or more of \
the files you are trying to commit. You need to do a CVS Update and then \
try to commit again.");
   }

   if (ScanTextsContain(scanTexts, CVSPARSE("the :server: access method is not supported by this port of CVS")))
   {
      return _(
"Tortoise Tip:  Unfortunately TortoiseCVS no longer supports RSH out of the \
box.  There are workarounds, as well as fairly easy ways to help us fix this \
for the next version.  For more information see the FAQ.\n\n\
http://www.tortoisecvs.org/faq.html#rsh");
   }

   if (ScanTextsContain(scanTexts, CVSPARSE("could not chdir to ")))
   {
      return _(
"Tortoise Tip:  This error may be caused by having a folder and a file with \
the same name (apart from capitalization) in the repository. Such a repository \
CANNOT be checked out on a Windows file system.\n\
\n\
You can also get this error on SourceForge. You need to log into the server \
with SSH to force creation of your home folder. Do this by connecting to \
username@cvs.projectname.sf.net either with Putty or a command line SSH.");
   }

   if ((ScanTextsContain(scanTexts, CVSPARSE("can't stat")) &&
        ScanTextsContain(scanTexts, CVSPARSE("No such file or directory")))
       || (ScanTextsContain(scanTexts, CVSPARSE("checkaddfile: Assertion")))
       || (ScanTextsContain(scanTexts, CVSPARSE("findnode: Assertion"))))
   {
      return _(
"Tortoise Tip: This error can be caused because there used to be a file \
in the repository with a name differing only by case from the one you are adding. \
For example TEST.EXE rather than test.exe. To fix this go into the repository, or \
get your administrator to, and rename that file to have the same case. It will be \
in the Attic folder underneath the folder you are in. You will also need to get \
the local sandbox folder you are in out of CVS again, as it may have bogus \
entries to the misspelt filename.");
   }

   if (ScanTextsContain(scanTexts, CVSPARSE("Files being edited")))
   {
      return _(
"Tortoise Tip: Someone else is editing the file you want to work on. \
If the file is binary, you must wait for the other user to commit the file \
before you can edit it. Otherwise, you may edit in parallel with the other \
user; CVS will eventually merge your changes with those of the other user. \
However, you might want to talk to the other user to ensure that you are not \
both working on the same lines of the file.");
   }

   if (ScanTextsContain(scanTexts, CVSPARSE("use 'cvs commit' to remove"), 0))
      return _("Tortoise Tip: Use \"cvs commit\" on the folder that the file is in to remove it permanently.");
   
   return wxT("");
}

