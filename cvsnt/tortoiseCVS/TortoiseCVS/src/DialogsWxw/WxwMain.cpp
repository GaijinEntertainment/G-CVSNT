// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - October 2000

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

// The TortoiseAct program takes as parameters a command name (verb)
// and a list of files.  It then performs that TortoiseCVS action,
// including any user interface necessary.

// The parameters are passed by file rather than in command line
// arguments, and the command line points to the file.

// TortoiseShell dll calls TortoiseAct everytime a context menu
// is selected.

// For Visual C++ make sure you set these values when you compile wxWindows:
// wxUSE_DEBUG_NEW_ALWAYS to 0
// wxUSE_IOSTREAMH to 0
// wxUSE_MEMORY_TRACING to 0
// wxUSE_GLOBAL_MEMORY_OPERATORS to 0
// Otherwise it is incompatible with templates, and adds memory debugging
// features that Visual C++ has already anyway.

#include <Utils/TortoiseDebug.h>
#include <Utils/TortoiseRegistry.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/LangUtils.h>
#include <Utils/Preference.h>
#include <locale.h>
#include <wx/wx.h>
#include "WxwMain.h"

#if defined(_DEBUG) && (_MSC_VER >= 1300)
// Autocreate mini dumps on crash
MiniDumper* g_MiniDumper = 0;
#endif


IMPLEMENT_APP(MyApp)

bool MyApp::OnInit()
{
   TDEBUG_ENTER("MyApp::OnInit");
#ifdef _DEBUG
   if (TortoiseRegistry::ReadBoolean("LocaleVerbose", false))
   {
      // For debugging of wxLocale stuff
      wxLog::SetVerbose();
   }
#endif
   // This is for debugging heap allocation
   //_CrtSetBreakAlloc(0);

#if defined(_DEBUG) && (_MSC_VER >= 1300)
   // Autocreate mini dumps on crash
   g_MiniDumper = new MiniDumper(wxT("TortoiseCVS"), "TortoiseAct");
#endif

   // Localization setup
   myLocale = new wxLocale();
   const wxLanguageInfo* info = GetLangInfoFromCanonicalName(GetStringPreference("LanguageIso"));
   if (info)
   {
      if (myLocale->Init(info->Language, wxLOCALE_CONV_ENCODING))
      {
         // Get TortoiseCVS installation directory
         std::string localedir = EnsureTrailingDelimiter(GetTortoiseDirectory());
         localedir += "locale";
         // Add "locale" subdir to catalog lookup path
         myLocale->AddCatalogLookupPathPrefix(MultibyteToWide(localedir).c_str());
         if (!myLocale->AddCatalog(wxT("TortoiseCVS")))
         {
            TDEBUG_TRACE("Error loading message catalog for locale "
                         << myLocale->GetCanonicalName().c_str()
                         << ". Messages will not be translated.");
         }
      }
      else
      {
         TDEBUG_TRACE("Failed initializing localization framework. Messages will not be translated.");
      }
   }
   m_Act = new TortoiseAct();
   return true;
}



int MyApp::OnRun()
{
   TDEBUG_ENTER("MyApp::OnRun");
   // Perform the action
   if (m_Act->SussArgs( __argc, __argv))
      m_Act->PerformAction();
   return 0;
}



int MyApp::OnExit()
{
   delete m_Act;
   delete myLocale;
#if defined(_DEBUG) && (_MSC_VER >= 1300)
   if (g_MiniDumper)
      delete g_MiniDumper;
#endif
   return 0;
}


#if defined(_DEBUG)
void MyApp::OnUnhandledException()
{
   throw;
}
#endif

HWND GetRemoteHandle()
{
#ifdef _USRDLL
   // cvssccplugin
   return NULL;
#else
   return wxGetApp().m_Act->myRemoteHandle;
#endif
}

