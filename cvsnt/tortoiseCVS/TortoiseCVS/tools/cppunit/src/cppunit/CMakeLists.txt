# TortoiseCVS - a Windows shell extension for easy version control
#
# Copyright (C) 2008 - Torsten Martinsen
# <torsten@bullestock.net> - June 2008
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

INCLUDE_DIRECTORIES(
    ../../include
)

ADD_DEFINITIONS(
   
)

SET(CPP_SOURCE
   AdditionalMessage.cpp
   Asserter.cpp
   BriefTestProgressListener.cpp
   CompilerOutputter.cpp
   DefaultProtector.cpp
   DynamicLibraryManager.cpp
   DynamicLibraryManagerException.cpp
   Exception.cpp
   Message.cpp
   PlugInManager.cpp
   PlugInParameters.cpp
   Protector.cpp
   ProtectorChain.cpp
   RepeatedTest.cpp
   SourceLine.cpp
   StringTools.cpp
   SynchronizedObject.cpp
   Test.cpp
   TestAssert.cpp
   TestCase.cpp
   TestCaseDecorator.cpp
   TestComposite.cpp
   TestDecorator.cpp
   TestFactoryRegistry.cpp
   TestFailure.cpp
   TestLeaf.cpp
   TestNamer.cpp
   TestPath.cpp
   TestPlugInDefaultImpl.cpp
   TestResult.cpp
   TestResultCollector.cpp
   TestRunner.cpp
   TestSetUp.cpp
   TestSuccessListener.cpp
   TestSuite.cpp
   TestSuiteBuilderContext.cpp
   TextOutputter.cpp
   TextTestProgressListener.cpp
   TextTestResult.cpp
   TextTestRunner.cpp
   TypeInfoHelper.cpp
   Win32DynamicLibraryManager.cpp
   XmlDocument.cpp
   XmlElement.cpp
   XmlOutputter.cpp
   XmlOutputterHook.cpp
)

ADD_LIBRARY(cppunit STATIC ${CPP_SOURCE})
