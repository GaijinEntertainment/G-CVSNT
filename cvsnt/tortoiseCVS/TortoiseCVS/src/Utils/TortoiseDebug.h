// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - May 2000

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#ifndef TORTOISE_DEBUG_H
#define TORTOISE_DEBUG_H

#include "FixCompilerBugs.h"

#include <map>
#include <string>
#include <windows.h>
#if (_MSC_VER >= 1300)
   #include <dbghelp.h>
#endif
#include "FixWinDefs.h"
#include "SyncUtils.h"
#include "StringUtils.h"


#ifdef _MSC_VER
        // Fancier GUI Visual C++ asserts
        #include <crtdbg.h>
#else
        // Standard C assert
        #include <assert.h>
#ifndef _ASSERT
        #define _ASSERT assert
#endif

        // This seems needed by Borland and GCC.
        // Who else?  What standard is it?
        #include <errno.h>
        #ifndef ENOMEM
                #define ENOMEM 12
        #endif
#endif

#ifndef ASSERT
        #define ASSERT _ASSERT
#endif



#if (defined(_DEBUG) || defined (FORCE_DEBUG)) && !defined(NO_DEBUG)
        // Simple debug macro
        #include <iostream>
        #include <iomanip>
        #include <sstream>
        #include <map>
        #include <windows.h>

        #define TDEBUG_ON
        #define TDEBUG_USED(s) s
#ifdef TORTOISESHELL
        #define TDEBUG_ENTER(s) _tDebug_Obj _the_trace(s, "TortoiseShell")
#else
        #define TDEBUG_ENTER(s) _tDebug_Obj _the_trace(s, "TortoiseAct")
#endif

        #define TDEBUG_TRACE(s) do {                                            \
                                   std::stringstream _the_msg;                  \
                                   _the_msg << __LINE__ << ": " << s;           \
                                   _the_trace.WriteMessage(_the_msg.str());     \
                                } while (0)

#define TDEBUG(s) do {                           \
    std::stringstream _the_msg;                  \
    _the_msg << s;                               \
    _tDebug_Obj::Write(_the_msg.str());          \
} while (0)

#else

#   define TDEBUG_USED(s)
#   define TDEBUG_ENTER(s)
#   define TDEBUG_TRACE(s)
#   define TDEBUG(s)

#endif

#if (defined(_DEBUG) || defined (FORCE_DEBUG)) && !defined(NO_DEBUG)

// Class for tracing
class _tDebug_Obj
{
public:
   _tDebug_Obj(const std::string& sPrefix, const std::string& sConfigName);
  ~_tDebug_Obj();
  void WriteMessage(const std::string& msg);
  static void Write(const std::string& msg);
  static void Indent();
  static void Unindent();
  static int GetIndentLevel();
  std::string m_Prefix;
  static std::string m_ConfigName;
private:
  typedef std::map<DWORD, int> MapIndent;
  DWORD m_dwTimeEnter;
  static MapIndent m_MapIndent;
  static CriticalSection m_CsIndent;
};


int inline tdebug_printf(const char *format, ...)
{
   char buff[255];
   va_list args;
   va_start(args, format);

   int nSize = _vsnprintf(buff, sizeof(buff), format, args);
   std::string s = buff;
   FindAndReplace<std::string>(s, "\n", "\r\n");
   s += "\r\n";
   OutputDebugStringA(s.c_str());
   return nSize;
}

int inline tdebug_fprintf(FILE*, const char *format, ...)
{
   char buff[255];
   va_list args;
   va_start(args, format);

   int nSize = _vsnprintf(buff, sizeof(buff), format, args);
   std::string s = buff;
   FindAndReplace<std::string>(s, "\n", "\r\n");
   s += "\r\n";
   OutputDebugStringA(s.c_str());
   return nSize;
}


#if (_MSC_VER >= 1300)
// based on dbghelp.h
typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
                                         CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
                                         CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
                                         CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

class MiniDumper
{
public:
   MiniDumper(LPCTSTR szAppName, LPCSTR szFileName);
   ~MiniDumper();

private:
   static wxString ourAppName;
   static char m_szFileName[256];
   static LPTOP_LEVEL_EXCEPTION_FILTER m_OldFilter;
   static LONG WINAPI TopLevelFilter(struct _EXCEPTION_POINTERS *pExceptionInfo);
};
#endif

#endif // DEBUG

#endif
