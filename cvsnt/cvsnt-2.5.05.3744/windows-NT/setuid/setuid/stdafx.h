// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifdef _WIN32
// Microsoft braindamage reversal.  
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#endif

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define WIN32_NO_STATUS
#define _WIN32_WINNT 0x0500
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>

#define SECURITY_WIN32
#include <security.h>
#include <ntsecapi.h>
#include <ntsecpkg.h>
#include <lm.h>
#include <objbase.h>
#include <ntdsapi.h>
#include <dsgetdc.h>

#include <stdio.h>
#include <string.h>
#include <tchar.h>
#include <stdarg.h>

#pragma comment (lib,"netapi32.lib")

void SuidDebug(const wchar_t *fmt, ...);
LSA_STRING *AllocateLsaStringLsa(LPCSTR szString);
UNICODE_STRING *AllocateUnicodeStringLsa(LPCWSTR szString);

extern PLSA_SECPKG_FUNCTION_TABLE g_pSec;

#ifdef _DEBUG
#define DEBUG SuidDebug
#else
#define DEBUG while(0) SuidDebug
#endif

/* MS BUG:  DNLEN hasn't been maintained so when you're on a legacy-free win2k domain
    you can apparently get a domain that's >DNLEN in size */
#undef DNLEN
#define DNLEN 256


