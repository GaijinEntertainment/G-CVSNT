//
// From MSDN.  License as per MSDN docs.
//
// NOTE: When building, link explicitly with the following libraries:
//                               wininet.lib
//                               ws2_32.lib
//                               urlmon.lib

//
// Note: This requires an up to date SDK - the one that ships with VC.NET is a little too old since
// these APIs were only made public recently even though they existed in Win95.
//
// FIXME: Could probably work around this with a cut/paste of the relevant bits

#include <config.h>
#include "../lib/api_system.h"

#define WIN32_LEAN_AND_MEAN	1
#include <windows.h>
#include "wininet.h"
#include <urlmon.h>
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "../cvs_string.h"

/* ==================================================================
                            HELPER FUNCTIONS
   ================================================================== */

/////////////////////////////////////////////////////////////////////
//  ResolveHostName                               (a helper function)
/////////////////////////////////////////////////////////////////////
DWORD __stdcall ResolveHostName( LPSTR   lpszHostName,
                                 LPSTR   lpszIPAddress,
                                 LPDWORD lpdwIPAddressSize )
{
  DWORD dwIPAddressSize;
  addrinfo Hints;
  LPADDRINFO lpAddrInfo;
  LPADDRINFO IPv4Only;
  DWORD error;

  // Figure out first whether to resolve a name or an address literal.
  // If getaddrinfo( ) with the AI_NUMERICHOST flag succeeds, then
  // lpszHostName points to a string representation of an IPv4 or IPv6 
  // address. Otherwise, getaddrinfo( ) should return EAI_NONAME.
  ZeroMemory( &Hints, sizeof(addrinfo) );
  Hints.ai_flags    = AI_NUMERICHOST;  // Only check for address literals.
  Hints.ai_family   = PF_UNSPEC;       // Accept any protocol family.
  Hints.ai_socktype = SOCK_STREAM;     // Constrain results to stream socket.
  Hints.ai_protocol = IPPROTO_TCP;     // Constrain results to TCP.

  error = getaddrinfo( cvs::idn(lpszHostName), NULL, &Hints, &lpAddrInfo );
  if( error != EAI_NONAME )
  {
    if( error != 0 )
    {
      error = ( error == EAI_MEMORY ) ?
              ERROR_NOT_ENOUGH_MEMORY : ERROR_INTERNET_NAME_NOT_RESOLVED;
      goto quit;
    }
    freeaddrinfo( lpAddrInfo );

    // An IP address (either v4 or v6) was passed in, so if there is 
    // room in the lpszIPAddress buffer, copy it back out and return.
    dwIPAddressSize = lstrlenA( lpszHostName );

    if( ( *lpdwIPAddressSize < dwIPAddressSize ) ||
        ( lpszIPAddress == NULL ) )
    {
      *lpdwIPAddressSize = dwIPAddressSize + 1;
      error = ERROR_INSUFFICIENT_BUFFER;
      goto quit;
    }
    lstrcpyA( lpszIPAddress, lpszHostName );
    goto quit;
  }

  // Call getaddrinfo( ) again, this time with no flag set.
  Hints.ai_flags = 0;
  error = getaddrinfo( cvs::idn(lpszHostName), NULL, &Hints, &lpAddrInfo );
  if( error != 0 )
  {
    error = ( error == EAI_MEMORY ) ?
            ERROR_NOT_ENOUGH_MEMORY : ERROR_INTERNET_NAME_NOT_RESOLVED;
    goto quit;
  }

  // Convert the IP address in addrinfo into a string.
  // (the following code only handles IPv4 addresses)
  IPv4Only = lpAddrInfo;
  while( IPv4Only->ai_family != AF_INET )
  {
    IPv4Only = IPv4Only->ai_next;
    if( IPv4Only == NULL )
    {
      error = ERROR_INTERNET_NAME_NOT_RESOLVED;
      goto quit;
    }
  }
  error = getnameinfo( IPv4Only->ai_addr, (socklen_t)IPv4Only->ai_addrlen, lpszIPAddress,
                       *lpdwIPAddressSize, NULL, 0, NI_NUMERICHOST );
  if( error != 0 )
    error = ERROR_INTERNET_NAME_NOT_RESOLVED;

quit:
  return( error );
}


/////////////////////////////////////////////////////////////////////
//  IsResolvable                                  (a helper function)
/////////////////////////////////////////////////////////////////////
BOOL __stdcall IsResolvable( LPSTR lpszHost )
{
  char szDummy[255];
  DWORD dwDummySize = sizeof(szDummy) - 1;

  if( ResolveHostName( lpszHost, szDummy, &dwDummySize ) )
    return( FALSE );
  return TRUE;
}


/////////////////////////////////////////////////////////////////////
//  GetIPAddress                                  (a helper function)
/////////////////////////////////////////////////////////////////////
DWORD __stdcall GetIPAddress( LPSTR   lpszIPAddress,
                              LPDWORD lpdwIPAddressSize )
{
  char szHostBuffer[255];

  if( gethostname( szHostBuffer, sizeof(szHostBuffer) - 1 ) != ERROR_SUCCESS )
    return( ERROR_INTERNET_INTERNAL_ERROR );
  return( ResolveHostName( szHostBuffer, lpszIPAddress, lpdwIPAddressSize ) );
}


/////////////////////////////////////////////////////////////////////
//  IsInNet                                       (a helper function)
/////////////////////////////////////////////////////////////////////
BOOL __stdcall IsInNet( LPSTR lpszIPAddress, LPSTR lpszDest, LPSTR lpszMask )
{
  DWORD dwDest;
  DWORD dwIpAddr;
  DWORD dwMask;

  dwIpAddr = inet_addr( lpszIPAddress );
  dwDest   = inet_addr( lpszDest );
  dwMask   = inet_addr( lpszMask );

  if( ( dwDest == INADDR_NONE ) ||
      ( dwIpAddr == INADDR_NONE ) ||
      ( ( dwIpAddr & dwMask ) != dwDest ) )
    return( FALSE );

  return( TRUE );
}

bool GetProxyForHost(const char *url, const char *hostname, cvs::string& proxy, cvs::string& proxy_port)
{
	static bool bInitialised = false, bWorking = false;

	char WPADLocation[1024]= "";
	char TempPath[MAX_PATH];
	char TempFile[MAX_PATH];
	DWORD dwProxyHostNameLength;
	DWORD returnVal;
	HMODULE hModJS;

	// Declare and populate an AutoProxyHelperVtbl structure, and then 
	// place a pointer to it in a containing AutoProxyHelperFunctions 
	// structure, which will be passed to InternetInitializeAutoProxyDll:

	// Failure to compile this line means you have an out of date platfrom SDK.
	static const AutoProxyHelperVtbl Vtbl =
	{
		IsResolvable,
		GetIPAddress,
		ResolveHostName,
		IsInNet
	};
	static const AutoProxyHelperFunctions HelperFunctions = { &Vtbl };

	// Declare function pointers for the three autoproxy functions
	static pfnInternetInitializeAutoProxyDll    pInternetInitializeAutoProxyDll;
	static pfnInternetDeInitializeAutoProxyDll  pInternetDeInitializeAutoProxyDll;
	static pfnInternetGetProxyInfo              pInternetGetProxyInfo;

	static cvs::string static_proxy,static_port;


	if(!bInitialised)
	{
		bInitialised=true;
		bWorking=false;

		if( !( hModJS = LoadLibraryA( "jsproxy.dll" ) ) )
			return false;

		if( !( pInternetInitializeAutoProxyDll = (pfnInternetInitializeAutoProxyDll)
					GetProcAddress( hModJS, "InternetInitializeAutoProxyDll" ) ) ||
			!( pInternetDeInitializeAutoProxyDll = (pfnInternetDeInitializeAutoProxyDll)
					GetProcAddress( hModJS, "InternetDeInitializeAutoProxyDll" ) ) ||
			!( pInternetGetProxyInfo = (pfnInternetGetProxyInfo)
					GetProcAddress( hModJS, "InternetGetProxyInfo" ) ) )
			return false;

		INTERNET_PER_CONN_OPTIONA pco[] =
		{
			{ INTERNET_PER_CONN_FLAGS },
			{ INTERNET_PER_CONN_AUTOCONFIG_URL },
			{ INTERNET_PER_CONN_PROXY_SERVER },
			{ INTERNET_PER_CONN_PROXY_BYPASS },
			{ INTERNET_PER_CONN_AUTOCONFIG_SECONDARY_URL }
		};
		INTERNET_PER_CONN_OPTION_LISTA pcol = { sizeof(pco), NULL, sizeof(pco)/sizeof(pco[0]), 0, pco }; 

		DWORD dwLen=sizeof(pcol);
		if(!InternetQueryOptionA(NULL,INTERNET_OPTION_PER_CONNECTION_OPTION,&pcol,&dwLen))
			return false;

		DWORD pt = pco[0].Value.dwValue;
		if(pt&PROXY_TYPE_AUTO_DETECT)
		{
			// Autodetect proxy file.  We only want to do this once becuase it's slow
			// We do DNS first... DHCP can take up to 10 seconds to complete.
			if(!DetectAutoProxyUrl( WPADLocation, sizeof(WPADLocation),PROXY_AUTO_DETECT_TYPE_DNS_A) &&
			   !DetectAutoProxyUrl( WPADLocation, sizeof(WPADLocation), PROXY_AUTO_DETECT_TYPE_DHCP ))
			return false;
		}
		else if(pt&PROXY_TYPE_AUTO_PROXY_URL)
		{
			// Download proxy.pac from url
			strncpy(WPADLocation,pco[1].Value.pszValue,sizeof(WPADLocation));
			LocalFree((HLOCAL)pco[1].Value.pszValue);
		}
		else if(pt&PROXY_TYPE_PROXY)
		{
			// Explicitly set proxy server
			LPSTR szValue = pco[2].Value.pszValue;
			static_port="";
			char *p=strrchr(szValue,':');
			if(p)
			{
				*p='\0';
				static_port=p+1;
			}
			static_proxy=szValue;
			LocalFree((HLOCAL)szValue);
		}
		else 
		{
			return false; // Fail the detect, forcing us to go direct
		}

		if(!static_proxy.length())
		{
			GetTempPathA( sizeof(TempPath)/sizeof(TempPath[0]), TempPath );
			GetTempFileNameA( TempPath, NULL, 0, TempFile );
			URLDownloadToFileA( NULL, WPADLocation, TempFile, NULL, NULL );

			if( !( returnVal = pInternetInitializeAutoProxyDll( 0, TempFile, NULL, 
																(AutoProxyHelperFunctions*)&HelperFunctions, NULL ) ) )
				return false;

			// Delete the temporary file
			DeleteFileA( TempFile );
		}

		bWorking = true;
	}

	if(!bWorking)
		return false;

	if(static_proxy.length())
	{
		proxy = static_proxy;
		proxy_port = static_port;
	}
	else
	{
		dwProxyHostNameLength=0;
		LPSTR szProxy = NULL;
		if( !pInternetGetProxyInfo( (LPSTR) url, (DWORD)strlen(url), 
									(LPSTR) hostname, (DWORD)strlen(hostname),
									&szProxy, &dwProxyHostNameLength ) )
			return false;

		if(!szProxy || !strcmp(szProxy,"DIRECT"))
			return false; // Direct connection

		if(strncmp(szProxy,"PROXY ",6))
			return false; // This should be the only possible string (maybe "SOCKS" but that's little used)

		proxy_port="";
		char *p = strrchr(szProxy,':');
		if(p)
		{
			proxy_port=p+1;
			*p='\0';
		}
		proxy=szProxy+6;
		// Not documented - how to free this?
		LocalFree((HLOCAL)szProxy);
	}
	return true;
}
