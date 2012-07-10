/*	cvsnt lockserver
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <config.h>

#ifdef _WIN32
#include <process.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <cvsapi.h>
#include <cvstools.h>

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#include "LockService.h"

bool g_bStop = false;
extern bool g_bTestMode;
#ifdef HAVE_MDNS
bool use_mdns = true;
#endif
static int m_port;
static int m_pserver_port;

#ifdef _WIN32
BOOL NotifySCM(DWORD dwState, DWORD dwWin32ExitCode, DWORD dwProgress);
#endif

#if !defined(_WIN32) && !defined(TRUE)
#undef FALSE
#undef TRUE
enum { FALSE, TRUE };
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
	
	OpenLockClient(s);

	while(s->getline(line))
	{
		if(!ParseLockCommand(s,line.c_str()))
			break;
	}

	CloseLockClient(s);

#ifdef _WIN32
	if(g_bTestMode)
		printf("Terminating thread %08x\n",GetCurrentThreadId());
#endif
#ifndef _WIN32
	return NULL;
#endif
}

#ifdef HAVE_MDNS
static void _mdns_thread_proc(void* param)
{
	int nResp;
	const char *pResp = NULL;
	char szResp[64];
	char serverName[256];

	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","ResponderName",szResp,sizeof(szResp)))
	{
		pResp = szResp;
		if(!strcmp(pResp,"none"))
		{
			if(g_bTestMode)
				printf("mdns disabled\n");
			return;
		}
		if(!strcmp(pResp,"default"))
			pResp = NULL;
	} // Old responder type
  	else if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","ResponderType",nResp))
	{
		switch(nResp)
		{
		case 0:
			if(g_bTestMode)
				printf("mdns disabled\n");
			return;
		case 1:
			pResp = "mini";
			break;
		case 2:
			pResp = "apple";
			break;
		case 3:
			pResp = "howl";
			break;
		default:
			pResp = "mini";
			break;
		}
	}

	if(!pResp)
	{
		if(g_bTestMode)
			printf("Using default mdns Responder\n");
	}
	else
	{
		if(g_bTestMode)
			printf("Using %s mdns Responder\n",pResp);
	}

	if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","ServerName",serverName,sizeof(serverName)))
	{
		if(!gethostname(serverName,sizeof(serverName)))
		{
			char *p=strchr(serverName,'.');
			if(p) *p='\0';
		}
		else
			strcpy(serverName,"unknown");
	}

	CMdnsHelperBase *mdns = CMdnsHelperBase::CreateHelper(pResp,CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDMdns));

	if(mdns && !mdns->open())
	{
		if(g_bTestMode)
			printf("Publishing mdns records for %s._cvspserver._tcp\n",serverName);
		mdns->publish(serverName,"_cvspserver._tcp",NULL,m_pserver_port,NULL);

		while(!g_bStop)
		{
			mdns->step();
#ifdef _WIN32
			Sleep(10);
#else
			usleep(10);
#endif
		}
		mdns->close();
	}
	else
	{
		if(g_bTestMode)
			printf("mdns initialisation failed\n");
	}
	delete mdns;
}

#ifdef _WIN32
static void mdns_thread_proc(void* param)
#else
static void *mdns_thread_proc(void* param)
#endif
{
	_mdns_thread_proc(param);
#ifndef _WIN32
	return NULL;
#endif
}
#endif

static int start_thread(CSocketIO* sock)
{
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

#ifdef HAVE_MDNS
static int start_mdns_thread()
{
#ifdef _WIN32
	_beginthread(mdns_thread_proc,0,NULL);
	return 0;
#elif HAVE_PTHREAD_H
	pthread_t         a_thread = 0;
	pthread_create( &a_thread, NULL, mdns_thread_proc, NULL);	
	pthread_detach( a_thread );
#else
	CServerIo::log(CServerIo::logNotice,"mdns publish not done due to lack of thread support");
	// Don't do mdns
#endif
}
#endif

void run_server(int port, int seq, int local_only)
{
	CSocketIO listen_sock;
    char szLockServer[64];
	int nResp;
	char szResp[64];

	m_port = port;

#ifdef HAVE_MDNS
	use_mdns = true;
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","ResponderName",szResp,sizeof(szResp)))
	{
		if(!strcmp(szResp,"none"))
			use_mdns = false;
	} // Old responder type
    else if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","ResponderType",nResp))
		use_mdns = nResp?true:false;
#endif

	sprintf(szLockServer,"%d",m_port);

	if(g_bTestMode)
	{
		printf("Initialising socket...\n");
	}
#if defined( __HP_aCC ) && defined ( __ia64 )
	if(!listen_sock.create("127.0.0.1",szLockServer,local_only?true:false))
#else
	if(!listen_sock.create(NULL,szLockServer,local_only?true:false))
#endif
	{
		if(g_bTestMode)
		{
			printf("Couldn't create listening socket... %s\n",listen_sock.error());
#if defined( __HP_aCC )
			printf("HP: Try and listen on 127.0.0.1 instead\n");
#endif
		}
#if defined( __HP_aCC ) 
		CServerIo::log(CServerIo::logError,"HP: Try and listen on 127.0.0.1 instead");
		if(!listen_sock.create("127.0.0.1",szLockServer,local_only?true:false))
		{
		printf("Couldn't create listening socket there either... %s\n",listen_sock.error());
#endif
		CServerIo::log(CServerIo::logError,"Failed to create listening socket: %s",listen_sock.error());
		return;
#if defined( __HP_aCC ) 
		}
#endif
	}

#ifdef _WIN32
	if(!g_bTestMode)
		NotifySCM(SERVICE_START_PENDING, 0, seq++);
#endif

	if(g_bTestMode)
			printf("Starting lock server on port %d/tcp...\n",m_port);

	if(!listen_sock.bind())
	{
		if(g_bTestMode)
			printf("Couldn't bind listening socket... %s\n",listen_sock.error());
		CServerIo::log(CServerIo::logError,"Failed to bind listening socket: %s",listen_sock.error());
#ifdef _WIN32
		if(!g_bTestMode)
			NotifySCM(SERVICE_STOPPED,0,1);
#endif
		return;
	}

	g_bStop=FALSE;

	InitServer();

#ifdef _WIN32
	if(!g_bTestMode)
		NotifySCM(SERVICE_START_PENDING, 0, seq++);
#endif

#ifdef HAVE_MDNS
	if(use_mdns)
	{
		m_pserver_port = 2401;
		CGlobalSettings::GetGlobalValue("cvsnt","PServer","PServerPort", m_pserver_port);
		start_mdns_thread();
	}
#endif

	// Process running, wait for closedown
	CServerIo::log(CServerIo::logNotice,"CVS Lock service initialised successfully");
#ifdef _WIN32
	if(!g_bTestMode)
		NotifySCM(SERVICE_RUNNING, 0, 0);
#endif

	while(!g_bStop && listen_sock.accept(15000))
	{
		for(size_t n=0; n<listen_sock.accepted_sockets().size(); n++)
			start_thread(listen_sock.accepted_sockets()[n].Detach());
	}
	g_bStop = true;
	CloseServer();
}
