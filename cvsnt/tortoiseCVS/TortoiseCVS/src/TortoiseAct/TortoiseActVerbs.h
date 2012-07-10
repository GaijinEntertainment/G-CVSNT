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

#ifndef TORTOISEACTVERBS_H
#define TORTOISEACTVERBS_H

#include <string>

// Note: All constant verb strings declared must
// be completely lower-case; this is exploited
// when looking up verbs (saves CPU cycles)
// and the unit tests do assert this.

extern const std::string CvsAboutVerb;
extern const std::string CvsAddIgnoreExtVerb;
extern const std::string CvsAddIgnoreVerb;
extern const std::string CvsAddRecursiveVerb;
extern const std::string CvsAddVerb;
extern const std::string CvsAnnotateVerb;
extern const std::string CvsApplyPatchVerb;
extern const std::string CvsBranchVerb;
extern const std::string CvsCheckoutVerb;
extern const std::string CvsCommandVerb;
extern const std::string CvsCommitVerb;
extern const std::string CvsDiffVerb;
extern const std::string CvsEditVerb;
extern const std::string CvsExclusiveEditVerb;
extern const std::string CvsGetVerb;
extern const std::string CvsHelpVerb;
extern const std::string CvsHistoryVerb;
extern const std::string CvsListEditorsVerb;
extern const std::string CvsLogVerb;
extern const std::string CvsMakeModuleVerb;
extern const std::string CvsMakePatchVerb;
extern const std::string CvsMergeConflictsVerb;
extern const std::string CvsMergeVerb;
extern const std::string CvsPrefsVerb;
extern const std::string CvsRebuildIconsVerb;
extern const std::string CvsRefreshStatusVerb;
extern const std::string CvsReleaseVerb;
extern const std::string CvsRemoveVerb;
extern const std::string CvsRenameVerb;
extern const std::string CvsResolveConflictsVerb;
extern const std::string CvsRestoreVerb;
extern const std::string CvsRevisionGraphVerb;
extern const std::string CvsSaveAsVerb;
extern const std::string CvsSwitchVerb;
extern const std::string CvsTagVerb;
extern const std::string CvsTortoiseCodeVerb;
extern const std::string CvsUneditVerb;
extern const std::string CvsUpdateDialogVerb;
extern const std::string CvsUpdateVerb;
extern const std::string CvsUpdateCleanVerb;
extern const std::string CvsUrlVerb;
extern const std::string CvsViewVerb;
extern const std::string CvsWebLogVerb;
extern const std::string CvsWxWindowsCodeVerb;

// The enumeration elements must be kept in the same order 
// as the elements in the SortedLowerCaseVerb array.
enum cvsCommandVerb
{
   cvsAbout = 0,
   cvsAdd,
   cvsAddIgnore,
   cvsAddIgnoreExt,
   cvsAddRecursive,
   cvsAnnotate,
   cvsApplyPatch,
   cvsBranch,
   cvsCheckout,
   cvsCommand,
   cvsCommit,
   cvsDiff,
   cvsEdit,
   cvsExclusiveEdit,
   cvsGet,
   cvsHelp,
   cvsHistory,
   cvsListEditors,
   cvsLog,
   cvsMakeModule,
   cvsMakePatch,
   cvsMerge,
   cvsMergeConflicts,
   cvsPrefs,
   cvsRebuildIcons,
   cvsRefreshStatus,
   cvsRelease,
   cvsRemove,
   cvsRename,
   cvsResolveConflicts,
   cvsRestore,
   cvsRevisionGraph,
   cvsSaveAs,
   cvsSwitch,
   cvsTag,
   cvsTortoiseCode,
   cvsUnedit,
   cvsUpdate,
   cvsUpdateClean,
   cvsUpdateDialog,
   cvsUrl,
   cvsView,
   cvsWebLog,
   unknown // Must always be the last element.
};

// Recognize a verb passed as a string parameter
// and return an enumeration matching the verb.
// Return "unknown" if the passed in string does
// not match a known verb.
cvsCommandVerb TextToVerb(const std::string& Text);

#endif
