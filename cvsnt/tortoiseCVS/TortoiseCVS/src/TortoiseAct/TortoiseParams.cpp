// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2003 - Torsten Martinsen
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


#include "StdAfx.h"
#include "TortoiseParams.h"
#include "../Utils/Translate.h"


int TortoiseParams::ParseCommandLine(char* argv[], int argc, int start)
{
   int i = start;
   std::string sParameter;
   std::string sValue;   
   VectorValues *vv = 0;

   while (i < argc)
   {
      // do we have a command line parameter?
      if (argv[i][0] == '-')
      {
         // Create new parameter 
         sParameter = argv[i];
         vv = &(m_Params[sParameter]);    
      }
      else
      {
         if (vv != 0)
         {
            // Add value to existing parameter
            sValue = Unquote(argv[i]);
            vv->push_back(sValue);
         }
      }
      i++;
   }

   return i;
}



// Get value of parameter
std::string TortoiseParams::GetValue(const char* pszParam)
{
   return GetValue(pszParam, 0);
}



// Get value of parameter
std::string TortoiseParams::GetValue(const char* pszParam, unsigned int index)
{
   std::string sResult = "";
   MapParams::iterator it;
   VectorValues *vv = 0;

   it = m_Params.find(pszParam);
   if (it == m_Params.end())
      goto Cleanup;

   vv = &(it->second);
   if (vv->size() <= index)
      goto Cleanup;
      
   sResult = (*vv)[index];

Cleanup:
   return sResult;
}



// Does parameter exists
bool TortoiseParams::Exists(const char* pszParam)
{
   MapParams::iterator it;

   it = m_Params.find(pszParam);
   if (it == m_Params.end())
   {
      return false;
   }
   else
   {
      return true;
   }
}



// Get number of values
int TortoiseParams::GetValueCount(const char* pszParam)
{
   int iResult = 0;
   MapParams::iterator it;
   VectorValues *vv = 0;

   it = m_Params.find(pszParam);
   if (it == m_Params.end())
      goto Cleanup;

   vv = &(it->second);
   iResult = static_cast<int>(vv->size());

Cleanup:
   return iResult;
}



// unquote a parameter
std::string TortoiseParams::Unquote(const char* pszStr)
{
   std::string sResult = pszStr;
   if (sResult.size() < 2)
      goto Cleanup;

   if (((sResult[0] == '"') && (sResult[sResult.size() - 1] == '"'))
      || ((sResult[0] == '\'') && (sResult[sResult.size() - 1] == '\'')))
   {
      return sResult.substr(1, sResult.size() - 2);
   }

Cleanup:
   return sResult;
}

