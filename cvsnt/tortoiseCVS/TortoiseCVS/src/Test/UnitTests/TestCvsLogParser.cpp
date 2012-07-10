// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2009 - Torsten Martinsen
// <torsten@bullestock.net> - October 2009

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

#include <cvstree/common.h>

class TestCvsLogParser : public CppUnit::TestFixture
{
public:
   void TestRevNum()
   {
      CRevNumber r1("1.1");
      CRevNumber r2("1.2");
      CPPUNIT_ASSERT_EQUAL(0, r1.cmp(r1));
      CPPUNIT_ASSERT_EQUAL(-1, r1.cmp(r2));
      CPPUNIT_ASSERT_EQUAL(1, r2.cmp(r1));
   }

   void TestParse()
   {
      std::string path(GetDirectoryPart(__FILE__));
      std::string file(path+"/log.txt");
      std::istringstream iss;
      iss.str(GetFileContents(file));
      std::ostringstream oss;
      FlexLexer* lexer = CreateCvsLogLexer(&iss, &oss);
      lexer->set_debug(1);
      ParserData parserData;
      StartCvsLogParser(lexer, &parserData, true);
      delete lexer;
      parserData.rcsFile->print(std::cout);
      CLogNode* tree = CvsLogGraph(parserData);
   }

   std::string GetFileContents(const std::string& filename)
   {
      std::string path(__FILE__);
      path = GetDirectoryPart(path);
      path = EnsureTrailingDelimiter(path);
      path += filename;
      HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL, OPEN_EXISTING, 0, 0);
      CPPUNIT_ASSERT(h != INVALID_HANDLE_VALUE);
      DWORD highSize = 0;
      DWORD lowSize = GetFileSize(h, &highSize);
      if (lowSize == INVALID_FILE_SIZE)
      {
         // We must check the error code to determine if this really is an error.
         CPPUNIT_ASSERT_EQUAL(GetLastError(), static_cast<DWORD>(NO_ERROR));
      }
      CloseHandle(h);
      std::ifstream is(path.c_str(), std::ios_base::binary);
      CPPUNIT_ASSERT(is.good());
      char* buffer(new char[lowSize]);
      is.read(buffer, lowSize);
      CPPUNIT_ASSERT(!is.fail());
      std::string s(buffer, lowSize);
      delete[] buffer;
      return s;
   }
   
   CPPUNIT_TEST_SUITE(TestCvsLogParser);
   CPPUNIT_TEST(TestRevNum);
   CPPUNIT_TEST(TestParse);
   CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TestCvsLogParser, Suites::CVSGlue());
