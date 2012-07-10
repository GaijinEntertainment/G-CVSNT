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
#include "Suite.h"

#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

namespace Suites 
{
   std::string Utils()
   {
      return "Utils";
   }
   std::string CVSGlue()
   {
      return "CVSGlue";
   }
   std::string Act()
   {
      return "TortoiseAct";
   }
}

namespace CppUnitTest
{
   CppUnit::Test* suite()
   {
      CppUnit::TestFactoryRegistry &registry = CppUnit::TestFactoryRegistry::getRegistry();

      registry.registerFactory(&CppUnit::TestFactoryRegistry::getRegistry(Suites::CVSGlue()));
      registry.registerFactory(&CppUnit::TestFactoryRegistry::getRegistry(Suites::Utils()));
      registry.registerFactory(&CppUnit::TestFactoryRegistry::getRegistry(Suites::Act()));

      return registry.makeTest();
   }

}  // namespace CppUnitTest
