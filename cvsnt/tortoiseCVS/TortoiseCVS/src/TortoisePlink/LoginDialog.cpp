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

#include "LoginDialog.h"
#include "TortoisePlinkRes.h"
#include <string>

#include <windows.h>
#include <dbghelp.h>

HINSTANCE g_hmodThisDll;
HWND g_hwndMain;

class LoginDialog
{
public:
   LoginDialog(const std::string& prompt, bool is_pw);
   
   static bool DoLoginDialog(std::string& password, const std::string& prompt, bool is_pw);

private:
   bool myOK;
   HWND _hdlg;

   std::string  myPassword;
   std::string  myPrompt;
   bool         myIsPW;
   
   void CreateModule(void);
   void RetrieveValues();
   
   std::string GetPassword();

   friend BOOL CALLBACK LoginDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class MiniDumper
{
public:
   MiniDumper(LPCTSTR szAppName, LPCSTR szFileName);
   ~MiniDumper();

private:
    static std::string ourAppName;
   static char m_szFileName[256];
   static LPTOP_LEVEL_EXCEPTION_FILTER m_OldFilter;
   static LONG WINAPI TopLevelFilter(struct _EXCEPTION_POINTERS *pExceptionInfo);
};


BOOL DoLoginDialog(char* password, int maxlen, const char* prompt, BOOL is_pw)
{
   g_hmodThisDll = GetModuleHandle(0);
   g_hwndMain = GetParentHwnd();
   std::string passwordstr;
   BOOL res = LoginDialog::DoLoginDialog(passwordstr, prompt, is_pw ? true : false);
   if (res)
      strncpy(password, passwordstr.c_str(), maxlen);
   return res;
}


BOOL CALLBACK LoginDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   if (uMsg == WM_INITDIALOG)
   {
      LoginDialog* pDlg = (LoginDialog*) lParam;
      pDlg->_hdlg = hwndDlg;
      //SetWindowLong(hwndDlg, GWL_USERDATA, lParam);
      SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
      // Set prompt text
      SendDlgItemMessage(hwndDlg, IDC_LOGIN_PROMPT, WM_SETTEXT,
                         pDlg->myPrompt.length(), (LPARAM) pDlg->myPrompt.c_str());
      // Make sure edit control has the focus
      //SendDlgItemMessage(hwndDlg, IDC_LOGIN_PASSWORD, WM_SETFOCUS, 0, 0);
      if (GetDlgCtrlID((HWND) wParam) != IDC_LOGIN_PASSWORD)
      { 
         SetFocus(GetDlgItem(hwndDlg, IDC_LOGIN_PASSWORD));
         return FALSE; 
      } 
      return TRUE; 
   }
   else if (uMsg == WM_COMMAND && LOWORD(wParam) == IDCANCEL && HIWORD(wParam) == BN_CLICKED)
   {
      //LoginDialog* pDlg = (LoginDialog*) GetWindowLong(hwndDlg, GWL_USERDATA);
      LoginDialog* pDlg = (LoginDialog*) GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
      pDlg->myOK = false;
      EndDialog(hwndDlg, IDCANCEL);
      return 1;
   }
   else if (uMsg == WM_COMMAND && LOWORD(wParam) == IDOK && HIWORD(wParam) == BN_CLICKED)
   {
      LoginDialog* pDlg = (LoginDialog*) GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
      //LoginDialog* pDlg = (LoginDialog*) GetWindowLong(hwndDlg, GWL_USERDATA);
      pDlg->myOK = true;
      pDlg->RetrieveValues();
      EndDialog(hwndDlg, IDOK);
      return 1;
   }
   return 0;
}

LoginDialog::LoginDialog(const std::string& prompt, bool is_pw)
{
   myPrompt = prompt;
   myIsPW = is_pw;
}

void LoginDialog::CreateModule(void)
{
   DialogBoxParam(g_hmodThisDll, MAKEINTRESOURCE(IDD_LOGIN), g_hwndMain,
                  (DLGPROC)(LoginDialogProc), (long)this);
}


bool LoginDialog::DoLoginDialog(std::string& password, const std::string& prompt, bool is_pw)
{
   LoginDialog *pDlg = new LoginDialog(prompt, is_pw);

   pDlg->CreateModule();

   bool ret = pDlg->myOK;
   password = pDlg->myPassword;
   
   delete pDlg;

   return ret;
}


std::string LoginDialog::GetPassword()
{
   char szTxt[256];
   SendDlgItemMessage(_hdlg, IDC_LOGIN_PASSWORD, WM_GETTEXT, sizeof(szTxt), (LPARAM)szTxt);
   std::string strText = szTxt;
   return strText;
}

void LoginDialog::RetrieveValues()
{
   myPassword = GetPassword();
}


BOOL IsWinNT()
{
   OSVERSIONINFO vi;
   vi.dwOSVersionInfoSize = sizeof(vi);
   if (GetVersionEx(&vi))
   {
      if (vi.dwPlatformId == VER_PLATFORM_WIN32_NT)
      {
         return TRUE;
      }
   }
   return FALSE;
}



HWND GetParentHwnd()
{
   // Try to use TCVS_HWND
   char buf[256];
   if (GetEnvironmentVariable("TCVS_HWND", buf, sizeof(buf)))
   {
      HWND hwnd = (HWND) atoi(buf);
      if (hwnd)
         return hwnd;
   }

   if (IsWinNT())
   {
      return GetDesktopWindow();
   }
   else
   {
      return GetForegroundWindow();
   }
}


void InitMiniDump()
{
    new MiniDumper("TortoiseCVS", "TortoisePlink");
}


std::string MiniDumper::ourAppName;
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


typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
                                         CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
                                         CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
                                         CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

LONG MiniDumper::TopLevelFilter(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
   LONG retval = EXCEPTION_CONTINUE_SEARCH;

   HMODULE hDll = ::LoadLibraryA("DBGHELP.DLL");

   std::string szResult;

   if (hDll)
   {
      MINIDUMPWRITEDUMP pDump = (MINIDUMPWRITEDUMP) ::GetProcAddress(hDll, "MiniDumpWriteDump");
      if (pDump)
      {
          // work out a good place for the dump file
          char buffer[MAX_PATH + 1];
          GetModuleFileNameA(0, buffer, sizeof(buffer));
          std::string sDumpPath = buffer;
          std::string::size_type i = sDumpPath.find_last_of("\\");
          sDumpPath = sDumpPath.substr(0, i);
          sDumpPath += std::string("/") + m_szFileName + ".dmp";

         // ask the user if they want to save a dump file
         if (::MessageBox(0,
                          "Something bad happened to TortoiseCVS, and it crashed. Would you like to save a crash dump file?",
                          ourAppName.c_str(), MB_YESNO) == IDYES)
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
                  szResult = "Saved dump file to " + sDumpPath;
                  retval = EXCEPTION_EXECUTE_HANDLER;
               }
               else
               {
                   szResult = "Failed to save dump file";
               }
               ::CloseHandle(hFile);
            }
            else
            {
                szResult = "Failed to create dump file " + sDumpPath;
            }
         }
      }
      else
      {
         szResult = "DBGHELP.DLL too old";
      }
   }
   else
   {
      szResult = "DBGHELP.DLL not found";
   }

   if (!szResult.empty())
      ::MessageBox(0, szResult.c_str(), ourAppName.c_str(), MB_OK);

   if (m_OldFilter)
      retval = m_OldFilter(pExceptionInfo);
   
   return retval;
}
