// installer.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "../../version.h"

static const TCHAR *register_url = _T("http://march-hare.com/cvspro/prods-pre.asp");
static const TCHAR *contact_url = _T("http://march-hare.com/cvspro/prods.asp");
static const TCHAR *subscribe_url = _T("http://www.cvsnt.org/cgi-bin/mailman/subscribe/cvsnt");
static const TCHAR *check_url = _T("https://www.march-hare.com/WEBTOOLS/CUSTOMER/Login.aspx");

static bool HttpRequest(MSIHANDLE hInstall, HINTERNET hInternet, LPCTSTR url, LPCTSTR extra, LPBYTE& lpBuf, DWORD& dwSize, DWORD& dwStatusCode, LPVOID *plpHdrBuf=NULL);

// This is undefined in VS2008
#ifndef SM_SERVERR2
	#define SM_SERVERR2	89
#endif

struct __parm
{
	__parm(bool b) { type = 0; }
	__parm(LPCTSTR f) { type=1; pf = f; }
	__parm(UINT f) { type=2; uf = f; }
	__parm(int f) { type=2; uf = (UINT)f; }
	int type;
	LPCTSTR pf;
	UINT uf;

	void set(MSIHANDLE hRecord, UINT field)
	{ 
		switch(type)
		{
		case 1: MsiRecordSetString(hRecord,field,pf); break;
		case 2: MsiRecordSetInteger(hRecord,field,uf); break;
		}
	}
};

void Log(MSIHANDLE hInstall, LPCTSTR f1, __parm f2 = false, __parm f3 = false, __parm f4 = false)
{
	PMSIHANDLE hRecord = MsiCreateRecord(4);
	MsiRecordSetString(hRecord,0,f1);
	f2.set(hRecord,1);
	f3.set(hRecord,2);
	f4.set(hRecord,3);
	MsiProcessMessage(hInstall,INSTALLMESSAGE(INSTALLMESSAGE_INFO),hRecord);

	// Can't log during UI processing due to an MSI bug, so we send it to the debug channel as well
	OutputDebugString(f1);
	OutputDebugString(_T("\r\n"));
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}

static void GetOsVersion(LPTSTR os, LPTSTR servicepack)
{
	typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
	typedef BOOL (WINAPI *PGPI)(DWORD, DWORD, DWORD, DWORD, PDWORD);

	DWORD dwType;

	// most time this is called the error status is not checked and the values always used
	// so it is better to always return something valid
	int major=0, minor=0;
	ZeroMemory(os,sizeof(os));
	ZeroMemory(servicepack,sizeof(servicepack));

	OSVERSIONINFOEX vi = {sizeof(OSVERSIONINFOEX)};
	if(!GetVersionEx((OSVERSIONINFO*)&vi))
 	{
 		_tcscpy(os,_T("NT4 or older.  Not supported."));
  		return; // NT4 or older.  Not supported.
 	}

	SYSTEM_INFO si = {0};
	PGNSI pGNSI = (PGNSI)GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "GetNativeSystemInfo");
	if(pGNSI) pGNSI(&si);
	else GetSystemInfo(&si);

	major = (int)vi.dwMajorVersion;
	minor = (int)vi.dwMinorVersion;
	_stprintf(os,_T("Windows %d.%d"),(int)vi.dwMajorVersion,(int)vi.dwMinorVersion);

	if (vi.dwPlatformId==VER_PLATFORM_WIN32_NT && vi.dwMajorVersion >4)
	{
		if (vi.dwMajorVersion == 6)
		{
			if(vi.dwMinorVersion == 0)
			{
				if(vi.wProductType == VER_NT_WORKSTATION)
					lstrcpy(os,_T("Windows Vista"));
				else lstrcpy(os,_T("Windows Server 2008"));
			}
			else if(vi.dwMinorVersion == 1)
			{
				if(vi.wProductType == VER_NT_WORKSTATION)
					lstrcpy(os,_T("Windows 7"));
				else lstrcpy(os,_T("Windows Server 2008 R2"));
			}
			else
			{
				lstrcpy(os,_T("unknown OS"));
			}

		PGPI pGPI = (PGPI) GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "GetProductInfo");
		pGPI( 6, 0, 0, 0, &dwType);

		switch( dwType )
		{
		case PRODUCT_ULTIMATE:
		   lstrcat(os, _T(" Ultimate"));
		   break;
		case PRODUCT_HOME_PREMIUM:
		   lstrcat(os,_T(" Home Premium"));
		   break;
		case PRODUCT_HOME_BASIC:
		   lstrcat(os,_T(" Home Basic"));
		   break;
		case PRODUCT_ENTERPRISE:
		   lstrcat(os,_T(" Enterprise"));
		   break;
		case PRODUCT_BUSINESS:
		   lstrcat(os,_T(" Business"));
		   break;
		case PRODUCT_STARTER:
		   lstrcat(os, _T(" Starter"));
		   break;
		case PRODUCT_CLUSTER_SERVER:
		   lstrcat(os, _T(" Cluster Server"));
		   break;
		case PRODUCT_DATACENTER_SERVER:
		   lstrcat(os, _T(" Datacenter"));
		   break;
		case PRODUCT_DATACENTER_SERVER_CORE:
		   lstrcat(os, _T(" Datacenter (core)"));
		   break;
		case PRODUCT_ENTERPRISE_SERVER:
		   lstrcat(os,_T(" Enterprise"));
		   break;
		case PRODUCT_ENTERPRISE_SERVER_CORE:
		   lstrcat(os,_T(" Enterprise (core)"));
		   break;
		case PRODUCT_ENTERPRISE_SERVER_IA64:
		   lstrcat(os, _T(" Enterprise for Itanium"));
		   break;
		case PRODUCT_SMALLBUSINESS_SERVER:
		   lstrcat(os, _T(" Small Business Server"));
		   break;
		case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
		   lstrcat(os, _T(" Small Business Server Premium"));
		   break;
		case PRODUCT_STANDARD_SERVER:
		   lstrcat(os, _T(" Standard"));
		   break;
		case PRODUCT_STANDARD_SERVER_CORE:
		   lstrcat(os, _T(" Standard (core)"));
		   break;
		case PRODUCT_WEB_SERVER:
		   lstrcat(os, _T(" Web Server"));
		   break;
		}

		if ( si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64 )
			lstrcat(os, _T(" (64bit)"));
		else if (si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_INTEL )
			lstrcat(os, _T(" (32-bit)"));
		}

      if ( vi.dwMajorVersion == 5 && vi.dwMinorVersion == 2 )
      {
         if( GetSystemMetrics(SM_SERVERR2) )
            lstrcpy(os,_T("Windows 2003 R2"));
         else if ( vi.wSuiteMask==VER_SUITE_STORAGE_SERVER )
            lstrcpy(os,_T("Windows Storage Server 2003"));
         else if( vi.wProductType == VER_NT_WORKSTATION &&
                   si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64)
			lstrcpy(os,_T("Windows XP x64"));
         else
			lstrcpy(os,_T("Windows 2003"));

         // Test for the server type.
         if ( vi.wProductType != VER_NT_WORKSTATION )
         {
            if ( si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_IA64 )
            {
                if( vi.wSuiteMask & VER_SUITE_DATACENTER )
                   lstrcat(os, _T(" Datacenter for Itanium"));
                else if( vi.wSuiteMask & VER_SUITE_ENTERPRISE )
                   lstrcat(os, _T(" Enterprise for Itanium"));
            }
            else if ( si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64 )
            {
                if( vi.wSuiteMask & VER_SUITE_DATACENTER )
                   lstrcat(os, _T(" Datacenter x64"));
                else if( vi.wSuiteMask & VER_SUITE_ENTERPRISE )
                   lstrcat(os, _T("Enterprise x64"));
                else
					lstrcat(os, _T("Standard x64") );
            }

            else
            {
                if ( vi.wSuiteMask & VER_SUITE_COMPUTE_SERVER )
					lstrcat(os, _T(" Compute Cluster") );
                else if( vi.wSuiteMask & VER_SUITE_DATACENTER )
					lstrcat(os, _T(" Datacenter"));
                else if( vi.wSuiteMask & VER_SUITE_ENTERPRISE )
					lstrcat(os, _T("Enterprise"));
                else if ( vi.wSuiteMask & VER_SUITE_BLADE )
					lstrcat(os, _T(" Web"));
                else
					lstrcat(os, _T(" Standard"));
            }
         }
      }

      if (vi.dwMajorVersion == 5 && vi.dwMinorVersion == 1)
      {
         lstrcpy(os,_T("Windows XP "));
         if( vi.wSuiteMask & VER_SUITE_PERSONAL )
			lstrcat(os,_T("Home"));
         else
			lstrcat(os,_T("Professional"));
      }

      if ( vi.dwMajorVersion == 5 && vi.dwMinorVersion == 0 )
      {
         lstrcpy(os,_T("Windows 2000"));

         if ( vi.wProductType == VER_NT_WORKSTATION )
            lstrcat(os,_T(" Professional"));
         else 
         {
            if( vi.wSuiteMask & VER_SUITE_DATACENTER )
				lstrcat(os,_T(" Datacenter Server"));
            else if( vi.wSuiteMask & VER_SUITE_ENTERPRISE )
				lstrcat(os,_T(" Advanced Server"));
            else
				lstrcat(os,_T(" Server"));
         }
      }

	  lstrcpy(servicepack, vi.szCSDVersion);
   }
}

static bool HttpRequest(MSIHANDLE hInstall, HINTERNET hInternet, LPCTSTR url, LPCTSTR extra, LPBYTE& lpBuf, DWORD& dwSize, DWORD& dwStatusCode, LPVOID *plpHdrBuf)
{
	WINHTTP_PROXY_INFO api = {0};
	WINHTTP_CURRENT_USER_IE_PROXY_CONFIG iepc={0};
	if(WinHttpGetIEProxyConfigForCurrentUser(&iepc))
	{
		if(!iepc.fAutoDetect && iepc.lpszProxy)
		{
			api.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
			api.lpszProxy = iepc.lpszProxy;
			api.lpszProxyBypass = iepc.lpszProxyBypass;
			WinHttpSetOption(hInternet,WINHTTP_OPTION_PROXY,&api,sizeof(api));
		}
		if(iepc.lpszProxy) GlobalFree(GlobalHandle(iepc.lpszProxy));
		if(iepc.lpszProxyBypass) GlobalFree(GlobalHandle(iepc.lpszProxyBypass));
		if(iepc.lpszAutoConfigUrl) GlobalFree(GlobalHandle(iepc.lpszAutoConfigUrl));
	}

	URL_COMPONENTS urlc = { sizeof(urlc) };
	TCHAR szHostName[64];
	DWORD dw;

	urlc.dwSchemeLength=-1;
	urlc.dwHostNameLength=-1;
	urlc.dwUrlPathLength=-1;
	urlc.dwExtraInfoLength=-1;

	if(!WinHttpCrackUrl(url,0,0,&urlc))
	{
		Log(hInstall,_T("WinHttCrackUrl failed"));
		dw = GetLastError();
		return false;
	}

	lstrcpyn(szHostName,urlc.lpszHostName,urlc.dwHostNameLength+1);
	szHostName[urlc.dwHostNameLength+1]='\0';

	HINTERNET hSession = WinHttpConnect(hInternet, szHostName, urlc.nPort,  0);
	if(!hSession)
	{
		Log(hInstall,_T("WinHttpConnect failed"));
		return false;
	}

	TCHAR newurl[512];
	DWORD dwurlLen=sizeof(newurl)/sizeof(newurl[0]);

	urlc.lpszExtraInfo = (LPTSTR)extra;
	urlc.dwExtraInfoLength=lstrlen(extra);
	if(!WinHttpCreateUrl(&urlc,ICU_ESCAPE,newurl,&dwurlLen))
	{
		Log(hInstall,_T("WinHttpCreateUrl failed"));
		WinHttpCloseHandle(hSession);
		return false;
	}
	memset(&urlc,0,sizeof(urlc));
	urlc.dwStructSize=sizeof(urlc);
	urlc.dwSchemeLength=-1;
	urlc.dwHostNameLength=-1;
	urlc.dwUrlPathLength=-1;
	urlc.dwExtraInfoLength=-1;
	if(!WinHttpCrackUrl(newurl,0,0,&urlc))
	{
		Log(hInstall,_T("WinHttpCrackUrl failed"));
		WinHttpCloseHandle(hSession);
		return false;
	}

	HINTERNET hRequest = WinHttpOpenRequest(hSession, _T("GET"),  urlc.lpszUrlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, urlc.nScheme==INTERNET_SCHEME_HTTPS?WINHTTP_FLAG_SECURE:0);
	if(!hRequest)
	{
		Log(hInstall,_T("WinHttpOpenRequest failed"));
		WinHttpCloseHandle(hSession);
		return false;
	}

	LPTSTR szOptionalData = 0;
	DWORD dwOptionalLength = 0;
	if(!WinHttpSendRequest(hRequest,WINHTTP_NO_ADDITIONAL_HEADERS,0,szOptionalData,dwOptionalLength,dwOptionalLength,NULL))
	{
		Log(hInstall,_T("WinHttpSendRequest failed"));
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hSession);
		return false;
	}

	if(!WinHttpReceiveResponse(hRequest,NULL))
	{
		Log(hInstall,_T("WinHttpReceiveResponse failed"));
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hSession);
		return false;
	}

	dwSize = sizeof(dwStatusCode);
	if(!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &dwStatusCode, &dwSize, NULL)) 
	{
		Log(hInstall,_T("WinHttpQueryHeaders(1) failed"));
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hSession);
		return false;
	}

	if (plpHdrBuf!=NULL)
	{

		if(!WinHttpQueryHeaders( hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                             WINHTTP_HEADER_NAME_BY_INDEX, NULL, 
                             &dwSize, WINHTTP_NO_HEADER_INDEX)) 
		{
			// Allocate memory for the buffer.
			if( GetLastError( ) == ERROR_INSUFFICIENT_BUFFER )
			{
				BOOL  bResult = FALSE;	

				*plpHdrBuf = new WCHAR[dwSize/sizeof(WCHAR)];

				// Now, use WinHttpQueryHeaders to retrieve the header.
				bResult = WinHttpQueryHeaders( hRequest, 
                                       WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                       WINHTTP_HEADER_NAME_BY_INDEX, 
                                       *plpHdrBuf, &dwSize, 
                                       WINHTTP_NO_HEADER_INDEX);
				if(!bResult)
				{
					Log(hInstall,_T("WinHttpQueryHeaders(3) failed"));
					WinHttpCloseHandle(hRequest);
					WinHttpCloseHandle(hSession);
					return false;
				}
			}
			else
			{
				Log(hInstall,_T("WinHttpQueryHeaders(2) failed"));
				WinHttpCloseHandle(hRequest);
				WinHttpCloseHandle(hSession);
				return false;
			}
		}
	}

	lpBuf = NULL;
	dwSize = 0;

	if(!WinHttpQueryDataAvailable(hRequest,&dwSize))
	{
		Log(hInstall,_T("WinHttpQueryDataAvailable failed"));
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hSession);
		return true;
	}

	if(dwSize)
	{
		lpBuf = new BYTE[dwSize+1];
		lpBuf[dwSize]='\0';
		if(!WinHttpReadData(hRequest,lpBuf,dwSize,NULL))
		{
			Log(hInstall,_T("WinHttpReadData failed"));
			delete[] lpBuf;
			lpBuf = NULL;
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hSession);
			return false;
		}
	}

	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hSession);
	return true;
}

static bool PostInstallData(MSIHANDLE hInstall, bool allusers,LPCTSTR url, LPCTSTR package, bool install, LPCTSTR serial, LPCTSTR os, LPCTSTR servicepack, LPCTSTR version)
{
	HINTERNET hInternet = WinHttpOpen(_T("March Hare Installer"), WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if(hInternet)
	{
		TCHAR extra[512];
		LPBYTE lpBuf;
		DWORD dwSize, dwStatusCode;

		if(install)
				_sntprintf(extra,sizeof(extra)/sizeof(extra[0]),_T("?register=%s&os=%s&sp=%s&ver=%s"),package,os,servicepack,version);
			else
				_sntprintf(extra,sizeof(extra)/sizeof(extra[0]),_T("?register=remove&id=%s"),serial);

		if(!HttpRequest(hInstall, hInternet, url, extra, lpBuf, dwSize, dwStatusCode))
			return false;

		if(!lpBuf)
		{
			Log(hInstall,_T("Http Request returned no data"));
			return false;
		}

		if(install)
		{
			// Stick serial in the registry, theoretically
			const char *p = strstr((const char *)lpBuf,"number:");
			if(p)
			{
				p+=7;
				const char *q=strchr(p,'\n');
				if(!q)
					q=p+strlen(p);
				TCHAR serial[256];
				serial[MultiByteToWideChar(CP_UTF8,0,p,(int)(q-p),serial,sizeof(serial)/sizeof(serial[0]))]='\0';
				Log(hInstall,_T("Serial is [1]"),serial);
				MsiSetProperty(hInstall,_T("REGISTRATION_SERIAL"),serial);
			}
		}

		delete[] lpBuf;
		WinHttpCloseHandle(hInternet);
	}
	else
		Log(hInstall,_T("WinHttpOpen failed"));
	return true;
}

static bool SubscribeOrContact(MSIHANDLE hInstall, LPCTSTR subscribe_url, LPCTSTR contact_url, int subscribe, int contact, LPCTSTR email)
{
	if(!subscribe && !contact)
		return true;

	HINTERNET hInternet = WinHttpOpen(_T("March Hare Installer"), WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if(hInternet)
	{
		if(contact)
		{
			TCHAR extra[512];
			LPBYTE lpBuf;
			DWORD dwSize;
			DWORD dwStatusCode;

			_sntprintf(extra,sizeof(extra)/sizeof(extra[0]),_T("?format=downbox&Checkbox1=1&email=%s"),email);

			if(!HttpRequest(hInstall, hInternet, contact_url, extra, lpBuf, dwSize, dwStatusCode))
			{
				WinHttpCloseHandle(hInternet);
				return false;
			}

			if(lpBuf) delete[] lpBuf;
		}

		if(subscribe)
		{
			TCHAR extra[512];
			LPBYTE lpBuf;
			DWORD dwSize;
			DWORD dwStatusCode;

			_sntprintf(extra,sizeof(extra)/sizeof(extra[0]),_T("?email=%s"),email);

			if(!HttpRequest(hInstall, hInternet, subscribe_url, extra, lpBuf, dwSize, dwStatusCode))
			{
				WinHttpCloseHandle(hInternet);
				return false;
			}

			if(lpBuf) delete[] lpBuf;
		}
		WinHttpCloseHandle(hInternet);
	}
	else
		Log(hInstall,_T("WinHttpOpen failed"));
	return true;
}

static bool CheckRegistration(MSIHANDLE hInstall, LPCTSTR check_url, LPCTSTR email, LPCTSTR password)
{
	HINTERNET hInternet = WinHttpOpen(_T("March Hare Installer"), WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if(hInternet)
	{
		TCHAR extra[512];
		LPBYTE lpBuf;
		DWORD dwSize;
		DWORD dwStatusCode;
		LPBYTE lpHdrBuffer = NULL;

		_sntprintf(extra,sizeof(extra)/sizeof(extra[0]),_T("?ReturnUrl=/webtools/customer/chkreg.aspx&email=%s&password=%s"),email,password);

		if(!HttpRequest(hInstall, hInternet, check_url, extra, lpBuf, dwSize, dwStatusCode, (LPVOID *)&lpHdrBuffer))
		{
			WinHttpCloseHandle(hInternet);
			return false;
		}
		LPCTSTR szlpBuf=(LPCTSTR)lpBuf;
		LPCTSTR szlpHdrBuffer=(LPCTSTR)lpHdrBuffer;

		if((dwStatusCode/100)!=2)
		{
			Log(hInstall,_T("Server failed HTTP request"));
			if(lpBuf) delete[] lpBuf;
			WinHttpCloseHandle(hInternet);
			return false;
		}

		const char *szlpBufGood = strstr((const char *)lpBuf,"CVSNT march-hare.com Registration Check Successful");
		LPCTSTR szlpHdrBufferGood = _tcsstr(szlpHdrBuffer,_T("CVSNT+march-hare.com+Registration+Check+Successful"));
		if(!lpBuf || !lpHdrBuffer || dwSize < 50 || (szlpBufGood==NULL && szlpHdrBufferGood==NULL) )
		{
			Log(hInstall,_T("Registration check failed"));
			if(lpBuf) delete[] lpBuf;
			WinHttpCloseHandle(hInternet);
			return false;
		}
		Log(hInstall,_T("Registration check response"));
		Log(hInstall,(LPCTSTR)lpBuf);

		if(lpBuf) delete[] lpBuf;
		WinHttpCloseHandle(hInternet);
	}
	else
	{
		Log(hInstall,_T("WinHttpOpen failed"));
		return false;
	}
	return true;
}

UINT WINAPI CustomActionInstall(MSIHANDLE hInstall)
{
	TCHAR type[64],szallusers[64];
	DWORD dwSize = sizeof(type)/sizeof(type[0]);
	bool allusers;

	Log(hInstall,_T("installer.dll - Install"));

	UINT dwErr = MsiGetProperty(hInstall, _T("CVSNT_INSTALLTYPE"), type, &dwSize);
	if(dwErr)
	{
		// One option here is to just barf, but that'll lead to it not getting registered, better to 
		// make some sort of assumption
		lstrcpy(type,_T("server"));
	}
	dwSize = sizeof(szallusers)/sizeof(szallusers[0]);
	dwErr = MsiGetProperty(hInstall, _T("ALLUSERS"), szallusers, &dwSize);
	if(dwErr)
	{
		// One option here is to just barf, but that'll lead to it not getting registered, better to 
		// make some sort of assumption
		lstrcpy(szallusers,_T("0"));
	}
	allusers=_ttoi(szallusers)?true:false;
	if(!lstrlen(type))
	{
		Log(hInstall,_T("CVSNT_INSTALLTYPE is not set"));
		return 0;
	}

	Log(hInstall,_T("CVSNT_INSTALLTYPE is [1]"),type);

	CoInitialize(NULL);

	TCHAR os[128];
	TCHAR servicepack[128];

	GetOsVersion(os,servicepack);

	Log(hInstall,_T("OS Version is [1], ServicePack is [2]"),os,servicepack);
	
	PostInstallData(hInstall,allusers,register_url, type, true, NULL, os, servicepack, _T( CVSNT_PRODUCTVERSION_SHORT ));

	return 0;
}

UINT WINAPI CustomActionUninstall(MSIHANDLE hInstall)
{
	TCHAR type[64],szallusers[64];
	DWORD dwSize = sizeof(type)/sizeof(type[0]);
	bool allusers;

	Log(hInstall,_T("installer.dll - Uninstall"));

	UINT dwErr = MsiGetProperty(hInstall, _T("CVSNT_INSTALLTYPE"), type, &dwSize);
	if(dwErr)
	{
		// One option here is to just barf, but that'll lead to it not getting registered, better to 
		// make some sort of assumption
		lstrcpy(type,_T("server"));
	}
	dwSize = sizeof(szallusers)/sizeof(szallusers[0]);
	dwErr = MsiGetProperty(hInstall, _T("ALLUSERS"), szallusers, &dwSize);
	if(dwErr)
	{
		// One option here is to just barf, but that'll lead to it not getting registered, better to 
		// make some sort of assumption
		lstrcpy(szallusers,_T("0"));
	}
	allusers=_ttoi(szallusers)?true:false;
	if(!lstrlen(type))
	{
		Log(hInstall,_T("CVSNT_INSTALLTYPE is not set"));
		return 0;
	}

	Log(hInstall,_T("CVSNT_INSTALLTYPE is [1]"),type);

	CoInitialize(NULL);

	TCHAR serial[256];
	DWORD dwSerial = sizeof(serial);
	HKEY hKey;
	if(!RegOpenKey(allusers?HKEY_LOCAL_MACHINE:HKEY_CURRENT_USER,_T("Software\\March Hare Software Ltd\\Keys"),&hKey))
	{
		if(RegQueryValueEx(hKey,type,NULL,NULL,(LPBYTE)serial,&dwSerial))
			lstrcpy(serial,_T("Unknown"));
		RegCloseKey(hKey);
	}
	else
		lstrcpy(serial,_T("Unknown"));

	PostInstallData(hInstall,allusers,register_url, type, false,serial, NULL, NULL, _T( CVSNT_PRODUCTVERSION_SHORT ));

	return 0;
}

UINT WINAPI CustomActionCheck(MSIHANDLE hInstall)
{
	TCHAR email[1024],password[256];
	DWORD dwSize;
	
	Log(hInstall,_T("installer.dll - Register"));

	MsiSetProperty(hInstall,_T("REGISTRATION_CHECK"),_T("0"));

	dwSize = sizeof(email)/sizeof(email[0]);
	UINT dwErr = MsiGetProperty(hInstall, _T("REGISTERED_EMAIL"), email, &dwSize);
	if(dwErr!=ERROR_SUCCESS)
	{
		Log(hInstall, _T("REGISTERED_EMAIL not set"));
		return ERROR_INSTALL_FAILURE;
	}

	dwSize = sizeof(password)/sizeof(password[0]);
	dwErr = MsiGetProperty(hInstall, _T("REGISTERED_PASSWORD"), password, &dwSize);
	if(dwErr!=ERROR_SUCCESS)
	{
		Log(hInstall, _T("REGISTERED_PASSWORD not set"));
		return ERROR_INSTALL_FAILURE;
	}

	CoInitialize(NULL);

	if(CheckRegistration(hInstall, check_url, email, password))
	{
		MsiSetProperty(hInstall,_T("REGISTRATION_CHECK"),_T("1"));
		return ERROR_SUCCESS; 
	}

	// There's no 'Ignore the button press and stay on the current dialog' return.  ERROR_NO_MORE_ITEMS *sounds* like it
	// should do it but is ignored by the UI.
	return ERROR_INSTALL_FAILURE;
}

UINT WINAPI CustomActionEmail(MSIHANDLE hInstall)
{
	TCHAR type[256];
	DWORD dwSize = sizeof(type)/sizeof(type[0]);
	int contact,subscribe;
	
	Log(hInstall,_T("installer.dll - email"));

	MsiSetProperty(hInstall,_T("EMAIL_SUBSCRIBE"),_T("0"));

	dwSize = sizeof(type)/sizeof(type[0]);
	UINT dwErr = MsiGetProperty(hInstall, _T("SUBSCRIBE_ME"), type, &dwSize);
	if(dwErr) subscribe=0;
	else subscribe=_ttoi(type);

	dwSize = sizeof(type)/sizeof(type[0]);
	dwErr = MsiGetProperty(hInstall, _T("CONTACT_ME"), type, &dwSize);
	if(dwErr) contact=0;
	else contact=_ttoi(type);

	dwSize = sizeof(type)/sizeof(type[0]);
	dwErr = MsiGetProperty(hInstall, _T("CONTACT_EMAIL"), type, &dwSize);
	if(dwErr) type[0]='\0';

	Log(hInstall,_T("SUBSCRIBE_ME is [1]"),subscribe);
	Log(hInstall,_T("CONTACT_ME is [1]"),contact);
	Log(hInstall,_T("CONTACT_EMAIL is [1]"),type);

	if(type[0] && (subscribe||contact))
	{
		CoInitialize(NULL);

		if (SubscribeOrContact(hInstall, subscribe_url, contact_url,subscribe,contact, type))
		{
			MsiSetProperty(hInstall,_T("EMAIL_SUBSCRIBE"),_T("1"));
		}
	}

	return ERROR_SUCCESS; 
}

UINT WINAPI CustomActionSerial(MSIHANDLE hInstall)
{
	TCHAR serial[256], package[256], szallusers[64];
	DWORD dwSize = sizeof(package)/sizeof(package[0]);
	int serialno;
	bool allusers;
	bool keyopened=false;
	LSTATUS setresult;
	
	Log(hInstall,_T("installer.dll - serial"));

	dwSize = sizeof(package)/sizeof(package[0]);
	UINT dwErr = MsiGetProperty(hInstall, _T("CVSNT_INSTALLTYPE"), package, &dwSize);
	if(dwErr)
	{
		// One option here is to just barf, but that'll lead to it not getting registered, better to 
		// make some sort of assumption
		lstrcpy(package,_T("server"));
	}
	if(!lstrlen(package))
	{
		Log(hInstall,_T("CVSNT_INSTALLTYPE is not set"));
		return 0;
	}

	Log(hInstall,_T("CVSNT_INSTALLTYPE is [1]"),package);

	dwSize = sizeof(szallusers)/sizeof(szallusers[0]);
	dwErr = MsiGetProperty(hInstall, _T("ALLUSERS"), szallusers, &dwSize);
	if(dwErr)
	{
		// One option here is to just barf, but that'll lead to it not getting registered, better to 
		// make some sort of assumption
		lstrcpy(szallusers,_T("0"));
	}
	allusers=_ttoi(szallusers)?true:false;

	dwSize = sizeof(serial)/sizeof(serial[0]);
	dwErr = MsiGetProperty(hInstall, _T("REGISTRATION_SERIAL"), serial, &dwSize);
	if(dwErr) serialno=0;
	else serialno=_ttoi(serial);

	Log(hInstall,_T("REGISTRATION_SERIAL is [1]"),serialno);

	HKEY hKey, hKeyTop;
	Log(hInstall,_T("Serial is [1]"),serial);

	keyopened = true;
	DWORD createerrno;
	if((createerrno=RegOpenKeyEx(allusers?HKEY_LOCAL_MACHINE:HKEY_CURRENT_USER,_T("Software\\March Hare Software Ltd\\Keys"),0,KEY_READ|KEY_WRITE,&hKey))!=ERROR_SUCCESS)
	{
		LPVOID createerrstr=NULL;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
									NULL,createerrno,0,(LPTSTR)&createerrstr,0,NULL);
		Log(hInstall,_T("Cannot Open [1]\\Software\\March Hare Software Ltd\\Keys: [2]"),(allusers)?_T("HKLM"):_T("HKCU"),(LPCTSTR)createerrstr);
		LocalFree((HLOCAL)createerrstr);
		DWORD createerrnum;
		if ((createerrnum=RegCreateKeyEx(allusers?HKEY_LOCAL_MACHINE:HKEY_CURRENT_USER,_T("Software\\March Hare Software Ltd\\Keys"),0,NULL,REG_OPTION_NON_VOLATILE,KEY_READ|KEY_WRITE,NULL,&hKey,NULL))!=ERROR_SUCCESS)
		{
			LPVOID createerrtxt=NULL;
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
										NULL,createerrnum,0,(LPTSTR)&createerrtxt,0,NULL);
			Log(hInstall,_T("Cannot Create [1]\\Software\\March Hare Software Ltd\\Keys: [2]"),(allusers)?_T("HKLM"):_T("HKCU"),(LPCTSTR)createerrtxt);
			keyopened = false;
			LocalFree((HLOCAL)createerrtxt);
		}
	}
	if(!keyopened)
	if(RegOpenKeyEx(allusers?HKEY_LOCAL_MACHINE:HKEY_CURRENT_USER,_T("Software\\March Hare Software Ltd"),0,KEY_READ|KEY_WRITE,&hKeyTop)!=ERROR_SUCCESS)
	{
		if (RegCreateKeyEx(allusers?HKEY_LOCAL_MACHINE:HKEY_CURRENT_USER,_T("Software\\March Hare Software Ltd"),0,NULL,REG_OPTION_NON_VOLATILE,KEY_READ|KEY_WRITE,NULL,&hKeyTop,NULL)==ERROR_SUCCESS)
		{
			keyopened = true;
			Log(hInstall,_T("Created HKLM\\Software\\March Hare Software Ltd OK"));
			if(RegOpenKeyEx(allusers?HKEY_LOCAL_MACHINE:HKEY_CURRENT_USER,_T("Software\\March Hare Software Ltd\\Keys"),0,KEY_READ|KEY_WRITE,&hKey)!=ERROR_SUCCESS)
			{
				DWORD createerrnm;
				if ((createerrnm=RegCreateKeyEx(allusers?HKEY_LOCAL_MACHINE:HKEY_CURRENT_USER,_T("Software\\March Hare Software Ltd\\Keys"),0,NULL,REG_OPTION_NON_VOLATILE,KEY_READ|KEY_WRITE,NULL,&hKey,NULL))!=ERROR_SUCCESS)
				{
					LPVOID createerrmsg=NULL;
					FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
											NULL,createerrnm,0,(LPTSTR)&createerrmsg,0,NULL);
					Log(hInstall,_T("Cannot Create [1]\\Software\\March Hare Software Ltd\\Keys: [2]"),(allusers)?_T("HKLM"):_T("HKCU"),(LPCTSTR)createerrmsg);
					keyopened = false;
					LocalFree((HLOCAL)createerrmsg);
				}
			}
			RegCloseKey(hKeyTop);
		}
	}
	else
		RegCloseKey(hKeyTop);
	if(keyopened)
	{
		setresult = RegSetValueEx(hKey,package,NULL,REG_SZ,(BYTE*)serial,(lstrlen(serial)+1)*sizeof(serial[0]));
		if (allusers)
			if (setresult == ERROR_SUCCESS)
				Log(hInstall,_T("Created OK serial number registry key for all users"));
			else
				Log(hInstall,_T("FAILED to create serial number registry key for all users"));
		else
			if (setresult == ERROR_SUCCESS)
				Log(hInstall,_T("Created OK serial number registry key for this user"));
			else
				Log(hInstall,_T("FAILED to create serial number registry key for this user"));
		RegCloseKey(hKey);
	}
	else
	{
		if (allusers)
		{
			if(RegCreateKey(HKEY_CURRENT_USER,_T("Software\\March Hare Software Ltd\\Keys"),&hKey)==ERROR_SUCCESS)
			{
				setresult = RegSetValueEx(hKey,package,NULL,REG_SZ,(BYTE*)serial,(lstrlen(serial)+1)*sizeof(serial[0]));
				if (setresult == ERROR_SUCCESS)
					Log(hInstall,_T("Created OK serial number registry key in this user only - couldn't create for all users"));
				else
					Log(hInstall,_T("FAILED to create serial number registry key in this user only - couldn't create for all users"));
				RegCloseKey(hKey);
			}
			else
				Log(hInstall,_T("Cannot create serial number registry key for this user"));
		}
		else
			Log(hInstall,_T("Cannot create serial number registry key for any user"));
	}

	return ERROR_SUCCESS; 
}


UINT WINAPI CustomWelcome(MSIHANDLE hInstall)
{
	Log(hInstall,_T("installer.dll - welcome"));
	return ERROR_SUCCESS; 
}
