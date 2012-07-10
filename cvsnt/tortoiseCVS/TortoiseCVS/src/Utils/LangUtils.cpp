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
#include "LangUtils.h"
#include "TortoiseDebug.h"

// Get wxLang from canonical name
int GetLangFromCanonicalName(const std::string& sCanonicalName)
{
   int lang;
   const wxLanguageInfo* info;
   if (sCanonicalName.empty())
   {
      return wxLocale::GetSystemLanguage();
   }
   std::string wxCanonicalName(sCanonicalName);
   for (lang = static_cast<int>(wxLANGUAGE_DEFAULT);
      lang < static_cast<int>(wxLANGUAGE_USER_DEFINED);
      lang++)
   {
      info = wxLocale::GetLanguageInfo(lang);
      if (info)
      {
         if (wxCanonicalName == wxAscii(info->CanonicalName.c_str()))
            return lang;
      }
   }
   return -1;
}



// Get wxLangInfo from canonical name
const wxLanguageInfo* GetLangInfoFromCanonicalName(const std::string& sCanonicalName)
{
   int lang = wxLANGUAGE_DEFAULT;
   int i;
   const wxLanguageInfo* info;
   if (sCanonicalName.empty())
   {
      lang = wxLocale::GetSystemLanguage();
   }
   else
   {
      for (i = static_cast<int>(wxLANGUAGE_DEFAULT);
         i < static_cast<int>(wxLANGUAGE_USER_DEFINED);
         i++)
      {
         info = wxLocale::GetLanguageInfo(i);
         if (info)
         {
            if (sCanonicalName == wxAscii(info->CanonicalName.c_str()))
            {
               lang = i;
               break;
            }
         }
      }
   }
   return wxLocale::GetLanguageInfo(lang);
}
