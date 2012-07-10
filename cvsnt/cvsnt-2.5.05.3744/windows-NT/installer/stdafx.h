// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once


#define WIN32_LEAN_AND_MEAN		
#define STRICT
#define WINVER 0x500
#define _WIN32_WINNT 0x500
#define _CRT_SECURE_NO_WARNINGS
#define  _CRT_NON_CONFORMING_SWPRINTFS
#include <windows.h>
#include <tchar.h>

#include <msiquery.h>
#include <winhttp.h>
#include <objbase.h>
#include <shlwapi.h>