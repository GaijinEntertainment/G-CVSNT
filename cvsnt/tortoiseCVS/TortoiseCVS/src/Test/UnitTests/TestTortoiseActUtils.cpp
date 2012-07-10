// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2006 - Torsten Martinsen
// <torsten@vip.cybercity.dk> - October 2006

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

#include <Utils/PathUtils.h>
#include <TortoiseAct/TortoiseActUtils.h>


class TestTortoiseActUtils : public CppUnit::TestFixture
{
public:
    void setUp()
    {
        myBaseDir = __FILE__;
        MakeLowerCase(myBaseDir);
        myBaseDir = StripLastPart(StripLastPart(myBaseDir));
    }

    void TestComputeCommitSelectionMissing()
    {
        std::string add1(EnsureTrailingDelimiter(myBaseDir) + "add1.txt");
        added.push_back(make_pair(add1, std::string()));
        std::string ren1(EnsureTrailingDelimiter(myBaseDir) + "ren1.txt");
        renamed.push_back(make_pair(ren1, std::string()));
        
        userSelection.push_back(make_pair(ren1, CVSStatus::STATUS_RENAMED));
        CPPUNIT_ASSERT(!ComputeCommitSelection(userSelection, &modified, &added, &removed, &renamed, result));
    }

    void TestComputeCommitSelectionRename()
    {
        std::string add1(EnsureTrailingDelimiter(myBaseDir) + "add1.txt");
        added.push_back(make_pair(add1, std::string()));
        std::string ren1(EnsureTrailingDelimiter(myBaseDir) + "ren1.txt");
        renamed.push_back(make_pair(ren1, std::string()));

       userSelection.push_back(make_pair(add1, CVSStatus::STATUS_ADDED));
       userSelection.push_back(make_pair(ren1, CVSStatus::STATUS_RENAMED));
       CPPUNIT_ASSERT(ComputeCommitSelection(userSelection, &modified, &added, &removed, &renamed, result));

       CPPUNIT_ASSERT_EQUAL(size_t(1), result.size());
       CPPUNIT_ASSERT_EQUAL(myBaseDir, result[0]);
    }

    void TestComputeCommitSelectionAll()
    {
        std::string mod1(EnsureTrailingDelimiter(myBaseDir) + "mod1.txt");
        modified.push_back(make_pair(mod1, std::string()));
        std::string add1(EnsureTrailingDelimiter(myBaseDir) + "add1.txt");
        added.push_back(make_pair(add1, std::string()));
        std::string rem1(EnsureTrailingDelimiter(myBaseDir) + "rem1.txt");
        removed.push_back(make_pair(rem1, std::string()));
        std::string ren1(EnsureTrailingDelimiter(myBaseDir));
        ren1 += EnsureTrailingDelimiter("subdir");
        ren1 += "ren1.txt";
        renamed.push_back(make_pair(ren1, std::string()));
        
        userSelection.push_back(make_pair(mod1, CVSStatus::STATUS_CHANGED));
        userSelection.push_back(make_pair(add1, CVSStatus::STATUS_ADDED));
        userSelection.push_back(make_pair(ren1, CVSStatus::STATUS_RENAMED));
        userSelection.push_back(make_pair(rem1, CVSStatus::STATUS_REMOVED));
        CPPUNIT_ASSERT(ComputeCommitSelection(userSelection, &modified, &added, &removed, &renamed, result));

        CPPUNIT_ASSERT_EQUAL(size_t(4), result.size());
        std::vector<std::string> expected;
        expected.push_back(mod1);
        expected.push_back(add1);
        expected.push_back(rem1);
        expected.push_back(GetDirectoryPart(ren1));     // For renamed files, entire folder should be committed
        CPPUNIT_ASSERT(Includes(expected, result));
    }

    
private:
    CommitDialog::SelectedFiles userSelection;
    FilesWithBugnumbers added, modified, removed, renamed;
    std::vector<std::string> result;

    std::string myBaseDir;

    CPPUNIT_TEST_SUITE(TestTortoiseActUtils);
    CPPUNIT_TEST(TestComputeCommitSelectionMissing);
    CPPUNIT_TEST(TestComputeCommitSelectionRename);
    CPPUNIT_TEST(TestComputeCommitSelectionAll);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TestTortoiseActUtils, Suites::Utils());
