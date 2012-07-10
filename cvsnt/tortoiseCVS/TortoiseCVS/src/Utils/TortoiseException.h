// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2001 - Francis Irving
// <francis@flourish.org> - May 2001

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

#ifndef TORTOISE_EXCEPTION_H
#define TORTOISE_EXCEPTION_H

#include "FixCompilerBugs.h"

#ifndef POSTINST
#include <wx/string.h>

void TortoiseFatalError(const wxString& errorMessage);
// Get Win32 error message
wxString GetWin32ErrorMessage(int err);

#endif

// Default is to call GetLastError(), otherwise specify your error code
void DisplaySystemError(int err = -1);

#endif
