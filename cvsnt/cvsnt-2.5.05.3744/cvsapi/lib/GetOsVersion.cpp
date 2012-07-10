/*
	CVSNT Generic API
    Copyright (C) 2004 Tony Hoyle and March-Hare Software Ltd

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

/* The Windows version of this routine is mostly directly from MSDN and covered by the
   relevant Microsoft license */
#include <config.h>
#include "api_system.h"
#include <string.h>
#include <stdio.h>
#include "GetOsVersion.h"

#ifndef _WIN32
bool GetOsVersion(char *os, char *servicepack, int& major, int& minor)
{
	if (os)
		strcpy(os,OS_VERSION);
	if (servicepack)
		strcpy(servicepack,"");
	major=minor=0;
	return true;
}
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef SM_SERVERR2
#define SM_SERVERR2 89
#endif

bool GetOsVersion(char *os, char *servicepack, int& major, int& minor)
{
	DWORD dwType;

	typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
	typedef BOOL (WINAPI *PGPI)(DWORD, DWORD, DWORD, DWORD, PDWORD);

	// most time this is called the error status is not checked and the values always used
	// so it is better to always return something valid
	major=minor=0;
	ZeroMemory(os,sizeof(os));
	ZeroMemory(servicepack,sizeof(servicepack));

	OSVERSIONINFOEXA vi = {sizeof(OSVERSIONINFOEXA)};
	if(!GetVersionExA((OSVERSIONINFOA*)&vi))
	{
		strcpy(os,"NT4 or older.  Not supported.");
		return false; // NT4 or older.  Not supported.
	}

	SYSTEM_INFO si = {0};
	PGNSI pGNSI = (PGNSI)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetNativeSystemInfo");
	if(pGNSI) pGNSI(&si);
	else GetSystemInfo(&si);

	major = (int)vi.dwMajorVersion;
	minor = (int)vi.dwMinorVersion;
	sprintf(os,"Windows %d.%d",(int)vi.dwMajorVersion,(int)vi.dwMinorVersion);

	if (vi.dwPlatformId==VER_PLATFORM_WIN32_NT && vi.dwMajorVersion >4)
	{
		if (vi.dwMajorVersion == 6)
		{
			if(vi.dwMinorVersion == 0)
			{
				if(vi.wProductType == VER_NT_WORKSTATION)
					strcpy(os,"Windows Vista");
				else strcpy(os,"Windows Server 2008");
			}
			else if(vi.dwMinorVersion == 1)
			{
				if(vi.wProductType == VER_NT_WORKSTATION)
					strcpy(os,"Windows 7");
				else strcpy(os,"Windows Server 2008 R2");
			}
			else
			{
				strcpy(os,"unknown OS");
			}

			PGPI pGPI = (PGPI) GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetProductInfo");
			pGPI( 6, 0, 0, 0, &dwType);

			switch( dwType )
			{
			case PRODUCT_ULTIMATE:
				strcat(os, " Ultimate");
				break;
			case PRODUCT_HOME_PREMIUM:
				strcat(os," Home Premium");
				break;
			case PRODUCT_HOME_BASIC:
				strcat(os," Home Basic");
				break;
			case PRODUCT_ENTERPRISE:
				strcat(os," Enterprise");
				break;
			case PRODUCT_BUSINESS:
				strcat(os," Business");
				break;
			case PRODUCT_STARTER:
				strcat(os, " Starter");
				break;
			case PRODUCT_CLUSTER_SERVER:
				strcat(os, " Cluster Server");
				break;
			case PRODUCT_DATACENTER_SERVER:
				strcat(os, " Datacenter");
				break;
			case PRODUCT_DATACENTER_SERVER_CORE:
				strcat(os, " Datacenter (core)");
				break;
			case PRODUCT_ENTERPRISE_SERVER:
				strcat(os," Enterprise");
				break;
			case PRODUCT_ENTERPRISE_SERVER_CORE:
				strcat(os," Enterprise (core)");
				break;
			case PRODUCT_ENTERPRISE_SERVER_IA64:
				strcat(os, " Enterprise for Itanium");
				break;
			case PRODUCT_SMALLBUSINESS_SERVER:
				strcat(os, " Small Business Server");
				break;
			case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
				strcat(os, " Small Business Server Premium");
				break;
			case PRODUCT_STANDARD_SERVER:
				strcat(os, " Standard");
				break;
			case PRODUCT_STANDARD_SERVER_CORE:
				strcat(os, " Standard (core)");
				break;
			case PRODUCT_WEB_SERVER:
				strcat(os, " Web Server");
				break;
			default:
				{
					char t[64];
					snprintf(t,64,"Unknown type %d",dwType);
					strcat(os,t);
				}
				break;
			}
			if ( si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64 )
			strcat(os, " (64bit)");
			else if (si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_INTEL )
			strcat(os, " (32-bit)");
		}

		if ( vi.dwMajorVersion == 5 && vi.dwMinorVersion == 2 )
		{
			if( GetSystemMetrics(SM_SERVERR2) )
				strcpy(os,"Windows 2003 R2");
			else if ( vi.wSuiteMask==VER_SUITE_STORAGE_SERVER )
				strcpy(os,"Windows Storage Server 2003");
			else if( vi.wProductType == VER_NT_WORKSTATION &&
					si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64)
				strcpy(os,"Windows XP x64");
			else
				strcpy(os,"Windows 2003");

			// Test for the server type.
			if ( vi.wProductType != VER_NT_WORKSTATION )
			{
				if ( si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_IA64 )
				{
					if( vi.wSuiteMask & VER_SUITE_DATACENTER )
						strcat(os, " Datacenter for Itanium");
					else if( vi.wSuiteMask & VER_SUITE_ENTERPRISE )
						strcat(os, " Enterprise for Itanium");
				}
				else if ( si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64 )
				{
					if( vi.wSuiteMask & VER_SUITE_DATACENTER )
						strcat(os, " Datacenter x64");
					else if( vi.wSuiteMask & VER_SUITE_ENTERPRISE )
						strcat(os, " Enterprise x64");
					else
						strcat(os, " Standard x64" );
				}
				else
				{
					if ( vi.wSuiteMask & VER_SUITE_COMPUTE_SERVER )
						strcat(os, " Compute Cluster" );
					else if( vi.wSuiteMask & VER_SUITE_DATACENTER )
						strcat(os, " Datacenter");
					else if( vi.wSuiteMask & VER_SUITE_ENTERPRISE )
						strcat(os, " Enterprise");
					else if ( vi.wSuiteMask & VER_SUITE_BLADE )
						strcat(os, " Web");
					else
						strcat(os, " Standard");
				}
			}
		}

		if (vi.dwMajorVersion == 5 && vi.dwMinorVersion == 1)
		{
			strcpy(os,"Windows XP ");
			if( vi.wSuiteMask & VER_SUITE_PERSONAL )
				strcat(os,"Home");
			else
				strcat(os,"Professional");
		}

		if ( vi.dwMajorVersion == 5 && vi.dwMinorVersion == 0 )
		{
			strcpy(os,"Windows 2000");

			if ( vi.wProductType == VER_NT_WORKSTATION )
				strcat(os," Professional");
			else 
			{
				if( vi.wSuiteMask & VER_SUITE_DATACENTER )
					strcat(os," Datacenter Server");
				else if( vi.wSuiteMask & VER_SUITE_ENTERPRISE )
					strcat(os," Advanced Server");
				else
					strcat(os," Server");
			}
		}
	}

	strcpy(servicepack,vi.szCSDVersion);

	return true;
}
#endif
