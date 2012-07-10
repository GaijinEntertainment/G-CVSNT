// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2002 - Torsten Martinsen
// <torsten@tiscali.dk> - September 2002

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

#include "TortoiseDebug.h"
#include "StringUtils.h"

#include <algorithm>
#include <functional>
#include <sstream>
#include <ctype.h>

// 8-bit safe variants of <ctype> functions

int myisspace(char ch)
{
   if (static_cast<unsigned char>(ch) < 128)
      return isspace(ch);
   return 0;
}

int MaxStringLength(const std::vector<std::string>& stringvec)
{
   unsigned int max = 0;
   for (std::vector<std::string>::const_iterator it = stringvec.begin(); it != stringvec.end(); ++it)
      if ((*it).size() > max)
         max = static_cast<unsigned int>((*it).size());
   return max;
}

void MakeLowerCase(std::string& s)
{
   std::transform(s.begin(), s.end(), s.begin(), tolower);
}

void MakeUpperCase(std::string& s)
{
   std::transform(s.begin(), s.end(), s.begin(), toupper);
}


// Remove leading whitespaces from a string
std::string TrimLeft(const std::string& str)
{
   std::string::const_iterator iter = std::find_if(str.begin(), str.end(),
                                                   std::not1(std::ptr_fun(myisspace)));
   if (iter == str.end())
      iter = str.begin();
   return std::string(iter, str.end());
}

// Remove trailing whitespaces from a string
std::string TrimRight(const std::string& str)
{
   std::string::const_reverse_iterator iter = std::find_if(str.rbegin(),
                                                           str.rend(), std::not1(std::ptr_fun(myisspace)));
   return std::string(str.begin(), iter.base());
}



// Remove leading and trailing whitespaces from a string
std::string Trim(const std::string& str)
{
   std::string::const_iterator begin = std::find_if(str.begin(), str.end(),
                                                    std::not1(std::ptr_fun(myisspace)));
   if (begin == str.end())
      begin = str.begin();
   std::string::const_reverse_iterator end = std::find_if(str.rbegin(),
                                                          str.rend(), std::not1(std::ptr_fun(myisspace)));
   return std::string(begin, end.base());
}



// Test if string starts with substr
bool StartsWith(const std::string& str, const std::string& substr)
{
   return str.substr(0, substr.length()) == substr;
}



// Quotes a string
std::string Quote(const std::string& str)
{
   std::string sResult = "\"" + str + "\"";
   return sResult;
}



// Cuts the first token off a delimited list
std::string CutFirstToken(std::string& sList, const std::string sDelimiter)
{
   std::string::size_type p = sList.find(sDelimiter);
   std::string sResult;

   if (p == std::string::npos)
   {
      sResult = sList;
      sList = "";
      return sResult;
   }
   else
   {
      sResult = sList.substr(0, p);
      sList = sList.substr(p + sDelimiter.length(), sList.length());
      return sResult;
   }
}


#ifndef POSTINST

wxString Printf(const wxChar* format, ...)
{
   va_list args;
   va_start(args, format);
   return wxString::FormatV(format, args);
}


std::string PrintfA(const char* format, ...)
{
    char* buf = 0;
    std::string res;
    va_list args;
    va_start(args, format);
    int size = 1024;
    int len = -1;
    while (len == -1)
    {
        delete[] buf;

        buf = new char[size + 1];
        len = _vsnprintf(buf, size, format, args);
        buf[size] = '\0';
        size *= 2;
    }

    res = buf;
    delete[] buf;
    return res;
}


// Convert Unicode string to multibyte string
std::string WideToMultibyte(const std::wstring& wide, UINT CodePage)
{
   char* narrow = NULL;
   // Determine length of string
   int ret = WideCharToMultiByte(CodePage, 0, wide.c_str(), static_cast<int>(wide.length()), NULL, 
                                 0, NULL, NULL);
   narrow = new char[ret + 1];
   ret = WideCharToMultiByte(CodePage, 0, wide.c_str(), static_cast<int>(wide.length()), narrow, 
                             ret, NULL, NULL);
   std::string s(narrow, ret);
   delete[] narrow;
   return s;
}


// Convert multibyte string to Unicode string 
std::wstring MultibyteToWide(const std::string& multibyte, UINT CodePage)
{
   wchar_t* wide = NULL;
   int ret = MultiByteToWideChar(CodePage, 0, multibyte.c_str(), 
                                 static_cast<int>(multibyte.length()), 0, 0);
   wide = new wchar_t[ret + 1];
   ret = MultiByteToWideChar(CodePage, 0, multibyte.c_str(), static_cast<int>(multibyte.length()),
                             wide, ret);
   std::wstring s(wide, ret);
   delete[] wide;
   return s;
}

#if wxUSE_UNICODE

wxString ExpandEnvStrings(const wxString& str)
{
    wxString result;

    // Determine size
    DWORD dwSize = ExpandEnvironmentStringsW(str.c_str(), 0, 0);
    if (dwSize > 0)
    {
        ++dwSize;
        wxChar* buf = new wxChar[dwSize + 1];
        ExpandEnvironmentStringsW(str.c_str(), buf, dwSize);
        result = buf;
        delete[] buf;
    }
    return result;
}

#endif

#endif // POSTINST

// Serialize a vector of strings
std::string SerializeStringVector(const std::vector<std::string>& vStrings, 
                                  const std::string& sDelimiter)
{
   std::string sResult;
   std::vector<std::string>::const_iterator it = vStrings.begin();
   if (it != vStrings.end())
   {
      sResult = *it;
      it++;
   }
   while (it != vStrings.end())
   {
      sResult += sDelimiter + *it;
      it++;
   }
   return sResult;
}


// Expand environment strings
std::string ExpandEnvStrings(const std::string& str)
{
    std::string result;

    // Determine size
    DWORD dwSize = ExpandEnvironmentStringsA(str.c_str(), 0, 0);
    if (dwSize > 0)
    {
        ++dwSize;
        char* buf = new char[dwSize + 1];
        ExpandEnvironmentStringsA(str.c_str(), buf, dwSize);
        result = buf;
        delete[] buf;
    }
    return result;
}

// Replace parameters (%something)
std::string ReplaceParams(const std::string& str, 
                          const std::map<std::string, std::string> params)
{
   std::stringstream result;
   unsigned int i = 0;
   char c;
   int state = 0;
   unsigned int var_start = 0;
   bool hasMore;
   while (i < str.length()) 
   {
      c = str[i];
      hasMore = (i < str.length() - 1);
      switch(state)
      {
         // Normal processing
         case 0:
         {
            if (c == '%' && hasMore)
            {
               var_start = i + 1;
               state = 1;
            }
            else
            {
               result << c;
            }
            break;
         }

         // Found '%'
         case 1:
         {
            if ((c == '_' || isalnum(c)) && hasMore)
            { 
               // do nothing
            }
            else
            {
               // replace variable
               if (var_start < i)
               {
                  std::string var = str.substr(var_start, i - var_start);
                  std::map<std::string, std::string>::const_iterator it = 
                     params.find(var);
                  if (it != params.end())
                  {
                     result << it->second;
                  }
                  else
                  {
                     result << '%' << var;
                  }
               }
               else
               {
                  result << '%';
               }
               result << c;
               if (c != '%')
               {
                  state = 0;
               }
            }
            break;
         }

         // should never get here
         default:
            ASSERT(false);
      }
      i++;
   }

   return result.str();
}


inline char OctetToHexChar(char c)
{
   unsigned char nibble = 0x0F & c;
   return ( nibble < 0x0A ) ? '0' + nibble : 'A' - 0x0A + nibble ;
}



std::string EscapeChar(char c)
{
   std::string escaped = "%  ";
   escaped[2] = OctetToHexChar(c);
   escaped[1] = OctetToHexChar(c >> 4);
   return escaped;
}

std::string EscapeUrl(const std::string& s)
{
   std::string escaped;
   for (size_t i = 0; i < s.size(); ++i)
   {
      char ch = s[i];
      if (!strchr("%\"<>\\^[]`+$,@;!#'", ch))
         escaped.append(1, ch);
      else
         escaped += EscapeChar(ch);
   }
   return escaped;
}
