// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2005 - Torsten Martinsen
// <torsten@tiscali.dk> - November 2005

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
#include <cppunit/extensions/HelperMacros.h>
#include "suite.h"

#include <TortoiseAct/TortoiseActVerbs.h>


#define TestTransformation(text, enum_elem)                             \
   CPPUNIT_ASSERT_EQUAL(text, *SortedLowerCaseVerbArray[enum_elem]);    \
   CPPUNIT_ASSERT_EQUAL(TextToVerb(text), enum_elem);                   \
   TestedArray[enum_elem] = true;                                       \
   ArrayEntryCounter++;


class TestVerbs : public CppUnit::TestFixture
{
public:
   void TestTextToEnumTransformation()
   {
      int ArrayEntryCounter = 0;
      bool TestedArray[SortedLowerCaseVerbArraySize];

      // Globally set the array contents to zero
      memset(&TestedArray, 0, sizeof(TestedArray));

      CPPUNIT_ASSERT_EQUAL(static_cast<int>(unknown), SortedLowerCaseVerbArraySize);

      TestTransformation(CvsAboutVerb, cvsAbout);
      TestTransformation(CvsAddIgnoreVerb, cvsAddIgnore);
      TestTransformation(CvsAddIgnoreExtVerb, cvsAddIgnoreExt);
      TestTransformation(CvsAddRecursiveVerb, cvsAddRecursive);
      TestTransformation(CvsAddVerb, cvsAdd);
      TestTransformation(CvsAnnotateVerb, cvsAnnotate);
      TestTransformation(CvsApplyPatchVerb, cvsApplyPatch);
      TestTransformation(CvsBranchVerb, cvsBranch);
      TestTransformation(CvsCheckoutVerb, cvsCheckout);
      TestTransformation(CvsCommandVerb, cvsCommand);
      TestTransformation(CvsCommitVerb, cvsCommit);
      TestTransformation(CvsDiffVerb, cvsDiff);
      TestTransformation(CvsEditVerb, cvsEdit);
      TestTransformation(CvsExclusiveEditVerb, cvsExclusiveEdit);
      TestTransformation(CvsGetVerb, cvsGet);
      TestTransformation(CvsHelpVerb, cvsHelp);
      TestTransformation(CvsHistoryVerb, cvsHistory);
      TestTransformation(CvsListEditorsVerb, cvsListEditors);
      TestTransformation(CvsLogVerb, cvsLog);
      TestTransformation(CvsMakeModuleVerb, cvsMakeModule);
      TestTransformation(CvsMakePatchVerb, cvsMakePatch);
      TestTransformation(CvsMergeConflictsVerb, cvsMergeConflicts);
      TestTransformation(CvsMergeVerb, cvsMerge);
      TestTransformation(CvsPrefsVerb, cvsPrefs);
      TestTransformation(CvsRebuildIconsVerb, cvsRebuildIcons);
      TestTransformation(CvsRefreshStatusVerb, cvsRefreshStatus);
      TestTransformation(CvsReleaseVerb, cvsRelease);
      TestTransformation(CvsRemoveVerb, cvsRemove);
      TestTransformation(CvsRenameVerb, cvsRename);
      TestTransformation(CvsResolveConflictsVerb, cvsResolveConflicts);
      TestTransformation(CvsRestoreVerb, cvsRestore);
      TestTransformation(CvsRevisionGraphVerb, cvsRevisionGraph);
      TestTransformation(CvsSaveAsVerb, cvsSaveAs);
      TestTransformation(CvsTagVerb, cvsTag);
      TestTransformation(CvsTortoiseCodeVerb, cvsTortoiseCode);
      TestTransformation(CvsUneditVerb, cvsUnedit);
      TestTransformation(CvsUpdateDialogVerb, cvsUpdateDialog);
      TestTransformation(CvsUpdateVerb, cvsUpdate);
      TestTransformation(CvsUpdateCleanVerb, cvsUpdateClean);
      TestTransformation(CvsUrlVerb, cvsUrl);
      TestTransformation(CvsViewVerb, cvsView);
      TestTransformation(CvsWebLogVerb, cvsWebLog);

      CPPUNIT_ASSERT_EQUAL(ArrayEntryCounter, SortedLowerCaseVerbArraySize);

      for (int i = 0; i < SortedLowerCaseVerbArraySize; i++)
         CPPUNIT_ASSERT(TestedArray[i]);
   }

   void TestSortedLowerCaseVerbArray()
   {
      for (unsigned int i = 0; i < SortedLowerCaseVerbArraySize-1; i++)
      {
         CPPUNIT_ASSERT(*SortedLowerCaseVerbArray[i] < *SortedLowerCaseVerbArray[i+1]);
         CPPUNIT_ASSERT(IsLowerCaseString(*SortedLowerCaseVerbArray[i]));
      }

      CPPUNIT_ASSERT(IsLowerCaseString(*SortedLowerCaseVerbArray[SortedLowerCaseVerbArraySize-1]));
   }

   bool IsLowerCaseString(const std::string& s)
   {
      std::string TempString(s);
      MakeLowerCase(TempString);
      
      return (TempString == s);
   }

   CPPUNIT_TEST_SUITE(TestVerbs);
   CPPUNIT_TEST(TestTextToEnumTransformation);
   CPPUNIT_TEST(TestSortedLowerCaseVerbArray);
   CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TestVerbs, Suites::Act());
