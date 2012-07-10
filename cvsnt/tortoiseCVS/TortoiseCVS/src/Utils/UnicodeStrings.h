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

#include <windows.h>
#include "FixWinDefs.h"
#include <string>


// Strip a UTF8 header off a string
bool StripUtf8Header(std::string& str);

// Strip a UCS2 header off a string
bool StripUcs2Header(std::wstring& str);

// Convert UTF8 string to UCS2
std::wstring Utf8ToUcs2(const std::string& sUtf8);

// Convert UCS2 string to UTF8
std::string Ucs2ToUtf8(const std::wstring& sUcs2);

// Convert a file from utf8 to ucs2
bool ConvertFileUtf8ToUcs2(const std::string& sourceFilename, 
                           const std::string& destFilename);

// Convert a file from ucs2 to ascii
bool ConvertFileUcs2ToAscii(const std::string& sourceFilename, 
                            const std::string& destFilename, bool bStopOnMismatch, bool* pbSuccess);

// Convert a file from ucs2 to utf8
bool ConvertFileUcs2ToUtf8(const std::string& sourceFilename, 
                           const std::string& destFilename, bool bWriteUtf8Signature);

// Convert a file from ascii to ucs2
bool ConvertFileAsciiToUcs2(const std::string& sourceFilename, 
                            const std::string& destFilename);
