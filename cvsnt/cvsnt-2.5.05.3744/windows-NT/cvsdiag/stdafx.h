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

#define WIN32_LEAN_AND_MEAN
#define WINVER 0x400
#define _WIN32_WINNT 0x0400
#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>
#include <winsock2.h>
#include <ws2tcpip.h>
