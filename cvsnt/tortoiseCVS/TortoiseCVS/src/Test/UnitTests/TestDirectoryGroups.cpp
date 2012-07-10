// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2006 - Torsten Martinsen
// <torsten@vip.cybercity.dk> - September 2006

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

#include <TortoiseAct/DirectoryGroup.h>

class TestDirectoryGroups : public CppUnit::TestFixture
{
public:
    void setUp()
    {
        myBaseDir = __FILE__;
        MakeLowerCase(myBaseDir);
        myBaseDir = StripLastPart(StripLastPart(myBaseDir));
    }

    void TestFolderAndSubfolder()
    {
        std::string dir1 = myBaseDir;
        std::string dir2 = EnsureTrailingDelimiter(myBaseDir);
        dir2 += "unittests";
        std::vector<std::string> dirs;
        dirs.push_back(dir1);
        dirs.push_back(dir2);
        DirectoryGroups groups;
        CPPUNIT_ASSERT(groups.PreprocessFileList(dirs));
        CPPUNIT_ASSERT_EQUAL((size_t) 1, groups.size());
        DirectoryGroup& group = groups[0];
        CPPUNIT_ASSERT_EQUAL((size_t) 2, group.size());
        CPPUNIT_ASSERT_EQUAL(StripLastPart(myBaseDir), group.myDirectory);
        CPPUNIT_ASSERT_EQUAL(std::string(":ssh:bullestock@tortoisecvs.cvs.sourceforge.net:/cvsroot/tortoisecvs"), group.myCVSRoot);
        CPPUNIT_ASSERT_EQUAL(std::string("test"), group[0].myRelativeFile);
        CPPUNIT_ASSERT_EQUAL(dir1, group[0].myAbsoluteFile);
        CPPUNIT_ASSERT_EQUAL(std::string("test\\unittests"), group[1].myRelativeFile);
        CPPUNIT_ASSERT_EQUAL(dir2, group[1].myAbsoluteFile);
    }

    void TestFilesInSameFolder()
    {
        std::string file1 = EnsureTrailingDelimiter(myBaseDir);
        file1 += "unittests";
        std::string basedir = file1;
        file1 = EnsureTrailingDelimiter(file1);
        std::string file2 = file1;
        file1 += "file1.txt";
        file2 += "file2.txt";
        std::vector<std::string> files;
        files.push_back(file1);
        files.push_back(file2);
        DirectoryGroups groups;
        CPPUNIT_ASSERT(groups.PreprocessFileList(files));
        CPPUNIT_ASSERT_EQUAL((size_t) 1, groups.size());
        DirectoryGroup& group = groups[0];
        CPPUNIT_ASSERT_EQUAL((size_t) 2, group.size());
        CPPUNIT_ASSERT_EQUAL(basedir, group.myDirectory);
        CPPUNIT_ASSERT_EQUAL(std::string("file1.txt"), group[0].myRelativeFile);
        CPPUNIT_ASSERT_EQUAL(file1, group[0].myAbsoluteFile);
        CPPUNIT_ASSERT_EQUAL(std::string("file2.txt"), group[1].myRelativeFile);
        CPPUNIT_ASSERT_EQUAL(file2, group[1].myAbsoluteFile);
    }

    void TestFilesInTopFolder()
    {
        std::string root = StripLastPart(StripLastPart(myBaseDir));
        std::string file1 = EnsureTrailingDelimiter(root);
        file1 += "Readme.Overview.txt";
        std::string file2 = EnsureTrailingDelimiter(root);
        file2 += "TortoiseCVS.url";
        std::vector<std::string> files;
        files.push_back(file1);
        files.push_back(file2);
        DirectoryGroups groups;
        CPPUNIT_ASSERT(groups.PreprocessFileList(files));
        CPPUNIT_ASSERT_EQUAL((size_t) 1, groups.size());
        DirectoryGroup& group = groups[0];
        CPPUNIT_ASSERT_EQUAL((size_t) 2, group.size());
        CPPUNIT_ASSERT_EQUAL(root, group.myDirectory);
        CPPUNIT_ASSERT_EQUAL(std::string("Readme.Overview.txt"), group[0].myRelativeFile);
        CPPUNIT_ASSERT_EQUAL(file1, group[0].myAbsoluteFile);
        CPPUNIT_ASSERT_EQUAL(std::string("TortoiseCVS.url"), group[1].myRelativeFile);
        CPPUNIT_ASSERT_EQUAL(file2, group[1].myAbsoluteFile);
    }
    
    void TestFilesAndFoldersInSameFolder()
    {
        std::string file = EnsureTrailingDelimiter(myBaseDir);
        file += "wxwmainstub.cpp";
        std::string dir = EnsureTrailingDelimiter(myBaseDir);
        dir += "unittests";
        std::vector<std::string> dirs;
        dirs.push_back(file);
        dirs.push_back(dir);
        DirectoryGroups groups;
        CPPUNIT_ASSERT(groups.PreprocessFileList(dirs));
        CPPUNIT_ASSERT_EQUAL((size_t) 1, groups.size());
        DirectoryGroup& group = groups[0];
        CPPUNIT_ASSERT_EQUAL((size_t) 2, group.size());
        CPPUNIT_ASSERT_EQUAL(myBaseDir, group.myDirectory);
        CPPUNIT_ASSERT_EQUAL(std::string("wxwmainstub.cpp"), group[0].myRelativeFile);
        CPPUNIT_ASSERT_EQUAL(file, group[0].myAbsoluteFile);
        CPPUNIT_ASSERT_EQUAL(std::string("unittests"), group[1].myRelativeFile);
        CPPUNIT_ASSERT_EQUAL(dir, group[1].myAbsoluteFile);
    }

    void TestFilesInFolderAndSubfolder()
    {
        std::string file1 = EnsureTrailingDelimiter(myBaseDir);
        file1 += "wxwmainstub.cpp";
        std::string file2 = EnsureTrailingDelimiter(myBaseDir);
        file2 += EnsureTrailingDelimiter("unittests");
        file2 += "testdirectorygroups.cpp";
        std::vector<std::string> files;
        files.push_back(file1);
        files.push_back(file2);
        DirectoryGroups groups;
        CPPUNIT_ASSERT(groups.PreprocessFileList(files));
        CPPUNIT_ASSERT_EQUAL((size_t) 1, groups.size());
        DirectoryGroup& group = groups[0];
        CPPUNIT_ASSERT_EQUAL((size_t) 2, group.size());
        CPPUNIT_ASSERT_EQUAL(myBaseDir, group.myDirectory);
        CPPUNIT_ASSERT_EQUAL(std::string("wxwmainstub.cpp"), group[0].myRelativeFile);
        CPPUNIT_ASSERT_EQUAL(file1, group[0].myAbsoluteFile);
        CPPUNIT_ASSERT_EQUAL(std::string("unittests\\testdirectorygroups.cpp"), group[1].myRelativeFile);
        CPPUNIT_ASSERT_EQUAL(file2, group[1].myAbsoluteFile);
    }

    void TestMultipleFiles()
    {
        // docs\algeco-install.xml
        // docs\data-preparation\list_fic.bat
        // docs\schemas\logic-edito.svg
        // www\.htaccess
        // www\modules\_dataBase\config.php
        // www\modules\cachegen\js\cachegen.js

        std::string root = StripLastPart(StripLastPart(myBaseDir));
        std::string admin(root);
        admin += "\\admin";
        std::string f1(admin);
        f1 += "\\publicity.txt";
// admin\publicity.txt
        std::string f2(admin);
        f2 += "\\archive\\slashdot_1.html";
// admin\archive\slashdot_1.html
        std::string f3(admin);
        f3 += "\\t-shirt\\checkmeout.png";
// admin\t-shirt\checkmeout.png
        std::string src(root);
        src += "\\src";
        std::string f4(src);
        f4 += "\\.cvsignore";
// src\.cvsignore
        std::string f5(src);
        f5 += "\\cvsnt\\protocols\\ext.dll";
// src\cvsnt\protocols\ext.dll
        std::string f6(src);
        f6 += "\\cvsnt\\triggers\\info.dll";
// src\cvsnt\triggers\info.dll
        std::vector<std::string> files;
        files.push_back(f1);
        files.push_back(f2);
        files.push_back(f3);
        files.push_back(f4);
        files.push_back(f5);
        files.push_back(f6);
        DirectoryGroups groups;
        CPPUNIT_ASSERT(groups.PreprocessFileList(files));
        CPPUNIT_ASSERT_EQUAL((size_t) 1, groups.size());
        DirectoryGroup& group = groups[0];
        CPPUNIT_ASSERT_EQUAL((size_t) 6, group.size());
        CPPUNIT_ASSERT_EQUAL(root, group.myDirectory);
        CPPUNIT_ASSERT_EQUAL(std::string("admin\\publicity.txt"), group[0].myRelativeFile);
        CPPUNIT_ASSERT_EQUAL(f1, group[0].myAbsoluteFile);
        CPPUNIT_ASSERT_EQUAL(std::string("admin\\archive\\slashdot_1.html"), group[1].myRelativeFile);
        CPPUNIT_ASSERT_EQUAL(f2, group[1].myAbsoluteFile);
    }

    void TestMultipleModules()
    {
        std::string tcvsroot = StripLastPart(StripLastPart(myBaseDir));
        std::string poroot = StripLastPart(tcvsroot);
        poroot = EnsureTrailingDelimiter(poroot) + "po";
        std::vector<std::string> dirs;
        dirs.push_back(tcvsroot);
        dirs.push_back(poroot);
        DirectoryGroups groups;
        CPPUNIT_ASSERT(groups.PreprocessFileList(dirs));
        CPPUNIT_ASSERT_EQUAL((size_t) 1, groups.size());
        DirectoryGroup& group = groups[0];
        std::vector<DirectoryGroup::Entry*> entries = group.myEntries;
        CPPUNIT_ASSERT_EQUAL((size_t) 2, entries.size());
        CPPUNIT_ASSERT_EQUAL(std::string("..\\.."), group.myDirectory);
        CPPUNIT_ASSERT_EQUAL(tcvsroot, entries[0]->myAbsoluteFile);
        CPPUNIT_ASSERT_EQUAL(poroot, entries[1]->myAbsoluteFile);
    }
    
    std::string myBaseDir;
    
    CPPUNIT_TEST_SUITE(TestDirectoryGroups);
    CPPUNIT_TEST(TestFolderAndSubfolder);
    CPPUNIT_TEST(TestFilesInSameFolder);
    CPPUNIT_TEST(TestFilesInTopFolder);
    CPPUNIT_TEST(TestFilesAndFoldersInSameFolder);
    CPPUNIT_TEST(TestFilesInFolderAndSubfolder);
    CPPUNIT_TEST(TestMultipleFiles);
    CPPUNIT_TEST(TestMultipleModules);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TestDirectoryGroups, Suites::Act());
