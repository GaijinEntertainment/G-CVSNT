// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - May 2000

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

// Notes on cvsglue.dsp:
// This is a test project for testing glue code by itself.

#include <stdafx.h>

#include "../Utils/FixCompilerBugs.h"
#include <wx/wx.h>
#include <windows.h>
#include "../Utils/FixWinDefs.h"
#include <iostream>
#include <io.h>
#include <fcntl.h>
#include "CVSStatus.h"
#include "CVSAction.h"
#include "CVSRoot.h"
#include "../Utils/StringUtils.h"
#include "../Utils/TortoiseDebug.h"


void StatusTest(std::string file)
{
   CVSStatus::FileStatus status = CVSStatus::GetFileStatus(file);
   std::cout << "Status of " << file << ": " << status << std::endl;
   
   if (status == CVSStatus::STATUS_NOWHERENEAR_CVS)
      std::cout << "STATUS_NOWHERENEAR_CVS";
   if (status == CVSStatus::STATUS_INCVS_RO)
      std::cout << "STATUS_INCVS_RO";
   if (status == CVSStatus::STATUS_INCVS_RW)
      std::cout << "STATUS_INCVS_RW";
   if (status == CVSStatus::STATUS_CHANGED)
      std::cout << "STATUS_CHANGED";
   if (status == CVSStatus::STATUS_CONFLICT)
      std::cout << "STATUS_CONFLICT";
   if (status == CVSStatus::STATUS_NOTINCVS)
      std::cout << "STATUS_NOTINCVS";
   if (status == CVSStatus::STATUS_OUTERDIRECTORY)
      std::cout << "STATUS_OUTERDIRECTORY";
   if (status == CVSStatus::STATUS_IGNORED)
      std::cout << "STATUS_IGNORED";
   if (status == CVSStatus::STATUS_STATIC)
      std::cout << "STATUS_STATIC";
   
   std::cout << std::endl;
}

class MyApp : public wxApp
{
public:
   bool OnInit();
};

DECLARE_APP(MyApp)

IMPLEMENT_APP(MyApp)

bool MyApp::OnInit()
{
   return true;
}

static bool TestCVSRoot()
{
   CVSRoot root;

   // :ext: with username
   root.SetCVSROOT(":ext:bullestock@cvs.tortoisecvs.sourceforge.net:/cvsroot/tortoisecvs");
   ASSERT(root.GetProtocol() == "ext");
   ASSERT(root.GetUser() == "bullestock");
   ASSERT(root.GetServer() == "cvs.tortoisecvs.sourceforge.net");
   ASSERT(root.GetDirectory() == "/cvsroot/tortoisecvs");
   ASSERT(root.GetPort() == "");

   // :sserver: with username and port
   root.SetCVSROOT(":sserver:ddoubleday@freepository.com:3436/remotePath");
   ASSERT(root.GetProtocol() == "sserver");
   ASSERT(root.GetUser() == "ddoubleday");
   ASSERT(root.GetServer() == "freepository.com");
   ASSERT(root.GetDirectory() == "/remotePath");
   ASSERT(root.GetPort() == "3436");

   // :pserver: with username
   root.SetCVSROOT(":pserver:tma@cvs-gh-it.gatehouse:/cvs/gh");
   ASSERT(root.GetProtocol() == "pserver");
   ASSERT(root.GetUser() == "tma");
   ASSERT(root.GetServer() == "cvs-gh-it.gatehouse");
   ASSERT(root.GetDirectory() == "/cvs/gh");
   ASSERT(root.GetPort() == "");

   // :pserver: without username
   root.SetCVSROOT(":pserver:cvs-gh-it.gatehouse:/cvs/gh");
   ASSERT(root.GetProtocol() == "pserver");
   ASSERT(root.GetUser() == "");
   ASSERT(root.GetServer() == "cvs-gh-it.gatehouse");
   ASSERT(root.GetDirectory() == "/cvs/gh");
   ASSERT(root.GetPort() == "");

   return true;
}

void main()
{
   std::cout << "CVS glue test" << std::endl;

//   TestCVSRoot();
   
#if 0
   std::string protocol, userName, password, serverMachine, repositoryDirectory, module;
   if (DoCheckoutDialog(protocol, userName, password, serverMachine, repositoryDirectory, module))
   {
      std::cout << "OK" << std::endl;
      std::cout << protocol << " " << userName << " " << password << " " << serverMachine
                << " " << repositoryDirectory << " " << module;
   }
   else 
   {
      std::cout << "Cancel" << std::endl;
      return;
   }
#endif
   
#if 0
   bool text;
   // std::string file = "D:\\\\cvswork\\\\DockingStation\\\\Code\\\\website\\\\website.sln";
   // std::string file = "D:\\\\cvswork\\\\TortoiseCVS\\\\PathUtils.h";
   std::string file = "D:\\\\cvswork\\\\tester\\\\foo";
   bool result = DoTextBinaryDialog(text, file);
   return;
#endif
   
#if 0
   CVSAction glue;
   glue.SetCloseIfOK(false);
   {
      MakeArgs args;
      args.add("update");
      args.add(".");
      glue.Command("D:\\\\cvswork\\\\foo", args);
   }
#endif
   
#if 0
   // CVSStatus::GetFileStatus("D:\\\\cvswork\\\\test\\\\tester\\\\Copy (2) of moose.txt");
   
   //StatusTest("e:\\cvswork\\testeroo\\tester\\a\\b.txt");
   //StatusTest("e:\\cvswork\\testeroo\\tester\\a\\needs.txt");

   StatusTest("a:\\CVSROOT\\taginfo");
   StatusTest("c:\\user\\tma\\TortoiseCVS\\src\\CVSGlue\\CVSStatus.cpp");
   StatusTest("c:\\user\\tma\\TortoiseCVS\\src\\CVSGlue");
   StatusTest("c:\\user\\tma\\TortoiseCVS\\src\\test");
#endif
   
#if 0
   std::vector<std::string> needsAdding;
// CVSStatus::RecursiveNeedsAdding("e:\\cvswork\\testeroo\\tester\\a", needsAdding);
   CVSStatus::RecursiveNeedsAdding("e:\\cvswork\\tortoisecvs\\wxw", needsAdding);
   {
      std::cout << "NEEDS ADDING: ";
      for (int i = 0; i < needsAdding.size(); ++i)
         std::cout << needsAdding[i] << std::endl;
      std::cout << std::endl << std::endl;
   }

   std::vector<std::string> modified, added, removed;
   CVSStatus::RecursiveScan("e:\\cvswork\\testeroo\\tester\\a", modified, added, removed);
   {
      std::cout << "MODIFIED: ";
      for (int i = 0; i < modified.size(); ++i)
         std::cout << modified[i] << " ";
      std::cout << std::endl << std::endl;
   }
   {
      std::cout << "ADDED: ";
      for (int i = 0; i < added.size(); ++i)
         std::cout << added[i] << " ";
      std::cout << std::endl << std::endl;
   }
   {
      std::cout << "REMOVED: ";
      for (int i = 0; i < removed.size(); ++i)
         std::cout << removed[i] << " ";
      std::cout << std::endl << std::endl;
   }
#endif
   
   StatusTest("c:\\autoexec.bat");
   StatusTest("c:\\user\\tma\\tcvs\\head\\TortoiseCVS\\src\\TortoiseCVS.opt");
   StatusTest("c:\\temp\\New Text Document.txt");
   StatusTest("D:\\mytest\\sandbox\\static.txt");
   std::cout << "HasStickyTag: " << CVSStatus::HasStickyTag("c:\\user\\tma\\tcvs\\1.6.x\\TortoiseCVS\\src\\.cvsignore") << std::endl;
   std::cout << "HasStickyDate: " << CVSStatus::HasStickyDate("c:\\user\\tma\\tcvs\\1.6.x\\TortoiseCVS\\src\\.cvsignore") << std::endl;
   std::cout << "GetStickyDate: " << CVSStatus::GetStickyDate("c:\\user\\tma\\tcvs\\1.6.x\\TortoiseCVS\\src\\.cvsignore") << std::endl;
}
