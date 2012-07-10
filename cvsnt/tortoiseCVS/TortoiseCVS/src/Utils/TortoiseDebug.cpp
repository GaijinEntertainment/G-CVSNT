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




#include "StdAfx.h"
#include "TortoiseDebug.h"
#include "Preference.h"
#include "TortoiseUtils.h"
#include "PathUtils.h"
#include "SyncUtils.h"
#include "StringUtils.h"
#include "Preference.h"
#include <sstream>
#include <fstream>
#include <iomanip>


#if (defined(_DEBUG) || defined (FORCE_DEBUG)) && !defined(NO_DEBUG)

extern HINSTANCE        g_hInstance;   // Instance handle for this module

_tDebug_Obj::MapIndent _tDebug_Obj::m_MapIndent;
CriticalSection _tDebug_Obj::m_CsIndent;
std::string _tDebug_Obj::m_ConfigName;


_tDebug_Obj::_tDebug_Obj(const std::string& sPrefix, const std::string& sConfigName) 
{ 
   m_Prefix = sPrefix;
   m_ConfigName = sConfigName;
   m_dwTimeEnter = GetTickCount();
   std::ostringstream msg;
   msg << "Begin " << sPrefix;
   WriteMessage(msg.str());
   Indent();
}



_tDebug_Obj::~_tDebug_Obj() 
{ 
   std::ostringstream msg;
   DWORD dw = GetTickCount() - m_dwTimeEnter;
   msg << "End " << m_Prefix << ", " << dw << "ms";
   Unindent();
   WriteMessage(msg.str());
}



void _tDebug_Obj::WriteMessage(const std::string& msg)
{
#if !defined(FORCE_DEBUG)
   bool enableDebug = GetBooleanPreference("Debug " + m_ConfigName);
   if (enableDebug)
#endif
   {
       std::string s;
       int l = GetIndentLevel();
       for (int i = 0; i < l; i++)
       {
           s += "  ";
       }
       s += msg;
       int debugLogs = GetIntegerPreference("Debug Logs");
      if (debugLogs >= 1)
      {
         std::ostringstream ss;
         SYSTEMTIME st;
         GetLocalTime(&st);
         char buffer[MAX_PATH + 1];
         GetModuleFileNameA(0, buffer, sizeof(buffer));
         ss << std::setw(2) << std::setfill('0') << st.wHour << '.'
            << std::setw(2) << std::setfill('0') << st.wMinute << '.'
            << std::setw(2) << std::setfill('0') << st.wSecond << '.'
            << std::setw(3) << std::setfill('0') << st.wMilliseconds << ':'
            << ExtractLastPart(buffer) << ":"
            << std::setw(4) << std::setfill('0') << GetCurrentProcessId() 
            << ":" << std::setw(4) << std::setfill('0') << GetCurrentThreadId() 
            << ":" << s << "\r\n";

         std::string file = GetStringPreference("Log Directory");
         if (file.empty())
            file = GetTemporaryPath();
         file = EnsureTrailingDelimiter(file) + "TortoiseCVS-" + m_ConfigName + ".log";
         DWORD dwNoCache = 0;
         if (debugLogs == 2)
         {
            dwNoCache = FILE_FLAG_WRITE_THROUGH;
         }

         HANDLE h = CreateFileA(file.c_str(), GENERIC_WRITE, 
                                FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL | dwNoCache, 0);
         SetFilePointer(h, 0, 0, FILE_END);
         std::string msg = ss.str();
         DWORD dw = static_cast<DWORD>(msg.size());
         DWORD dw1;
         WriteFile(h, msg.c_str(), dw, &dw1, 0);
         CloseHandle(h);
      }
      std::ostringstream ss;
      ss << std::setw(4) << std::setfill('0') << GetCurrentThreadId() << ':'
         << s << "\n";

      OutputDebugStringA(ss.str().c_str());
   }
}


void _tDebug_Obj::Write(const std::string& msg)
{
#if !defined(FORCE_DEBUG)
   bool enableDebug = GetBooleanPreference("Debug " + m_ConfigName);
   if (enableDebug)
#endif
   {
      OutputDebugStringA((msg + std::string("\n")).c_str());
   }
}

void _tDebug_Obj::Indent()
{
   CSHelper cs(m_CsIndent, true);
   DWORD id = GetCurrentThreadId();
   MapIndent::iterator it = m_MapIndent.find(id);
   if (it == m_MapIndent.end())
   {
      m_MapIndent[id] = 1;
   }
   else
   {
      (it->second)++;
   }
}



void _tDebug_Obj::Unindent()
{
   CSHelper cs(m_CsIndent, true);
   DWORD id = GetCurrentThreadId();
   MapIndent::iterator it = m_MapIndent.find(id);
   if (it != m_MapIndent.end())
   {
      if (it->second > 1)
      {
         (it->second)--;
      }
      else
      {
         m_MapIndent.erase(it);
      }
   }

}



int _tDebug_Obj::GetIndentLevel()
{
   CSHelper cs(m_CsIndent, true);
   DWORD id = GetCurrentThreadId();
   MapIndent::iterator it = m_MapIndent.find(id);
   if (it == m_MapIndent.end())
   {
      return 0;
   }
   else
   {
      return it->second;
   }
}

#if (_MSC_VER >= 1300)

wxString MiniDumper::ourAppName;
char MiniDumper::m_szFileName[256];
LPTOP_LEVEL_EXCEPTION_FILTER MiniDumper::m_OldFilter = 0;


MiniDumper::MiniDumper(LPCTSTR szAppName, LPCSTR szFileName)
{
   ourAppName = szAppName;
   strcpy(m_szFileName, szFileName);

   m_OldFilter = ::SetUnhandledExceptionFilter(TopLevelFilter);
}


MiniDumper::~MiniDumper()
{
}


LONG MiniDumper::TopLevelFilter(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
   LONG retval = EXCEPTION_CONTINUE_SEARCH;

   // firstly see if dbghelp.dll is around and has the function we need
   // look next to the EXE first, as the one in System32 might be old 
   // (e.g. Windows 2000)
   HMODULE hDll = NULL;
   std::string sDir = GetTortoiseDirectory();
   sDir = EnsureTrailingDelimiter(sDir) + "DBGHELP.DLL";
   hDll = ::LoadLibraryA(sDir.c_str());

   if (hDll == 0)
   {
      // load any version we can
      hDll = ::LoadLibraryA("DBGHELP.DLL");
   }

   wxString szResult;

   if (hDll)
   {
      MINIDUMPWRITEDUMP pDump = (MINIDUMPWRITEDUMP) ::GetProcAddress(hDll, "MiniDumpWriteDump");
      if (pDump)
      {
         // work out a good place for the dump file
         std::string sDumpPath = GetTemporaryPath();
         sDumpPath = EnsureTrailingDelimiter(sDumpPath) + m_szFileName + ".dmp";

         // ask the user if they want to save a dump file
         if (::MessageBox(NULL,
                          _(
"Something bad happened to TortoiseCVS, and it crashed. Would you like to save a crash dump file?"),
                          ourAppName, MB_YESNO) == IDYES)
         {
            // create the file
            HANDLE hFile = ::CreateFileA(sDumpPath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, 
                                         NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

            if (hFile != INVALID_HANDLE_VALUE)
            {
               _MINIDUMP_EXCEPTION_INFORMATION ExInfo;

               ExInfo.ThreadId = ::GetCurrentThreadId();
               ExInfo.ExceptionPointers = pExceptionInfo;
               ExInfo.ClientPointers = NULL;

               // write the dump
               BOOL bOK = pDump(GetCurrentProcess(), GetCurrentProcessId(), 
                                hFile, MiniDumpNormal, &ExInfo, NULL, NULL);
               if (bOK)
               {
                  szResult.Printf(_("Saved dump file to '%s'"), wxTextCStr(sDumpPath));
                  retval = EXCEPTION_EXECUTE_HANDLER;
               }
               else
               {
                  szResult.Printf(_("Failed to save dump file (error %d)"), GetLastError());
               }
               ::CloseHandle(hFile);
            }
            else
            {
               szResult.Printf(_("Failed to create dump file (error %d)"), GetLastError());
            }
         }
      }
      else
      {
         szResult = _("DBGHELP.DLL too old");
      }
   }
   else
   {
      szResult = _("DBGHELP.DLL not found");
   }

   if (!szResult.empty())
      ::MessageBox(0, szResult.c_str(), ourAppName, MB_OK);

   if (m_OldFilter)
      retval = m_OldFilter(pExceptionInfo);
   
   return retval;
}

#endif

#endif
