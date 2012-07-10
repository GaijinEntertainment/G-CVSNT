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

#include <windows.h>
#include <tchar.h>

#include <stdio.h>
#include <sstream>

#include <Utils/Translate.h>

#include "FixWinDefs.h"
#include "TortoiseException.h"

// Default is to call GetLastError(), otherwise specify your error code
void DisplaySystemError(int err)
{
   if (err == -1)
      err = GetLastError();

   TCHAR buffer[4096];
   buffer[0] = 0;
   if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, err, 0, buffer, 4095, NULL))
       _stprintf(buffer, _("Error %d"), err);
        
   ::MessageBox(0, buffer, _("TortoiseCVS"), MB_OK );
}

#ifndef POSTINST

void TortoiseFatalError(const wxString& errorMessage)
{
   ::MessageBox(0, errorMessage.c_str(), _("TortoiseCVS"), MB_OK );
   exit(1);
}


// Get Win32 error message
wxString GetWin32ErrorMessage(int err)
{
   PVOID msg;
   wxString result;
   if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 
                     0, err, 0, (LPTSTR) &msg, 0, 0))
   {
      result.assign((LPCTSTR) msg);
      LocalFree(msg);
   }
   return result;
}

#endif
