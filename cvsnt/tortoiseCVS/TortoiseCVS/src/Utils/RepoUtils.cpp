// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2005 - March Hare Software Ltd
// <arthur.barrett@march-hare.com> - July 2005

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
#include "../Utils/TimeUtils.h"
#include "../Utils/RepoUtils.h"
#include "../Utils/TortoiseRegistry.h"
#include "../Utils/TortoiseDebug.h"
#include <windows.h>
#include "FixWinDefs.h"
#include <sstream>

// *********************** CVSRepoDataSortCriteria *************************

void CVSRepoDataSortCriteria::DoClick(int column)
{
    if (column == key)
        ascending = !ascending;
    else
    {
        key = column;
        ascending = true;
    }
}

void CVSRepoDataSortCriteria::GetRegistryData()
{
    TortoiseRegistry::ReadInteger("RepoSortKey", key);
    if (key < 0 || key >= REPO_NUM_COLUMNS)
        key = 0;  // discard corrupt registry entry
    ascending = TortoiseRegistry::ReadBoolean("RepoSortAscending", true);
}

void CVSRepoDataSortCriteria::SaveRegistryData() const
{
    TortoiseRegistry::WriteInteger("RepoSortKey", key);
    TortoiseRegistry::WriteBoolean("RepoSortAscending", ascending);
}

int CVSRepoDataSortCriteria::Compare(const std::string &s1, const std::string &s2) const
{
    if (ascending)
        return s1.compare(s2);
    else
        return -s1.compare(s2);
}
