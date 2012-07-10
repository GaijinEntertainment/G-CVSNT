// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - September 2000

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


#include <StdAfx.h>
#include "MenuDescription.h"
#include <Utils/PathUtils.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/LaunchTortoiseAct.h>
#include <Utils/TortoiseDebug.h>

MenuDescription::MenuDescription(const std::string& verb, const std::string& menuText,
                                 int flags, const std::string& helpText, const std::string& icon)
{
   TDEBUG_ENTER("MenuDescription::MenuDescription");
   TDEBUG_TRACE("Verb: '" << verb << "', Icon: '" << icon << "'");
   myFlags = flags;
   myVerb = verb;
   myMenuText = menuText;
   myHelpText = helpText;
   myIcon = icon;
}

// Visual C++ 5.0 needs a default constructor
MenuDescription::MenuDescription()
{
}

int MenuDescription::GetFlags() const
{
   return myFlags;
}

std::string MenuDescription::GetVerb() const
{
   return myVerb;
}

std::string MenuDescription::GetMenuText() const
{
   return myMenuText;
}

std::string MenuDescription::GetHelpText() const
{
   return myHelpText;
}

std::string MenuDescription::GetIcon() const
{
   return myIcon;
}

int MenuDescription::GetIndex() const
{
   return myIndex;
}

void MenuDescription::SetIndex(int index)
{
   myIndex = index;
}

void MenuDescription::Perform(const std::vector<std::string>& files, HWND hwnd)
{
   LaunchTortoiseAct(false, GetVerb(), &files, 0, hwnd);
}
