// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2002 - Stefan Hoffmeister
// <Stefan.Hoffmeister@Econos.de> - October 2002

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

#include <cassert>
#include <locale>
#include "TortoiseActVerbs.h"
#include "../Utils/StringUtils.h"

const std::string CvsAboutVerb("cvsabout");
const std::string CvsAddIgnoreExtVerb("cvsaddignoreext");
const std::string CvsAddIgnoreVerb("cvsaddignore");
const std::string CvsAddRecursiveVerb("cvsaddrecursive");
const std::string CvsAddVerb("cvsadd");
const std::string CvsAnnotateVerb("cvsannotate");
const std::string CvsApplyPatchVerb("cvsapplypatch");
const std::string CvsBranchVerb("cvsbranch");
const std::string CvsCheckoutVerb("cvscheckout");
const std::string CvsCommandVerb("cvscommand");
const std::string CvsCommitVerb("cvscommit");
const std::string CvsDiffVerb("cvsdiff");
const std::string CvsEditVerb("cvsedit");
const std::string CvsExclusiveEditVerb("cvseditexclusive");
const std::string CvsGetVerb("cvsget");
const std::string CvsHelpVerb("cvshelp");
const std::string CvsHistoryVerb("cvshistory");
const std::string CvsListEditorsVerb("cvslisteditors");
const std::string CvsLogVerb("cvslog");
const std::string CvsMakeModuleVerb("cvsmakemodule");
const std::string CvsMakePatchVerb("cvsmakepatch");
const std::string CvsMergeVerb("cvsmerge");
const std::string CvsMergeConflictsVerb("cvsmergeconflicts");
const std::string CvsPrefsVerb("cvsprefs");
const std::string CvsRebuildIconsVerb("cvsrebuildicons");
const std::string CvsRefreshStatusVerb("cvsrefreshstatus");
const std::string CvsReleaseVerb("cvsrelease");
const std::string CvsRemoveVerb("cvsremove");
const std::string CvsRenameVerb("cvsrename");
const std::string CvsResolveConflictsVerb("cvsresolveconflicts");
const std::string CvsRestoreVerb("cvsrestore");
const std::string CvsRevisionGraphVerb("cvsrevisiongraph");
const std::string CvsSaveAsVerb("cvssaveas");
const std::string CvsSwitchVerb("cvsswitch");
const std::string CvsTagVerb("cvstag");
const std::string CvsTortoiseCodeVerb("cvstortoisecode");
const std::string CvsUneditVerb("cvsunedit");
const std::string CvsUpdateVerb("cvsupdate");
const std::string CvsUpdateCleanVerb("cvsupdateclean");
const std::string CvsUpdateDialogVerb("cvsupdatedialog");
const std::string CvsUrlVerb("cvsurl");
const std::string CvsViewVerb("cvsview");
const std::string CvsWebLogVerb("cvsweblog");
const std::string CvsWxWindowsCodeVerb("cvswxwindowscode");


// Note: This array must always remain sorted in 
// ascending order on the content of the verb text.
// The unit tests verify that this is the case.
static const std::string* SortedLowerCaseVerbArray[] =
{
   &CvsAboutVerb,
   &CvsAddVerb,
   &CvsAddIgnoreVerb,
   &CvsAddIgnoreExtVerb,
   &CvsAddRecursiveVerb,
   &CvsAnnotateVerb,
   &CvsApplyPatchVerb,
   &CvsBranchVerb,
   &CvsCheckoutVerb,
   &CvsCommandVerb,
   &CvsCommitVerb,
   &CvsDiffVerb,
   &CvsEditVerb,
   &CvsExclusiveEditVerb,
   &CvsGetVerb,
   &CvsHelpVerb,
   &CvsHistoryVerb,
   &CvsListEditorsVerb,
   &CvsLogVerb,
   &CvsMakeModuleVerb,
   &CvsMakePatchVerb,
   &CvsMergeVerb,
   &CvsMergeConflictsVerb,
   &CvsPrefsVerb,
   &CvsRebuildIconsVerb,
   &CvsRefreshStatusVerb,
   &CvsReleaseVerb,
   &CvsRemoveVerb,
   &CvsRenameVerb,
   &CvsResolveConflictsVerb,
   &CvsRestoreVerb,
   &CvsRevisionGraphVerb,
   &CvsSaveAsVerb,
   &CvsSwitchVerb,
   &CvsTagVerb,
   &CvsTortoiseCodeVerb,
   &CvsUneditVerb,
   &CvsUpdateVerb,
   &CvsUpdateCleanVerb,
   &CvsUpdateDialogVerb,
   &CvsUrlVerb,
   &CvsViewVerb,
   &CvsWebLogVerb,
};

static const int SortedLowerCaseVerbArraySize = 
   sizeof(SortedLowerCaseVerbArray) / sizeof(SortedLowerCaseVerbArray[0]);

cvsCommandVerb TextToVerb(const std::string& Text)
{
   std::string s(Text);
   // Remove this later
   if (s == "CVSCommitDialog")
      s = "cvscommit";

   int High = SortedLowerCaseVerbArraySize-1;
   int Low = 0;
   int Middle;
   int ComparisonResult;

   MakeLowerCase(s);

   // Perform a binary search on the sorted verb text array
   // to locate the matching verb (enumeration element)
   do
   {                    
      Middle = (High + Low) / 2;

      ComparisonResult = s.compare(*SortedLowerCaseVerbArray[Middle]);
      if (ComparisonResult == 0)
         break;

      if (ComparisonResult < 0)
         High = Middle - 1;
      else
         Low = Middle + 1;
   } while (High >= Low);

   if (ComparisonResult == 0)
      return static_cast<cvsCommandVerb>(Middle);
   else
      return unknown;
}
