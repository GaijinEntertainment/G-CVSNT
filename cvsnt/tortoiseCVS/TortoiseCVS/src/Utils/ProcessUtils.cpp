// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2002 - Torsten Martinsen
// <torsten@image.dk> - February 2002

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

#include <windows.h> // get user name
#include "FixWinDefs.h"
#include <lmcons.h>  // get user name

#include "ProcessUtils.h"
#include "TortoiseUtils.h"
#include "TortoiseException.h"
#include "Translate.h"
#include "ShellUtils.h"
#include "StringUtils.h"
#include "OsVersion.h"

// Remake the arguments, into the form they would need to 
// be typed into a shell.  Quotes are put round arguments
// which contain spaces.
std::string Requote(const std::vector<std::string>& args)
{
   std::string remade;
   int n = static_cast<int>(args.size());
   for (int i = 0; i < n; ++i)
   {
      bool quote = false;
      if (args[i].find_first_of(' ') != std::string::npos)
         quote = true;
      if (args[i].empty())
         quote = true;
      
      if (quote)
         remade += "\"";
      remade += args[i];
      if (quote)
         remade += "\"";
      if (i != n - 1)
         remade += " ";
   }
   
   return remade;
}


void LaunchURL(const std::string& url)
{
   DWORD Status = (DWORD) ShellExecuteA(0, "open", url.c_str(), 0, 0, SW_SHOWNORMAL);
   if (Status <= 32)
   {
      wxString message = Printf(_("Failed to launch %s"), MultibyteToWide(url).c_str());
      MessageBox(NULL, message.c_str(), _("TortoiseCVS"), MB_OK);
      return;
   }
}


void RestartExplorer()
{
    std::string command(GetTortoiseDirectory());
    command += "TortoiseSetupHelper";
    if (IsWow64())
        command += "64";
    command += ".exe ";
    command += wxAscii(_("Restarting Explorer"));
    command += " b";
    LaunchCommand(command, true, true);
}


std::string GetUserName()
{
   char userName[UNLEN + 1];
   DWORD len = UNLEN;
   if (GetUserNameA(userName, &len))
      return std::string(userName);
   TortoiseFatalError(_("GetUserName() failed"));
   return "";   // NOTREACHED
}
