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

#ifndef PROCESS_UTILS_H
#define PROCESS_UTILS_H

#include <string>
#include <vector>
#include "FixWinDefs.h"

// Launch a URL in a browser
void LaunchURL(const std::string& url);

// Restart Windows Explorer
void RestartExplorer();


// Remake the arguments, into the form they would need to 
// be typed into a shell.  Quotes are put round arguments
// which contain spaces.
std::string Requote(const std::vector<std::string>& args);

// Get Windows user name
std::string GetUserName();

#endif
