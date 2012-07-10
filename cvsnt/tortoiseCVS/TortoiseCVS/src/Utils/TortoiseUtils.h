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

#ifndef TORTOISE_UTILS_H
#define TORTOISE_UTILS_H

#include "FixCompilerBugs.h"
#include <string>
#include <vector>
#include <windows.h>
#include "FixWinDefs.h"

#include "OsVersion.h"

// Get install dir (always includes trailing slash)
std::string GetTortoiseDirectory();

// Retrieve the home directory value - this may be a user-defined
// location or automatically calculated data.
// Return "true" on success; HomeDirectory will be set.
// If "false" is returned, HomeDirectory is unmodified
// and it was not possible to determine a home directory.
bool GetHomeDirectory(std::string& HomeDirectory);

// Retrieve the *calculated* home directory value.
// Return "true" on success; HomeDirectory will be set.
// If "false" is returned, HomeDirectory is unmodified
// and it was not possible to determine a home directory.
bool GetCalculatedHomeDirectory(std::string& HomeDirectory);

// Get environment variable
std::string GetEnvVar(const std::string& name);

// Set environment variable
void SetEnvVar(const std::string& name, const std::string& value);

// Schedule a command for execution
bool ScheduleCommand(const std::string& sName, const std::string& sCommand);

// Get default text editor command
std::string GetTextEditorCommand(const char* filename);

#endif
