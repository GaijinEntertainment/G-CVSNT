// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifdef _WIN32
// Microsoft braindamage reversal.  
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0400
#include <windows.h>
#include <lm.h>
#include <lmcons.h>
#include <winsock2.h>
#include <dbghelp.h>
#include <objbase.h>
#include <ntdsapi.h>
#include <dsgetdc.h>
#include <process.h>
#include <ntsecapi.h>
#include <tchar.h>
#include <shellapi.h>

#include <stdio.h>
#include "../windows-NT/setuid/libsuid/suid.h"

