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

#include <CVSGlue/CVSRoot.h>

class TestCVSRoot : public CppUnit::TestFixture
{
public:

   void TestGetComponents()
   {
      CVSRoot root;

      // :ext: with username
      root.SetCVSROOT(":ext:bullestock@cvs.tortoisecvs.sourceforge.net:/cvsroot/tortoisecvs");
      CPPUNIT_ASSERT_EQUAL(std::string("ext"), root.GetProtocol());
      CPPUNIT_ASSERT_EQUAL(std::string("bullestock"), root.GetUser());
      CPPUNIT_ASSERT_EQUAL(std::string("cvs.tortoisecvs.sourceforge.net"), root.GetServer());
      CPPUNIT_ASSERT_EQUAL(std::string("/cvsroot/tortoisecvs"), root.GetDirectory());
      CPPUNIT_ASSERT_EQUAL(std::string(""), root.GetPort());

      // :sserver: with username and port
      root.SetCVSROOT(":sserver:ddoubleday@freepository.com:3436/remotePath");
      CPPUNIT_ASSERT_EQUAL(std::string("sserver"), root.GetProtocol());
      CPPUNIT_ASSERT_EQUAL(std::string("ddoubleday"), root.GetUser());
      CPPUNIT_ASSERT_EQUAL(std::string("freepository.com"), root.GetServer());
      CPPUNIT_ASSERT_EQUAL(std::string("/remotePath"), root.GetDirectory());
      CPPUNIT_ASSERT_EQUAL(std::string("3436"), root.GetPort());

      // :pserver: with username
      root.SetCVSROOT(":pserver:tma@cvs-gh-it.gatehouse:/cvs/gh");
      CPPUNIT_ASSERT_EQUAL(std::string("pserver"), root.GetProtocol());
      CPPUNIT_ASSERT_EQUAL(std::string("tma"), root.GetUser());
      CPPUNIT_ASSERT_EQUAL(std::string("cvs-gh-it.gatehouse"), root.GetServer());
      CPPUNIT_ASSERT_EQUAL(std::string("/cvs/gh"), root.GetDirectory());
      CPPUNIT_ASSERT_EQUAL(std::string(""), root.GetPort());

      // :pserver: without username
      root.SetCVSROOT(":pserver:cvs-gh-it.gatehouse:/cvs/gh");
      CPPUNIT_ASSERT_EQUAL(std::string("pserver"), root.GetProtocol());
      CPPUNIT_ASSERT_EQUAL(std::string(""), root.GetUser());
      CPPUNIT_ASSERT_EQUAL(std::string("cvs-gh-it.gatehouse"), root.GetServer());
      CPPUNIT_ASSERT_EQUAL(std::string("/cvs/gh"), root.GetDirectory());
      CPPUNIT_ASSERT_EQUAL(std::string(""), root.GetPort());
   }
   
   CPPUNIT_TEST_SUITE(TestCVSRoot);
   CPPUNIT_TEST(TestGetComponents);
   CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TestCVSRoot, Suites::CVSGlue());
