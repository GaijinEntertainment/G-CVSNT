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

#ifndef HTTP_SNAFFLE_H
#define HTTP_SNAFFLE_H

#include <vector>
#include <string>
#include <windows.h>
#include "FixWinDefs.h"
#include <wininet.h>

class HttpSnaffle
{
public:
   HttpSnaffle();

   // Specify the name of your application (web servers get told what
   // application is retrieving things from them) and the name of
   // the server
   bool Open(const std::string& userAgent,
                            const std::string& serverName,
                            int port = INTERNET_DEFAULT_HTTP_PORT);
    
   // Query if we are open (i.e. Open was called and the open went OK)
   bool IsOpen();

   // Closes an existing connection and reopens it (use this if there
   // has been an error to retry)
   bool Reopen();

   // Download binary data from the given URL on the server to the given stream
   bool StartFetch(std::string objectPath);
   // Returns the status (not found, redirected, OK, etc. - >=400 is an error)
   int FetchStatusCode();
   // Returns any additional text when an error occurred
   std::string FetchStatusMessage();
   // Keep calling this until it returns zero. Otherwise, returns the
   // number of bytes just downloaded, or -1 for an error.
   int FetchMore(std::ostream& out);

   // Calls the above functions to fetch a whole file
   void FetchWholeFile(std::string objectPath, std::ostream& out);

   ~HttpSnaffle();

private:
   // Internal
   void Dumper(HINTERNET hResource, std::ostream& out);
   void CloseHandles();

   std::string          myServerName;
   std::string          myUserAgent;
   int                  myPort;
    
   // Handles from Windows API internet functions
   HINTERNET            myInet;
   HINTERNET            myServerConnection;
   HINTERNET            myRequest;
   DWORD                myContext;

   // Temporary buffer to store stuff as we get it
   std::vector<char>    myBuffer;
};


#endif
