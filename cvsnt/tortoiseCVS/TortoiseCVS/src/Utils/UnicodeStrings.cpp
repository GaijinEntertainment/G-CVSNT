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
#include "UnicodeStrings.h"
#include "PathUtils.h"
#include "LineReader.h"
#include "StringUtils.h"
#include <fstream>
#include <sstream>


// Strip a UTF8 header off a string
bool StripUtf8Header(std::string& str)
{
   if (str.substr(0, 3) == "\xef\xbb\xbf")
   {
      str = str.substr(3, std::string::npos);
      return true;
   }
   else
   {
      return false;
   }
}


// Strip a UCS2 header off a file
bool StripUcs2Header(std::wstring& str)
{
   if (str[0] == L'\xfeff')
   {
      str = str.substr(1, std::string::npos);
      return true;
   }
   else
   {
      return false;
   }
}


// Convert UTF8 string to UCS-2
std::wstring Utf8ToUcs2(const std::string& sUtf8)
{
   std::string::const_iterator it = sUtf8.begin();
   std::wstringstream ws;

   int state = 0;
   wchar_t wc = 0;

   while (it != sUtf8.end())
   {
      unsigned short c = (unsigned char)(*it);
      switch (state)
      {
         // reading first char
      case 0:
         if ((c >> 7) == 0) // 0xxxxxxx
         {
            ws << (wchar_t) c;
         }
         else if ((c >> 5) == 0x6) // 110xxxxx 10xxxxxx
         {
            wc = (c & '\x1f') << 6;
            state = 1;
         }
         else if ((c >> 4) == 0x0e) // 1110xxxx 10xxxxxx 10xxxxxx 
         {
            wc = (c & '\x0f') << 12;
            state = 2;
         }
         else if ((c >> 3) == 0x1e) // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx 
         {
            wc = 0;
            state = 3;
         }
         else if ((c >> 2) == 0x3e) // 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 
         {
            wc = 0;
            state = 4;
         }
         else if ((c >> 1) == 0x7e) // 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx  
         {
            wc = 0;
            state = 5;
         }
         break;

      // reading last byte
      case 1:
         if ((c >> 6) == 0x2)
         {
            wc |= c & '\x3f';
            ws << wc;
            state--;
         }
         else
         {
            state = 0;
         }
         break;

      // reading 2nd last byte 
      case 2:
      // read 3rd last byte
      case 3:
      // read 4th last byte
      case 4:
      // read 5th last byte
      case 5:
         if ((c >> 6) == 0x2)
         {
            wc |= (c & '\x3f') << ((state - 1) * 6);
            state--;
         }
         else
         {
            state = 0;
         }
         break;
      }

      it++;
   }

   return ws.str();
}


// Convert UCS2 string to UTF8
std::string Ucs2ToUtf8(const std::wstring& sUcs2)
{
   std::wstring::const_iterator it = sUcs2.begin();
   std::string line;

   while (it != sUcs2.end())
   {
      if (*it < 0x80) // 0xxxxxxx
      {
         line += static_cast<char>(*it);
      }
      else if (*it < 0x0800) // 110xxxxx 10xxxxxx
      {
         line += char('\xC0' | (*it >> 6));
         line += char('\x80' | (*it & '\x3f'));
      }
      else // 1110xxxx 10xxxxxx 10xxxxxx
      {
         char c = char('\xE0' | (*it >> 12));
         line += char('\xE0' | (*it >> 12));
         c = char('\x80' | ((*it >> 6) & '\x3f'));
         line += char('\x80' | ((*it >> 6) & '\x3f'));
         c = ('\x80' | (*it & '\x3f'));
         line += char('\x80' | (*it & '\x3f'));
      }
      it++;
   }
   return line;
}



// Convert a file from utf8 to ucs2
bool ConvertFileUtf8ToUcs2(const std::string& sourceFilename, 
                           const std::string& destFilename)
{
   std::ifstream sourceFile(sourceFilename.c_str(), std::ios_base::in | std::ios_base::binary);
   std::ofstream destFile(destFilename.c_str(), std::ios_base::out 
                          | std::ios_base::trunc | std::ios_base::binary);

   if (!FileExists(sourceFilename.c_str()))
      return false;

   // Write unicode header
   destFile << "\xff\xfe";

   bool bFirstLine = true;
   std::string line;
   std::wstring wline;
   LineReader<std::string> lr(&sourceFile, LineReader<std::string>::CrlfStyleDos);

   while (lr.ReadLine(line))
   {
      // remove UTF8 signature from first line
      if (bFirstLine)
         StripUtf8Header(line);

      // Convert to unicode
      wline = Utf8ToUcs2(line);

      if (!bFirstLine)
         wline.insert(0, L"\x000d\x000a");
      else
         bFirstLine = false;

      destFile.write(reinterpret_cast<const char*>(wline.c_str()),
                     static_cast<size_t>(wline.length() * sizeof(wchar_t)));
   }
   return true;
}



// Convert a file from ucs2 to ascii
bool ConvertFileUcs2ToAscii(const std::string& sourceFilename, 
                            const std::string& destFilename,
                            bool bStopOnMismatch, bool* pbSuccess)
{
   std::ifstream sourceFile(sourceFilename.c_str(),
                            std::ios_base::in | std::ios_base::binary);
   std::ofstream destFile(destFilename.c_str(),
                          std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
   if (pbSuccess)
   {
      *pbSuccess = false;
   }

   if (!FileExists(sourceFilename.c_str()))
   {
      return false;
   }


   bool bFirstLine = true;
   std::string line;
   std::wstring wline;
   std::wstring wline2;
   LineReader<std::wstring> lr(&sourceFile, LineReader<std::wstring>::CrlfStyleDos);
   while (lr.ReadLine(wline))
   {
      // remove UCS2 signature from first line
      if (bFirstLine)
      {
         StripUcs2Header(wline);
      }

      // Convert to ascii
      line = WideToMultibyte(wline);
      if (bStopOnMismatch)
      {
         wline2 = MultibyteToWide(line);
         if (wline != wline2)
         {
            return false;
         }
      }

      if (!bFirstLine)
      {
         line.insert(0, "\x0d\x0a");
      }
      else
      {
         bFirstLine = false;
      }

      destFile.write(const_cast<char*>(line.c_str()), line.length() * sizeof(char));
   }

   if (pbSuccess)
   {
      *pbSuccess = true;
   }
   return true;
}



// Convert a file from ucs2 to utf8
bool ConvertFileUcs2ToUtf8(const std::string& sourceFilename, 
                           const std::string& destFilename,
                           bool bWriteUtf8Signature)
{
   std::ifstream sourceFile(sourceFilename.c_str(), std::ios_base::in | std::ios_base::binary);
   std::ofstream destFile(destFilename.c_str(),
                          std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);

   if (!FileExists(sourceFilename.c_str()))
   {
      return false;
   }


   // Write utf8 header
   if (bWriteUtf8Signature)
   {
      destFile << "\xef\xbb\xbf";
   }

   bool bFirstLine = true;
   std::string line;
   std::wstring wline;
   LineReader<std::wstring> lr(&sourceFile, LineReader<std::wstring>::CrlfStyleDos);
   while (lr.ReadLine(wline))
   {
      // Remove UCS2 signature from first line
      if (bFirstLine)
      {
         StripUcs2Header(wline);
      }

      // Convert to utf8
      line = Ucs2ToUtf8(wline);

      if (!bFirstLine)
      {
         line.insert(0, "\x0d\x0a");
      }
      else
      {
         bFirstLine = false;
      }
      destFile.write(const_cast<char*>(line.c_str()), line.length() * sizeof(char));
   }

   return true;
}



// Convert a file from ascii to ucs2
bool ConvertFileAsciiToUcs2(const std::string& sourceFilename, 
                            const std::string& destFilename)
{
   std::ifstream sourceFile(sourceFilename.c_str(), std::ios_base::in | std::ios_base::binary);
   std::ofstream destFile(destFilename.c_str(),
                          std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);

   if (!FileExists(sourceFilename.c_str()))
   {
      return false;
   }

   // Write unicode header
   destFile << "\xff\xfe";

   bool bFirstLine = true;
   std::string line;
   std::wstring wline;

   LineReader<std::string> lr(&sourceFile, LineReader<std::string>::CrlfStyleDos);
   while (lr.ReadLine(line))
   {
      // Convert to unicode
      wline = MultibyteToWide(line);

      if (!bFirstLine)
      {
         wline.insert(0, L"\x000d\x000a");
      }
      else
      {
         bFirstLine = false;
      }
      // Ugly :-)
      destFile.write(const_cast<char*>(reinterpret_cast<const char*>(wline.c_str())),
                     wline.length() * sizeof(wchar_t));
   }
   return true;
}
