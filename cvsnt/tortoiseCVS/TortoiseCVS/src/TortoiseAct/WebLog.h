// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - October 2000

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

#ifndef WEB_LOG_H
#define WEB_LOG_H

#include <string>
#include "../Utils/HttpSnaffle.h"
#include <wininet.h>

class ProgressDialog;
class ThreadSafeProgress;

// Can scan a server for a web log URL, querying the user if it can't find one

class WebLog
{
public:
   WebLog(const std::string& cvsRoot,
          const std::string& ambientPart,
          bool progressDialogVisible = true,
          ProgressDialog* progressDialog = 0);
   bool PerformSearch(std::string& url);
   bool PerformSearchAndQuery(std::string& url);
   std::string GetReturnedHTML();

private:
   // Returns HTTP error code, or zero for OK
   int CheckURLWorks(const std::string& url);

   // This next one should go elsewhere if it is useful other than in this class:
   static std::string SourceForgeProject(const std::string& server, 
                                         const std::string& directory);

   // Returns HTTP error code, or zero for OK
   int TrySingleURL(const std::string& urlStubPrefix, const std::string& urlStubSuffix);

   // Examine the HTML for clues that ViewCVS actually failed the request,
   // but faked the error code. If found, adjust the error code appropriately.
   void ViewCVSStatusHack(int& result, const std::string& url);
   
   bool ScanForURL(std::string& urlStubPrefix, std::string& urlStubSuffix);
   std::string MakeFullURL(const std::string& urlStubPrefix, const std::string& urlStubSuffix);
   bool AutomaticallyFindURL(std::string& urlStubPrefix, std::string& urlStubSuffix);
   bool QueryUser(bool queryWasForced, std::string& urlStubPrefix, std::string& urlStubSuffix);
   void ThreadMain();

   bool IsHTTPStatusOK(int statuscode);

   std::string  myCvsRoot;
   std::string  myServer;
   std::string  myDirectory;
   std::string  myAmbientPart;
   bool         myQueriedBefore;
   bool         myProgressDialogVisible;

   bool         myThreadResult;
   std::string  myThreadPrefix;
   std::string  myThreadSuffix;

   int          myHttpErrorCode;
   std::string  myHttpErrorMsg;
   std::string  myHttpErrorUrl;

   std::string  myReturnedHTML;
   ProgressDialog* myProgressDialog;
   ThreadSafeProgress* myThreadSafeProgress;
};

#endif
