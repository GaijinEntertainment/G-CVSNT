/*
	CVSNT mdns helpers - minimdns
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <config.h>
#include <cvsapi.h>
#include "mdns_mini.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define sock_errno WSAGetLastError()
#else
#define SOCKET int
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define sock_errno errno
#endif

void CMdnsHelperMini::_browse_srv_func(const char *name, uint16_t port, const char *target, void *userdata)
{
	((CMdnsHelperMini*)userdata)->browse_srv_func(name,port,target);
}

void CMdnsHelperMini::browse_srv_func(const char *name, uint16_t port, const char *target)
{
	m_callbacks->srv_fn(name,port,target,m_userdata);
}

void CMdnsHelperMini::_browse_txt_func(const char *name, const char *txt, void *userdata)
{
	((CMdnsHelperMini*)userdata)->browse_txt_func(name,txt);
}

void CMdnsHelperMini::browse_txt_func(const char *name, const char *txt)
{
	m_callbacks->txt_fn(name,txt,m_userdata);
}

void CMdnsHelperMini::_browse_ipv4_func(const char *name, const ipv4_address_t *ipv4, void *userdata)
{
	((CMdnsHelperMini*)userdata)->browse_ipv4_func(name,ipv4);
}

void CMdnsHelperMini::browse_ipv4_func(const char *name, const ipv4_address_t *ipv4)
{
	m_callbacks->ipv4_fn(name,ipv4->address,m_userdata);
}

void CMdnsHelperMini::_browse_ipv6_func(const char *name, const ipv6_address_t *ipv6, void *userdata)
{
	((CMdnsHelperMini*)userdata)->browse_ipv6_func(name,ipv6);
}

void CMdnsHelperMini::browse_ipv6_func(const char *name, const ipv6_address_t *ipv6)
{
	m_callbacks->ipv6_fn(name,ipv6->address,m_userdata);
}

CMdnsHelperMini::CMdnsHelperMini()
{
	m_handle = NULL;
}

CMdnsHelperMini::~CMdnsHelperMini()
{
	close();
}

int CMdnsHelperMini::open()
{
	m_handle = mdns_open();
	return m_handle?0:-1;
}

int CMdnsHelperMini::publish(const char *instance, const char *service, const char *location, int port, const char *text)
{
	char host[1024];

	char tmp[256];
	strncpy(tmp,service,sizeof(tmp));
	char *p=tmp+strlen(tmp)-1;
	if(strlen(tmp)>0 && *p=='.') *(p--)='\0';
	if(strlen(tmp)>6 && !strcmp(p-5,".local")) *(p-5)='\0';

	service = tmp;

	int ret;
	mdns_service_item_t *serv = new mdns_service_item_t;
	serv->Instance = strdup(instance);
	serv->Service = strdup(service);
	serv->Port = port;
	serv->Location = NULL;
	serv->ipv4 = NULL;
	serv->ipv6 = NULL;

	if(gethostname(host,sizeof(host)))
		strcpy(host,"unknown");
	p=strchr(host,'.');
	if(p) *p='\0';

	strcat(host,".local");

	serv->Location = strdup(host);

	if(!location)
	{
		if(gethostname(host,sizeof(host)))
			strcpy(host,"unknown");
		location = host;
	}

	addrinfo hint = {0}, *addr = NULL, *ai;

	int err=getaddrinfo(cvs::idn(location),NULL,&hint,&addr);

	// This is unsatisfactory because it relies on hostname->ip returns active ip addresses for
	// both ipv4 and ipv6.. which is dependent on user network configuration being exactly correct.
	//
	// On Win32 you can call GetAdaptersAddresses, but that's XP+ only, and on Unix you can call
	// getifaddrs but that's not available on all platforms.  To look at for evs probably.. too messy
	// to work on for cvsnt.  A call in CSocketIo to do this would do nicely.

	if(err)
	{
#ifdef EAI_SYSTEM
		if(err==EAI_SYSTEM) err=sock_errno;
#endif
		CServerIo::trace(3,"Unable to resolve host %s: %s",PATCH_NULL(location),gai_strerror(err));
		return false;
	}

	for(ai = addr; ai; ai=ai->ai_next)
	{
		if(ai->ai_family==PF_INET6 && !serv->ipv6)
		{
			sockaddr_in6 *sin = (sockaddr_in6*)ai->ai_addr;
			// On Win32 this works... endinanness?  Not sure what getaddrinfo is returning here.
			if(!memcmp(&sin->sin6_addr,&in6addr_loopback,sizeof(in6addr_loopback)))
				continue;
			serv->ipv6 = new ipv6_address_t;
			memcpy(serv->ipv6->address,&sin->sin6_addr,sizeof(serv->ipv6->address));
		}
		if(ai->ai_family==PF_INET && !serv->ipv4)
		{
			sockaddr_in *sin = (sockaddr_in*)ai->ai_addr;
			int type = ntohl(sin->sin_addr.s_addr)>>24;
			if(type!=127 && type!=255)
			{
				serv->ipv4 = new ipv4_address_t;
				memcpy(serv->ipv4->address,&sin->sin_addr,sizeof(serv->ipv4->address));
			}
		}
	}
	freeaddrinfo(addr);

	if(!serv->ipv4 && !serv->ipv6)
	{
		printf("No valid ipv4/ipv6 address found for host\n");
		if(serv->Instance) free((void*)serv->Instance);
		if(serv->Service) free((void*)serv->Service);
		if(serv->Location) free((void*)serv->Location);
		delete serv;
		return -1;
	}

	if(!serv->ipv4) printf("No valid ipv4 address found for host\n");
	if(!serv->ipv6) printf("No valid ipv6 address found for host\n");

	if(!(ret=mdns_add_service(m_handle,serv)))
	{
		m_services.push_back(serv);
	}
	else
	{
		if(serv->Instance) free((void*)serv->Instance);
		if(serv->Service) free((void*)serv->Service);
		if(serv->Location) free((void*)serv->Location);
		delete serv->ipv4;
		delete serv->ipv6;
		delete serv;
	}
	return ret;
}

int CMdnsHelperMini::step()
{
	return mdns_server_step(m_handle);
}

int CMdnsHelperMini::browse(const char *service, MdnsBrowseCallback *callbacks, void *userdata)
{
	struct mdns_callback serv_fn = 
	{
		NULL,/*name_func*/
		_browse_srv_func,
		_browse_txt_func,
		_browse_ipv4_func,
		_browse_ipv6_func
	};

	if(!callbacks->ipv4_fn)
		serv_fn.ipv4_func = NULL;
	if(!callbacks->ipv6_fn)
		serv_fn.ipv6_func = NULL;
	if(!callbacks->txt_fn)
		serv_fn.txt_func = NULL;
	if(!callbacks->srv_fn)
		serv_fn.srv_func = NULL;

	m_userdata = userdata;
	m_callbacks = callbacks;

	return mdns_query_services(m_handle,service,&serv_fn,this,0);
}

int CMdnsHelperMini::close()
{
	mdns_close(m_handle);
	m_handle = NULL;
	for(size_t n=0; n<m_services.size(); n++)
	{
		mdns_service_item_t *serv = m_services[n];
		if(serv->Instance) free((void*)serv->Instance);
		if(serv->Service) free((void*)serv->Service);
		if(serv->Location) free((void*)serv->Location);
		delete serv->ipv4;
		delete serv->ipv6;
		delete serv;
	}
	m_services.resize(0);
	return 0;
}

#ifdef _WIN32
  #define MDNS_EXPORT __declspec(dllexport)
#elif defined(HAVE_GCC_VISIBILITY)
  #define MDNS_EXPORT __attribute__ ((visibility("default")))
#else
  #define MDNS_EXPORT 
#endif

extern "C" MDNS_EXPORT CMdnsHelperBase *CreateHelper()
{
	return new CMdnsHelperMini();
}

#undef MDNS_EXPORT
