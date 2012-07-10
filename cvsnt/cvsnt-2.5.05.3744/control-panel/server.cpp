#ifdef _WIN32
// Microsoft braindamage reversal.  
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#endif
#include "config.h"

#ifdef _WIN32
#include <process.h>
#endif
#include <vector>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "../cvsapi/cvsapi.h"

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#include "CvsControl.h"

bool g_bStop = false;
extern bool g_bTestMode;

#ifndef _WIN32
#ifndef FALSE
enum { FALSE, TRUE };
#endif
void ReportError(int error, char *fmt,...)
{
  char buf[512];
  va_list va;

  va_start(va,fmt);
  vsnprintf(buf,sizeof(buf),fmt,va);
  fprintf(stderr,"%s%s\n",error?"Error: ":"",buf);
}
#else
void ReportError(BOOL bError, LPCSTR szError, ...);
BOOL NotifySCM(DWORD dwState, DWORD dwWin32ExitCode, DWORD dwProgress);
#endif

#ifdef _WIN32
static void thread_proc(void* param)
#else
static void *thread_proc(void* param)
#endif
{
	CSocketIOPtr s = (CSocketIO*)param;
	cvs::string line;
	int v=1;

	s->setnodelay(true);
	s->blocking(true);

#ifdef _WIN32
	if(g_bTestMode)
		printf("Starting thread %08x\n",GetCurrentThreadId());
#endif

	while(s->getline(line))
	{
		if(!ParseControlCommand(s,line.c_str()))
			break;
	}
	CloseControlClient(s);

#ifdef _WIN32
	if(g_bTestMode)
		printf("Terminating thread %08x\n",GetCurrentThreadId());
#endif
#ifndef _WIN32
	return NULL;
#endif
}

static int start_thread(CSocketIOPtr s)
{
	CSocketIO* sock = s.Detach();
#ifdef _WIN32
	_beginthread(thread_proc,0,(void*)sock);
	return 0;
#elif HAVE_PTHREAD_H
	pthread_t         a_thread = 0;
	pthread_create( &a_thread, NULL, thread_proc, 
               (void *)sock);	
	pthread_detach( a_thread );
#else
	thread_proc((void*)sock);
#endif
}

void run_server(int port, int seq, int local_only)
{
	CSocketIO listen_sock;
    char szControlServer[64];

	sprintf(szControlServer,"%d",port);

	if(g_bTestMode)
	{
		printf("Initialising socket...\n");
	}
	if(!listen_sock.create(NULL,szControlServer,local_only?true:false))
	{
		if(g_bTestMode)
			printf("Couldn't create listening socket... %s",listen_sock.error());
		ReportError(TRUE,"Failed to create listening socket: %s",listen_sock.error());
		return;
	}

#ifdef _WIN32
	if(!g_bTestMode)
		NotifySCM(SERVICE_START_PENDING, 0, seq++);
#endif

	if(g_bTestMode)
			printf("Starting control server on port %d/tcp...\n",port);

	if(!listen_sock.bind())
	{
		if(g_bTestMode)
			printf("Couldn't bind listening socket... %s\n",listen_sock.error());
		ReportError(TRUE,"Failed to bind listening socket: %s",listen_sock.error());
#ifdef _WIN32
		if(!g_bTestMode)
			NotifySCM(SERVICE_STOPPED,0,1);
#endif
		return;
	}

	g_bStop=FALSE;

//	InitServer();

#ifdef _WIN32
	if(!g_bTestMode)
		NotifySCM(SERVICE_START_PENDING, 0, seq++);
#endif

	// Process running, wait for closedown
	ReportError(FALSE,"CVS Control service initialised successfully");
#ifdef _WIN32
	if(!g_bTestMode)
		NotifySCM(SERVICE_RUNNING, 0, 0);
#endif

	while(!g_bStop && listen_sock.accept(2000))
	{
		for(size_t n=0; n<listen_sock.accepted_sockets().size(); n++)
		{
			OpenControlClient(listen_sock.accepted_sockets()[n]);
			start_thread(listen_sock.accepted_sockets()[n]);
		}
	}
}
