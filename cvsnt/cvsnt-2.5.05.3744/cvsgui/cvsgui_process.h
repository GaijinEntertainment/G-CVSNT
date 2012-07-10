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
#define WRITE_BUFFER_SIZE	512

/* C++ has a "bool" type built in. */
#ifndef __cplusplus
typedef unsigned char bool;
#endif

typedef struct _CvsProcess CvsProcess;

/// Structure to hold cvsgui protocol callbacks
typedef struct
{
	long (*consoleout)(const char* txt, long len, const CvsProcess* process);	/*!< Get cvs stdout */
	long (*consoleerr)(const char* txt, long len, const CvsProcess* process);	/*!< Get cvs stderr */
	const char* (*getenv)(const char* name, const CvsProcess* process);		/*!< Ask about environmental variable */
	void (*exit)(int code, const CvsProcess* process);					/*!< Tells the exit code */
} CvsProcessCallbacks;

/// Structure to hold the cvs process info
struct _CvsProcess
{
	unsigned int open : 1;					/*!< Flag to indicate if the process is open */
	unsigned int destroy : 1;				/*!< Flag to indicate whether the process should be destroyed */
#ifdef WIN32
	unsigned int starting : 1;	 			/*!< Flag to indicate whether the process starting or not */
	DWORD threadID;							/*!< Process thread ID */
#endif
	pid_t pid;								/*!< Process process id */

	char** args;							/*!< Process command line arguments */
	int argc;

	pipe_t my_read;							/*!< Application read file descriptor */
	pipe_t my_write;						/*!< Application write file descriptor */

	pipe_t his_read;						/*!< Process read file descriptor */
	pipe_t his_write;						/*!< Process write file descriptor */

	pipe_t pstdin;							/*!< Tty stdin descriptor used by the child */
	pipe_t pstdout;							/*!< Tty stdout descriptor used by the child */
	pipe_t pstderr;							/*!< Tty stderr descriptor used by the child */

	char write_buffer[WRITE_BUFFER_SIZE];	/*!< Buffer for writing */
	int write_buffer_index;					/*!< Buffer index for writing */
	CvsProcessCallbacks* callbacks;			/*!< Jump table */

	void* appData;							/*!< Pointer to any application-specific data */

#ifdef WIN32
	HANDLE threads[4];						/*!< Protocol thread's handles */
	DWORD threadsID[4];						/*!< Protocol thread's IDs */

	HANDLE stopProcessEvent;				/*!< Stop command request event */
#endif
};

/// Structure to hold the cvs startup information
typedef struct _CvsProcessStartupInfo
{
	int hasTty;						/*!< Flag to indicate that the child process has a TTY (console) */
	const char* currentDirectory;	/*!< Working directory for cvs process */

#ifdef __cplusplus
	_CvsProcessStartupInfo()
	{
		hasTty = false;
		currentDirectory = NULL;
	}
#endif

} CvsProcessStartupInfo;


#ifdef __cplusplus
extern "C" {
#endif

CvsProcess* cvs_process_run(char* name, int argc, char** argv, 
							CvsProcessCallbacks* callbacks, CvsProcessStartupInfo* startupInfo,
							void* appData);

void cvs_process_stop(CvsProcess* cvs_process);

int cvs_process_is_active(const CvsProcess* cvs_process);
int cvs_process_give_time(void);

void cvs_process_init(void);

#ifdef __cplusplus
}
#endif

#endif /* CVSGUI_PROCESS_H */
