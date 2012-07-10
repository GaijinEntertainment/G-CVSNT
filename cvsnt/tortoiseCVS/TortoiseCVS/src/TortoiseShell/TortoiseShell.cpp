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

#if defined(__BORLANDC__)
#include <condefs.h>
USEUNIT("..\CVSGlue\CvsEntries.cpp");
USEUNIT("..\CVSGlue\CVSStatus.cpp");
USEUNIT("..\CVSGlue\FileTraversal.cpp");
USEUNIT("..\CVSGlue\getline.c");
USEUNIT("..\CVSGlue\ndir.c");
USEUNIT("..\CVSGlue\PreserveChdir.cpp");
USEUNIT("..\CVSGlue\TextBinary.cpp");
USEUNIT("..\CVSGlue\ustr.cpp");
USEUNIT("..\Utils\CrapLexer.cpp");
USEUNIT("..\Utils\PathUtils.cpp");
USEUNIT("..\Utils\TimeUtils.cpp");
USEUNIT("..\Utils\TortoiseRegistry.cpp");
USEUNIT("..\Utils\TortoiseUtils.cpp");
USEUNIT("..\Utils\UnicodeStrings.cpp");
USEUNIT("..\ContextMenus\DefaultMenus.cpp");
USEUNIT("..\ContextMenus\HasMenu.cpp");
USEUNIT("..\ContextMenus\MenuDescription.cpp");
USEUNIT("ContextMenu.cpp");
USEUNIT("IconOverlay.cpp");
USEUNIT("PropSheet.cpp");
USEUNIT("ShellExt.cpp");
USEDEF("ShellExt.def");
USERC("TortoiseShell.rc");
//---------------------------------------------------------------------------
#endif // defined(__BORLANDC__)

#define DllEntryPoint
