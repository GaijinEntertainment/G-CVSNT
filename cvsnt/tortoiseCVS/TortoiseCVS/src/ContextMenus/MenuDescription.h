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

#ifndef MENU_DESCRIPTION_H
#define MENU_DESCRIPTION_H

#include <vector>
#include <string>
#include <windows.h>
#include "../Utils/FixWinDefs.h"

class MenuDescription
{
public:
   MenuDescription(const std::string& verb,
                   const std::string& menuText,
                   int flags,
                   const std::string& helpText,
                   const std::string& icon);
   MenuDescription();

   int          GetFlags() const;
   std::string  GetVerb() const;
   std::string  GetMenuText() const;
   std::string  GetHelpText() const;
   std::string  GetIcon() const;
   int          GetIndex() const;
   void         SetIndex(int index);

   // TODO: I _so_ don't want this header to have a dependence on windows.h
   // How to refactor it so it doesn't need to?  (For HWND)
   void Perform(const std::vector<std::string>& files, HWND hwnd);

private:
   // Flags
   int          myFlags;
   // Current index of menu entry. -1 if menu is not currently shown.
   int          myIndex;
   // Action to perform
   std::string  myVerb;
   // Icon name
   std::string  myIcon;
   // Menu item text
   std::string  myMenuText;
   // Help text shown in Explorer status bar
   std::string  myHelpText;
};


#endif
