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


#include "StdAfx.h"
#include "ShellUtils.h"


std::string GetSpecialFolder(int nFolder)
{
    char buffer[_MAX_PATH+1];
    HRESULT hr = SHGetFolderPathA(0, nFolder, 0, SHGFP_TYPE_CURRENT, buffer);
    std::string result;
    if (hr == S_OK)
        result = buffer;
    return result;
}


void ItemListFree(LPITEMIDLIST pidl)
{
    if ( pidl )
    {
        LPMALLOC pMalloc;
        SHGetMalloc(&pMalloc);
        if ( pMalloc )
        {
            pMalloc->Free(pidl);
            pMalloc->Release();
        }
        else
        {
            ASSERT(false);
        }
    }
}


// Get path from IDList
std::string GetPathFromIDList(LPCITEMIDLIST pidl)
{
    std::string result;
    static char dir[MAX_PATH + 1];
   
    if(SHGetPathFromIDListA(pidl, dir))
        result = dir;
    return result;
}


