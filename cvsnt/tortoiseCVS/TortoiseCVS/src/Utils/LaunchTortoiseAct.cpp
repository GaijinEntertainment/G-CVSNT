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

#include "StdAfx.h"
#include "PathUtils.h"
#include "ShellUtils.h"
#include "TortoiseUtils.h"
#include "LaunchTortoiseAct.h"


void LaunchTortoiseAct(bool bWait, const std::string& verb, 
                       const std::vector<std::string> *files, 
                       const std::vector<std::string> *args,
                       HWND hwnd)
{
   TDEBUG_ENTER("LaunchTortoiseAct");
   // Launch the external acting command with appropriate parameters
   std::string externalApp = GetTortoiseDirectory() + "TortoiseAct.exe";
   std::string command = "\"" + externalApp + "\" ";
   command += verb;

   // Write list of files out to a temporary file
   // (this is just an easy way to pass the data to TortoiseAct.exe
   // - sending the actual filenames on the command line wouldn't work
   // under Windows as there is quite a short length limit)
   if (files && !files->empty())
   {
      std::string tempFile = UniqueTemporaryFile();
      std::ofstream out(tempFile.c_str(), std::ios::out);
      for (unsigned int i = 0; i < files->size(); ++i)
         out << (*files)[i] << std::endl;
      command += " -f \"" + tempFile + "\"";
   }

   // Write arguments
   if (args && !args->empty())
   {
      std::vector<std::string>::const_iterator it = args->begin();
      while (it != args->end())
      {
         command += " " + *it;
         it++;
      }
   }

   // Write window handle
   if (hwnd != 0)
   {
      char buf[100];
      _snprintf(buf, sizeof(buf), " -h 0x%p", reinterpret_cast<void*>(hwnd));
      command += buf;
   }

   TDEBUG_TRACE("LaunchTortoiseAct: " << command);

   if (!LaunchCommand(command, bWait))
   {
      wxString message = _("Failed to launch executable to perform action");
      message += wxT("\n");
      message += MultibyteToWide(command).c_str();
      MessageBox(0, message.c_str(), _("TortoiseCVS"), MB_OK);
      return;
   }
}

void LaunchTortoiseAct(bool bWait, const std::string& verb, 
                       const std::string& file, const std::string& args, 
                       HWND hwnd)
{
   std::vector<std::string> vFiles;
   std::vector<std::string> vArgs;

   if (!file.empty())
      vFiles.push_back(file);

   if (!args.empty())
      vArgs.push_back(args);
   
   LaunchTortoiseAct(bWait, verb, &vFiles, &vArgs, hwnd);
}
