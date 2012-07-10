/*
	CVSNT Zeroconf support
    Copyright (C) 2005 Tony Hoyle and March-Hare Software Ltd

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
#include "lib/api_system.h"
#include "cvs_string.h"
#include "ServerIO.h"
#include "Zeroconf.h"

#ifdef _WIN32
#define STRICT
#define WIN32_LEAN_AND_MEAN
#define FD_SETSIZE 1024
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define sock_errno WSAGetLastError()
#undef gai_strerror
#define gai_strerror(_e) (const char *)cvs::narrow(gai_strerrorW(_e))
#else
#include <sys/socket.h>
#include <unistd.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#define sock_errno errno
#endif

#include "SocketIO.h"

#include "mdns.h"

#include <map>
#include <vector>

CZeroconf::server_struct_t::server_struct_t()
{
	port=0; 
	addr=NULL;
	bogus = false;
}

CZeroconf::server_struct_t::~server_struct_t()
{
	if(addr) freeaddrinfo(addr); 
}

void CZeroconf::service_srv_func(const char *name, unsigned short port, const char *target, void *userdata)
{
	CServerIo::trace(3,"Zeroconf server response from %s: %hu, %s",name,port,target);
	((CZeroconf*)userdata)->_service_srv_func(name,port,target);
}

void CZeroconf::_service_srv_func(const char *name, unsigned short port, const char *target)
{
	cvs::string nm = name;
	size_t i = nm.find(m_service);
	if(i==cvs::string::npos)
		return;
	nm.resize(i-1);

	server_struct_t s;
	s.port = port;
	s.servicename = nm;
	s.server = target;
	m_servers[name]=s;
}

void CZeroconf::service_txt_func(const char *name, const char *txt, void *userdata)
{
	CServerIo::trace(3,"Zeroconf txt response from %s: %s",name,txt);
	((CZeroconf*)userdata)->_service_txt_func(name,txt);
}

void CZeroconf::_service_txt_func(const char *name, const char *txt)
{
	m_servers[name].text+=txt;
}

void CZeroconf::service_ipv4_func(const char *name, const unsigned char address[4], void *userdata)
{
	CServerIo::trace(3,"Zeroconf ipv4 response from %s: %d.%d.%d.%d",name,address[0],address[1],address[2],address[3]);
	((CZeroconf*)userdata)->_service_ipv4_func(name,address);
}

void CZeroconf::_service_ipv4_func(const char *name, const unsigned char address[4])
{
	memcpy(m_name_lookup[name].ipv4, address,4);
	m_name_lookup[name].ipv4_set = true;
}

void CZeroconf::service_ipv6_func(const char *name, const unsigned char address[16], void *userdata)
{
	CServerIo::trace(3,"Zeroconf ipv6 response from %s: ...",name);
	((CZeroconf*)userdata)->_service_ipv6_func(name,address);
}

void CZeroconf::_service_ipv6_func(const char *name, const unsigned char address[16])
{
	memcpy(m_name_lookup[name].ipv6, address, 16);
	m_name_lookup[name].ipv6_set = true;
}

CZeroconf::CZeroconf(const char *type, const char *library_dir)
{
	m_mdns_type = type;
	m_library_dir = library_dir;
}

CZeroconf::~CZeroconf()
{
}

bool CZeroconf::BrowseForService(const char *service, unsigned flags)
{
	struct MdnsBrowseCallback fn = 
	{
		service_srv_func,
		service_txt_func,
		service_ipv4_func,
		service_ipv6_func
	};

	if(!(flags&zcText))
		fn.txt_fn = NULL;
	if(!(flags&(zcAddress|zcHost)))
	{
		fn.ipv4_fn = NULL;
		fn.ipv6_fn = NULL;
	}

	CSocketIO::init();

	CMdnsHelperBase *mdns = CMdnsHelperBase::CreateHelper(m_mdns_type, m_library_dir);

	if(!mdns)
		return false;

	if(mdns->open())
	{
		delete mdns;
		return false;
	}

	m_servers.clear();
	m_name_lookup.clear();
	m_flags = flags;
	m_service = service;

	mdns->browse(service,&fn,this);
	
	mdns->close();
	delete mdns;

	if(flags&(zcAddress|zcHost))
	{
		for(servers_t::iterator i = m_servers.begin(); i!=m_servers.end(); ++i)
		{
			bool have_address = false;
			if(m_name_lookup.find(i->second.server)!=m_name_lookup.end())
			{
				const name_lookup_struct_t& addr = m_name_lookup[i->second.server];
				addrinfo hints = {0};
				const char *p=strchr(service,'.');
				if(p && !strcmp(p,"._tcp"))
				{
					hints.ai_socktype=SOCK_STREAM;
					hints.ai_protocol=IPPROTO_TCP;
				}
				else if(p && !strcmp(p,"._udp"))
				{
					hints.ai_socktype=SOCK_DGRAM;
					hints.ai_protocol=IPPROTO_UDP;
				}
				hints.ai_flags = AI_NUMERICHOST;

				cvs::string szAddress,szPort;
				cvs::sprintf(szPort,8,"%hu",i->second.port);

				if(addr.ipv6_set)
				{
					cvs::sprintf(szAddress,32,"%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
						ntohs(*(short*)&addr.ipv6[0]), ntohs(*(short*)&addr.ipv6[2]),
						ntohs(*(short*)&addr.ipv6[4]), ntohs(*(short*)&addr.ipv6[6]),
						ntohs(*(short*)&addr.ipv6[8]), ntohs(*(short*)&addr.ipv6[10]),
						ntohs(*(short*)&addr.ipv6[12]), ntohs(*(short*)&addr.ipv6[14]));

					struct addrinfo *addr = NULL;
					if(getaddrinfo(cvs::idn(szAddress.c_str()),szPort.c_str(),&hints,&addr))
					{
						CServerIo::trace(3,"getaddrinfo(%s) failed: %s",szAddress.c_str(),gai_strerror(sock_errno));
					}
					else
					{
						if(flags&zcHost)
						{
							char hoststr[NI_MAXHOST];
							if(getnameinfo(addr->ai_addr,(socklen_t)addr->ai_addrlen,hoststr,sizeof(hoststr),NULL,0,0))
							{
								CServerIo::trace(3,"getnameinfo() failed: %s",gai_strerror(sock_errno));
							}
							else
								i->second.host = hoststr;
						}

						if(flags&zcAddress)
							i->second.addr=addr;
						else
							freeaddrinfo(addr);
						have_address = true;
					}
				}
				if(addr.ipv4_set)
				{
					cvs::sprintf(szAddress,32,"%u.%u.%u.%u",addr.ipv4[0],addr.ipv4[1],addr.ipv4[2],addr.ipv4[3]);
					struct addrinfo *addr = NULL;
					if(getaddrinfo(cvs::idn(szAddress.c_str()),szPort.c_str(),&hints,&addr))
					{
						CServerIo::trace(3,"getaddrinfo(%s) failed: %s",szAddress.c_str(),gai_strerror(sock_errno));
					}
					else
					{
						if(flags&zcHost && !i->second.host.size())
						{
							char hoststr[NI_MAXHOST];
							if(getnameinfo(addr->ai_addr,(socklen_t)addr->ai_addrlen,hoststr,sizeof(hoststr),NULL,0,0))
							{
								CServerIo::trace(3,"getnameinfo() failed: %s",gai_strerror(sock_errno));
							}
							else
								i->second.host = hoststr;
						}

						if(flags&zcAddress)
						{
							addr->ai_next = i->second.addr;
							i->second.addr=addr;
						}
						else
							freeaddrinfo(addr);
						have_address = true;
					}
				}
			}
			if(!have_address)
				i->second.bogus=true;
		}
	}
	m_it = m_servers.begin();
	return true;
}

const CZeroconf::server_struct_t *CZeroconf::EnumServers(bool& first)
{
	if(first)
		m_it = m_servers.begin();

	first = false;

	const server_struct_t *s;
	do
	{
		if(m_it==m_servers.end())
			return NULL;

		s = &(m_it++)->second;
	} while(s->bogus);

	return s;
}

