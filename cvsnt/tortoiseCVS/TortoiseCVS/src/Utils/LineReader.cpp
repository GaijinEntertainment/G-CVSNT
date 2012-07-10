// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2003 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - March 2003

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
#include "LineReader.h"

const wchar_t *LineReader_crlfs_ws[] =
{
    L"\x0d\x0a", //  DOS/Windows style
    L"\x0a",     //  UNIX style
    L"\x0d"      //  Macintosh style
};

const char *LineReader_crlfs[] =
{
    "\x0d\x0a", //  DOS/Windows style
    "\x0a",     //  UNIX style
    "\x0d"      //  Macintosh style
};

/*const LineReader<std::wstring>::value_type *LineReader<std::wstring>::crlfs[] =
{
    L"\x0d\x0a", //  DOS/Windows style
    L"\x0a",     //  UNIX style
    L"\x0d"      //  Macintosh style
};*/
/*const LineReader<std::string>::value_type *LineReader<std::string>::crlfs[] =
{
    "\x0d\x0a", //  DOS/Windows style
    "\x0a",     //  UNIX style
    "\x0d"      //  Macintosh style
};*/



// Set crlf style
void LineReader<std::wstring>::SetCrlfStyle(CrlfStyle crlfStyle)
{
   m_crlfStyle = crlfStyle;
   m_crlf = LineReader_crlfs_ws[m_crlfStyle];
   m_crlfLength = wcslen(m_crlf);
}


// Set crlf style
void LineReader<std::string>::SetCrlfStyle(CrlfStyle crlfStyle)
{
   m_crlfStyle = crlfStyle;
   m_crlf = LineReader_crlfs[m_crlfStyle];
   m_crlfLength = strlen(m_crlf);
}



// Find buffer content
const LineReader<std::wstring>::value_type*
LineReader<std::wstring>::MemFind(const value_type* buf, size_type count, 
                                  const value_type* findbuf, size_type findcount)
{
   if ((count <= 0) || (findcount <= 0))
   {
      return 0;
   }

   // look for first char
   const value_type* pos = buf;
   while (count >= findcount + - (pos - buf))
   {
      pos = wmemchr(pos, findbuf[0], count - findcount + 1);
      if (pos == 0)
      {
         return 0;
      }

      // compare rest of buffer
      if (wmemcmp(pos, findbuf, findcount) == 0)
      {
         return pos;
      }
      else
      {
         pos++;
      }
   }
   return 0;
}


// Find buffer content
const LineReader<std::string>::value_type*
LineReader<std::string>::MemFind(const value_type* buf, size_type count, 
                                 const value_type* findbuf, size_type findcount)
{
   if ((count <= 0) || (findcount <= 0))
   {
      return 0;
   }

   // look for first char
   const value_type* pos = buf;
   while (count >= findcount + (pos - buf))
   {
      pos = (char*) memchr(pos, findbuf[0], count - findcount + 1);
      if (pos == 0)
      {
         return 0;
      }

      // compare rest of buffer
      if (memcmp(pos, findbuf, findcount) == 0)
      {
         return pos;
      }
      else
      {
         pos++;
      }
   }
   return 0;
}
