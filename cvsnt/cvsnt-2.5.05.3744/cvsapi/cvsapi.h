/*
	CVSNT Generic API
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef CVSAPI__H
#define CVSAPI__H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef WINVER
	#define WINVER 0x501
#endif
#ifndef _WIN32_WINNT
	#define _WIN32_WINNT 0x501
#endif

#ifdef _WIN32
// Microsoft braindamage reversal.  
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#endif

#include <windows.h>
#include <tchar.h>
#define SECURITY_WIN32
#include <security.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>
#else
#include <stdlib.h>
#include <limits.h>
#endif

#ifdef _WIN32
#include "win32/config.h"
#else
#include <config.h>
#endif
#include "lib/api_system.h"

#include "cvs_smartptr.h"
#include "cvs_string.h"
#include "GetOptions.h"
#include "RunFile.h"
#include "TokenLine.h"
#include "FileEntry.h"
#include "FileAccess.h"
#include "DirectoryAccess.h"
#include "TagDate.h"
#include "SqlConnection.h"
#include "SqlRecordset.h"
#include "ServerIO.h"
#include "Codepage.h"
//#include "diff/DiffBase.h"
//#include "diff/StringDiff.h"
#include "SplitPath.h"
#include "SocketIO.h"
#include "md5calc.h"
#include "XmlTree.h"
#include "XmlNode.h"
#include "rpcBase.h"
#include "SSPIHandler.h"
#include "HttpSocket.h"
#include "Zeroconf.h"
#include "LibraryAccess.h"
#include "DnsApi.h"
#include "crypt.h"
#include "lib/getdate.h"
#include "lib/GetOsVersion.h"

#endif

