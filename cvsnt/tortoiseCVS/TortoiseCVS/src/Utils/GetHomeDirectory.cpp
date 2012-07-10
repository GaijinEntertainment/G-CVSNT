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
#include "TortoiseRegistry.h"
#include "TortoiseUtils.h"
#include "Preference.h"


bool GetHomeDirectory(std::string& HomeDirectory)
{
   TDEBUG_ENTER("GetHomeDirectory");
   std::string home("");
   bool SuccessValue;

   // We usually recalculate home every time - so ignore the
   // value in the registry.  The user can turn that off though!
   if (!GetBooleanPreference("Always Recalculate Home"))
      home = GetStringPreference("HOME");

   if (home.empty() || !IsDirectory(home.c_str()))
      SuccessValue = GetCalculatedHomeDirectory(home);
   else
      SuccessValue = true;

   TDEBUG_TRACE("Home directory is " << home);
 
   SuccessValue = (SuccessValue && !home.empty() && IsDirectory(home.c_str()));
   if (SuccessValue)
      HomeDirectory = home;

   return SuccessValue;
}
