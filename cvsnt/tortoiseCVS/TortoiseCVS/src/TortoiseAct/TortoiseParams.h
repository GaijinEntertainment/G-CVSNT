// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2003 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - February 2003

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

#ifndef TORTOISE_PARAMS_H
#define TORTOISE_PARAMS_H

#include <string>
#include <map>
#include <vector>



class TortoiseParams
{
public:
   // parse command line for parameters
   int ParseCommandLine(char* argv[], int argc, int start);
   
   // Get value of parameter
   std::string GetValue(const char* pszParam);

   // Get value of parameter
   std::string GetValue(const char* pszParam, unsigned int index);

   // Does parameter exists
   bool Exists(const char* pszParam);

   // Get number of values
   int GetValueCount(const char* pszParam);

private:
   // unquote a parameter
   static std::string Unquote(const char* pszStr);

   // Map for parameters
   typedef std::vector<std::string> VectorValues;
   typedef std::map<std::string, VectorValues> MapParams;
   MapParams m_Params;
};

#endif


