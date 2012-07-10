/*
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 1, or (at your option)
** any later version.

** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.

** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
 * Author : Alexandre Parenteau <aubonbeurre@hotmail.com> --- June 1998
 */

/*
 * CvsIgnore.h --- parse .cvsignore
 */

#ifndef CVSIGNORE_H
#define CVSIGNORE_H

#include <vector>
#include <string>
#include <windows.h>
#include "../Utils/FixWinDefs.h"
#include "../Utils/PathUtils.h"

// Open and parse .cvsignore in the current dir
void BuildIgnoredList(std::vector<std::string>& ignlist, const std::string& path,
                      DWORD *timeStamp = NULL, FileChangeParams *fcp = NULL);

// tells if the name matches an entry in global or local .cvsignore
bool MatchIgnoredList(const char* filename, const std::vector<std::string>& ignlist);

// tells if the name matches an entry in global .cvsignore
bool MatchIgnoredList(const char* filename);

// Invalidate cached ignoredlist
void InvalidateIgnoredCache();

// Build default ignored list
void BuildDefaultIgnoredList();

// Initialize
void InitIgnoredList();

#endif /* CVSIGNORE_H */
