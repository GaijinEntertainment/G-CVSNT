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

#ifdef _WIN32
// Microsoft braindamage reversal. 
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif

#ifdef HAVE_CONFIG_H
extern "C" {
#include "config.h"
}
#endif

#ifdef HAVE_STRINGS_H
#	include <strings.h>
#endif

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdarg.h> // hpux requirement f/c++ apps

#ifndef WIN32
#	include <sys/wait.h>
#	include <sys/time.h>
#	include <sys/param.h>
#	include <unistd.h>
#endif

#include <stack>
#include <vector>
#include <algorithm>

#include "cvsgui_process.h"
#include "cvsgui_protocol.h"

static int cvs_process_write(pipe_t fd, guint8* buf, gulong count);
static int cvs_process_flush(pipe_t fd);
static void cvs_process_push(CvsProcess* cvs_process);
static void cvs_process_pop(void);
static void cvs_process_recv_message(CvsProcess* p);
static void cvs_process_handle_message(WireMessage* msg);

void cvs_process_kill(CvsProcess* cvs_process);
static void cvs_process_close(CvsProcess* cvs_process, int kill_it);
static void cvs_process_destroy(CvsProcess* cvs_process);
static CvsProcess* cvs_process_new(char* name, int argc, char** argv);

static std::deque<CvsProcess*>cvs_process_stack;		/*!< Process stack container */
static std::vector<CvsProcess*>open_cvs_process;		/*!< Open process container */

static CvsProcess* current_cvs_process = NULL;			/*!< Current CVS process */
static int current_write_buffer_index = 0;				/*!< Current write buffer index */
static char* current_write_buffer = NULL;				/*!< Current write buffer */

static char process_write_buffer[WRITE_BUFFER_SIZE];	/*!< Buffer for writing, only for the process */

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
	static CRITICAL_SECTION m_destroyLock;	/*!< Destroy lock */
	static CRITICAL_SECTION m_stackLock;	/*!< Stack lock */

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

/*!
	Terminate cvs process
	\param cvs_process Cvs process to be terminated
	\return true if process was terminate, false otherwise
	\note See also Q178893 - HOWTO: Terminate an Application "Cleanly" in Win32
*/
bool cvs_process_terminate(CvsProcess* cvs_process)
{
	bool res = FALSE;

	EnumWindows((WNDENUMPROC)PostCloseEnum, (LPARAM)cvs_process->threadID);
	
	if( WaitForSingleObject(cvs_process->pid, 300) == WAIT_OBJECT_0 )
	{
		res = TRUE;
	}
	else
	{
		res = TerminateProcess(cvs_process->pid, 0) != 0;
	}

	return res;
}

/// Critical section used when destroying cvs process
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

/// Critical section used for cvs process stack operations
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

/// Dummy critical section used when destroying cvs process
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

/// Dummy critical section used for cvs process stack operations
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
*/
static void _gp_quit_read(pipe_t fd, WireMessage* msg)
{
	GPT_QUIT* t = (GPT_QUIT*)malloc(sizeof(GPT_QUIT));
	if( t == 0L )
		return;

	if( !wire_read_int32(fd, (guint32*)&t->code, 1) )
		return;

	msg->data = t;
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
	free(msg->data);
}

/*!
	Read the environment message from the pipe
	\param fd Pipe
	\param msg Message
*/
static void _gp_getenv_read(pipe_t fd, WireMessage* msg)
{
	GPT_GETENV* t = (GPT_GETENV*)malloc(sizeof(GPT_GETENV));
	if( t == 0L )
		return;

	if( !wire_read_int8(fd, &t->empty, 1) )
		return;

	if( !wire_read_string(fd, &t->str, 1) )
		return;

	msg->data = t;
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
	free(t->str);
	free(t);
}

/*!
	Read the console message from the pipe
	\param fd Pipe
	\param msg Message
*/
static void _gp_console_read(pipe_t fd, WireMessage* msg)
{
	GPT_CONSOLE* t = (GPT_CONSOLE*)malloc(sizeof(GPT_CONSOLE));
	if( t == 0L )
		return;

	if( !wire_read_int8(fd, &t->isStderr, 1) )
		return;

	if( !wire_read_int32(fd, &t->len, 1) )
		return;

	if( !wire_read_string(fd, &t->str, 1) )
		return;

	msg->data = t;
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
	free(t->str);
	free(t);
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
	GPT_QUIT* t = (GPT_QUIT*)malloc(sizeof(GPT_QUIT));
	
	msg.type = GP_QUIT;
	msg.data = t;

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
	GPT_GETENV* t = (GPT_GETENV*)malloc(sizeof(GPT_GETENV));

	msg.type = GP_GETENV;
	msg.data = t;

	t->empty = env == 0L;
	t->str = strdup(env == 0L ? "" : env);

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
	WireMessage msg;
	char* res;

	memset(&msg, 0, sizeof(WireMessage));
	if( !wire_read_msg(fd, &msg) || msg.type != GP_GETENV )
	{
		fprintf(stderr, "cvsgui protocol error !\n");
		exit(-1);
	}

	GPT_GETENV* t = (GPT_GETENV*)msg.data;
	res = t->empty ? 0L : strdup(t->str);
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
	GPT_CONSOLE* t = (GPT_CONSOLE*)malloc(sizeof(GPT_CONSOLE));

	if( binary )
	{
		// Sending an empty message to indicate the binary stream coming
	    gp_console_write(fd, "", 0, 0, 0);
	}

	msg.type = GP_CONSOLE;
	msg.data = t;

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
	Create new cvs process object and initialize it's data members
	\param name Command
	\param argc Arguments count
	\param argv Arguments
	\return Pointer to the newly allocated object
	\note You must free allocated memory using cvs_process_destroy
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

#ifdef WIN32
	cvs_process->starting = TRUE;
	cvs_process->threadID = 0;
#endif
	
	cvs_process->pid = 0;

	cvs_process->callbacks = 0L;
	cvs_process->argc = argc + 4;
	cvs_process->args = (char**)malloc((cvs_process->argc + 1) * sizeof(char*));
	cvs_process->args[0] = strdup(name);
	cvs_process->args[1] = strdup("-cvsgui");
	cvs_process->args[2] = (char*)malloc(16 * sizeof(char));
	cvs_process->args[3] = (char*)malloc(16 * sizeof(char));
	
	for(int i = 0; i < argc; i++)
	{
		cvs_process->args[4 + i] = strdup(argv[i]);
	}
	
	cvs_process->args[cvs_process->argc] = 0L;
	cvs_process->my_read = 0;
	cvs_process->my_write = 0;
	cvs_process->his_read = 0;
	cvs_process->his_write = 0;
	cvs_process->write_buffer_index = 0;
	cvs_process->pstdin = 0;
	cvs_process->pstdout = 0;
	cvs_process->pstderr = 0;
	cvs_process->appData = 0L;

#ifdef WIN32
	memset(cvs_process->threads, 0, sizeof(cvs_process->threads));
	memset(cvs_process->threadsID, 0, sizeof(cvs_process->threadsID));

	cvs_process->stopProcessEvent = NULL;
#endif

	return cvs_process;
}

/*!
	Destroy cvs process object and free allocated memory
	\param cvs_process Cvs process to be destroyed
*/
static void cvs_process_destroy(CvsProcess* cvs_process)
{
	if( cvs_process )
	{
		cvs_process_close(cvs_process, TRUE);

		if( cvs_process->args != 0L )
		{
			for(int i = 0; i < cvs_process->argc; i++)
			{
				if( cvs_process->args[i] )
				{
					free(cvs_process->args[i]);
					cvs_process->args[i] = 0L;
				}
			}

			free(cvs_process->args);
			cvs_process->args = 0L;
		}

		if( cvs_process == current_cvs_process )
			cvs_process_pop();

		if( !cvs_process->destroy )
		{
			cvs_process->destroy = TRUE;
#ifdef WIN32
			// wait so the threads do not continue to use the pointer
			HANDLE WaitH[4];
			int cnt = 0;
			DWORD curT = GetCurrentThreadId();
			
			for(int i = 0; i < 4; i++)
			{
				if( cvs_process->threads[i] && curT != cvs_process->threadsID[i] )
					WaitH[cnt++] = cvs_process->threads[i];
			}
			
			WaitForMultipleObjects(cnt, WaitH, TRUE, INFINITE);

			// Close the handles
			for(int i = 0; i < 4; i++)
			{
				if( cvs_process->threads[i] && CloseHandle(cvs_process->threads[i]) )
				{
					cvs_process->threads[i] = NULL;
				}
			}

			if( cvs_process->stopProcessEvent )
			{
				CloseHandle(cvs_process->stopProcessEvent);
			}
#endif
			free(cvs_process);
		}
	}
}

#ifdef WIN32
/*!
	Given an array of arguments that one might pass to spawnv 
	construct a command line that one might pass to CreateProcess
	
	\param argc Arguments count
	\param argv Arguments
	\return The built command string
	\note Tries to quote things appropriately
*/
static char* build_command(int argc, char* const* argv)
{
	int len;

	/* Compute the total length the command will have.	*/
	{
		int i;

		len = 0;
		for(i = 0; i < argc; i++)
		{
			char* p;

			len += 2;  /* for the double quotes */

			for(p = argv[i]; *p; p++)
			{
				if( *p == '"' )
					len += 2;
				else
					len++;
			}

			len++;	/* for the space or the '\0'  */
		}
	}

	{
		/* The + 10 is in case len is 0.  */
		char* command = (char*)malloc(len + 10);
		int i;
		char* p;

		if( !command )
		{
			errno = ENOMEM;
			return command;
		}

		p = command;
		*p = '\0';
		/* copy each element of argv to command, putting each command
		in double quotes, and backslashing any quotes that appear
		within an argument.  */
		for(i = 0; i < argc; i++)
		{
			char* a;
			*p++ = '"';
			for(a = argv[i]; *a; a++)
			{
				if( *a == '"' )
					*p++ = '\\', *p++ = '"';
				else
					*p++ = *a;
			}

			*p++ = '"';
			*p++ = ' ';
		}

		if( p > command )
			p[-1] = '\0';

		return command;
	}
}

/// Read file buffer size
#define BUFFSIZE 1024

/// Thread return value to mark successful completion
#define THREAD_EXIT_SUCCESS (1)

/*!
	Worker thread to read the cvs process stdout
	\param process Cvs process
	\return THREAD_EXIT_SUCCESS
*/
static DWORD GetChldOutput(CvsProcess* process)
{
	char buff[BUFFSIZE];
	DWORD dread;
	
	while( ReadFile(process->pstdout, buff, BUFFSIZE-1, &dread, NULL) )
	{
		buff[dread] = '\0';
		process->callbacks->consoleout(buff, dread, process);
	}

	return THREAD_EXIT_SUCCESS;
}

/*!
	Worker thread to read the cvs process stderr
	\param process Cvs process
	\return THREAD_EXIT_SUCCESS
*/
static DWORD GetChldError(CvsProcess* process)
{
	char buff[BUFFSIZE];
	DWORD dread;
	
	while( ReadFile(process->pstderr, buff, BUFFSIZE-1, &dread, NULL) )
	{
		buff[dread] = '\0';
		process->callbacks->consoleerr(buff, dread, process);
	}

	return THREAD_EXIT_SUCCESS;
}

/*!
	Worker thread to monitor the cvs process operation and stop request
	\param process Cvs process
	\return THREAD_EXIT_SUCCESS
*/
static DWORD MonitorChld(CvsProcess* process)
{
	HANDLE waitHandles[2] = { process->pid, process->stopProcessEvent };

	const DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
	if( waitResult == (WAIT_OBJECT_0 + 1) )
	{
		cvs_process_kill(process);
	}
	else
	{
		cvs_process_destroy(process);
	}

	return THREAD_EXIT_SUCCESS;
}

/*!
	Worker thread to read and process pipe communication
	\param process Cvs process
	\return THREAD_EXIT_SUCCESS
*/
static DWORD ServeProtocol(CvsProcess* process)
{
	while( process->starting )
	{
		Sleep(20);
		continue;
	}

	DWORD avail;
	while( cvs_process_is_active(process) && process->open )
	{
		if( !PeekNamedPipe(process->my_read, 0L, 0 , 0L, &avail, 0L) )
			break;

		if( avail == 0 )
		{
			Sleep(20);
			continue;
		}

		cvs_process_recv_message(process);
	}

	return THREAD_EXIT_SUCCESS;
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

		// Killing the cvs process avoids getting stuck in a SIGSTOP
		cvs_process_destroy(sigtt_cvs_process);
		callbacks->consoleerr(SIGTT_ERR, strlen(SIGTT_ERR), sigtt_cvs_process);
	}

	sigtt_cvs_process = NULL;
} 
#endif

/*!
	Start the cvs process
	\param name Command to be started
	\param argc Arguments count
	\param argv Arguments
	\param callbacks <b>stdout</b>/<b>stderr</b> redirection callbacks
	\param startupInfo Additional parameters to control command's execution
	\param appData Application-specific data
	\return Pointer to the new CvsProcess on success, NULL otherwise
*/
CvsProcess* cvs_process_run(const char* name, int argc, char** argv, 
							CvsProcessCallbacks* callbacks, CvsProcessStartupInfo* startupInfo,
							void* appData)
{
	if( !callbacks || !startupInfo )
		return 0L;

	CvsProcess* cvs_process = cvs_process_new(name, argc, argv);
	
	if( !cvs_process || !callbacks || !startupInfo )
		return 0L;

	cvs_process->callbacks = callbacks;

	cvs_process->appData = appData;

#ifndef WIN32

	int my_read[2] = { 0, 0 };
	int my_write[2] = { 0, 0 };

	/* Open two pipes. (Bidirectional communication). */
	if( (pipe(my_read) == -1) || (pipe(my_write) == -1) )
	{
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
	
	/* If we are running non interactively (i.e. gcvs&), it is
	*  possible that the cvs process would require entering a password
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
		/* Execute the filter. The "_exit" call should never
		*  be reached, unless some strange error condition
		*  exists.
		*/
		execvp(cvs_process->args[0], cvs_process->args);
		_exit(1);
	}
	else if( cvs_process->pid == -1 )
	{	
		cvs_process_destroy(cvs_process);
		// fork failed
		sigtt_cvs_process = NULL;
		return 0L;
	}
	
	close(cvs_process->his_read);
	cvs_process->his_read  = -1;
	
	close(cvs_process->his_write);
	cvs_process->his_write = -1;
	
#else
	
	SECURITY_ATTRIBUTES lsa = { 0 };
	STARTUPINFOA si = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	
	HANDLE dupIn = 0L, dupOut = 0L;
	HANDLE stdChildIn = 0L, stdChildOut = 0L, stdChildErr = 0L;
	HANDLE stdoldIn = 0L, stdoldOut = 0L, stdoldErr = 0L;
	HANDLE stddupIn = 0L, stddupOut = 0L, stddupErr = 0L;
	char* command = 0L;
	BOOL resCreate;
	int cnt, i;
	LPTHREAD_START_ROUTINE threadsFunc[4];
	
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
	
	CloseHandle(dupIn);
	dupIn = 0;
	
	if( !DuplicateHandle(GetCurrentProcess(), dupOut,
		GetCurrentProcess(), &cvs_process->my_read, 0, FALSE, DUPLICATE_SAME_ACCESS) )
	{
		goto error;
	}
	
	CloseHandle(dupOut);
	dupOut = 0;
	
	if( !startupInfo->hasTty )
	{
		// redirect stdout, stderr, stdin
		if( !CreatePipe(&stdChildIn, &stddupIn, &lsa, 0) )
			goto error;
		
		if( !CreatePipe(&stddupOut, &stdChildOut, &lsa, 0) )
			goto error;
		
		if( !CreatePipe(&stddupErr, &stdChildErr, &lsa, 0) )
			goto error;
		
		// same thing as above
		if( !DuplicateHandle(GetCurrentProcess(), stddupIn,
			GetCurrentProcess(), &cvs_process->pstdin, 0, FALSE, DUPLICATE_SAME_ACCESS) )
		{
			goto error;
		}
		
		CloseHandle(stddupIn);
		stddupIn = 0;
		
		if( !DuplicateHandle(GetCurrentProcess(), stddupOut,
			GetCurrentProcess(), &cvs_process->pstdout, 0, FALSE, DUPLICATE_SAME_ACCESS) )
		{
			goto error;
		}
		
		CloseHandle(stddupOut);
		stddupOut = 0;
		
		if( !DuplicateHandle(GetCurrentProcess(), stddupErr,
			GetCurrentProcess(), &cvs_process->pstderr, 0, FALSE, DUPLICATE_SAME_ACCESS) )
		{
			goto error;
		}
		
		CloseHandle(stddupErr);
		stddupErr = 0;
	}
	
	// Build the arguments for cvs
	sprintf(cvs_process->args[2], "%d", (int)cvs_process->his_read);
	sprintf(cvs_process->args[3], "%d", (int)cvs_process->his_write);
	
	command = build_command(cvs_process->argc, cvs_process->args);
	if( command == 0L )
		goto error;
	
	// Redirect Console StdHandles and set the start options
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = startupInfo->hasTty ? 0 : STARTF_USESTDHANDLES;
	si.hStdInput = stdChildIn;
	si.hStdOutput = stdChildOut;
	si.hStdError = stdChildErr;
	
	if( !startupInfo->hasTty )
	{
		si.dwFlags |= STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_SHOWMINNOACTIVE;
	
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
	
	cnt = 0;
	
	// Create a stop process event
	cvs_process->stopProcessEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	if( !cvs_process->stopProcessEvent )
	{
		goto error;
	}
	
	// Now open-up threads which will serve the pipes
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
			(LPSECURITY_ATTRIBUTES)NULL,					// No security attributes.
			(DWORD)0,										// Use same stack size.
			(LPTHREAD_START_ROUTINE)threadsFunc[i],			// Thread procedure.
			(LPVOID)cvs_process,							// Parameter to pass.
			(DWORD)0,										// Run immediately.
			(LPDWORD)&cvs_process->threadsID[i])) == NULL )
		{
			TerminateProcess(cvs_process->pid, 0);
			goto error;
		}
	}
	
	// Close unnecessary Handles
	CloseHandle(cvs_process->his_read);
	cvs_process->his_read = 0;

	CloseHandle(cvs_process->his_write);
	cvs_process->his_write = 0;
	
	if( !startupInfo->hasTty )
	{
		CloseHandle(stdChildIn);
		CloseHandle(stdChildOut);
		CloseHandle(stdChildErr);
	}
	
	// Resume process execution
	ResumeThread(pi.hThread);
	CloseHandle(pi.hThread);

	// Small tactical delay to reduce problems with process termination
	Sleep(100);

	free(command);
	
	goto goodboy;
	
error:
	if( cvs_process->his_read != 0L )
		CloseHandle(cvs_process->his_read);

	if( cvs_process->his_write != 0L )
		CloseHandle(cvs_process->his_write);

	if( dupIn != 0L )
		CloseHandle(dupIn);

	if( dupOut != 0L )
		CloseHandle(dupOut);

	if( cvs_process->my_read != 0L )
		CloseHandle(cvs_process->my_read);

	if( cvs_process->my_write != 0L )
		CloseHandle(cvs_process->my_write);

	if( command != 0L )
		free(command);

	if( stdChildIn != 0L )
		CloseHandle(stdChildIn);

	if( stdChildOut != 0L )
		CloseHandle(stdChildOut);

	if( stdChildErr != 0L )
		CloseHandle(stdChildErr);

	if( stddupIn != 0L )
		CloseHandle(stddupIn);

	if( stddupOut != 0L )
		CloseHandle(stddupOut);

	if( stddupErr != 0L )
		CloseHandle(stddupErr);

	if( cvs_process->pstdin != 0L )
		CloseHandle(cvs_process->pstdin);

	if( cvs_process->pstdout != 0L )
		CloseHandle(cvs_process->pstdout);

	if( cvs_process->pstderr != 0L )
		CloseHandle(cvs_process->pstderr);
	
	cvs_process_destroy(cvs_process);
	return 0L;
	
goodboy:
#endif

	{
		CStackThreadLock locker;

		open_cvs_process.push_back(cvs_process);
	}

	cvs_process->open = TRUE;

#ifdef WIN32
	cvs_process->starting = FALSE;
#endif

	return cvs_process;
}

/*!
	Close cvs project
	\param cvs_process Cvs process to be closed
	\param kill_it Flag indicating whether to kill the process
*/
static void cvs_process_close(CvsProcess* cvs_process, int kill_it)
{
	CDestroyThreadLock locker;

	if( cvs_process && cvs_process->open )
	{
#ifdef WIN32

		if( GetCurrentThreadId() != cvs_process->threadsID[0] )
		{
			// We wait for all the protocol to be over before we close the process and the pipe.
			WaitForSingleObject(cvs_process->threads[0], 500);
			cvs_process->open = FALSE;
			WaitForSingleObject(cvs_process->threads[0], 500);
		}
#endif

		cvs_process->open = FALSE;

#ifndef WIN32
		int status;

		/* If necessary, kill the filter. */
		if( kill_it && cvs_process->pid )
			status = kill(cvs_process->pid, SIGKILL);

		/* Wait for the process to exit. This will happen
		 *	immediately if it was just killed.
		 */
		if( cvs_process->pid )
			waitpid(cvs_process->pid, &status, 0);

		/* Close the pipes. */
		if( cvs_process->my_read )
			close(cvs_process->my_read);

		if( cvs_process->my_write )
			close(cvs_process->my_write);

		if( cvs_process->his_read )
			close(cvs_process->his_read);

		if( cvs_process->his_write )
			close(cvs_process->his_write);
#else 
		cvs_process->starting = TRUE;

		if( kill_it && cvs_process->pid )
		{
			cvs_process_terminate(cvs_process);
		}

		if( cvs_process->pid )
			CloseHandle(cvs_process->pid);

		if( cvs_process->my_read )
			CloseHandle(cvs_process->my_read);

		if( cvs_process->my_write )
			CloseHandle(cvs_process->my_write);

		if( cvs_process->his_read )
			CloseHandle(cvs_process->his_read);

		if( cvs_process->his_write )
			CloseHandle(cvs_process->his_write);

		if( cvs_process->pstdin )
			CloseHandle(cvs_process->pstdin);

		if( cvs_process->pstdout )
			CloseHandle(cvs_process->pstdout);

		if( cvs_process->pstderr )
			CloseHandle(cvs_process->pstderr);
#endif

		wire_clear_error();

		/* Set the fields to null values */
		cvs_process->pid = 0;
		cvs_process->my_read = 0;
		cvs_process->my_write = 0;
		cvs_process->his_read = 0;
		cvs_process->his_write = 0;
		cvs_process->pstdin = 0;
		cvs_process->pstdout = 0;
		cvs_process->pstderr = 0;

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
	\param cvs_process Cvs process to be tested
	\return Zero if process not active, non-zero otherwise
*/
int cvs_process_is_active(const CvsProcess* cvs_process)
{
	int res;

	{
		CStackThreadLock locker;

		std::vector<CvsProcess*>::iterator i = std::find(open_cvs_process.begin(), open_cvs_process.end(), cvs_process);
		res = i != open_cvs_process.end() ? 1 : 0;
	}

	return res;
}

/*!
	Close a process. This kills the process and releases its resources
	\param cvs_process Cvs process to be killed
*/
void cvs_process_kill(CvsProcess* cvs_process)
{
	if( cvs_process_is_active(cvs_process) )
	{
		cvs_process_destroy(cvs_process);
	}
}

/*!
	Stop the process
	\param cvs_process Cvs process to be stopped
*/
void cvs_process_stop(CvsProcess* cvs_process)
{
#ifdef WIN32
	// On windows we need to kill process "from within" to avoid race condition with MonitorChld thread
	SetEvent(cvs_process->stopProcessEvent);
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
	}
	
	tv.tv_sec = 0;
	tv.tv_usec = 10000; // was 100000, but this blocks the interactive dialogs e.g. password dialog
	ready = select(maxfd + 1, &rset, 0L, 0L, &tv);

	std::vector<CvsProcess*> toFire;
	if( ready > 0 )
	{
		for(i = open_cvs_process.begin(); i != open_cvs_process.end(); ++i)
		{
			fd = (*i)->my_read;
			
			if( FD_ISSET(fd, &rset) )
				toFire.push_back(*i);
		}
	}

	for(i = toFire.begin(); i != toFire.end(); ++i)
	{
		fd = (*i)->my_read;
		
		if( FD_ISSET(fd, &rset) )
		{
			cvs_process_recv_message(*i);
			didone = 1;
		}
	}
	
	return didone;
#else
	
	// We don't have to do yield since we use preemptive threads
	return 0;
#endif
}

/*!
	Receive messages
	\param cvs_process Cvs process
*/
static void cvs_process_recv_message(CvsProcess* cvs_process)
{
	WireMessage msg;

	cvs_process_push(cvs_process);

	memset(&msg, 0, sizeof(WireMessage));

	if( !wire_read_msg(cvs_process->my_read, &msg) )
	{
		cvs_process_close(cvs_process, TRUE);
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
#ifndef WIN32
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
#ifdef WIN32
			// Small delay to work around problems on Win9x (we will have to find the "real" solution in the future though...)
			Sleep(100);
#endif
			current_cvs_process->callbacks->exit(t->code, current_cvs_process);
			cvs_process_close(current_cvs_process, FALSE);
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
	Write data to the cvs process
	\param fd Pipe
	\param buf Buffer
	\param count Buffer size
	\return TRUE if written, FALSE otherwise
*/
static int cvs_process_write(pipe_t fd, guint8* buf, gulong  count)
{
	gulong bytes;

	if( current_write_buffer == 0L )
		current_write_buffer = process_write_buffer;

	while( count > 0 )
	{
		if( (current_write_buffer_index + count) >= WRITE_BUFFER_SIZE )
		{
			bytes = WRITE_BUFFER_SIZE - current_write_buffer_index;
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
	int count;
	int bytes;

	if( current_write_buffer_index > 0 )
	{
		count = 0;
		while( count != current_write_buffer_index )
		{
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
	Puts cvs process to the stack
	\param cvs_process Cvs process to be put
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
	Take cvs process from the stack
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
