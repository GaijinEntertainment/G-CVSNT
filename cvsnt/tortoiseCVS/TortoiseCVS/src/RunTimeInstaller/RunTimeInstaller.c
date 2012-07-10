// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2007 - Torsten Martinsen
// <torsten@vip.cybercity.dk> - June 2007

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

#include <windows.h>

#include <cvsnt-version.h>
#include "overlays-version.h"

#define DBG(x)  OutputDebugStringA(x)

static int RegKeyExists(HKEY hkey, char* subKey);
static int LaunchCommand(char* command);
static int FileExists(char* filename);
static void GetRootDir(char* buf);
static int WindowsVersionIs2K();


// Arguments:
// 0   Program name
// 1   Path to MSVC redistributables
// 2   Setup type
// 3   'r' if installed DLL's should be renamed
// 4   's' if silent mode
int main(int argc, char** argv)
{
    int silent = (argv[4][0] == 's');
    char buf[_MAX_PATH];
   
#ifdef _WIN64
    DBG("--- x64 RunTimeInstaller\n");
#else
    DBG("--- x86 RunTimeInstaller\n");
#endif

    DBG("Installing CVSNT\n");   
    lstrcpyA(buf, "msiexec ");   
         
    if (!WindowsVersionIs2K())   
        lstrcatA(buf, "/norestart ");    // Win2K does not support this      

    // If custom, show default UI
    if (lstrcmpA(argv[2], "custom"))
    {
        if (silent)
            lstrcatA(buf, "/qn ");      // No UI
        else
            lstrcatA(buf, "/qb ");      // Basic UI
    }
    
    if (RegKeyExists(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" CVSNTREGKEY))
    {    
        DBG("CVSNT is already installed: SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" CVSNTREGKEY);
        lstrcatA(buf, "/fvomus ");   
    }    
    else if (RegKeyExists(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" CVSNTREGKEY))
    {    
        DBG("CVSNT is already installed: SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" CVSNTREGKEY);
        lstrcatA(buf, "/fvomus ");   
    }    
    else     
        lstrcatA(buf, "/i ");    
    lstrcatA(buf, argv[1]);      
    lstrcatA(buf, "\\" CVSNTINSTALLER);      
    DBG(buf);
    if (!LaunchCommand(buf))     
        return 3;

    // TortoiseOverlays

    lstrcpyA(buf, "msiexec /i ");
    lstrcatA(buf, argv[1]);
    lstrcatA(buf, "\\" OVERLAYS32INSTALLER " /qn /norestart");
    DBG(buf);
    if (!LaunchCommand(buf))
        return 4;
#ifdef _WIN64
    lstrcpyA(buf, "msiexec /i ");
    lstrcatA(buf, argv[1]);
    lstrcatA(buf, "\\" OVERLAYS64INSTALLER " /qn /norestart");
    DBG(buf);
    if (!LaunchCommand(buf))
        return 5;
#endif
    
    if ((argc < 4) || (argv[3][0] != 'r'))
        return 0;

    // Check for existing installation
    DBG("Check for existing installation");
    GetRootDir(buf);
    if (lstrlenA(buf))
    {
        char src[_MAX_PATH], dst[_MAX_PATH];
        // TortoiseCVS is already installed, rename DLL(s)
        DBG("Detected existing installation");
        lstrcpyA(src, buf);
        lstrcatA(src, "TortoiseShell.dll");
        lstrcpyA(dst, src);
        lstrcatA(dst, "_renamed");
        DeleteFileA(dst);
        DBG(src);
        if (!FileExists(src))
            DBG("No shell32 DLL");
        else if (!MoveFileA(src, dst))
        {
            DBG("Rename failed (32)");
            return 4;
        }
        lstrcpyA(src, buf);
        lstrcatA(src, "TortoiseAct.exe");
        lstrcpyA(dst, src);
        lstrcatA(dst, "_renamed");
        DeleteFileA(dst);
        DBG(src);
        if (!FileExists(src))
            DBG("No TortoiseAct");
        else if (!MoveFileA(src, dst))
        {
            DBG("Rename failed (TortoiseAct)");
            return 5;
        }
#ifdef _WIN64
        lstrcpyA(src, buf);
        lstrcatA(src, "TortoiseShell64.dll");
        lstrcpyA(dst, src);
        lstrcatA(dst, "_renamed");
        DeleteFileA(dst);
        DBG(src);
        if (!FileExists(src))
            DBG("No shell64 DLL");
        else if (!MoveFileA(src, dst))
        {
            DBG("Rename failed (64)");
            return 6;
        }
#endif
    }

    return 0;
}

static int LaunchCommand(char* command)
{
    int res = 0;
    PROCESS_INFORMATION processInfo;
    STARTUPINFOA startupInfo;
    SecureZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_SHOWMINIMIZED;
    res = CreateProcessA(0,
                         command,
                         0, 0,
                         FALSE,
                         CREATE_NO_WINDOW,
                         0, 0, &startupInfo, &processInfo);
    if (!res)
    {
        DBG("CreateProcess failed");
        return 0;
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE);

    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
    return 1;
}


static void GetRootDir(char* buf)
{
    HKEY key;
    *buf = 0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\TortoiseCVS", 0, KEY_READ | KEY_WOW64_32KEY, &key) == ERROR_SUCCESS)
    {
        DWORD len = _MAX_PATH;
        DWORD type;
        DWORD res = RegQueryValueExA(key, "RootDir", 0, &type, buf, &len);
        if (res != ERROR_SUCCESS)
        {
            *buf = 0;
        }
    }
}


static int RegKeyExists(HKEY hkey, char* subKey)
{
    HKEY key;
    if (RegOpenKeyExA(hkey, subKey, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return 0;
    RegCloseKey(key);
    return 1;
}


static int FileExists(char* filename)
{
    DWORD attributes = GetFileAttributesA(filename);
    return attributes != INVALID_FILE_ATTRIBUTES;
}


static int WindowsVersionIs2K()
{
    OSVERSIONINFO OsVersionInfo;
    OsVersionInfo.dwOSVersionInfoSize = sizeof(OsVersionInfo);
    GetVersionEx(&OsVersionInfo);
    return (OsVersionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT && 
            OsVersionInfo.dwMajorVersion == 5 && 
            OsVersionInfo.dwMinorVersion == 0);
}

// Code below is a minimal runtime to reduce the size of this application

#define _MAX_CMD_LINE_ARGS  32
char* _argv[_MAX_CMD_LINE_ARGS+1];
static char* _rawCmd = 0;

int _isspace(int c)
{
    return ((c >= 0x09 && c <= 0x0D) || (c == 0x20));
}

int _init_args()
{
    char* sysCmd;
    int sizeSysCmd;
    char* cmd;
    int argc;
    
    _argv[0] = 0;

    sysCmd = GetCommandLineA();
    sizeSysCmd = lstrlenA(sysCmd);

    // copy the system command line
    cmd = (char*) HeapAlloc(GetProcessHeap(), 0, sizeof(TCHAR)*(sizeSysCmd+1));
    _rawCmd = cmd;
    if (!cmd)
        return 0;
    lstrcpyA(cmd, sysCmd);

    // Handle a quoted filename
    if (*cmd == '"')
    {
        cmd++;
        _argv[0] = cmd;                     // argv[0] = exe name

        while (*cmd && *cmd != '"')
            cmd++;

        if (*cmd)
            *cmd++ = 0;
        else
            return 0;                       // no end quote!
    }
    else
    {
        _argv[0] = cmd;                     // argv[0] = exe name

        while (*cmd && !_isspace(*cmd))
            cmd++;

        if (*cmd)
            *cmd++ = 0;
    }

    argc = 1;
    for (;;)
    {
        while (*cmd && _isspace(*cmd))     // Skip over any whitespace
            cmd++;

        if (*cmd == 0)                      // End of command line?
            return argc;

        if (*cmd == '"')                    // Argument starting with a quote???
        {
            cmd++;

            _argv[argc++] = cmd;
            _argv[argc] = 0;

            while (*cmd && *cmd != '"')
                cmd++;

            if (*cmd == 0)
                return argc;

            if (*cmd)
                *cmd++ = 0;
        }
        else
        {
            _argv[argc++] = cmd;
            _argv[argc] = 0;

            while (*cmd && !_isspace(*cmd))
                cmd++;

            if (*cmd == 0)
                return argc;

            if (*cmd)
                *cmd++ = 0;
        }

        if (argc >= _MAX_CMD_LINE_ARGS)
            return argc;
    }
}

void _term_args()
{
    if (_rawCmd)
        HeapFree(GetProcessHeap(), 0, _rawCmd);
}


void mainCRTStartup()
{
    int ret, argc = _init_args();
    ret = main(argc, _argv);
    _term_args();
    ExitProcess(ret);
}
