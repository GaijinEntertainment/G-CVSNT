/*
	CVSNT Windows Scripting trigger handler
    Copyright (C) 2005 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <config.h>
#define STRICT
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0500
#include <windows.h>
#include <comutil.h>
#include <atlbase.h>
#include <atlcom.h>
#include "../cvsapi/cvsapi.h"

#include "server.h"

extern int server_codepage;

STDMETHODIMP CServer::Trace(short Level, BSTR Text)
{
	cvs::string tmp;
	tmp.resize(wcslen(Text)*3);
	tmp.resize(WideCharToMultiByte(server_codepage,0,Text,wcslen(Text),(char*)tmp.c_str(),tmp.size(),NULL,NULL));
	CServerIo::trace((int)Level,"%s",tmp.c_str());
	return S_OK;
}

STDMETHODIMP CServer::Warning(BSTR Text)
{
	cvs::string tmp;
	tmp.resize(wcslen(Text)*3);
	tmp.resize(WideCharToMultiByte(server_codepage,0,Text,wcslen(Text),(char*)tmp.c_str(),tmp.size(),NULL,NULL));
	CServerIo::warning("%s\n",tmp.c_str());
	return S_OK;
}

STDMETHODIMP CServer::Error(BSTR Text)
{
	cvs::string tmp;
	tmp.resize(wcslen(Text)*3);
	tmp.resize(WideCharToMultiByte(server_codepage,0,Text,wcslen(Text),(char*)tmp.c_str(),tmp.size(),NULL,NULL));
	CServerIo::error("%s\n",tmp.c_str());
	return S_OK;
}

