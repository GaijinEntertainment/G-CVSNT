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


#include "StdAfx.h"
#include "WebLog.h"
#include "../Utils/TortoiseRegistry.h"
#include "../Utils/TortoiseDebug.h"
#include "../Utils/PathUtils.h"
#include "../Utils/Keyboard.h"
#include "../Utils/StringUtils.h"
#include <Utils/Preference.h>

#include "../Utils/Thread.h"
#include "ThreadSafeProgress.h"
#include "../Utils/Translate.h"

#include "../DialogsWxw/LogConfigDialog.h"

#include <algorithm>
#include <sstream>

// List of candidate URLs and status codes
struct candidate
{
   candidate(const std::string& prefix,
             const std::string& suffix,
             int status)
      : urlPrefix(prefix), urlSuffix(suffix), statuscode(status)
   {
   }
   std::string urlPrefix;
   std::string urlSuffix;
   int         statuscode;
};

int WebLog::CheckURLWorks(const std::string& url)
{
   TDEBUG_ENTER("WebLog::CheckURLWorks");
   if (url == "plainlog")
      return HTTP_STATUS_OK;

   // Using HTTP query the path on the server.  If we get an error (i.e. >=400)
   // then tell the user, and query for a new URL-stub (this covers the location
   // on the server changing).

   URL_COMPONENTSA urlComp;
   memset(&urlComp, 0, sizeof(urlComp));
   char hostName[_MAX_PATH];
   char urlPath[_MAX_PATH];
   urlComp.lpszHostName = hostName;
   urlComp.dwHostNameLength = _MAX_PATH - 1;
   urlComp.lpszUrlPath = urlPath;
   urlComp.dwUrlPathLength = _MAX_PATH - 1;
   urlComp.dwStructSize = sizeof(urlComp);

   int result = 0;
   if (!InternetCrackUrlA(url.c_str(), 0, 0, &urlComp))
      result = 1000;    // Bogus HTTP status
   else
   {
      HttpSnaffle snaffler;
      if (!snaffler.Open("TortoiseCVS URL tester", urlComp.lpszHostName, urlComp.nPort))
         result = 1000;
      if (!snaffler.StartFetch(urlComp.lpszUrlPath))
         result = 1000;
      else
      {
         result = snaffler.FetchStatusCode();
         TDEBUG_TRACE("CheckURLWorks: error code " << result);
         if (result >= 400)
         {
            myHttpErrorUrl = url;
            myHttpErrorCode = result;
            myHttpErrorMsg = snaffler.FetchStatusMessage();
         }
         if (IsHTTPStatusOK(result))
         {
            // store the data, some bits of TortoiseCVS parse it
            std::ostringstream out;
            while (snaffler.FetchMore(out) > 0)
               ;
            myReturnedHTML = out.str();
            ViewCVSStatusHack(result, url);
            TDEBUG_TRACE("CheckURLWorks: HTML '" << myReturnedHTML << "'");
         }
      }
    }
    return result;
}

void WebLog::ViewCVSStatusHack(int& result, const std::string& url)
{
   TDEBUG_ENTER("ViewCVSStatusHack");
   if (myReturnedHTML.find("HTTP-like status code") != std::string::npos)
   {
      TDEBUG_TRACE("Found 'HTTP-like status code'");
      if (myReturnedHTML.find("404 Not Found") != std::string::npos)
         result = 404;
      TDEBUG_TRACE("Result: " << result);
      myHttpErrorUrl = url;
      myHttpErrorCode = result;
      myHttpErrorMsg = "[404 Not Found]";
   }
}

// static
std::string WebLog::SourceForgeProject(const std::string& server, 
                                       const std::string& directory)
{
    if (server.find(".sourceforge.net") == std::string::npos)
       return "";

    std::string s = RemoveTrailingDelimiter(directory);
    std::string::size_type pos = s.find_last_of("/");
    if (pos == std::string::npos || pos >= s.size() - 1)
        return "";

    std::string project = s.substr(pos + 1);
    std::transform(project.begin(), project.end(), project.begin(), tolower);
    return project;
}

WebLog::WebLog(const std::string& cvsRoot,
               const std::string& ambientPart,
               bool progressDialogVisible, ProgressDialog* progressDialog)
   : myCvsRoot(cvsRoot),
     myAmbientPart(ambientPart),
     myQueriedBefore(false), 
     myProgressDialogVisible(progressDialogVisible),
     myHttpErrorCode(0), 
     myProgressDialog(progressDialog),
     myThreadSafeProgress(0)
{
   CVSRoot tempCvsRoot;
   tempCvsRoot.SetCVSROOT(cvsRoot);
   if (tempCvsRoot.AllowServer())
   {
      myServer = tempCvsRoot.GetServer();
   }
   else
   {
      myServer = "localhost";
   }

   myDirectory = tempCvsRoot.GetDirectory();
}


int WebLog::TrySingleURL(const std::string& urlStubPrefix, const std::string& urlStubSuffix)
{
    TDEBUG_ENTER("WebLog::TrySingleURL");
    std::string url = MakeFullURL(urlStubPrefix, urlStubSuffix);
    wxString message(_("Trying "));
    message += wxText(url);
    message += wxT("...\n");
    if (myThreadSafeProgress)
        myThreadSafeProgress->NewText(message);
    TDEBUG_TRACE("Trying " << url);
    return CheckURLWorks(url);
}

bool WebLog::ScanForURL(std::string& urlStubPrefix, std::string& urlStubSuffix)
{
   TDEBUG_ENTER("WebLog::ScanForURL");
   const char* firstPart[] =
      { "/cgi-bin", "/viewcvs", "/cvsweb", "/bin", "" };
   const char* secondPart[] =
      { "/viewcvs.cgi", "/viewcvs", "/cvsweb.cgi", "/cvsweb.pl", "/cvsweb"};

   // Preprocess to get a list of prefixes to scan
   std::vector<std::string> prefixes;
   std::vector<std::string> suffixes;
   for (unsigned int j = 0; j < sizeof(secondPart) / sizeof(char*); ++j)
   {
      for (unsigned int i = 0; i < sizeof(firstPart) / sizeof(char*); ++i)
      {
         std::string first = firstPart[i];
         std::string second = secondPart[j];
         TDEBUG_TRACE("first: " << first << ", second: " << second);
         // Don't mix viewcvs and cvsweb parts, as that is silly and unlikely!
         if (!((first.find("viewcvs") != std::string::npos && second.find("cvsweb") != std::string::npos)
               || (first.find("cvsweb") != std::string::npos && second.find("viewcvs") != std::string::npos)))
         {
            prefixes.push_back(first + second);
         }
      }
   }

   // Sourceforge special case - add the project name into the path
   std::string sfProject = SourceForgeProject(myServer, myDirectory);
   if (!sfProject.empty())
   {
      TDEBUG_TRACE("sfProject: " << sfProject);
      prefixes.clear();
      prefixes.push_back("/" + sfProject);
   }

   // Try with no suffix first
   suffixes.push_back("");

   // Try with explicit "cvsroot" setting
   if (myDirectory.length() > 0)
      suffixes.push_back("?cvsroot=" + myDirectory.substr(1));
   
   // Actually perform the scan
   bool result = false;
   std::vector<candidate> candidates;
   for (unsigned int l = 0; !result && (l < suffixes.size()); ++l)
   {
      TDEBUG_TRACE("l " << l);
      for (unsigned int k = 0; k < prefixes.size(); ++k)
      {
         urlStubPrefix = "http://" + myServer + prefixes[k];
         urlStubSuffix = suffixes[l];
         TDEBUG_TRACE("urlStubSuffix " << urlStubSuffix);
         int statuscode = TrySingleURL(urlStubPrefix, urlStubSuffix);
         TDEBUG_TRACE("  status " << statuscode << " (" << HTTP_STATUS_OK << ")");
         if (statuscode == HTTP_STATUS_OK)
         {
            // This one works, stop trying more
            TDEBUG_TRACE("  Choosing OK '" << urlStubPrefix << "', '" << urlStubSuffix << "'");
            result = true;
            break;
         }
         if (myThreadSafeProgress->UserAborted())
         {
            result = false;
            break;
         }
         if (IsHTTPStatusOK(statuscode))
         {
            // This one *might* work, so save it for later
            candidate c(urlStubPrefix, urlStubSuffix, statuscode);
            candidates.push_back(c);
         }
      }
      if (result || myThreadSafeProgress->UserAborted())
      {
         if (myThreadSafeProgress->UserAborted())
         {
            result = false;
         }
      }
   }

   if (!result && !candidates.empty())
   {
      // There were no working URL's. Try to find one that might work with proper authentication.
      unsigned int i;
      for (i = 0; i < candidates.size(); ++i)
         if (candidates[i].statuscode == HTTP_STATUS_DENIED)
         {
            urlStubPrefix = candidates[i].urlPrefix;
            urlStubSuffix = candidates[i].urlSuffix;
            TDEBUG_TRACE("  Choosing DENIED '" << urlStubPrefix << "', '" << urlStubSuffix << "'");
            result = true;
            break;
         }
      for (i = 0; !result && (i < candidates.size()); ++i)
         if (candidates[i].statuscode == HTTP_STATUS_FORBIDDEN)
         {
            urlStubPrefix = candidates[i].urlPrefix;
            urlStubSuffix = candidates[i].urlSuffix;
            TDEBUG_TRACE("  Choosing FORBIDDEN '" << urlStubPrefix << "', '" << urlStubSuffix << "'");
            result = true;
            break;
         }
   }

   return result;
}

std::string WebLog::MakeFullURL(const std::string& urlStubPrefix, const std::string& urlStubSuffix)
{
    // Special case, not a real URL (means do plain CVS text log locally)
    if (urlStubPrefix == "plainlog")
        return urlStubPrefix;

    // Escape URL to protect special characters such as '#'.
    return EscapeUrl(urlStubPrefix + myAmbientPart + urlStubSuffix);
}

void WebLog::ThreadMain()
{
   TDEBUG_ENTER("WebLog::ThreadMain");
   wxString s(_("Scanning for web log URL on server"));
   s += wxT("\n\n");
   myThreadSafeProgress->NewText(s);

   // Test for obsolete Sourceforge registry keys
   // (it used to use a suffix)
   std::string sfProject = SourceForgeProject(myServer, myDirectory);
   if (!sfProject.empty())
   {
      TDEBUG_TRACE("sfProject: " << sfProject);
      if (myThreadSuffix != "")
         myThreadPrefix = "scanagain";
   }

   // If there is a URL in the registry, see if it is OK first
   if (!myThreadPrefix.empty() && myThreadPrefix != "scanagain")
   {
      TDEBUG_TRACE("myThreadPrefix: " << myThreadPrefix);
      myThreadResult = IsHTTPStatusOK(TrySingleURL(myThreadPrefix, myThreadSuffix));
   }
   if (myThreadSafeProgress->UserAborted())
   {
      TDEBUG_TRACE("User abort");
      myThreadResult = false;
      myThreadSafeProgress->Finished();
      return;
   }

   // If that doesn't work, look for other values
   if (!myThreadResult)
   {
      TDEBUG_TRACE("ScanForURL");
      myThreadResult = ScanForURL(myThreadPrefix, myThreadSuffix);
   }
   myThreadSafeProgress->Finished();
}

bool WebLog::AutomaticallyFindURL(std::string& urlStubPrefix, std::string& urlStubSuffix)
{
   TDEBUG_ENTER("WebLog::AutomaticallyFindURL");
   TDEBUG_TRACE("urlStubPrefix: " << urlStubPrefix << ", urlStubSuffix: " << urlStubSuffix);
   // Look for an already specified URL-stub for that server in the registry
   // First try the server name+CVSROOT-specific value
   urlStubPrefix = TortoiseRegistry::ReadString("WebLogURLPrefix\\" + myServer +
                                                std::string(":") + myDirectory);
   // If none found, try the server name-specific value
   if (urlStubPrefix.empty())
      urlStubPrefix = TortoiseRegistry::ReadString("WebLogURLPrefix\\" + myServer);
   urlStubSuffix = TortoiseRegistry::ReadString("WebLogURLSuffix\\" + myServer +
                                                std::string(":") + myDirectory);
   if (urlStubSuffix.empty())
      urlStubSuffix = TortoiseRegistry::ReadString("WebLogURLSuffix\\" + myServer);

   // Plain "cvs log" is always OK
   if (urlStubPrefix == "plainlog")
      return true;

   // Make thread to do the scanning, with progress dialog in this thread
   myThreadResult = false;
   ProgressDialog* progress;
   if (myProgressDialog)
   {
      progress = myProgressDialog;
   }
   else
   {
      // TODO: Make this a scoped preference
      CVSAction::close_t autoclose = static_cast<CVSAction::close_t>(GetIntegerPreference("Close Automatically"));
      progress = CreateProgressDialog(myProgressDialog, 0,
                                      (autoclose != CVSAction::CloseNever) ?
                                         ProgressDialog::acDefaultNoClose : ProgressDialog::acDefaultClose,
                                      myProgressDialogVisible);
   }
   myThreadSafeProgress = new ThreadSafeProgress(progress);
   myThreadPrefix = urlStubPrefix;
   myThreadSuffix= urlStubSuffix;
   Thread<WebLog> thread;
   thread.BeginThread(this, &WebLog::ThreadMain);
   myThreadSafeProgress->Main(0);
   urlStubPrefix = myThreadPrefix;
   urlStubSuffix = myThreadSuffix;
   bool result = myThreadResult;
   // progress->WaitForAbort(); // for testing
   delete myThreadSafeProgress;
   if (!myProgressDialog)
      DeleteProgressDialog(progress);
   myThreadSafeProgress = NULL;

   if (result)
   {
      // Store it in registry
      TortoiseRegistry::WriteString("WebLogURLPrefix\\" + myServer + std::string(":") + myDirectory,
                                    urlStubPrefix);
      TortoiseRegistry::WriteString("WebLogURLSuffix\\" + myServer + std::string(":") + myDirectory,
                                    urlStubSuffix);
   }

   return result;
}

// Query the user for a URL prefix and suffix for his cvsweb/viewcvs-style program
// Results are stored in the registry, so next automatic scan picks them up
bool WebLog::QueryUser(bool queryWasForced, std::string& urlStubPrefix, std::string& urlStubSuffix)
{
    // Work out what text to display
    wxString message;
    if (queryWasForced)
    {
        message = _("Please enter the URL where ViewCVS or CVSweb is.");
        message += wxT("\n\n");
    }
    else
    {
        if (myQueriedBefore)
        {
            if (myHttpErrorCode)
            {
               message += Printf(_("The URL %s responded with error code %d"), 
                                 wxText(myHttpErrorUrl).c_str(), myHttpErrorCode);
               if (!myHttpErrorMsg.empty())
               {
                  message += wxT(" (");
                  message += wxText(myHttpErrorMsg);
                  message += wxT(")");
               }
               
               message += wxT(".");
            }
            else
            {
               message += _("Sorry! The URL isn't responding, or is responding with an error.");
            }
                 
            message += _(
"This could happen because you are not connected to the internet, because the server is down, or because the path is wrong. \
If you are behind a firewall which requires a proxy, TortoiseCVS is using the Internet Explorer proxy. You might want to set it and try again.\n\n\
Please either try entering the URL again, or choose the plain text log.");
            message += wxT("\n\n");
        }
        else
        {
            message += _(
"TortoiseCVS couldn't find the URL on your server where ViewCVS or CVSweb is. \
Please enter the URL manually.");
            message += wxT("\n\n");
        }
    }

    // Explain how they can work it out
    message += _(
"You might be able to find it by browsing on the project's web site for a web interface to CVS. \
Alternatively ask your server administrator. Then copy the top level URL into the prefix box below. \
Usually you can leave the suffix box blank - it adds extra options to the end of the URL.");

    // TODO: Detect this case, and do something better:
    if (!queryWasForced)
         message += _("This dialog can also appear if you are trying to log a file that has never been committed.");

    // As a default - the combo box remembers more that the user has entered
    urlStubPrefix = "http://" + myServer + "/cgi-bin/cvsweb.cgi";
    urlStubSuffix = "";
    bool result = DoLogConfigDialog(myProgressDialog, urlStubPrefix, urlStubSuffix, 
       message, myServer);
    urlStubPrefix = RemoveTrailingDelimiter(urlStubPrefix);
    if (urlStubPrefix != "scanagain")
        myQueriedBefore = true;
    else
        myQueriedBefore = false;

    // Store what the user did, so the next automatic scan picks it up from the registry
    if (result)
    {
        TortoiseRegistry::WriteString("WebLogURLPrefix\\" + myServer 
           + std::string(":") + myDirectory, urlStubPrefix);
        TortoiseRegistry::WriteString("WebLogURLSuffix\\" + myServer 
           + std::string(":") + myDirectory, urlStubSuffix);
    }

    return result;
}

bool WebLog::PerformSearch(std::string& url)
{
   // let user override, and query if they want
   if (IsControlDown())
      return PerformSearchAndQuery(url);

   // otherwise just work it out by ourself
    std::string urlStubPrefix, urlStubSuffix;
    if (AutomaticallyFindURL(urlStubPrefix, urlStubSuffix))
    {
        url = MakeFullURL(urlStubPrefix, urlStubSuffix);
        return true;
    }
   return false;
}

bool WebLog::PerformSearchAndQuery(std::string& url)
{
   TDEBUG_ENTER("WebLog::PerformSearchAndQuery");
   TDEBUG_TRACE("url: " << url);
   std::string urlStubPrefix, urlStubSuffix;
   bool queryWasForced = false;

    // Holding down control disables the autoscan, and does a query straight away
   if (IsControlDown())
   {
      queryWasForced = true;
   }
   else
   {
      // Check TortoiseCVS.Weblog file
/*      if (GetURLFromConfigFile(urlStubPrefix, urlStubSuffix)
      {
         url = MakeFullURL(urlStubPrefix, urlStubSuffix);
         return true;
      }*/


      // They're not holding it down, so do the autoscan now
      if (AutomaticallyFindURL(urlStubPrefix, urlStubSuffix))
      {
         url = MakeFullURL(urlStubPrefix, urlStubSuffix);
         return true;
      }
   }

   while (true)
   {
      // Ask the user for a URL, give up if they cancel
      if (!QueryUser(queryWasForced, urlStubPrefix, urlStubSuffix))
         return false;
      queryWasForced = false;

      // If the user chose another autoscan, then do so
      if (urlStubPrefix == "scanagain")
      {
         if (AutomaticallyFindURL(urlStubPrefix, urlStubSuffix))
         {
            url = MakeFullURL(urlStubPrefix, urlStubSuffix);
            return true;
         }
      }
      else
      {
         // Check to see if their URL is OK
         if (IsHTTPStatusOK(TrySingleURL(urlStubPrefix, urlStubSuffix)))
         {
            url = MakeFullURL(urlStubPrefix, urlStubSuffix);
            return true;
         }
      }

      // Nothing worked, so loop round to ask the user again
   }
}

std::string WebLog::GetReturnedHTML()
{
   return myReturnedHTML;
}

bool WebLog::IsHTTPStatusOK(int statuscode)
{
   bool ret = false;
   switch (statuscode)
   {
   case HTTP_STATUS_OK:
   case HTTP_STATUS_DENIED:
   case HTTP_STATUS_FORBIDDEN:
      ret = true;
      break;
   default:
      break;
   }
   return ret;
}
