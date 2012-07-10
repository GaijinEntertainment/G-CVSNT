// $Id: HttpSnaffle.cpp,v 1.1 2012/03/04 01:06:54 aliot Exp $

// InstallLib - library of code for making Windows installation programs
// Copyright (C) 2000 - Creature Labs

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version
 
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Library General Public License for more details.
 
// You should have received a copy of the GNU Library General
// Public License alongwith this library; if not, write to the
// Free Software Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.

#include "StdAfx.h"
#include "HttpSnaffle.h"
#include <iostream>
#include <string>

HttpSnaffle::HttpSnaffle()
   : myInet(0),
     myServerConnection(0),
     myRequest(0),
     myContext(0)
{
}

void HttpSnaffle::CloseHandles()
{
    if (myServerConnection)
        InternetCloseHandle(myServerConnection);
    if (myInet)
        InternetCloseHandle(myInet);
    if (myRequest)
        InternetCloseHandle(myRequest);

    myInet = NULL;
    myServerConnection = NULL;
    myRequest = NULL;
}


HttpSnaffle::~HttpSnaffle()
{
    CloseHandles();
}

bool HttpSnaffle::Open(const std::string& userAgent, const std::string& serverName, int port)
{
    CloseHandles();

    myServerName = serverName;
    myUserAgent = userAgent;

    myInet = InternetOpenA(userAgent.c_str(),
                           INTERNET_OPEN_TYPE_PRECONFIG, "", "", 0);
    if (myInet == NULL)
        return false;

    myServerConnection = InternetConnectA(myInet, serverName.c_str(),
                                          port, "", "",
                                          INTERNET_SERVICE_HTTP, 0,
                                          myContext);

    if (myServerConnection == NULL)
        return false;

    return true;
}

bool HttpSnaffle::IsOpen()
{
    return myServerConnection != NULL;
}

bool HttpSnaffle::Reopen()
{
    return Open(myUserAgent, myServerName, myPort);
}

bool HttpSnaffle::StartFetch(std::string objectPath)
{
    // Under Win95/HTTP1.0 you need an initial slash
    if (objectPath[0] != '/')
        objectPath = std::string("/") + objectPath;

    // NB: If you consider setting INTERNET_FLAG_PRAGMA_NOCACHE, be
    // warned it doesn't work on a base Win95 machine.

    myRequest = HttpOpenRequestA(myServerConnection, NULL, objectPath.c_str(), NULL, NULL, 
                                 NULL, // mime types
                                 // Force a load from the server, not the cache.  This is in case
                                 // the file has changed, or has been corrupted during previous transfer.
                                 INTERNET_FLAG_RELOAD | INTERNET_FLAG_RESYNCHRONIZE, 
                                 myContext);

    if (myRequest == NULL)
        return false;

    BOOL result = HttpSendRequest(myRequest, 0, 0, 0, 0);
    if (!result)
        return false;

    return true;
}

int HttpSnaffle::FetchStatusCode()
{
    int length = -1;
    DWORD size = sizeof(length);
    DWORD index = 0;
    if (!HttpQueryInfo(myRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &length, &size, &index))
        return -1;
    return length;
}

std::string HttpSnaffle::FetchStatusMessage()
{
    char* p = 0;
    DWORD size = 0;
    DWORD index = 0;
    // First get size
    HttpQueryInfo(myRequest, HTTP_QUERY_STATUS_TEXT, p, &size, &index);
    // Then get actual data
    p = new char[size];
    std::string s;
    if (HttpQueryInfo(myRequest, HTTP_QUERY_STATUS_TEXT, p, &size, &index))
    {
        s = p;
        delete[] p;
    }
    return s;
}

int HttpSnaffle::FetchMore(std::ostream& out)
{
    // Find out how much there is to download
    DWORD dwSize;
    if (!InternetQueryDataAvailable(myRequest, &dwSize, 0, 0))
        return -1;

	if (!dwSize)
		return 0;

    // Make sure buffer is big enough
    myBuffer.resize(dwSize);

    // Read the data
    DWORD dwDownloaded;
    if (!InternetReadFile(myRequest, (LPVOID)&myBuffer[0], dwSize, &dwDownloaded))
        return -1;

    // See if we're done
    if (dwDownloaded == 0)
    {
        int statusCode = -1;
        DWORD size = sizeof(statusCode);
        DWORD index = 0;
        if (!HttpQueryInfo(myRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &size, &index))
            return -1;

        if (statusCode != HTTP_STATUS_OK)
            return -1;

        InternetCloseHandle(myRequest);
        myRequest = NULL;

        return 0;
    }

    // Write it out to a file
    // std::cout << "Read in " << dwDownloaded << " bytes" << std::endl;
    out.write(&myBuffer[0], dwDownloaded);

    return dwDownloaded;
}

void HttpSnaffle::FetchWholeFile(std::string objectPath, std::ostream& out)
{
    StartFetch(objectPath);
    int value = 1;
    while (value != 0)
    {
        value = FetchMore(out);
    }
}
