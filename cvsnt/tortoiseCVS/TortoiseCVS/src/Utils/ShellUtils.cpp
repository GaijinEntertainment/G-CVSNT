// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2001 - Francis Irving
// <francis@flourish.org> - May 2001

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

#include "PathUtils.h"
#include "StringUtils.h"
#include "UnicodeStrings.h"
#include "TortoiseRegistry.h"
#include "Translate.h"
#include <windows.h>
#include <map>
#include <sstream>

// TODO: Put shlwapi.h in mingw32
#ifndef __GNUC__
#include <shlwapi.h>

#else

#ifndef DLLVERSIONINFO
typedef struct _DllVersionInfo
{
    DWORD cbSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformID;
} DLLVERSIONINFO;
#endif

#ifndef DLLGETVERSIONPROC
typedef int (FAR WINAPI *DLLGETVERSIONPROC) (DLLVERSIONINFO *);
#endif

#endif

#include "ShellUtils.h"
#include "TortoiseException.h"
#include "TortoiseUtils.h"
#include "ProcessUtils.h"
#include "TortoiseDebug.h"
#include "Translate.h"

LPCITEMIDLIST GetNextItem(LPCITEMIDLIST pidl)
{
    if (pidl == NULL)
        return NULL;

    // Get size of the item identifier we are on
    int cb = pidl->mkid.cb;

    // If zero, then end of list
    if (cb == 0)
        return NULL;
    
    // Move on
    pidl = (LPITEMIDLIST) (((LPBYTE)pidl) + cb);

    // Return NULL if null-terminating, or pidl otherwise
    return (pidl->mkid.cb == 0) ? NULL : (LPITEMIDLIST) pidl;
}

int GetItemCount(LPCITEMIDLIST pidl)
{
    int count = 0;

    while (pidl != 0)
    {
       count++;
       pidl = GetNextItem(pidl);
    }

    return count;
}

UINT GetSize(LPCITEMIDLIST pidl)
{
    UINT total = 0;
    if (pidl)
    {
        total += sizeof(pidl->mkid.cb);
        while (pidl)
        {
            total += pidl->mkid.cb;
            pidl = GetNextItem(pidl);
        }
    }
    return total;
}

LPITEMIDLIST DuplicateItem(LPMALLOC pMalloc, LPCITEMIDLIST pidl)
{
    int cb = pidl->mkid.cb;
    if (cb == 0)
        return NULL;

    LPITEMIDLIST pidlRet = (LPITEMIDLIST)pMalloc->Alloc(cb + sizeof(USHORT));
    if (pidlRet == NULL)
        return NULL;

    CopyMemory(pidlRet, pidl, cb);
    *((USHORT*) (((LPBYTE)pidlRet) + cb)) = 0;
    return pidlRet;
}

// Tests the passed PIDL to see if it is the root Desktop Folder
bool IsDesktopFolder(LPCITEMIDLIST pidl)
{
   if (pidl != NULL)
      return pidl->mkid.cb == 0;
   else
      return false;
}

// Returns the concatination of the two PIDLs. Neither passed PIDLs are
// freed so it is up to the caller to free them.
LPITEMIDLIST AppendPIDL(LPCITEMIDLIST dest, LPCITEMIDLIST src)
{
   LPMALLOC pMalloc;
   if (!SUCCEEDED(SHGetMalloc(&pMalloc)))
      return NULL;

   int destSize = 0;
   int srcSize = 0;

   // Appending a PIDL to the DesktopPIDL is invalid so don't allow it.
   if (dest != NULL && !IsDesktopFolder(dest))
      destSize = GetSize(dest) - sizeof(dest->mkid.cb);
      
   if (src != NULL)
      srcSize = GetSize(src);
   
   LPITEMIDLIST sum = (LPITEMIDLIST)pMalloc->Alloc(destSize + srcSize);
   if (sum != NULL)
   {
      if (dest != NULL)
         CopyMemory((char*)sum, dest, destSize);
      if (src != NULL)
         CopyMemory((char*)sum + destSize, src, srcSize);
   }
   
   pMalloc->Release();
   return sum;
}

bool IsEqualPIDL(LPCITEMIDLIST a, LPCITEMIDLIST b)
{
   UINT asiz = GetSize(a);
   UINT bsiz = GetSize(b);
   if (asiz != bsiz)
      return false;

   if (memcmp(a, b, asiz) == 0)
      return true;

   return false;
}


// Strips the last ID from the list
LPITEMIDLIST StripLastID(LPCITEMIDLIST pidl)
{
   TDEBUG_ENTER("StripLastID");
   TDEBUG_TRACE(DisplayNamePIDL(pidl));
   LPMALLOC pMalloc;
   if (!SUCCEEDED(SHGetMalloc(&pMalloc)))
      return NULL;

   LPCITEMIDLIST pCurrent = pidl;
   LPCITEMIDLIST pLast = 0;
   DWORD dwSize = 0;

   if (pCurrent->mkid.cb == 0)
   {
      return 0;
   }

   pLast = pCurrent;
   pCurrent = (LPCITEMIDLIST) (((LPBYTE) pCurrent) + pCurrent->mkid.cb);

   while (pCurrent->mkid.cb != 0)
   {
      dwSize += pLast->mkid.cb;
      pLast = pCurrent;
      pCurrent = (LPITEMIDLIST) (((LPBYTE) pCurrent) + pCurrent->mkid.cb);
   }

   LPITEMIDLIST pidlParent = (LPITEMIDLIST) pMalloc->Alloc(dwSize + sizeof(USHORT));
   if (pidlParent != 0)
   {
      CopyMemory(pidlParent, pidl, dwSize);
      *((USHORT*) (((LPBYTE) pidlParent) + dwSize)) = 0;
   }
   TDEBUG_TRACE(DisplayNamePIDL(pidlParent));
   pMalloc->Release();
   return pidlParent;
}




// Get the last ID from the list
LPITEMIDLIST GetLastID(LPCITEMIDLIST pidl)
{
   LPMALLOC pMalloc;
   if (!SUCCEEDED(SHGetMalloc(&pMalloc)))
      return NULL;

   LPCITEMIDLIST pCurrent = pidl;
   LPCITEMIDLIST pLast = 0;

   while (pCurrent->mkid.cb != 0)
   {
      pLast = pCurrent;
      pCurrent = (LPITEMIDLIST) (((LPBYTE) pCurrent) + pCurrent->mkid.cb);
   }

   LPITEMIDLIST pidlLast = (LPITEMIDLIST) pMalloc->Alloc(pLast->mkid.cb + sizeof(USHORT));
   if (pidlLast != 0)
   {
      CopyMemory(pidlLast, pLast, pLast->mkid.cb);
      *((USHORT*) (((LPBYTE) pidlLast) + pLast->mkid.cb)) = 0;
   }
   
   pMalloc->Release();
   return pidlLast;
}


// Bind to parent
IShellFolder* BindToParent(LPCITEMIDLIST pidl)
{
   TDEBUG_ENTER("BindToParent");
   IShellFolder* psfDesktop = 0;
   IShellFolder* psfResult = 0;
   HRESULT hr = 0;
   
   LPITEMIDLIST pidlParent = StripLastID(pidl);
   TDEBUG_TRACE(DisplayNamePIDL(pidlParent));
   if (!pidlParent)
   {
      goto Cleanup;
   }

   hr = SHGetDesktopFolder(&psfDesktop);
   if (FAILED(hr))
   {
      goto Cleanup;
   }

   if (IsDesktopFolder(pidlParent))
   {
      psfResult = psfDesktop;
      psfDesktop = 0;
   }
   else
   {
      hr = psfDesktop->BindToObject(pidlParent, 0, IID_IShellFolder, 
         (LPVOID*) &psfResult);
      if (FAILED(hr))
      {
         goto Cleanup;
      }
   }


Cleanup:
   if (psfDesktop)
      psfDesktop->Release();

   if (pidlParent)
      ItemListFree(pidlParent);

   return psfResult;
}



std::string DisplayNamePIDL(LPCITEMIDLIST pidl)
{
   SHFILEINFOA sfi;
   if (SHGetFileInfoA((LPCSTR) pidl, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_DISPLAYNAME) == 0)
   {
      ASSERT(false);
      return "<DisplayNamePIDL failed>";
   }

   return sfi.szDisplayName;
}


// Is PIDL a shortcut
bool IsShortcut(LPCITEMIDLIST pidl)
{
   TDEBUG_ENTER("IsShortcut");
   DWORD dwAttributes = SFGAO_LINK;
   if (ShellGetFileAttributesPidl(pidl, &dwAttributes))
   {
      if (dwAttributes & SFGAO_LINK)
      {
         TDEBUG_TRACE("return true");
         return true;
      }
   }
   TDEBUG_TRACE("return false");
   return false;
}


// Get target of a shortcut
LPITEMIDLIST GetShortcutTarget(LPCITEMIDLIST pidl)
{
   IShellLink *pShLink = 0;
   IPersistFile *ppf = 0;
   std::wstring wsPath;
   LPITEMIDLIST pidlResult = 0;

   // If it's not a shortcut, exit
   if (!IsShortcut(pidl))
   {
      pidlResult = CloneIDList(pidl);
      goto Cleanup;
   }

   // get path of shortcut
   wsPath = MultibyteToWide(GetPathFromIDList(pidl));

   HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                 IID_IShellLink, (LPVOID*) &pShLink);

   if (FAILED(hr))
      goto Cleanup;

   hr = pShLink->QueryInterface(IID_IPersistFile, (LPVOID*) &ppf);
   if (FAILED(hr))
      goto Cleanup;

   hr = ppf->Load(wsPath.c_str(), STGM_READ);
   if (FAILED(hr))
      goto Cleanup;


   hr = pShLink->Resolve(GetDesktopWindow(), SLR_NO_UI);
   if (FAILED(hr))
      goto Cleanup;

   hr = pShLink->GetIDList(&pidlResult);
   if (FAILED(hr))
      goto Cleanup;

Cleanup:
   if (pShLink)
      pShLink->Release();
   
   if (ppf)
      ppf->Release();

   return pidlResult;
}



// Clone a PIDL
LPITEMIDLIST CloneIDList(LPCITEMIDLIST pidl)
{
   LPITEMIDLIST pidlResult = 0;
   DWORD dwSize;
   LPMALLOC pMalloc;

   if (!SUCCEEDED(SHGetMalloc(&pMalloc)))
      goto Cleanup;

   dwSize = GetSize(pidl);
   if (dwSize == 0)
      goto Cleanup;

   pidlResult = (LPITEMIDLIST) pMalloc->Alloc(dwSize);
   if (!pidlResult)
      goto Cleanup;

   CopyMemory(pidlResult, pidl, dwSize);

Cleanup:
   if (pMalloc)
      pMalloc->Release();

   return pidlResult;
}



bool LaunchCommand(const std::string& command, bool waitForEnd, bool minimized)
{
   TDEBUG_ENTER("LaunchCommand");
   TDEBUG_TRACE("Command: " << command);
   PROCESS_INFORMATION processInfo;
   STARTUPINFOA startupInfo;
   memset(&startupInfo, 0, sizeof(startupInfo));
   startupInfo.cb = sizeof(startupInfo);
   startupInfo.dwFlags = minimized ? STARTF_USESHOWWINDOW : 0;
   startupInfo.wShowWindow = SW_SHOWMINIMIZED;
   int res = CreateProcessA(0,
                            const_cast<char*>(command.c_str()),
                            0, 0,
                            FALSE,
                            CREATE_NO_WINDOW,
                            0, 0, &startupInfo, &processInfo);
   TDEBUG_TRACE("res " << res);
   if (res == 0)
   {
      return false;
   }

   if (waitForEnd)
   {
      TDEBUG_TRACE("Waiting...");
#if defined(TDEBUG_ON) 
      DWORD dw = 
#endif
      WaitWithMsgQueue(1, &processInfo.hProcess, true, INFINITE);
      TDEBUG_TRACE("LaunchCommand: dw " << dw);
   }

   CloseHandle(processInfo.hProcess);
   CloseHandle(processInfo.hThread);
   return true;
}

bool FileIsViewable(const std::string& filename)
{
    char buf[MAX_PATH];
    if (int(FindExecutableA(filename.c_str(), "", buf)) == SE_ERR_NOASSOC)
        return true; // No application is associated with the file
    // Fail if the file is associated with itself (i.e. executable)
    if (!strcmpi(filename.c_str(), buf))
        return false;
    return true;
}

bool LaunchFile(const std::string& filename, bool waitForEnd)
{
    TDEBUG_ENTER("LaunchFile");

    if (!FileIsViewable(filename))
        return false;

    SHELLEXECUTEINFOA sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpFile = filename.c_str();
    sei.nShow = SW_SHOW;
    sei.lpVerb = "edit";
    // First try "edit"
    if (!ShellExecuteExA(&sei))
    {
        // Then "open"
        sei.lpVerb = "open";
        if (!ShellExecuteExA(&sei))
        {
            if (sei.hInstApp == reinterpret_cast<HINSTANCE>(SE_ERR_NOASSOC))
            {
                // No association for this extension (typically when file has no extension)
                std::ostringstream ss;
                ss << "notepad " << filename;
                return LaunchCommand(ss.str(), waitForEnd);
            }
            ASSERT(sei.hInstApp > reinterpret_cast<HINSTANCE>(32));
            return false;
        }
    }
    
    if (waitForEnd && sei.hProcess)
    {
        TDEBUG_TRACE("Waiting...");
#if defined(TDEBUG_ON) 
        DWORD dw = 
#endif
           WaitWithMsgQueue(1, &sei.hProcess, true, INFINITE);
        TDEBUG_TRACE("LaunchCommand: dw " << dw);
    }

    if (sei.hProcess != 0)
      CloseHandle(sei.hProcess);

    return true;
}


// Test if PIDL points to a special folder
bool IsSpecialFolder(LPCITEMIDLIST pidl, int nFolder)
{
   bool result = false;
   LPITEMIDLIST mypidl = 0;
   HRESULT hr = SHGetSpecialFolderLocation(0, nFolder, &mypidl);
   if (FAILED(hr))
   {
      goto Cleanup;
   }

   result = IsEqualPIDL(pidl, mypidl);

Cleanup:
   if(mypidl)
      ItemListFree(mypidl);

   return result;
}


// Get IShellFolder from PIDL
IShellFolder* GetIShellFolderFromPIDL(LPCITEMIDLIST pidl)
{
   IShellFolder* psfDesktop = 0;
   IShellFolder* psfResult = 0;
   HRESULT hr = SHGetDesktopFolder(&psfDesktop);
   if (SUCCEEDED(hr))
   {
      hr = psfDesktop->BindToObject(pidl, 0, IID_IShellFolder, (LPVOID*) &psfResult);
      if (FAILED(hr))
      {
         psfResult = 0;
         goto Cleanup;
      }
   }

Cleanup:
   if (psfDesktop)
      psfDesktop->Release();
   return psfResult;
}




// Rebuild icons
bool RebuildIcons()
{
   TDEBUG_ENTER("RebuildIcons");
   const int BUFFER_SIZE = 1024;

   HKEY hRegKey = 0;
   LONG lRegResult = RegOpenKeyExA(HKEY_CURRENT_USER, "Control Panel\\Desktop\\WindowMetrics",
                                   0, KEY_READ | KEY_WRITE, &hRegKey);
   if (lRegResult != ERROR_SUCCESS)
   {
      TDEBUG_TRACE("Failed to open WindowMetrics key");
      return false;
   }

   char* buf = new char[BUFFER_SIZE];

   // Before Win2k, we're going to change the icon size, otherwise the color depht
   std::string sRegValueName;
   bool bUseSize = false;
   if (WindowsVersionIs2KOrHigher())
      sRegValueName = "Shell Icon BPP";
   else
   {
      bUseSize = true;
      sRegValueName = "Shell Icon Size";
   }

   // Read registry value
   DWORD dwSize = BUFFER_SIZE;
   lRegResult = RegQueryValueExA(hRegKey, sRegValueName.c_str(), NULL, NULL, (LPBYTE) buf, &dwSize);
   if (lRegResult == ERROR_FILE_NOT_FOUND)
   {
      strncpy(buf, "32", BUFFER_SIZE);
   }
   else if (lRegResult != ERROR_SUCCESS)
   {
      TDEBUG_TRACE("Failed to read " << sRegValueName);
      RegCloseKey(hRegKey);
      return false;
   }

   
   // Change registry value
   DWORD dwRegValue = atoi(buf);
   DWORD dwRegValueTemp = 0;
   if (!bUseSize)
   {
      if (dwRegValue == 4)
         dwRegValueTemp = 32;
      else
         dwRegValueTemp = 4;
   }
   else
   {
      if (dwRegValue == 31)
         dwRegValueTemp = 32;
      else
         dwRegValueTemp = 31;
   }

   dwSize = _snprintf(buf, BUFFER_SIZE, "%d", dwRegValueTemp) + 1; 
   lRegResult = RegSetValueExA(hRegKey, sRegValueName.c_str(), 0, REG_SZ, (LPBYTE) buf, dwSize); 
   if (lRegResult != ERROR_SUCCESS)
   {
      TDEBUG_TRACE("Failed to change " << sRegValueName);
      RegCloseKey(hRegKey);
      delete[] buf;
      return false;
   }


   // Update all windows
   PDWORD dwResult;
   SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, SPI_SETNONCLIENTMETRICS, 
                      0, SMTO_ABORTIFHUNG, 5000, reinterpret_cast<PDWORD_PTR>(&dwResult));

   // Reset registry value
   dwSize = _snprintf(buf, BUFFER_SIZE, "%d", dwRegValue) + 1; 
   lRegResult = RegSetValueExA(hRegKey, sRegValueName.c_str(), 0, REG_SZ, (LPBYTE) buf, dwSize); 
   if(lRegResult != ERROR_SUCCESS)
   {
      TDEBUG_TRACE("Failed to reset " << sRegValueName);
      RegCloseKey(hRegKey);
      delete[] buf;
      return false;
   }

   // Update all windows
   SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, SPI_SETNONCLIENTMETRICS, 
                      0, SMTO_ABORTIFHUNG, 5000, reinterpret_cast<PDWORD_PTR>(&dwResult));

   RegCloseKey(hRegKey);
   delete[] buf;
   return true;
}


// Notify shell of change
void ShellNotifyUpdateFile(const std::string& sFilename)
{
   std::vector<std::string> vFiles;
   vFiles.push_back(sFilename);
   ShellNotifyUpdateFiles(vFiles);
}


// Notify shell of change
void ShellNotifyUpdateFiles(const std::vector<std::string>& sFilenames)
{
   IMalloc *pMalloc = 0;
   IShellFolder *pFolder = 0;
   LPITEMIDLIST pidl = 0;
   std::vector<std::string>::const_iterator it = sFilenames.begin();
   std::wstring wsFilename;
   WIN32_FIND_DATAA fd;
   HANDLE hFindFile = INVALID_HANDLE_VALUE;
   bool bUseWildcards = false;

   // Get malloc
   if (!SUCCEEDED(SHGetMalloc(&pMalloc)))
      goto Cleanup;

   // Get desktop folder 
   if (!SUCCEEDED(SHGetDesktopFolder(&pFolder)))
      goto Cleanup;

   while (it != sFilenames.end())
   {
      // Resolve wildcards
      bUseWildcards = (it->find_first_of("*?") != std::string::npos);
      if (bUseWildcards)
      {
         hFindFile = FindFirstFileA(it->c_str(), &fd);
         if (hFindFile == INVALID_HANDLE_VALUE)
         {
            it++;
            continue;
         }
      }

      do
      {
         // Convert filename to widestring
         if (bUseWildcards)
         {
            std::string s = EnsureTrailingDelimiter(StripLastPart(*it)) + fd.cFileName;
            wsFilename = MultibyteToWide(s, 0);
         }
         else
         {
            wsFilename = MultibyteToWide(*it, 0);
         }

         // Convert filename to ID list
         if (!SUCCEEDED(pFolder->ParseDisplayName(0, 0, 
            (LPWSTR) wsFilename.c_str(), 0, &pidl, 0)))
            goto Cleanup;

         // Notify shell
         SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_IDLIST | SHCNF_FLUSH, pidl, 0);

         // Release memory
         pMalloc->Free(pidl);
         pidl = 0;

      } while (bUseWildcards && FindNextFileA(hFindFile, &fd));

      if (bUseWildcards)
      {
         FindClose(hFindFile);
         hFindFile = INVALID_HANDLE_VALUE;
      }
      
      it++;
   }

Cleanup:
   if (hFindFile != INVALID_HANDLE_VALUE)
      FindClose(hFindFile);
   if (pidl && pMalloc)
      pMalloc->Free(pidl);
   if (pFolder)
      pFolder->Release();
   if (pMalloc)
      pMalloc->Release();
}


// Notify shell of change
void ShellNotifyUpdateFiles(const std::string& sDirname, 
                            const std::vector<std::string>& sFilenames)
{
   std::string sFilename;
   std::vector<std::string> vFiles;
   std::vector<std::string>::const_iterator it = sFilenames.begin();
   while (it != sFilenames.end())
   {
      sFilename = EnsureTrailingDelimiter(sDirname) + *it;
      vFiles.push_back(sFilename);
      it++;
   }
   ShellNotifyUpdateFiles(vFiles);
}



void ShellNotifyUpdateDir(const std::string& /* sDirname */)
{
/*   std::wstring wsDirname = MultibyteToWide(sDirname);
   WCHAR buf[_MAX_PATH];
   wcsncpy(buf, wsDirname.c_str(), _MAX_PATH);
   SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH, buf, NULL);*/
}



// Wait while processing messages 
DWORD WaitWithMsgQueue(DWORD nCount, const HANDLE* pHandles, bool fWaitAll,
                       DWORD dwMilliseconds)
{
   DWORD dw, tcNow, tcEnd, dwTimeToWait; 
   DWORD dwResult = 0;
   MSG msg;
   DWORD i;
   bool *ba = 0;
   HANDLE myHandles[MAXIMUM_WAIT_OBJECTS];
   unsigned int myCount;

   // Initialize handle array
   for (i = 0; i < nCount; i++)
   {
      myHandles[i] = pHandles[i];
   }
   myCount = nCount;

   tcNow = GetTickCount();
   tcEnd = tcNow + dwMilliseconds;

   bool bRepeat = true;
   while (bRepeat) 
   {
      if ((dwMilliseconds != INFINITE) && (tcNow >= tcEnd))
      {
         dwResult = WAIT_TIMEOUT;
         goto Cleanup;
      }

      if (dwMilliseconds != INFINITE)
      {
         dwTimeToWait = tcEnd - tcNow;
      }
      else
      {
         dwTimeToWait = INFINITE;
      }

      dw = MsgWaitForMultipleObjects(myCount, myHandles, false, dwTimeToWait, QS_ALLINPUT);
      // Object got signaled
      if (dw < (WAIT_OBJECT_0 + myCount))
      {
         if (fWaitAll)
         {
            // Remove object
            i = dw - WAIT_OBJECT_0;
            myCount--;
            for (unsigned int j = i; j < myCount - 1 && myCount > 0; j++)
            {
               myHandles[j] = myHandles[j + 1];
            }

            // Exit if all objects have been signaled
            if (myCount == 0)
            {
               dwResult = WAIT_OBJECT_0;
               goto Cleanup;
            }
         }
         else
         {
            dwResult = dw;
            goto Cleanup;
         }
      }
      // Got message
      else if (dw == (WAIT_OBJECT_0 + nCount))
      {
         while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
         {
            if (msg.message == WM_QUIT)
            {
               dwResult = dw;
               goto Cleanup;
            }

            DispatchMessage(&msg);

            // Check for timeout
            tcNow = GetTickCount();
            if ((dwMilliseconds != INFINITE) && (tcNow >= tcEnd))
            {
               dwResult = WAIT_TIMEOUT;
               goto Cleanup;
            }
         }
      }
      // WAIT timed out
      else if (dw == WAIT_TIMEOUT)
      {
         dwResult = dw;
         goto Cleanup;
      }
      // WAIT failed
      else
      {
         dwResult = dw;
         goto Cleanup;
      }

      tcNow = GetTickCount();
   }

Cleanup:
   if (ba) 
      delete ba;
   return dwResult;
}

// Get path for iconset
std::string GetIconSetPath(const std::string& iconSet)
{
   // No icons => empty path
   if (iconSet.empty())
      return "";

   std::string dir = GetSpecialFolder(CSIDL_PROGRAM_FILES_COMMON);
   dir = EnsureTrailingDelimiter(dir) + "TortoiseOverlays\\icons\\" + iconSet;
   return EnsureTrailingDelimiter(dir);
}


// Get name of iconset
wxString GetIconSetName(const std::string& iconSet)
{
   // No icons => empty path
   if (iconSet.empty())
      return wxT("");

   std::string key("Icons\\");
   key += iconSet;
   wxString name(TortoiseRegistry::ReadWxString(key));
   if (name.empty())
      return wxText(iconSet);
   return wxGetTranslation(name);
}

// Get attributes for file (contains bugfix for SHGetFileInfo)
BOOL MyShellGetFileAttr(const void* data, DWORD *attr, bool bIsPidl)
{
   TDEBUG_ENTER("MyShellGetFileAttr");

   SHFILEINFOA fi;
   DWORD dwFlags = SHGFI_ATTRIBUTES;
   if (attr != 0)
   {
      fi.dwAttributes = *attr;
      dwFlags |= SHGFI_ATTR_SPECIFIED;
   }
   if (bIsPidl)
      dwFlags |= SHGFI_PIDL;

   DWORD dwResult = SHGetFileInfoA((LPCSTR) data, 0, &fi, sizeof(fi), dwFlags);
   *attr = fi.dwAttributes;
   return (dwResult != 0);
}


// Get attributes for file (contains bugfix for SHGetFileInfo)
BOOL ShellGetFileAttributes(const char* filename, DWORD *attr)
{
   return MyShellGetFileAttr(filename, attr, false);
}


// Get attributes for file (contains bugfix for SHGetFileInfo)
BOOL ShellGetFileAttributesPidl(LPCITEMIDLIST pidl, DWORD *attr)
{
   return MyShellGetFileAttr(pidl, attr, true);
}


// Get icon size
BOOL ShellGetIconSize(UINT iconsize, int *width, int *height)
{
   SHFILEINFOA sfi;
   HIMAGELIST himg = (HIMAGELIST) SHGetFileInfoA("*.txt", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), 
                                                 SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX | SHGFI_SHELLICONSIZE | iconsize);

   return ImageList_GetIconSize(himg, width, height);
}
