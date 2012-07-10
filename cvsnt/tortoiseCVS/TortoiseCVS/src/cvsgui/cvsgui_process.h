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
   \file cvsgui_process.h
   \brief CvsGui process code header
   \author Alexandre Parenteau <aubonbeurre@hotmail.com> --- November 1999
   \note To be used by GUI and CVS client
   \note Derived from plugin.h in GIMP
*/

#ifndef CVSGUI_PROCESS_H
#define CVSGUI_PROCESS_H

#include <stdio.h>
#include <sys/types.h>
#include "cvsgui_wire.h"

/// Write buffer size
#define CP_WRITE_BUFFER_SIZE  512

#ifdef WIN32
   /// Protocol threads count
#  define CP_THREADS_COUNT    4  
#endif
/* C++ has a "bool" type built in. */
#ifndef __cplusplus
typedef unsigned char bool;
#endif

typedef struct _CvsProcess CvsProcess;

/// Error information
typedef enum {
   success = 0,      /*!< Success */
   badArguments,     /*!< Invalid call arguments */
   noMemory,         /*!< No memory */
   commandTmpFileError,/*!< Problems with creating command file */
   commandFileError, /*!< Problems with opening command file */
   systemError       /*!< Indicates the system error */
} CvsProcessErrorInfo;
/// Structure to hold cvsgui protocol callbacks
typedef struct
{
   long (*consoleout)(const char* txt, const long len, const CvsProcess* process);  /*!< Get CVS stdout */
   long (*consoleerr)(const char* txt, const long len, const CvsProcess* process);  /*!< Get CVS stderr */
   const char* (*getenv)(const char* name, const CvsProcess* process);           /*!< Ask about environmental variable */
   void (*exit)(const int code, const CvsProcess* process);                /*!< Tells the exit code */
   void (*ondestroy)(const CvsProcess* process);                           /*!< Called before process structure is destroyed and it's memory released */
} CvsProcessCallbacks;

/// Structure to hold the cvs process info
struct _CvsProcess
{
   volatile unsigned int open : 1;        /*!< Flag to indicate if the process is open */
   volatile unsigned int destroy : 1;     /*!< Flag to indicate whether the process should be destroyed */
   volatile unsigned int gotExitCode : 1;    /*!< Flag to indicate whether we have received an exit code */

   pid_t pid;                       /*!< Process process id */

   char** args;                     /*!< Process command line arguments */
   int argc;

   pipe_t my_read;                     /*!< Application read file descriptor */
   pipe_t my_write;                 /*!< Application write file descriptor */

   pipe_t his_read;                 /*!< Process read file descriptor */
   pipe_t his_write;                /*!< Process write file descriptor */

   pipe_t pstdin;                   /*!< Tty stdin descriptor used by the child */
   pipe_t pstdout;                     /*!< Tty stdout descriptor used by the child */
   pipe_t pstderr;                     /*!< Tty stderr descriptor used by the child */

   char write_buffer[CP_WRITE_BUFFER_SIZE];/*!< Buffer for writing */
   int write_buffer_index;             /*!< Buffer index for writing */
   CvsProcessCallbacks* callbacks;        /*!< Jump table */

   void* appData;                   /*!< Pointer to any application-specific data */

#ifdef WIN32
   DWORD threadID;                     /*!< Process thread ID */

   HANDLE threads[CP_THREADS_COUNT];      /*!< Protocol thread's handles */
   DWORD threadsID[CP_THREADS_COUNT];     /*!< Protocol thread's IDs */
   HANDLE startThreadsEvent;           /*!< Stop command request event */

   HANDLE stopProcessEvent;            /*!< Stop command request event */

   const char* commandFileName;        /*!< File name used to overcome the command line length limit, may be NULL */
#endif
};

/// Structure to hold the CVS startup information and error information in case of failure to run CVS process
typedef struct _CvsProcessStartupInfo
{
   int hasTty;                /*!< Flag to indicate that the child process has a TTY (console) */
   const char* currentDirectory; /*!< Working directory for cvs process */

#ifdef WIN32
   WORD showWindowState;         /*!< Initial show state of CVS process window */
   int commandLineLimit;         /*!< Command line limit, zero marks no-limit */
#endif

   CvsProcessErrorInfo errorInfo;   /*!< Error number */

#ifdef WIN32
   DWORD lastErrorValue;         /*!< Stores the result of GetLastError call in case of failure */
#else
   int lastErrnoValue;           /*!< Stores the value of errno in case of failure */
#endif
#ifdef __cplusplus
   _CvsProcessStartupInfo()
   {
      hasTty = false;
      currentDirectory = NULL;

      errorInfo = success;
#ifdef WIN32
      showWindowState = SW_HIDE/*SW_SHOWMINNOACTIVE*/;
      // http://msdn2.microsoft.com/en-us/library/ms682425.aspx:
      // "The maximum length of this string is 32K characters".
      commandLineLimit = 32768;

      lastErrorValue = 0;
#else
      lastErrnoValue = 0;
#endif
   }
#endif

} CvsProcessStartupInfo;


#ifdef __cplusplus
extern "C" {
#endif

CvsProcess* cvs_process_run(const char* name, int argc, char** argv, 
                     CvsProcessCallbacks* callbacks, CvsProcessStartupInfo* startupInfo,
                     void* appData);

void cvs_process_stop(CvsProcess* cvs_process);
#ifndef WIN32
void cvs_process_destroy(CvsProcess* cvs_process);
#endif

int cvs_process_is_active(const CvsProcess* cvs_process);
int cvs_process_give_time(void);

void cvs_process_init(void);

#ifdef __cplusplus
}
#endif

#endif /* CVSGUI_PROCESS_H */
