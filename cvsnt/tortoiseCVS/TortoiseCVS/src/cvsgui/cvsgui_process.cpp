/*
** The cvsgui protocol used by WinCvs
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License as published by the Free Software Foundation; either
** version 2.1 of the License, or (at your option) any later version.
** 
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Lesser General Public License for more details.
** 
** You should have received a copy of the GNU Lesser General Public
** License along with this library; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*!
   \file cvsgui_process.cpp
   \brief CvsGui process code implementation
   \author Alexandre Parenteau <aubonbeurre@hotmail.com> --- November 1999
   \note To be used by GUI and CVS client
   \note Derived from plugin.c in GIMP
*/

#include <StdAfx.h>
#include <windows.h>
#include "../Utils/TortoiseDebug.h"

#ifdef HAVE_CONFIG_H
extern "C" {
#include "config.h"
}
#endif

#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#ifndef WIN32
#  include <sys/wait.h>
#  include <sys/time.h>
#  include <sys/param.h>
#  include <unistd.h>
#endif

#include <stack>
#include <vector>
#include <algorithm>

#include "cvsgui_process.h"
#include "cvsgui_protocol.h"

/// Number of extra arguments added to the command line
#define CP_EXTRA_ARGC_COUNT   4

/// Safely close the handle and reset it's value to NULL
#ifdef WIN32
#  define SafeCloseHandle(handle)      do{ if( handle ) { CloseHandle(handle); handle = 0L; } }while(0)
#else
#  define SafeCloseHandle(handle)      do{ if( handle ) { close(handle); handle = 0L; } }while(0)
#endif

/// Safely free the memory block and reset it's value to NULL
#define SafeFree(memblock) do{ if( memblock ) { free(memblock); memblock = 0L; } }while(0)

#ifdef WIN32
   /// Window class name buffer size
#  define CP_WNDCLASSNAME_SIZE   256

   /// Read file buffer size
#  define CP_READFILE_BUFFER_SIZE 1024

   /// Thread return value to mark successful completion
#  define CP_THREAD_EXIT_SUCCESS (1)

   /// Timeout period for process termination attempts
#  define CP_TERMINATION_TIMEOUT 3000

#  define CP_SERVEPROTOCOL_SLEEP 20
#endif
static int cvs_process_write(pipe_t fd, guint8* buf, gulong count);
static int cvs_process_flush(pipe_t fd);
static void cvs_process_push(CvsProcess* cvs_process);
static void cvs_process_pop(void);
static void cvs_process_recv_message(CvsProcess* p, gulong* rcount);
static void cvs_process_handle_message(WireMessage* msg);
#ifndef WIN32
static void cvs_process_handle_stdio(CvsProcess* cvs_process, bool isStderr);
#endif

void cvs_process_kill(CvsProcess* cvs_process);
static void cvs_process_close(CvsProcess* cvs_process, int kill_it);
static CvsProcess* cvs_process_new(const char* name, int argc, char** argv);

static std::vector<CvsProcess*>cvs_process_stack;     /*!< Process stack container */
static std::vector<CvsProcess*>open_cvs_process;      /*!< Open process container */

static CvsProcess* current_cvs_process = NULL;        /*!< Current CVS process */
static int current_write_buffer_index = 0;            /*!< Current write buffer index */
static char* current_write_buffer = NULL;          /*!< Current write buffer */

static char process_write_buffer[CP_WRITE_BUFFER_SIZE];  /*!< Buffer for writing, only for the process */


#ifdef WIN32

/// Use the runtime to initialize and delete a critical section
class CDummyInit
{
public:
   // Construction
   CDummyInit()
   {
      ::InitializeCriticalSection(&m_destroyLock);
      ::InitializeCriticalSection(&m_stackLock);
   }

   ~CDummyInit()
   {
      ::DeleteCriticalSection(&m_destroyLock);
      ::DeleteCriticalSection(&m_stackLock);
   }

private:
   // Data members
   static CRITICAL_SECTION m_destroyLock; /*!< Destroy lock */
   static CRITICAL_SECTION m_stackLock;   /*!< Stack lock */

public:
   // Interface
   
   /// Get the destroy lock
   inline static CRITICAL_SECTION* GetDestroyLock()
   {
      return &m_destroyLock;
   }

   /// Get the stack lock
   inline static CRITICAL_SECTION* GetStackLock()
   {
      return &m_stackLock;
   }
} sDummyObject;

CRITICAL_SECTION CDummyInit::m_destroyLock;
CRITICAL_SECTION CDummyInit::m_stackLock;

/*!
   Write data to a file
   \param fd File handle
   \param buf Data to be written
   \param count Buffer size
   \return The count of data written
   \note Wraps ::WriteFile API
*/
static int write(pipe_t fd, const void* buf, int count)
{
   DWORD write;
   if( !WriteFile(fd, buf, count, &write, NULL) )
   {
      errno = EINVAL;
      return -1;
   }

   return write;
}

/*!
   Wait for protocol threads to complete
   \param process CVS process
   \param milliseconds Time-out in milliseconds
   \note Use with causion to avoid deadlock
*/
static void WaitForProtocolThreads(CvsProcess* process, int milliseconds)
{
   HANDLE waitHandles[CP_THREADS_COUNT] = { 0 };
   int cnt = 0;
   const DWORD curThread = GetCurrentThreadId();
   
   for(int i = 0; i < CP_THREADS_COUNT; i++)
   {
      if( process->threads[i] && (curThread != process->threadsID[i]) )
         waitHandles[cnt++] = process->threads[i];
   }
   
   WaitForMultipleObjects(cnt, waitHandles, TRUE, milliseconds);
}

/*!
   Callback enumerating all top-level windows and posting the WM_CLOSE message to the process that matches given process ID
   \param hWnd Window handle
   \param lParam Process ID to post message to
   \return TRUE to continue enumerate, FALSE to stop
*/
BOOL CALLBACK PostCloseEnum(HWND hWnd, LPARAM lParam)
{
   DWORD pid = 0;

   GetWindowThreadProcessId(hWnd, &pid);
   
   if( pid == (DWORD)lParam )
   {
      PostMessage(hWnd, WM_CLOSE, 0, 0);
   }
   
   return TRUE;
}

typedef struct
{
   DWORD pid;                          /*!< Process ID for which to find console */
   char wndClassName[CP_WNDCLASSNAME_SIZE];  /*!< Window class name of console window */
   HWND hWnd;                          /*!< Returned handle for console window */
} FindConsoleWindowData;

/*!
   Callback enumerating all top-level windows and looking for console window
   matching given process ID and window class name
   \param hWnd Window handle
   \param lParam FindConsoleWindowData containing process ID and window class name
   \return TRUE to continue enumerate, FALSE to stop
*/
BOOL CALLBACK FindConsoleEnum(HWND hWnd, LPARAM lParam)
{
   FindConsoleWindowData* data = (FindConsoleWindowData*) lParam;
   DWORD pid;
   GetWindowThreadProcessId(hWnd, &pid);
   BOOL res = TRUE;
   
   if( pid == data->pid )
   {
      char wndClassName[CP_WNDCLASSNAME_SIZE] = { 0 };
      if (GetClassNameA(hWnd, wndClassName, CP_WNDCLASSNAME_SIZE-1) )
      {
         if( stricmp(wndClassName, data->wndClassName) == 0 )
         {
            data->hWnd = hWnd;
            res = FALSE;
         }
      }
   }
   return res;
}


/*!
   Find console window for process with given process ID
   \param pid Process ID for which to find console window
   \return Handle to console window
*/
HWND FindConsoleWindow(DWORD pid)
{
   FindConsoleWindowData data = { 0 };

   OSVERSIONINFO vi = { 0 };
   vi.dwOSVersionInfoSize = sizeof(vi);
   GetVersionEx(&vi);
   
   if( vi.dwPlatformId == VER_PLATFORM_WIN32_NT )
      strcpy(data.wndClassName, "ConsoleWindowClass");
   else
      strcpy(data.wndClassName, "tty");
   
   data.pid = pid;
   data.hWnd = 0;
   EnumWindows(FindConsoleEnum, (LPARAM)&data);
   
   return data.hWnd;
}


/*!
   Sent break key (Ctrl-C, Ctrl+Break) to the process with given process ID
   \param pid Process ID to which to send Ctrl-C
   \return true if successful, false otherwise
*/
bool SendBreakKey(DWORD pid)
{
   bool res = false;

   const HWND hWnd = FindConsoleWindow(pid);
   if( hWnd )
   {
      const BYTE controlScanCode = (BYTE)MapVirtualKey(VK_CONTROL, 0);
      BYTE vkBreakCode = VK_CANCEL;
      BYTE breakScanCode = (BYTE)MapVirtualKey(vkBreakCode, 0);
      
      /* Fake Ctrl-C if we can't manage Ctrl-Break. */
      vkBreakCode = 'C';
      breakScanCode = (BYTE)MapVirtualKey(vkBreakCode, 0);
      
      const HWND foregroundWnd = GetForegroundWindow();
      if( foregroundWnd && SetForegroundWindow(hWnd) )
      {
         /* Generate keystrokes as if user had typed Ctrl-Break or Ctrl-C. */
         keybd_event(VK_CONTROL, controlScanCode, 0, 0);
         keybd_event(vkBreakCode, breakScanCode, 0, 0);
         keybd_event(vkBreakCode, breakScanCode, KEYEVENTF_KEYUP, 0);
         keybd_event(VK_CONTROL, controlScanCode, KEYEVENTF_KEYUP, 0);
         
         SetForegroundWindow(foregroundWnd);
         res = true;
      }
   }

   return res;
}

/*!
   Terminate CVS process
   \param cvs_process CVS process to be terminated
   \return true if process was terminate, false otherwise
   \note See also Q178893 - HOWTO: Terminate an Application "Cleanly" in Win32
*/
bool cvs_process_terminate(CvsProcess* cvs_process)
{
   bool res = FALSE;

   // Check whether it's still alive first
   if( WaitForSingleObject(cvs_process->pid, 0) == WAIT_TIMEOUT )
   {
      // Send break key and wait for process to terminate
      SendBreakKey(cvs_process->threadID);
      if( WaitForSingleObject(cvs_process->pid, CP_TERMINATION_TIMEOUT) == WAIT_OBJECT_0 )
      {
         res = TRUE;
      }
      else
      {
         // Post the close message and wait again
         EnumWindows((WNDENUMPROC)PostCloseEnum, (LPARAM)cvs_process->threadID);
         if( WaitForSingleObject(cvs_process->pid, CP_TERMINATION_TIMEOUT) == WAIT_OBJECT_0 )
         {
            res = TRUE;
         }
         else
         {
            res = TerminateProcess(cvs_process->pid, 0) != 0;
         }
      }
   }
   return res;
}

/// Critical section used when destroying CVS process
struct CDestroyThreadLock
{
public:
   // Construction
   inline CDestroyThreadLock()
   {
      EnterCriticalSection(CDummyInit::GetDestroyLock());
   }

   inline ~CDestroyThreadLock()
   {
      LeaveCriticalSection(CDummyInit::GetDestroyLock());
   }
};

/// Critical section used for CVS process stack operations
struct CStackThreadLock
{
public:
   inline CStackThreadLock()
   {
      EnterCriticalSection(CDummyInit::GetStackLock());
   }
   
   inline ~CStackThreadLock()
   {
      LeaveCriticalSection(CDummyInit::GetStackLock());
   }
};

#else

/// Dummy critical section used when destroying CVS process
struct CDestroyThreadLock
{
public:
   inline CDestroyThreadLock()
   {
   }

   inline ~CDestroyThreadLock()
   {
   }
};

/// Dummy critical section used for CVS process stack operations
struct CStackThreadLock
{
public:
   inline CStackThreadLock()
   {
   }

   inline ~CStackThreadLock()
   {
   }
};

#endif

/*!
   Read the exit code message from the pipe
   \param fd Pipe
    \param msg Message
    \param rcount Received data count
*/
static void _gp_quit_read(pipe_t fd, WireMessage* msg, gulong* rcount)
{
   GPT_QUIT* t = (GPT_QUIT*)msg->data;
   if( t == 0 )
      return;

   if( !wire_read_int32(fd, (guint32*)&t->code, 1, rcount) )
      return;
}

/*!
   Write the exit code message to the pipe
   \param fd Pipe
   \param msg Message
*/
static void _gp_quit_write(pipe_t fd, WireMessage* msg)
{
   GPT_QUIT* t = (GPT_QUIT*)msg->data;
   if( !wire_write_int32(fd, (guint32*)&t->code, 1) )
      return;
}

/*!
   Destroy message data
   \param msg Message
*/
static void _gp_quit_destroy(WireMessage* msg)
{
}

/*!
   Read the environment message from the pipe
   \param fd Pipe
   \param msg Message
   \param rcount Received data count
*/
static void _gp_getenv_read(pipe_t fd, WireMessage* msg, gulong* rcount)
{
   GPT_GETENV* t = (GPT_GETENV*)msg->data;
   if( t == 0 )
      return;

   if( !wire_read_int8(fd, &t->empty, 1, rcount) )
      return;

   if( !wire_read_string(fd, &t->str, 1, rcount) )
      return;
}

/*!
   Write the environment message to the pipe
   \param fd Pipe
   \param msg Message
*/
static void _gp_getenv_write(pipe_t fd, WireMessage* msg)
{
   GPT_GETENV* t = (GPT_GETENV*)msg->data;
   if( !wire_write_int8(fd, &t->empty, 1) )
      return;

   if( !wire_write_string(fd, &t->str, 1, -1) )
      return;
}

/*!
   Destroy environment data
   \param msg Message
*/
static void _gp_getenv_destroy(WireMessage* msg)
{
   GPT_GETENV* t = (GPT_GETENV*)msg->data;
   SafeFree(t->str);
}

/*!
   Read the console message from the pipe
   \param fd Pipe
   \param msg Message
   \param rcount Received data count
*/
static void _gp_console_read(pipe_t fd, WireMessage* msg, gulong* rcount)
{
   GPT_CONSOLE* t = (GPT_CONSOLE*)msg->data;
   if( t == 0 )
      return;

   if( !wire_read_int8(fd, &t->isStderr, 1, rcount) )
      return;

   if( !wire_read_int32(fd, &t->len, 1, rcount) )
      return;

   if( !wire_read_string(fd, &t->str, 1, rcount) )
      return;
}

/*!
   Write the console message to the pipe
   \param fd Pipe
   \param msg Message
*/
static void _gp_console_write(pipe_t fd, WireMessage* msg)
{
   GPT_CONSOLE* t = (GPT_CONSOLE*)msg->data;
   
   if( !wire_write_int8(fd, &t->isStderr, 1) )
      return;
   
   if( !wire_write_int32(fd, &t->len, 1) )
      return;
   
   if( !wire_write_string(fd, &t->str, 1, t->len) )
      return;
}

/*!
   Destroy console data
   \param msg Message
*/
static void _gp_console_destroy(WireMessage* msg)
{
   GPT_CONSOLE* t = (GPT_CONSOLE*)msg->data;
   SafeFree(t->str);
}

/*!
   Register read, write and destroy functions
*/
static void gp_init()
{
   wire_register(GP_QUIT,
      _gp_quit_read,
      _gp_quit_write,
      _gp_quit_destroy);

   wire_register(GP_GETENV,
      _gp_getenv_read,
      _gp_getenv_write,
      _gp_getenv_destroy);

   wire_register(GP_CONSOLE,
      _gp_console_read,
      _gp_console_write,
      _gp_console_destroy);
}

/*!
   Write the exit code to the pipe
   \param fd Pipe
   \param code Exit code
   \return TRUE on success, FALSE otherwise
*/
int gp_quit_write(pipe_t fd, int code)
{
   WireMessage msg;
   GPT_QUIT* t = (GPT_QUIT*)msg.data;
   
   msg.type = GP_QUIT;

   t->code = code;

   if( !wire_write_msg(fd, &msg) )
      return FALSE;

   if( !wire_flush(fd) )
      return FALSE;

   return TRUE;
}

/*!
   Write the environment variable to the pipe
   \param fd Pipe
   \param env Name of the environment variable 
   \return TRUE on success, FALSE otherwise
*/
int gp_getenv_write(pipe_t fd, const char* env)
{
   WireMessage msg;
   GPT_GETENV* t = (GPT_GETENV*)msg.data;

   msg.type = GP_GETENV;

   t->empty = env == 0;
   t->str = strdup(env == 0 ? "" : env);

   if( !wire_write_msg(fd, &msg) )
      return FALSE;

   wire_destroy(&msg);
   if( !wire_flush(fd) )
      return FALSE;

   return TRUE;
}

/*!
   Read the environment variable from the pipe
   \param fd Pipe
   \return The value of environment variable 
*/
char* gp_getenv_read(pipe_t fd)
{
   WireMessage msg = { 0 };

   gulong rcount = 0;

    if( !wire_read_msg(fd, &msg, &rcount) || msg.type != GP_GETENV )
   {
      fprintf(stderr, "cvsgui protocol error !\n");
      exit(-1);
   }

   GPT_GETENV* t = (GPT_GETENV*)msg.data;
   char* res = t->empty ? 0L : strdup(t->str);
   wire_destroy(&msg);

   return res;
}

#ifdef WIN32
extern "C"
#endif

/*!
   Write the console data to the pipe
   \param fd Pipe
   \param str Data to be written
   \param len Data length
   \param isStderr Non-zero to write <b>stderr</b>, otherwise write <b>stdout</b>
   \param binary Non-zero if data is binary, text data otherwise
   \return TRUE on success, FALSE otherwise
   \note For binary data an additional empty message is written first before the actual data
*/
int gp_console_write(pipe_t fd, const char* str, int len, int isStderr, int binary)
{
   WireMessage msg;
   GPT_CONSOLE* t = (GPT_CONSOLE*)msg.data;

   if( binary )
   {
      // Sending an empty message to indicate the binary stream coming
       gp_console_write(fd, "", 0, 0, 0);
   }

   msg.type = GP_CONSOLE;

   t->isStderr = isStderr;
   t->len = len;
   t->str = (char*)malloc((len + 1) * sizeof(char));
   memcpy(t->str, str, len * sizeof(char));
   t->str[len] = '\0';

   if( !wire_write_msg(fd, &msg) )
      return FALSE;

   if( !wire_flush(fd) )
      return FALSE;

   return TRUE;
}

/*!
   Initialize the protocol library (both for the process and the application)
*/
void cvs_process_init()
{
   gp_init();
   wire_set_writer(cvs_process_write);
   wire_set_flusher(cvs_process_flush);
}

/*!
   Create new CVS process object and initialize it's data members
   \param name Command
   \param argc Arguments count
   \param argv Arguments
   \return Pointer to the newly allocated object
   \note Allocated memory must be freed using cvs_process_destroy
*/
static CvsProcess* cvs_process_new(const char* name, int argc, char** argv)
{
   CvsProcess* cvs_process;

   cvs_process_init();

   cvs_process = (CvsProcess*)malloc(sizeof(CvsProcess));
   if( cvs_process == 0L )
      return 0L;

   cvs_process->open = FALSE;
   cvs_process->destroy = FALSE;
   cvs_process->gotExitCode = FALSE;

   cvs_process->pid = 0;

   cvs_process->callbacks = 0;
   cvs_process->argc = argc + CP_EXTRA_ARGC_COUNT;
   cvs_process->args = (char**)malloc((cvs_process->argc + 1) * sizeof(char*));
   cvs_process->args[0] = strdup(name);
   cvs_process->args[1] = strdup("-cvsgui");
   cvs_process->args[2] = (char*)malloc(16 * sizeof(char));
   cvs_process->args[3] = (char*)malloc(16 * sizeof(char));
   
   for(int i = 0; i < argc; i++)
   {
      cvs_process->args[CP_EXTRA_ARGC_COUNT + i] = strdup(argv[i]);
   }
   
   cvs_process->args[cvs_process->argc] = 0;
   cvs_process->my_read = 0;
   cvs_process->my_write = 0;
   cvs_process->his_read = 0;
   cvs_process->his_write = 0;
   cvs_process->write_buffer_index = 0;
   cvs_process->pstdin = 0;
   cvs_process->pstdout = 0;
   cvs_process->pstderr = 0;
   cvs_process->appData = 0;

#ifdef WIN32
   cvs_process->threadID = 0;
   memset(cvs_process->threads, 0, sizeof(cvs_process->threads));
   memset(cvs_process->threadsID, 0, sizeof(cvs_process->threadsID));
   cvs_process->startThreadsEvent = NULL;

   cvs_process->stopProcessEvent = NULL;

   cvs_process->commandFileName = NULL;
#endif

   return cvs_process;
}

/*!
   Destroy CVS process object and free allocated memory
   \param cvs_process CVS process to be destroyed
*/
void cvs_process_destroy(CvsProcess* cvs_process)
{
   TDEBUG_ENTER("cvs_process_destroy");
   if( cvs_process )
   {
      cvs_process_close(cvs_process, TRUE);

      if( cvs_process == current_cvs_process )
         cvs_process_pop();

      if( !cvs_process->destroy )
      {
         cvs_process->destroy = TRUE;
         cvs_process->callbacks->ondestroy(cvs_process);

         if( cvs_process->args != 0L )
         {
#ifdef WIN32
            if( cvs_process->commandFileName )
            {
               if( unlink(cvs_process->commandFileName) == 0 )
               {
                  cvs_process->commandFileName = NULL;
               }
            }
#endif

            for(int i = 0; i < cvs_process->argc; i++)
            {
               SafeFree(cvs_process->args[i]);
            }
            
            SafeFree(cvs_process->args);
         }
         
#ifdef WIN32
         // Close the threads handles
         for(int i = 0; i < CP_THREADS_COUNT; i++)
         {
            SafeCloseHandle(cvs_process->threads[i]);
         }

         SafeCloseHandle(cvs_process->startThreadsEvent);

         SafeCloseHandle(cvs_process->stopProcessEvent);
#endif
         // Close the handles now
         SafeCloseHandle(cvs_process->pid);

         SafeCloseHandle(cvs_process->my_read);
         SafeCloseHandle(cvs_process->my_write);
         
         SafeCloseHandle(cvs_process->his_read);
         SafeCloseHandle(cvs_process->his_write);
         
         SafeCloseHandle(cvs_process->pstdin);
         SafeCloseHandle(cvs_process->pstdout);
         SafeCloseHandle(cvs_process->pstderr);

         // Free the process memory
         SafeFree(cvs_process);
      }
   }
}

#ifdef WIN32

/*!
   Compute the total length the command will have
   \param argc Arguments count
   \param argv Arguments
   \return The command length
*/
static int calc_command_len(int argc, char* const* argv)
{
    int len = 0;

    for(int i = 0; i < argc; i++)
    {
        if( argv[i] )
        {
            len += 2; /* for the double quotes */
         
            for(const char* a = argv[i]; *a; a++)
            {
                if( 0 < i )
                {
                    switch( *a )
                    {
                    case '"':
                        len++;
                        break;
                    case '\\':
                        if ( (*(a + 1) == '\0') || (*(a + 1) == '"') )
                        {
                            len++;
                        }
                        break;
                    default:
                        // Nothing to do
                        break;
                    }
                }

                len++;
            }
         
            len++;   /* for the space or the '\0' */
        }
    }

    return len;
}

/*!
   Given an array of arguments that one might pass to spawnv 
   construct a command line that one might pass to CreateProcess
   
   \param argc Arguments count
   \param argv Arguments
   \return The built command string
   \note Quote arguments appropriately and escape special characters
*/
static char* build_command(int argc, char* const* argv)
{
    int len = calc_command_len(argc, argv);

    /* The + 10 is in case len is 0. */
    char* command = (char*)malloc(len + 10);
    if( !command )
    {
        errno = ENOMEM;
        return command;
    }
   
    char* p = command;
    *p = '\0';
   
    /* Copy each element of argv to command, putting each command in double quotes */
    for(int i = 0; i < argc; i++)
    {
        if( argv[i] )
        {
            *p++ = '"';

            for(const char* a = argv[i]; *a; a++)
            {
                /* Escape any special characters that appear within an argument */
                if( 0 < i )
                {
                    switch( *a )
                    {
                    case '"':
                        *p++ = '\\';
                        break;
                    case '\\':
                        if ( (*(a + 1) == '\0') || (*(a + 1) == '"') )
                        {
                            *p++ = '\\';
                        }
                        break;
                    default:
                        // Nothing to do
                        break;
                    }
                }

                *p++ = *a;
            }
         
            *p++ = '"';
            *p++ = ' ';
        }
    }
   
    if( p > command )
        p[-1] = '\0';
   
    return command;
}

/*!
   Read CVS process data from pipe
   \param process CVS process
   \param readType Type of pipe to read: 
      - 0 to read stdout 
      - 1 to read stderr
   \note Attempts to read in a non-blocking fashion and quits when the CVS process exits
*/
void ReadProcessPipe(CvsProcess* process, const int readType)
{
   char buff[CP_READFILE_BUFFER_SIZE] = { 0 };
   
   const pipe_t& readPipe = readType ? process->pstderr : process->pstdout;

   while( TRUE )
   {
      DWORD bytesRead = 0;
      DWORD totalBytesAvail = 0;

      if( !PeekNamedPipe(readPipe, 0L, 0 , 0L, &totalBytesAvail, 0L) )
      {
         break;
      }
      
      if( totalBytesAvail == 0 )
      {
         if( WaitForSingleObject(process->pid, CP_SERVEPROTOCOL_SLEEP) == WAIT_OBJECT_0 )
         {
            if( !PeekNamedPipe(readPipe, 0L, 0 , 0L, &totalBytesAvail, 0L) || totalBytesAvail == 0 )
            {
               break;
            }
         }
         
         continue;
      }

      if( ReadFile(readPipe, buff, CP_READFILE_BUFFER_SIZE - 1, &bytesRead, NULL) )
      {
         buff[bytesRead] = '\0';
         
         if( readType )
         {
            process->callbacks->consoleerr(buff, bytesRead, process);
         }
         else
         {
            process->callbacks->consoleout(buff, bytesRead, process);
         }
      }
      else
      {
         break;
      }
   }
}

/*!
   Worker thread to read the CVS process stdout
   \param process CVS process
   \return THREAD_EXIT_SUCCESS
*/
static DWORD GetChldOutput(CvsProcess* process)
{
   const HANDLE enterHandles[2] = { process->startThreadsEvent, process->stopProcessEvent };
   
   if( WaitForMultipleObjects(2, enterHandles, FALSE, INFINITE) == WAIT_OBJECT_0 )
   {
      ReadProcessPipe(process, 0);
   }

   return CP_THREAD_EXIT_SUCCESS;
}

/*!
   Worker thread to read the CVS process stderr
   \param process CVS process
   \return THREAD_EXIT_SUCCESS
*/
static DWORD GetChldError(CvsProcess* process)
{
   const HANDLE enterHandles[2] = { process->startThreadsEvent, process->stopProcessEvent };
   
   if( WaitForMultipleObjects(2, enterHandles, FALSE, INFINITE) == WAIT_OBJECT_0 )
   {
      ReadProcessPipe(process, 1);
   }

   return CP_THREAD_EXIT_SUCCESS;
}

/*!
   Worker thread to monitor the CVS process operation and stop request
   \param process CVS process
   \return THREAD_EXIT_SUCCESS
*/
static DWORD MonitorChld(CvsProcess* process)
{
   const HANDLE enterHandles[2] = { process->startThreadsEvent, process->stopProcessEvent };
   
   if( WaitForMultipleObjects(2, enterHandles, FALSE, INFINITE) == WAIT_OBJECT_0 )
   {
      const HANDLE waitHandles[2] = { process->pid, process->stopProcessEvent };
      
      if( WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE) == WAIT_OBJECT_0 )
      {
         // Wait so the threads can complete
         WaitForProtocolThreads(process, INFINITE);
         cvs_process_destroy(process);
      }
      else
      {
            // In this case, no GP_QUIT message ever arrives.
            // We need to set the exit code manually.
            // We use a negative exit code to distinguish a kill
            // from a voluntary exit.
            process->callbacks->exit(-1, process);
         cvs_process_kill(process);
      }
   }

   return CP_THREAD_EXIT_SUCCESS;
}

/*!
   Worker thread to read and process pipe communication
   \param process CVS process
   \return THREAD_EXIT_SUCCESS
*/
static DWORD ServeProtocol(CvsProcess* process)
{
   const HANDLE enterHandles[2] = { process->startThreadsEvent, process->stopProcessEvent };
   
   if( WaitForMultipleObjects(2, enterHandles, FALSE, INFINITE) == WAIT_OBJECT_0 )
   {
      DWORD totalBytesAvail = 0;
      while( TRUE )
      {
         if( totalBytesAvail == 0 )
         {
            if( !PeekNamedPipe(process->my_read, 0L, 0 , 0L, &totalBytesAvail, 0L) )
            {
               break;
            }
         }
         
         if( totalBytesAvail == 0 )
         {
            if( WaitForSingleObject(process->pid, CP_SERVEPROTOCOL_SLEEP) == WAIT_OBJECT_0 )
            {
               if( !PeekNamedPipe(process->my_read, 0L, 0 , 0L, &totalBytesAvail, 0L) || totalBytesAvail == 0 )
               {
                  break;
               }
            }

            continue;
         }
         
         gulong rcount;
         cvs_process_recv_message(process, &rcount);

         // rcount == 0 is definitely an error condition, sometimes it occurs
         // when CVS commands are aborted. Once it occurs, it remains true in every
         // iteration. Also, it may occur on protocol errors.
         // Avoid spinning indefinitely, just bail out -- presumably CVS has been
         // killed already.
         if( !rcount )
             return 0;
         if( rcount > totalBytesAvail )
            totalBytesAvail = 0;
         else
            totalBytesAvail -= rcount;
      }
   }

   // Sometimes we do not get the exit code via the protocol. In that case, get it from Windows.
   if (!process->gotExitCode)
   {
       DWORD exitCode;
       if (GetExitCodeProcess(process->pid, &exitCode))
           process->callbacks->exit(exitCode, process);
   }
   
   return CP_THREAD_EXIT_SUCCESS;
}
#endif

#ifndef WIN32

/// Pointer to the process if sigtt_handler is forced to kill
CvsProcess* sigtt_cvs_process = NULL;

#define SIGTT_ERR "This CVS command required an interactive TTY, I had to kill it.\n"

/*!
   Signal handler
   \param sig Signal
*/
static void sigtt_handler(int sig)
{
   if( sigtt_cvs_process )
   {
      // Keep that for later
      CvsProcessCallbacks* callbacks = sigtt_cvs_process->callbacks;

      // Killing the CVS process avoids getting stuck in a SIGSTOP
      cvs_process_destroy(sigtt_cvs_process);
      callbacks->consoleerr(SIGTT_ERR, strlen(SIGTT_ERR), sigtt_cvs_process);
   }

   sigtt_cvs_process = NULL;
} 
#endif

/*!
   Start the CVS process
   \param name Command to be started
   \param argc Arguments count
   \param argv Arguments
   \param callbacks <b>stdout</b>/<b>stderr</b> redirection callbacks
   \param startupInfo Additional parameters to control command's execution
   \param appData Application-specific data
   \return Pointer to the new CvsProcess on success, NULL otherwise
   \note On WIN32 platform arguments are quoted and special characters are escaped
*/
CvsProcess* cvs_process_run(const char* name, int argc, char** argv, 
                            CvsProcessCallbacks* callbacks, CvsProcessStartupInfo* startupInfo,
                            void* appData)
{
   if( !callbacks || !startupInfo )
   {
      if( startupInfo )
      {
         startupInfo->errorInfo = badArguments;
      }

      assert(0);  // Bad arguments
      return 0L;
   }

   CvsProcess* cvs_process = cvs_process_new(name, argc, argv);
   if( !cvs_process )
   {
      startupInfo->errorInfo = noMemory;
      return 0L;
   }

   cvs_process->callbacks = callbacks;

   cvs_process->appData = appData;

#ifndef WIN32

   int my_read[2] = { 0, 0 };
   int my_write[2] = { 0, 0 };
   int stdChildOut = 0, stdChildErr = 0;

   /* Open two pipes. (Bidirectional communication). */
   if( (pipe(my_read) == -1) || (pipe(my_write) == -1) )
   {
      startupInfo->lastErrnoValue = errno;
      startupInfo->errorInfo = systemError;

      fprintf(stderr, "unable to open pipe\n");
      cvs_process_destroy(cvs_process);
      return 0L;
   }

   cvs_process->my_read = my_read[0];
   cvs_process->my_write = my_write[1];
   cvs_process->his_read = my_write[0];
   cvs_process->his_write = my_read[1];
   
   /* Remember the file descriptors for the pipes. */
   sprintf(cvs_process->args[2], "%d", cvs_process->his_read);
   sprintf(cvs_process->args[3], "%d", cvs_process->his_write);
   
   /* Add the relevant commands to spawn in a terminal if necessary */
   if( startupInfo->hasTty )
   {
      cvs_process->argc += 2;
      char** old_args = cvs_process->args;

      cvs_process->args = (char**)malloc((cvs_process->argc + 1) * sizeof(char*));
      cvs_process->args[0] = strdup("xterm");
      cvs_process->args[1] = strdup("-e");
      int i = 0;

      while( old_args[i] )
      {
         cvs_process->args[i+2] = old_args[i];
         i++;
      }
      
      cvs_process->args[cvs_process->argc] = 0L;
      free(old_args);
   }
#if TARGET_RT_MAC_MACHO
   else 
   {
      // redirect stdout / stderr to catch printf family output from CVS process
      int my_stdout[2] = { 0, 0 };
      int my_stderr[2] = { 0, 0 };
      
      if( (pipe(my_stdout) == -1) || (pipe(my_stderr) == -1) )
      {
         startupInfo->lastErrnoValue = errno;
         startupInfo->errorInfo = systemError;
         
         fprintf(stderr, "unable to open pipe\n");
         cvs_process_destroy(cvs_process);
         return 0L;
      }

      stdChildOut = my_stdout[1];
      cvs_process->pstdout = my_stdout[0];
      stdChildErr = my_stderr[1];
      cvs_process->pstderr = my_stderr[0];
   }
#endif
   
   /* If we are running non interactively (i.e. gcvs&), it is
   *  possible that the CVS process would require entering a password
   *  or something similar (happens in some cases for CVS_RSH=ssh)
   *  this would raise a SIGTTOU, which turns into a SIGSTOP if not handled
   */
   sigtt_cvs_process = cvs_process;
   signal(SIGTTIN, sigtt_handler); // TODO: check if necessary on Mac
   signal(SIGTTOU, sigtt_handler); // TODO: check if necessary on Mac
   
   /* Fork another process. We'l remember the process id
   *  so that we can later use it to kill the filter if
   *  necessary.
   */
   
   cvs_process->pid = fork();
   
   if( cvs_process->pid == 0 )
   {
      close(cvs_process->my_read);
      close(cvs_process->my_write);
      
      if( cvs_process->pstdout ) 
      {
         close(cvs_process->pstdout);
         dup2(stdChildOut, 1);
      }

      if( cvs_process->pstderr )
      {
         close(cvs_process->pstderr);
         dup2(stdChildErr, 2);
      }

      /* Execute the filter. The "_exit" call should never
      *  be reached, unless some strange error condition
      *  exists.
      */
      execvp(cvs_process->args[0], cvs_process->args);
      _exit(1);
   }
   else if( cvs_process->pid == -1 )
   {  
      startupInfo->lastErrnoValue = errno;
      startupInfo->errorInfo = systemError;
      
      cvs_process_destroy(cvs_process);
      // fork failed
      sigtt_cvs_process = NULL;
      return 0L;
   }
   
   close(cvs_process->his_read);
   cvs_process->his_read = -1;
   
   close(cvs_process->his_write);
   cvs_process->his_write = -1;
   
   if( stdChildOut )
      close(stdChildOut);

   if( stdChildErr )
      close(stdChildErr);
   
#else
   
   BOOL resCreate = FALSE;

   SECURITY_ATTRIBUTES lsa = { 0 };
   STARTUPINFOA si = { 0 };
   PROCESS_INFORMATION pi = { 0 };
   
   HANDLE dupIn = 0L, dupOut = 0L;
   HANDLE stdChildIn = 0L, stdChildOut = 0L, stdChildErr = 0L;
   HANDLE stdoldIn = 0L, stdoldOut = 0L, stdoldErr = 0L;
   HANDLE stddupIn = 0L, stddupOut = 0L, stddupErr = 0L;
   char* command = 0L;
   int i = 0;
   int cnt = 0;
   LPTHREAD_START_ROUTINE threadsFunc[CP_THREADS_COUNT] = { 0 };
   
   lsa.nLength = sizeof(SECURITY_ATTRIBUTES);
   lsa.lpSecurityDescriptor = NULL;
   lsa.bInheritHandle = TRUE;
   
   // Create the pipes used for the cvsgui protocol
   if( !CreatePipe(&cvs_process->his_read, &dupIn, &lsa, 0) )
      goto error;

   if( !CreatePipe(&dupOut, &cvs_process->his_write, &lsa, 0) )
      goto error;
   
   // Duplicate the application side handles so they lose the inheritance
   if( !DuplicateHandle(GetCurrentProcess(), dupIn,
      GetCurrentProcess(), &cvs_process->my_write, 0, FALSE, DUPLICATE_SAME_ACCESS) )
   {
      goto error;
   }

   SafeCloseHandle(dupIn);
   
   if( !DuplicateHandle(GetCurrentProcess(), dupOut,
      GetCurrentProcess(), &cvs_process->my_read, 0, FALSE, DUPLICATE_SAME_ACCESS) )
   {
      goto error;
   }
   
   SafeCloseHandle(dupOut);
   
   if( !startupInfo->hasTty )
   {
      // Redirect stdout, stderr, stdin
      if( !CreatePipe(&stdChildIn, &stddupIn, &lsa, 0) )
         goto error;
      
      if( !CreatePipe(&stddupOut, &stdChildOut, &lsa, 0) )
         goto error;
      
      if( !CreatePipe(&stddupErr, &stdChildErr, &lsa, 0) )
         goto error;
      
      // Same thing as above
      if( !DuplicateHandle(GetCurrentProcess(), stddupIn,
         GetCurrentProcess(), &cvs_process->pstdin, 0, FALSE, DUPLICATE_SAME_ACCESS) )
      {
         goto error;
      }
      
      SafeCloseHandle(stddupIn);
      
      if( !DuplicateHandle(GetCurrentProcess(), stddupOut,
         GetCurrentProcess(), &cvs_process->pstdout, 0, FALSE, DUPLICATE_SAME_ACCESS) )
      {
         goto error;
      }
      
      SafeCloseHandle(stddupOut);
      
      if( !DuplicateHandle(GetCurrentProcess(), stddupErr,
         GetCurrentProcess(), &cvs_process->pstderr, 0, FALSE, DUPLICATE_SAME_ACCESS) )
      {
         goto error;
      }
   
      SafeCloseHandle(stddupErr);
   }
   
   // Build the arguments for CVS
   sprintf(cvs_process->args[2], "%d", reinterpret_cast<int>(cvs_process->his_read));
   sprintf(cvs_process->args[3], "%d", reinterpret_cast<int>(cvs_process->his_write));
   
   // If command line is too long then use the global -F option to overcome the limit
   if( startupInfo->commandLineLimit > 0 && 
      calc_command_len(cvs_process->argc, cvs_process->args) > startupInfo->commandLineLimit )
   {
      // Create the temporary file
      char commandFileName[MAX_PATH] = "";

      if( ::GetTempPathA(MAX_PATH - 1, commandFileName) && 
         ::GetTempFileNameA(commandFileName, "cpc", 0, commandFileName) )
      {
         // Schedule file for removal at boot
         OSVERSIONINFO vi = { 0 };
         vi.dwOSVersionInfoSize = sizeof(vi);
         GetVersionEx(&vi);
         
         if( vi.dwPlatformId == VER_PLATFORM_WIN32_NT )
         {
            ::MoveFileExA(commandFileName, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
         }
         
         // Make the room for additional parameters
         cvs_process->argc += 2;
         char** old_args = cvs_process->args;
         
         cvs_process->args = (char**)malloc((cvs_process->argc + 1) * sizeof(char*));
         cvs_process->args[0] = old_args[0];
         cvs_process->args[1] = old_args[1];
         cvs_process->args[2] = old_args[2];
         cvs_process->args[3] = old_args[3];
         
         // Insert command file into the CVS process arguments
         cvs_process->args[4] = strdup("-F");
         cvs_process->args[5] = strdup(commandFileName);

         // Store to be able to delete the file at destruction time
         cvs_process->commandFileName = cvs_process->args[5];

         // Copy the arguments
         {
            for(int i = 4; old_args[i]; i++)
            {
               cvs_process->args[i+2] = old_args[i];
            }
         }
         
         cvs_process->args[cvs_process->argc] = 0L;
         free(old_args);
         
         // Find the probable begining of CVS command (after the global options)
         int cvsCommandPos = 6;

         {
            for(int i = cvsCommandPos; i < cvs_process->argc; i++)
            {
               if( *cvs_process->args[i] != '-' )
               {
                  cvsCommandPos = i;
                  break;
               }
            }
         }

         // Find the CVS command split position
         int commandSpliPos = cvs_process->argc;
         
         do{
            commandSpliPos = cvsCommandPos + (commandSpliPos - cvsCommandPos) / 2;

            // Can't be shorter than anticipated CVS command position
            if( commandSpliPos <= cvsCommandPos )
            {
               commandSpliPos = cvsCommandPos;
               break;
            }
         }while( calc_command_len(commandSpliPos, cvs_process->args) > startupInfo->commandLineLimit );
         
         // Build the command file part
         command = build_command(cvs_process->argc - commandSpliPos, cvs_process->args + commandSpliPos);
         if( command == 0L )
         {
            startupInfo->errorInfo = noMemory;
            goto error;
         }
         
         if( FILE* cmdFile = fopen(commandFileName, "wt") )
         {
            // Open the temp file and write the command to it
            fputs(command, cmdFile);
            fclose(cmdFile);
         }
         else
         {
            startupInfo->errorInfo = commandFileError;
            goto error;
         }
         
         SafeFree(command);
         
         // Build the remaining command
         command = build_command(commandSpliPos, cvs_process->args);
         if( command == 0L )
         {
            startupInfo->errorInfo = noMemory;
            goto error;
         }
      }
      else
      {
         startupInfo->errorInfo = commandTmpFileError;
         goto error;
      }
   }
   
   if( !command )
   {
      command = build_command(cvs_process->argc, cvs_process->args);
      if( command == 0L )
      {
         startupInfo->errorInfo = noMemory;
         goto error;
      }
   }

   // Redirect Console StdHandles and set the start options
   si.cb = sizeof(STARTUPINFO);
   si.dwFlags = startupInfo->hasTty ? 0 : STARTF_USESTDHANDLES;
   si.hStdInput = stdChildIn;
   si.hStdOutput = stdChildOut;
   si.hStdError = stdChildErr;
   
   if( !startupInfo->hasTty )
   {
      si.dwFlags |= STARTF_USESHOWWINDOW;
      si.wShowWindow = startupInfo->showWindowState;
   
      stdoldIn  = GetStdHandle(STD_INPUT_HANDLE);
      stdoldOut = GetStdHandle(STD_OUTPUT_HANDLE);
      stdoldErr = GetStdHandle(STD_ERROR_HANDLE);

      SetStdHandle(STD_INPUT_HANDLE,  stdChildIn);
      SetStdHandle(STD_OUTPUT_HANDLE, stdChildOut);
      SetStdHandle(STD_ERROR_HANDLE,  stdChildErr);
   }
   
   // Create Child Process
   resCreate = CreateProcessA(
      NULL,
      command,
      NULL,
      NULL,
      TRUE,
      NORMAL_PRIORITY_CLASS | CREATE_SUSPENDED,
      NULL,
      startupInfo->currentDirectory && strlen(startupInfo->currentDirectory) ? startupInfo->currentDirectory : NULL,
      &si,
      &pi);
   
   if( !resCreate )
   {
      // Set the error now before it gets overrode
      startupInfo->lastErrorValue = GetLastError();
      startupInfo->errorInfo = systemError;
   }

   if( !startupInfo->hasTty )
   {
      SetStdHandle(STD_INPUT_HANDLE,  stdoldIn);
      SetStdHandle(STD_OUTPUT_HANDLE, stdoldOut);
      SetStdHandle(STD_ERROR_HANDLE,  stdoldErr);
   }
   
   if( !resCreate )
      goto error;
   
   cvs_process->threadID = pi.dwProcessId;
   cvs_process->pid = pi.hProcess;
   
   // Create a stop process event
   cvs_process->stopProcessEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
   if( !cvs_process->stopProcessEvent )
   {
      goto error;
   }
   
   cvs_process->startThreadsEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
   if( !cvs_process->startThreadsEvent )
   {
      goto error;
   }
   
   // Now open-up threads which will serve the pipes
   cnt = 0;
   threadsFunc[cnt++] = (LPTHREAD_START_ROUTINE)ServeProtocol;
   threadsFunc[cnt++] = (LPTHREAD_START_ROUTINE)MonitorChld;
   
   if( !startupInfo->hasTty )
   {
      threadsFunc[cnt++] = (LPTHREAD_START_ROUTINE)GetChldOutput;
      threadsFunc[cnt++] = (LPTHREAD_START_ROUTINE)GetChldError;
   }
   
   for(i = 0; i < cnt; i++)
   {
      if( (cvs_process->threads[i] = ::CreateThread(
         (LPSECURITY_ATTRIBUTES)NULL,              // No security attributes
         (DWORD)0,                              // Use same stack size
         (LPTHREAD_START_ROUTINE)threadsFunc[i],         // Thread procedure
         (LPVOID)cvs_process,                   // Parameter to pass
         (DWORD)0,                              // Run immediately
         (LPDWORD)&cvs_process->threadsID[i])) == NULL )
      {
         goto error;
      }
   }
   
   // Close unused Handles
   SafeCloseHandle(cvs_process->his_read);
   SafeCloseHandle(cvs_process->his_write);

   if( !startupInfo->hasTty )
   {
      SafeCloseHandle(stdChildIn);
      SafeCloseHandle(stdChildOut);
      SafeCloseHandle(stdChildErr);
   }
   
   // Resume process execution
   ResumeThread(pi.hThread);

   SafeCloseHandle(pi.hThread);
   SafeFree(command);
   
   goto goodboy;
   
error:
   if( startupInfo->errorInfo == success )
   {
      startupInfo->lastErrorValue = GetLastError();
      startupInfo->errorInfo = systemError;
   }

   // Signal the protocol threads to stop
   SetEvent(cvs_process->stopProcessEvent);

   if( cvs_process->pid )
   {
      TerminateProcess(cvs_process->pid, 0);
   }

   SafeCloseHandle(dupIn);
   SafeCloseHandle(dupOut);

   SafeCloseHandle(stdChildIn);
   SafeCloseHandle(stdChildOut);
   SafeCloseHandle(stdChildErr);

   SafeCloseHandle(stddupIn);
   SafeCloseHandle(stddupOut);
   SafeCloseHandle(stddupErr);

   cvs_process_destroy(cvs_process);
   
   SafeFree(command);
   
   return 0L;

goodboy:
#endif

   {
      CStackThreadLock locker;

      open_cvs_process.push_back(cvs_process);
   }

   cvs_process->open = TRUE;

#ifdef WIN32
   // Signal the protocol threads to start
   SetEvent(cvs_process->startThreadsEvent);
#endif

   return cvs_process;
}

/*!
   Close CVS process
   \param cvs_process CVS process to be closed
   \param kill_it Flag indicating whether to kill the process
*/
static void cvs_process_close(CvsProcess* cvs_process, int kill_it)
{
   CDestroyThreadLock locker;

   if( cvs_process && cvs_process->open )
   {
#ifdef WIN32

      if( kill_it && cvs_process->pid )
      {
         cvs_process_terminate(cvs_process);
      }

      // Wait so the threads do not continue to use the process data
      WaitForProtocolThreads(cvs_process, INFINITE);
#endif

      cvs_process->open = FALSE;

#ifndef WIN32
      int status;

      /* If necessary, kill the filter. */
      if( kill_it && cvs_process->pid )
         status = kill(cvs_process->pid, SIGKILL);

      /* Wait for the process to exit. This will happen
       * immediately if it was just killed.
       */
      if( cvs_process->pid )
         waitpid(cvs_process->pid, &status, 0);
#endif

      wire_clear_error();

      {
         CStackThreadLock locker;

         std::vector<CvsProcess*>::iterator i = std::find(open_cvs_process.begin(), open_cvs_process.end(), cvs_process);
         if( i != open_cvs_process.end() )
            open_cvs_process.erase(i);
      }
   }
}

/*!
   Tells if the process is still alive
   \param cvs_process CVS process to be tested
   \return Zero if process not active, non-zero otherwise
*/
int cvs_process_is_active(const CvsProcess* cvs_process)
{
   int res = 0;

   {
      CStackThreadLock locker;

      std::vector<CvsProcess*>::iterator i = std::find(open_cvs_process.begin(), open_cvs_process.end(), cvs_process);
      res = i != open_cvs_process.end() ? 1 : 0;
   }

   return res;
}

/*!
   Close a process. This kills the process and releases its resources
   \param cvs_process CVS process to be killed
*/
void cvs_process_kill(CvsProcess* cvs_process)
{
   if( cvs_process_is_active(cvs_process) )
   {
#ifdef TARGET_RT_MAC_MACHO
      cvs_process_close(cvs_process, TRUE);
#else
      cvs_process_destroy(cvs_process);
#endif
   }
}

/*!
   Stop the process
   \param cvs_process CVS process to be stopped
*/
void cvs_process_stop(CvsProcess* cvs_process)
{
#ifdef WIN32
   if( cvs_process )
   {
      // On windows we need to kill process "from within" to avoid race condition with MonitorChld thread
      SetEvent(cvs_process->stopProcessEvent);
   }
#else
   cvs_process_kill(cvs_process);
#endif
}

/*!
   Called by the application to answer calls from the process
   \return Non-zero if a message from cvs was handled, zero otherwise
*/
int cvs_process_give_time(void)
{
#ifndef WIN32

   fd_set rset;
   int ready;
   int maxfd = 0;
   int fd;
   struct timeval tv;
   int didone = 0;
   
   FD_ZERO(&rset);
   
   std::vector<CvsProcess*>::iterator i;
   
   for(i = open_cvs_process.begin(); i != open_cvs_process.end(); ++i)
   {
      fd = (*i)->my_read;
      
      FD_SET(fd, &rset);
      if( fd > maxfd )
         maxfd = fd;
      
      fd = (*i)->pstdout;
      if( fd ) 
      {
         FD_SET(fd, &rset);
         if( fd > maxfd )
            maxfd = fd;
      }
   
      fd = (*i)->pstderr;
      if( fd ) 
      {
         FD_SET(fd, &rset);
         if( fd > maxfd )
            maxfd = fd;
      }
   }
   tv.tv_sec = 0;
   tv.tv_usec = 10000; // was 100000, but this blocks the interactive dialogs e.g. password dialog
   ready = select(maxfd + 1, &rset, 0, 0, &tv);

   std::vector<CvsProcess*> toFire;
   if( ready > 0 )
   {
      for(i = open_cvs_process.begin(); i != open_cvs_process.end(); ++i)
      {
         fd = (*i)->my_read;
         
         if( FD_ISSET(fd, &rset) )
            toFire.push_back(*i);
         else 
         {
            int stdout_fd, stderr_fd;
            
            stdout_fd = (*i)->pstdout;
            stderr_fd = (*i)->pstderr;
            
            if( (stdout_fd != 0 && FD_ISSET(stdout_fd, &rset)) || (stderr_fd != 0 && FD_ISSET(stderr_fd, &rset)) )
               toFire.push_back(*i);
         }
      }
   }

   for(i = toFire.begin(); i != toFire.end(); ++i)
   {
      fd = (*i)->pstdout;
      if( fd != 0 && FD_ISSET(fd, &rset) )
      {
         cvs_process_handle_stdio(*i, false);
      }
      
      fd = (*i)->pstderr;
      if( fd != 0 && FD_ISSET(fd, &rset) )
      {
         cvs_process_handle_stdio(*i, true);
      }
      
      fd = (*i)->my_read;
      if( FD_ISSET(fd, &rset) )
      {
            gulong rcount;

            cvs_process_recv_message(*i, &rcount); // rcount ignored
         didone = 1;
      }
   }
   
   return didone;
#else
   
   // We don't have to do yield since we use preemptive threads
   ::Sleep(1);
   return 0;
#endif
}

/*!
   Receive messages
   \param cvs_process CVS process
   \param rcount Received data count
*/
static void cvs_process_recv_message(CvsProcess* cvs_process, gulong* rcount)
{
   WireMessage msg = { 0 };

   *rcount = 0;

   cvs_process_push(cvs_process);

   if( !wire_read_msg(cvs_process->my_read, &msg, rcount) )
   {
#ifndef WIN32
      cvs_process_close(cvs_process, TRUE);
#else
      cvs_process_stop(cvs_process);
#endif
   }
   else
   {
      cvs_process_handle_message(&msg);
      wire_destroy(&msg);
   }

   if( cvs_process_is_active(current_cvs_process) )
   {
      if( !current_cvs_process->open )
      {
#if !defined(WIN32) && !defined(TARGET_RT_MAC_MACHO)
         cvs_process_destroy(current_cvs_process);
#endif
      }
      else
         cvs_process_pop();
   }
}

/*!
   Handle message
   \param msg Message to be handled
*/
static void cvs_process_handle_message(WireMessage* msg)
{
   switch(msg->type)
   {
   case GP_QUIT:
      {
         GPT_QUIT* t = (GPT_QUIT*)msg->data;
         current_cvs_process->callbacks->exit(t->code, current_cvs_process);
         current_cvs_process->gotExitCode = TRUE;
#ifndef WIN32
         cvs_process_close(current_cvs_process, FALSE);
#endif
         break;
      }
   case GP_GETENV:
      {
         GPT_GETENV* t = (GPT_GETENV*)msg->data;
         cvs_process_push(current_cvs_process);
         gp_getenv_write(current_cvs_process->my_write, current_cvs_process->callbacks->getenv(t->str, current_cvs_process));
         cvs_process_pop();
         break;
      }
   case GP_CONSOLE:
      {
         GPT_CONSOLE* t = (GPT_CONSOLE*)msg->data;
         if( t->isStderr )
            current_cvs_process->callbacks->consoleerr(t->str, t->len, current_cvs_process);
         else
            current_cvs_process->callbacks->consoleout(t->str, t->len, current_cvs_process);
         
         break;
      }
   }
}

/*!
   Write data to the CVS process
   \param fd Pipe
   \param buf Buffer
   \param count Buffer size
   \return TRUE if written, FALSE otherwise
*/
static int cvs_process_write(pipe_t fd, guint8* buf, gulong  count)
{
   gulong bytes;

   if( current_write_buffer == 0 )
      current_write_buffer = process_write_buffer;

   while( count > 0 )
   {
      if( (current_write_buffer_index + count) >= CP_WRITE_BUFFER_SIZE )
      {
         bytes = CP_WRITE_BUFFER_SIZE - current_write_buffer_index;
         memcpy(&current_write_buffer[current_write_buffer_index], buf, bytes);
         current_write_buffer_index += bytes;
         
         if( !wire_flush(fd) )
            return FALSE;
      }
      else
      {
         bytes = count;
         memcpy(&current_write_buffer[current_write_buffer_index], buf, bytes);
         current_write_buffer_index += bytes;
      }

      buf += bytes;
      count -= bytes;
   }

   return TRUE;
}

/*!
   Flush cvs process data
   \param fd Pipe
   \return TRUE if succesfull, FALSE otherwise
*/
static int cvs_process_flush(pipe_t fd)
{

   if( current_write_buffer_index > 0 )
   {
      int count = 0;

      while( count != current_write_buffer_index )
      {
         int bytes;
         do
         {
            bytes = write(fd, &current_write_buffer[count], (current_write_buffer_index - count));
         }while( (bytes == -1) && (errno == EAGAIN) );

         if( bytes == -1 )
            return FALSE;

         count += bytes;
      }

      current_write_buffer_index = 0;
   }

   return TRUE;
}

/*!
   Puts CVS process to the stack
   \param cvs_process CVS process to be put
*/
static void cvs_process_push(CvsProcess* cvs_process)
{
   CStackThreadLock locker;

   if( cvs_process )
   {
      current_cvs_process = cvs_process;

      cvs_process_stack.push_back(current_cvs_process);

      current_write_buffer_index = current_cvs_process->write_buffer_index;
      current_write_buffer = current_cvs_process->write_buffer;
   }
   else
   {
      current_write_buffer_index = 0;
      current_write_buffer = NULL;
   }
}

/*!
   Take CVS process from the stack
*/
static void cvs_process_pop()
{
   CStackThreadLock locker;

   if( current_cvs_process )
   {
      current_cvs_process->write_buffer_index = current_write_buffer_index;

      cvs_process_stack.pop_back();
   }

   if( !cvs_process_stack.empty() )
   {
      current_cvs_process = cvs_process_stack.back();
      current_write_buffer_index = current_cvs_process->write_buffer_index;
      current_write_buffer = current_cvs_process->write_buffer;
   }
   else
   {
      current_cvs_process = NULL;
      current_write_buffer_index = 0;
      current_write_buffer = NULL;
   }
}
#ifndef WIN32
/*!
   Output data written to [redirected] stdout / stderr on the child process.
   \param cvs_process CVS process to be put
   \param isStderr true for stderr, false for stdout

*/
static void cvs_process_handle_stdio(CvsProcess* cvs_process, bool isStderr)
{
   char buf[512];
   int fd = isStderr ? cvs_process->pstderr : cvs_process->pstdout;
   int bytes;
   
   if( fd )
   {
      for(;;)
      {
         bytes = read(fd, buf, sizeof(buf));
         if( bytes <= 0 )
            break;

         if( isStderr )
            cvs_process->callbacks->consoleerr(buf, bytes, cvs_process);
         else
            cvs_process->callbacks->consoleout(buf, bytes, cvs_process);
      }
   }
}
#endif
