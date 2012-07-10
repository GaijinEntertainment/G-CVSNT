// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2002 - Torsten Martinsen
// <torsten@vip.cybercity.dk> - August 2002

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

#include <windows.h>
#include <tlhelp32.h>
#include <shlwapi.h>

#include <iostream>
#include <iomanip>
#include <sstream>

#include <wx/progdlg.h>

#include "TortoiseSetupHelper.h"


struct ProcessInfo
{
    ProcessInfo(DWORD p,
                const wxString& i,
                const wxString& t)
        : pid(p),
          image(i),
          title(t)
    {
    }
    
    DWORD pid;
    wxString image;
    wxString title;
};

static wxString windowTitle;

static bool TerminateProcess(const ProcessInfo& process);
static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam);
static bool GetProcesses(std::vector<ProcessInfo>& processes);
static bool IsExplorerRunning();
static bool IsProcessRunning(int pid);
static std::string GetRootDir();


#if 1
#define DBG(xxx)                                \
        do {                                    \
        std::ostringstream ss;                  \
        ss << xxx << std::endl;                 \
        OutputDebugStringA(ss.str().c_str());   \
        } while (0)
#else
#define DBG(xxx)
#endif

// This application is called
//
// b) From the installer, with second argument 'a':
//    - Restart Explorer(s)
//    - Delete any renamed DLL's
//
// c) From the Preferences dialog, after changing overlay icons, with second argument 'b':
//    - Restart Explorer(s)
//
// The first argument is always the localized version of "Restarting Explorer".


IMPLEMENT_APP(SetupHelperApp)

bool SetupHelperApp::OnInit()
{
   return true;
}


int SetupHelperApp::OnRun()
{
#ifdef _WIN64
    DBG("--- x64 SetupHelper");
#else
    DBG("--- x86 SetupHelper");
#endif
    
    if (__argc < 3)
        return -1;

    std::vector<ProcessInfo> processes;
    DBG("Get processes (1)"); 
    if (!GetProcesses(processes))
        return -1;
    DBG("Got processes (1):");
    for (size_t i = 0; i < processes.size(); ++i)
    {
        DBG(i << ": " << processes[i].pid);
    }

    // Kill all Explorer and cvslock processes
    
    for (size_t i = 0; i < processes.size(); ++i)
    {
        if (!_tcsicmp(processes[i].image.c_str(), wxT("explorer.exe")))
        {
            DBG("Killing Explorer");
            // Show dialog
            wxProgressDialog* dlg = new wxProgressDialog(wxT("TortoiseCVS Setup"), wxText(__argv[1]));
            dlg->Show();
            // Kill Explorer
            TerminateProcess(processes[i]);
            int i = 0;
            while (IsProcessRunning(processes[i].pid) && (i < 50))
            {
                dlg->Update((i+1)*100/50);
                Sleep(50);
            }
            
            // Avoid the Active Desktop error message
            HKEY hKey;
            if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer",
                              0, KEY_WRITE, &hKey))
            {
                DWORD data = 0;
                RegSetValueExA(hKey, "FaultKey", 0, REG_DWORD, reinterpret_cast<BYTE*>(&data), sizeof(DWORD));
            }

            dlg->Destroy();
        }
        else if (!_tcsicmp(processes[i].image.c_str(), wxT("cvslock.exe")))
        {
            // Kill CVSNT lock service
            TerminateProcess(processes[i]);
        }
    }

    // Show dialog
    wxProgressDialog* dlg = new wxProgressDialog(wxT("TortoiseCVS Setup"), wxText(__argv[1]));
    dlg->Show();

    // Wait for Explorer restart
    for (int i = 0; i < 50; ++i)
    {
       if (IsExplorerRunning())
          break;
       dlg->Update((i+1)*100/50);
       Sleep(50);
    }
    if (!IsExplorerRunning())
    {
       dlg->Update(0);
       STARTUPINFOA startupinfo;
       startupinfo.cb = sizeof(STARTUPINFOA);
       startupinfo.lpReserved = 0;
       startupinfo.lpDesktop = 0;
       startupinfo.lpTitle = 0;
       startupinfo.dwFlags = 0;
       startupinfo.cbReserved2 = 0;
       startupinfo.lpReserved2 = 0;
       PROCESS_INFORMATION processinfo;
       CreateProcessA(0, "explorer.exe", 0, 0, FALSE, 0, 0, 0, &startupinfo, &processinfo);
       CloseHandle(processinfo.hProcess);
       CloseHandle(processinfo.hThread);
       for (int i = 0; i < 10; ++i)
       {
          Sleep(50);
          dlg->Update((i+1)*100/10);
       }
    }
    dlg->Destroy();
    
    DBG("Check remaining processes");
    bool anyLeft = true;
    while (anyLeft)
    {
        processes.clear();
        DBG("Get processes (2)");
        if (!GetProcesses(processes))
            return -1;
        DBG("Got processes (2):");
        for (size_t i = 0; i < processes.size(); ++i)
        {
            DBG(i << ": " << processes[i].pid);
        }
        
        anyLeft = false;
        for (size_t i = 0; i < processes.size(); ++i)
        {
            if (_tcsicmp(processes[i].image.c_str(), wxT("explorer.exe")))
            {
                anyLeft = true;
                // Ask user to close application
                wxString msg;
                msg << wxT("The application ");
                if (processes[i].title.empty())
                {
                    // No title found (may be minimized to tray etc.)
                    msg << processes[i].image;
                }
                else
                    msg << wxT("'") << processes[i].title << wxT("' (") << processes[i].image << wxT(")");
                msg << wxT("\nneeds to be closed before TortoiseCVS setup can continue.\n")
                    << wxT("Please close the application and click OK, or click Cancel to leave the process ")
                    << wxT("running.\nIn the latter case, you will be required to reboot Windows after installation.");
                if (MessageBox(0, msg.c_str(), wxT("TortoiseCVS Setup"), MB_OKCANCEL | MB_ICONINFORMATION) == IDCANCEL)
                    return 1;
            }
        }
    }

    DBG("Loop done");

    if (__argv[2][0] == 'b')
        return 0;

    // Delete any renamed DLL's
    // TODO: Make this work even when installing to a different directory
    std::string rootDir(GetRootDir());
    std::string src(rootDir);
    src += "TortoiseShell.dll_renamed";
    DBG("Deleting " << src);
    DeleteFileA(src.c_str());
    src = rootDir;
    src += "TortoiseShell64.dll_renamed";
    DBG("Deleting " << src);
    DeleteFileA(src.c_str());
    src = rootDir;
    src += "TortoiseAct.exe_renamed";
    DBG("Deleting " << src);
    DeleteFileA(src.c_str());

    DBG("Exit: 0");
    return 0;
}


int SetupHelperApp::OnExit()
{
   return 0;
}


#if defined(_DEBUG)
void SetupHelperApp::OnUnhandledException()
{
   throw;
}
#endif


static bool TerminateProcess(const ProcessInfo& process)
{
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, process.pid);
    if (h)
    {
        bool res = ::TerminateProcess(h, 0) ? true : false;
        CloseHandle(h);
        return res;
    }
    return false;
}

static BOOL CALLBACK EnumWindowsCallback(HWND windowHandle, LPARAM lParam)
{
    DWORD pid;
    GetWindowThreadProcessId(windowHandle, &pid);
    if (pid == lParam)
    {
        // This window belongs to the process
        if (!windowTitle.empty())
            return TRUE;
        TCHAR text[256];
        GetWindowText(windowHandle, text, sizeof(text));
        if ((GetWindowLong(windowHandle, GWL_STYLE) & WS_VISIBLE))
        {
            windowTitle = text;
            return FALSE;
        }
    }
    return TRUE;
}

// Get PID and name of each process that has our DLL loaded.
static bool GetProcesses(std::vector<ProcessInfo>& processes)
{
   // PSAPI function pointers.
   BOOL (WINAPI* lpfEnumProcesses)(DWORD*, DWORD, DWORD*);
   BOOL (WINAPI* lpfEnumProcessModules)(HANDLE, HMODULE*, DWORD, LPDWORD );
   DWORD (WINAPI* lpfGetModuleBaseName)(HANDLE, HMODULE, LPCTSTR, DWORD);
   
   bool found = false;
         
   // Load library and get function pointers.
   HINSTANCE hInstLib = LoadLibraryA("PSAPI.DLL");
   if (!hInstLib)
   {
       DBG("Failed to load PSAPI.DLL");
       return false;
   }
   
   // Get procedure addresses.
   lpfEnumProcesses = (BOOL (WINAPI*)(DWORD*,DWORD,DWORD*)) GetProcAddress(hInstLib, "EnumProcesses");
   lpfEnumProcessModules = (BOOL (WINAPI*)(HANDLE, HMODULE*, DWORD, LPDWORD)) GetProcAddress( hInstLib, "EnumProcessModules" );
   lpfGetModuleBaseName =(DWORD (WINAPI*)(HANDLE, HMODULE, LPCTSTR, DWORD )) GetProcAddress( hInstLib, "GetModuleBaseNameW" );

   if (!lpfEnumProcesses ||
       !lpfEnumProcessModules ||
       !lpfGetModuleBaseName)
   {
       DBG("GetProcAddress failed");
       FreeLibrary(hInstLib);
       return false;
   }

   DWORD pidList[1000];
   DWORD iCbneeded;
   if (!lpfEnumProcesses(pidList, sizeof(pidList), &iCbneeded))
   {
       DBG("EnumProcesses failed");
       FreeLibrary(hInstLib);
       return false;
   }

   // How many processes are there?
   int iNumProc = iCbneeded/sizeof(DWORD);

   // Get and match the name of each process
   for (int i = 0; i < iNumProc; ++i)
   {
       // First, get a handle to the process
       HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pidList[i]);
       if (processHandle)
       {
           // Get modules loaded by process
           int maxModules = 1000;
           HMODULE* hMod = 0;
           while (1)
           {
               hMod = new HMODULE[maxModules];
               // Determine number of modules
               if (!lpfEnumProcessModules(processHandle, hMod, maxModules*sizeof(HMODULE), &iCbneeded))
               {
                   DWORD err = GetLastError();
                   if (err = ERROR_PARTIAL_COPY)
                   {
                       // This means that we are looking at the SYSTEM process. Skip it.
                       delete[] hMod;
                       hMod = 0;
                       break;
                   }
               }
               else if (iCbneeded <= maxModules*sizeof(HMODULE))
                   break;
               delete[] hMod;
               if (maxModules > 50000)
               {
                   DBG("maxModules: " << maxModules);
                   return false; // This is getting ridiculous...
               }
           }
           if (!hMod)
               continue;
           // Explorer?
           TCHAR executableName[MAX_PATH];
           if (!lpfGetModuleBaseName(processHandle, hMod[0], executableName, MAX_PATH))
           {
               DBG("GetModuleBaseName failed for process " << pidList[i]);
               return false;
           }
           //DBG("PROCESS " << wxAscii(executableName));
           if (!_tcsicmp(executableName, wxT("explorer.exe")))
           {
               // Explorer must always be killed
               processes.push_back(ProcessInfo(pidList[i], executableName, wxT("")));
               continue;
           }
           // Check if any of the modules are our DLL
           int numModules = iCbneeded/sizeof(HMODULE);
           for (int j = 1; j < numModules; ++j)
           {
               TCHAR name[MAX_PATH];
               if (!lpfGetModuleBaseName(processHandle, hMod[j], name, MAX_PATH))
               {
                   DBG("GetModuleBaseName failed for module " << j);
                   return false;
               }
               if (!_tcsicmp(name, wxT("tortoiseshell.dll")) ||
                   !_tcsicmp(name, wxT("tortoiseshell64.dll")))
               {
                   DBG("Process " << pidList[i] << " has " << wxAscii(name) << " loaded");
                   // Yes, this process must be terminated
                   windowTitle.clear();
                   EnumWindows(EnumWindowsCallback, pidList[i]);
                   processes.push_back(ProcessInfo(pidList[i], executableName, windowTitle));
               }
           }
           delete[] hMod;
           CloseHandle(processHandle);
       }
   }
   FreeLibrary(hInstLib);
   DBG("GetProcesses SUCCESS");
   return true;
}


static bool IsExplorerRunning()
{
    std::vector<ProcessInfo> processes;
    if (!GetProcesses(processes))
        return false;
    for (size_t i = 0; i < processes.size(); ++i)
    {
        if (!_tcsicmp(processes[i].image.c_str(), wxT("explorer.exe")))
            return true;
    }
    return false;
}


static bool IsProcessRunning(int pid)
{
    std::vector<ProcessInfo> processes;
    if (!GetProcesses(processes))
        return false;
    for (size_t i = 0; i < processes.size(); ++i)
    {
        if (processes[i].pid == pid)
            return true;
    }
    return false;
}


static std::string GetRootDir()
{
    HKEY key;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\TortoiseCVS", 0, KEY_READ | KEY_WOW64_32KEY, &key) == ERROR_SUCCESS)
    {
        DWORD len = 2048;
        DWORD type;
        std::auto_ptr<unsigned char> buf(new unsigned char[len]);
        long res = RegQueryValueExA(key, "RootDir", 0, &type, buf.get(), &len);
        if (res == ERROR_MORE_DATA)
        {
            buf.reset(new unsigned char[len]);
            res = RegQueryValueExA(key, "RootDir", 0, &type, buf.get(), &len);
        }
        return std::string(reinterpret_cast<char*>(buf.get()));
    }
    return "";
}
