// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - Janurary 2001

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


#ifndef CONFLICT_PARSER_H
#define CONFLICT_PARSER_H

#include <string>


// A parser for CVS conflict files
class ConflictParser
{
public:
   // Parse a file
   static bool ParseFile(const std::string& sConflictFileName, const std::string& sWorkingCopyFileName, 
      const std::string& sNewRevisionFileName, bool& bNestedConflicts);

private:
};



#endif
