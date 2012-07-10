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
#include "../Utils/TimeUtils.h"
#include "../Utils/LogUtils.h"
#include "../Utils/TortoiseRegistry.h"
#include "../Utils/TortoiseDebug.h"
#include <windows.h>
#include "FixWinDefs.h"
#include <sstream>

// *********************** CVSRevisionDataSortCriteria *************************

void CVSRevisionDataSortCriteria::DoClick(int column)
{
    if (column == key)
        ascending = !ascending;
    else
    {
        key = column;
        ascending = true;
    }
}

void CVSRevisionDataSortCriteria::GetRegistryData()
{
    TortoiseRegistry::ReadInteger("PropSortKey", key);
    if (key < 0 || key >= NUM_COLUMNS)
        key = 0;  // discard corrupt registry entry
    ascending = TortoiseRegistry::ReadBoolean("PropSortAscending", true);
}

void CVSRevisionDataSortCriteria::SaveRegistryData() const
{
    TortoiseRegistry::WriteInteger("PropSortKey", key);
    TortoiseRegistry::WriteBoolean("PropSortAscending", ascending);
}

int CVSRevisionDataSortCriteria::Compare(const std::string &s1, const std::string &s2) const
{
    if (ascending)
        return s1.compare(s2);
    else
        return -s1.compare(s2);
}

int CVSRevisionDataSortCriteria::CompareRevisions(const std::string &s1, const std::string &s2) const
{
   return StaticCompareRevisions(s1, s2, ascending);
}

int CVSRevisionDataSortCriteria::Compare(int i1, int i2) const
{
    int ret = 0;

    if (i1 > i2)
        ret = 1;
    else if (i1 < i2)
        ret = -1;
    return ret;
}

int CVSRevisionDataSortCriteria::Compare(const struct tm& t1, const struct tm& t2) const
{
    int ret = 0;

    if (t1.tm_year > t2.tm_year)
        ret = 1;
    else if (t1.tm_year < t2.tm_year)
        ret = -1;
    else
    {
       if (t1.tm_mon > t2.tm_mon)
          ret = 1;
       else if (t1.tm_mon < t2.tm_mon)
          ret = -1;
       else
       {
          if (t1.tm_mday > t2.tm_mday)
             ret = 1;
          else if (t1.tm_mday < t2.tm_mday)
             ret = -1;
          else
          {
             if (t1.tm_hour > t2.tm_hour)
                ret = 1;
             else if (t1.tm_hour < t2.tm_hour)
                ret = -1;
             else
             {
                if (t1.tm_min > t2.tm_min)
                   ret = 1;
                else if (t1.tm_min < t2.tm_min)
                   ret = -1;
                else
                {
                   if (t1.tm_sec > t2.tm_sec)
                      ret = 1;
                   else if (t1.tm_sec < t2.tm_sec)
                      ret = -1;
                }
             }
          }
       }
    }

    if (ascending)
        return ret;
    else
        return -ret;
}

int CVSRevisionDataSortCriteria::Compare(const CRevFile& rev1, const CRevFile& rev2) const
{
    int res = 0;

    switch (key)
    {
    case COL_REVISION:
       res = rev1.RevNum().cmp(rev2.RevNum());
       break;
    case COL_CHECKEDIN:
        res = Compare(rev1.RevTime(), rev2.RevTime());
        break;
    case COL_AUTHOR:
        res = Compare(rev1.Author().c_str(), rev2.Author().c_str());
        break;
    case COL_CHANGES:
        res = Compare(rev1.ChgPos() + abs(rev1.ChgNeg()), rev2.ChgPos() + abs(rev2.ChgNeg()));
        break;
    case COL_BUGNUMBER:
        res = Compare(rev1.BugNumber().c_str(), rev2.BugNumber().c_str());
        break;
    case COL_COMMENT:
        res = Compare(rev1.DescLog().c_str(), rev2.DescLog().c_str());
        break;
    }
    return res;
}


int CVSRevisionDataSortCriteria::StaticCompareRevisions(const std::string &s1, 
                                                        const std::string &s2, 
                                                        bool ascending)
{
    int r1, r2;
    const char* p1 = s1.c_str();
    const char* p2 = s2.c_str();
    while (1)
    {
        r1 = atoi(p1);
        r2 = atoi(p2);
        if (r1 != r2)
            break;
        while (*p1 && (*p1 != '.'))
            ++p1;
        if (!*p1)
        {
            r1 = 0;
            break;
        }
        ++p1;
        while (*p2 && (*p2 != '.'))
            ++p2;
        if (!*p2)
        {
            r2 = 0;
            break;
        }
        ++p2;
    }

    int res = 0;
    if (r1 != r2)
    {
        if (ascending)
            res = r1 > r2 ? 1 : -1;
        else
            res = r1 < r2 ? 1 : -1;
    }
    return res;
}




// Check whether revision is a magic branch number
bool IsBranch(const CRevNumber& revNumber)
{
   TDEBUG_ENTER("IsBranch");
   TDEBUG_TRACE(revNumber.str());
   if (revNumber.size() <= 2)
      return false;

   return (revNumber.IntList()[revNumber.size() - 2] == 0);
}
