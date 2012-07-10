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

#ifndef HAS_MENU_H
#define HAS_MENU_H

#include <vector>
#include <string>

enum MenuFlags
{
   // General parameters:
   ON_SUB_MENU = 1,

   // Whether the menu is present:

   EXACTLY_ONE = 2, // There must be only one file/directory
   ALWAYS_HAS = 4, // The menu is always present
   HAS_EDIT = 8,
   HAS_EXCL_EDIT = 16,
   
   // By default, at least one file must match the TYPE_
   // flags below.  Use this to force it to be all files.
   ALL_MUST_MATCH_TYPE = 32,

   // These correspond to CVSStatus::STATUS_*
   // If several are present, it counts as an "or" between them
   TYPE_IGNORED = 64,
   TYPE_NOTINCVS = 128,
   TYPE_NOWHERENEAR_CVS = 256,
   TYPE_INCVS_RO = 512,
   TYPE_INCVS_RW = 1024,
   TYPE_INCVS_LOCKED = 2048,
   TYPE_ADDED = 4096,
   TYPE_CHANGED = 8192,
   TYPE_RENAMED = 16384,
   TYPE_CONFLICT = 32768,
   TYPE_STATIC = 65536,
   TYPE_OUTERDIRECTORY = 131072, // Special for top level directories that contain stuff which is in CVS
   
   // If multiple ones of these are present, then they
   // must all be true (along with the STATUS_ type flags above)
   TYPE_DIRECTORY = 262144,
   TYPE_FILE = 524288
};

// Tests the given files against the flags to see if a particular
// menu item should be present.
bool HasMenu(int flags, const std::vector<std::string>& files);

void HasMenus(std::vector<int> menuFlags, const std::vector<std::string>& files, std::vector<bool>& hasMenus);



#endif
