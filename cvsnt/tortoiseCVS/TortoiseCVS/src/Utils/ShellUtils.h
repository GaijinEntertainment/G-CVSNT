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

#ifndef SHELL_UTILS_H
#define SHELL_UTILS_H

#include <windows.h>
#include "FixWinDefs.h"
#include <shlobj.h>
#include <string>
#include <vector>


// Explorer pidl data structure walking functions.
// These are probably compatible with Borland builder ones
// of the same name (judging by usenet posting snippets).
// _Who_ invented this stuff in an API?  Madness.
LPCITEMIDLIST GetNextItem(LPCITEMIDLIST pidl);
int GetItemCount(LPCITEMIDLIST pidl);
UINT GetSize(LPCITEMIDLIST pidl);
LPITEMIDLIST DuplicateItem(LPMALLOC pMalloc, LPCITEMIDLIST pidl);
// And some more found from usenet, ported from Delphi...
bool IsDesktopFolder(LPCITEMIDLIST pidl);
LPITEMIDLIST AppendPIDL(LPCITEMIDLIST dest, LPCITEMIDLIST src);
// And more...
void ItemListFree(LPITEMIDLIST pidl);
bool IsEqualPIDL(LPCITEMIDLIST a, LPCITEMIDLIST b); // this possibly doesn't work at all, test it when you try to use it
std::string DisplayNamePIDL(LPCITEMIDLIST pidl);

// From "Shell Clipboard Formats" in the MSDN library:
// << The following two macros can be used to retrieve PIDLs from a CIDA structure. 
// The first takes a pointer to the structure and retrieves the PIDL of the parent folder. 
// The second takes a pointer to the structure and retrieves one of the other PIDLs, 
// identified by its zero-based index. >>
#define GetPIDLFolder(pida) (LPCITEMIDLIST)(((LPBYTE)pida)+(pida)->aoffset[0])
#define GetPIDLItem(pida, i) (LPCITEMIDLIST)(((LPBYTE)pida)+(pida)->aoffset[i+1])

// Launch a command, optionally wait for termination
bool LaunchCommand(const std::string& command, bool waitForEnd, bool minimized = false);

// Return true if it is safe to view this file, i.e. it is not an executable
bool FileIsViewable(const std::string& filename);

// Launch a file to be opened by the registered type in the shell
bool LaunchFile(const std::string& filename, bool waitForEnd);

std::string DesktopFolder();

// Path of special folder
std::string GetSpecialFolder(int nFolder);

// Test if PIDL points to a special folder
bool IsSpecialFolder(LPCITEMIDLIST pidl, int nFolder);

// Get path from IDList
std::string GetPathFromIDList(LPCITEMIDLIST pidl);

// Strips the last ID from the list
LPITEMIDLIST StripLastID(LPCITEMIDLIST pidl);

// Get the last ID from the list
LPITEMIDLIST GetLastID(LPCITEMIDLIST pidl);

// Bind to parent
IShellFolder* BindToParent(LPCITEMIDLIST pidl);

// Is PIDL a shortcut
bool IsShortcut(LPCITEMIDLIST pidl);

// Get target of a shortcut
LPITEMIDLIST GetShortcutTarget(LPCITEMIDLIST pidl);

// Clone a PIDL
LPITEMIDLIST CloneIDList(LPCITEMIDLIST pidl);


// Rebuild icons
bool RebuildIcons();

// Notify shell of change
void ShellNotifyUpdateFile(const std::string& sFilename);

// Notify shell of change
void ShellNotifyUpdateFiles(const std::string& sDirname, 
                            const std::vector<std::string>& sFilenames);

// Notify shell of change
void ShellNotifyUpdateFiles(const std::vector<std::string>& sFilenames);

void ShellNotifyUpdateDir(const std::string& sDirname);

// Wait while processing messages 
DWORD WaitWithMsgQueue(DWORD nCount, const HANDLE* pHandles, bool fWaitAll,
                       DWORD dwMilliseconds);

// Get path for iconset
std::string GetIconSetPath(const std::string& iconSet);

#ifndef POSTINST
// Get name of iconset
wxString GetIconSetName(const std::string& iconSet);
#endif

// Get attributes for file (contains bugfix for SHGetFileInfo)
BOOL ShellGetFileAttributes(const char* filename, DWORD *attr);

// Get attributes for file (contains bugfix for SHGetFileInfo)
BOOL ShellGetFileAttributesPidl(LPCITEMIDLIST pidl, DWORD *attr);

// Get icon size
BOOL ShellGetIconSize(UINT iconsize, int *width, int *height);


#endif
