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

#ifndef LOG_UTILS_H
#define LOG_UTILS_H

#include <string>
#include "../cvstree/cvslog.h"


enum
{
   COL_REVISION,
   COL_CHECKEDIN,
   COL_AUTHOR,
   COL_CHANGES,
   COL_BUGNUMBER,
   COL_COMMENT,
   NUM_COLUMNS
};

// *********************** CVSRevisionDataSortCriteria *************************
// How to sort revisions
class CVSRevisionDataSortCriteria
{
public:
   CVSRevisionDataSortCriteria()
      : key(-1),
        ascending(true)
   {
   }

   virtual void DoClick(int column);
   virtual void GetRegistryData();
   virtual void SaveRegistryData() const;

   virtual int Compare(const CRevFile&, const CRevFile&) const;
   virtual int Compare(const std::string&, const std::string&) const;
   virtual int CompareRevisions(const std::string&, const std::string&) const;
   virtual int Compare(const struct tm&, const struct tm&) const;
   virtual int Compare(int i1, int i2) const;

   static int StaticCompareRevisions(const std::string &s1, const std::string &s2, 
                                     bool ascending);

   int          key;
   bool         ascending;
};


// Check whether revision is a magic branch number
bool IsBranch(const CRevNumber& revNumber);

#endif
