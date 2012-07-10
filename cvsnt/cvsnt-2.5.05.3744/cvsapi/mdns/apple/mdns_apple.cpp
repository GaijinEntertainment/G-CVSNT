/*
	CVSNT mdns helpers - apple
    Copyright (C) 2004 Tony Hoyle and March-Hare Software Ltd

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

#ifdef _WIN32
// Microsoft braindamage reversal.  
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#else
#define SOCKET int
#endif

#include <errno.h>
#include <config.h>
#include <cvsapi.h>
#include "mdns_apple.h"

#ifndef max
#define max(a,b) (((a)>(b))?(a):(b))
#endif

void CMdnsHelperApple::_reg_reply(DNSServiceRef m_client, DNSServiceFlags flags, DNSServiceErrorType errorCode,
	const char *name, const char *regtype, const char *domain, void *context)
{
	((CMdnsHelperApple*)context)->reg_reply(m_client,flags,errorCode,name,regtype,domain);
}

void CMdnsHelperApple::reg_reply(DNSServiceRef m_client, DNSServiceFlags flags, DNSServiceErrorType errorCode,
	const char *name, const char *regtype, const char *domain)
{
	printf("Got a reply for %s.%s%s: ", name, regtype, domain);
	switch (errorCode)
		{
		case kDNSServiceErr_NoError:      printf("Name now registered and active\n"); break;
		case kDNSServiceErr_NameConflict: printf("Name in use, please choose another\n"); break;
		default:                          printf("Error %d\n", errorCode); return;
		}
}

void CMdnsHelperApple::_browse_reply(DNSServiceRef client, DNSServiceFlags flags, uint32_t ifIndex, DNSServiceErrorType errorCode,
	const char *replyName, const char *replyType, const char *replyDomain, void *context)
{
	((CMdnsHelperApple*)context)->browse_reply(client,flags,ifIndex,errorCode,replyName,replyType,replyDomain);
}

struct browse_host_t
{
	unsigned short port;
	cvs::string target;
	cvs::string txt;
	unsigned char ipv4[4];
	unsigned char ipv6[16];
};

void CMdnsHelperApple::browse_reply(DNSServiceRef client, DNSServiceFlags flags, uint32_t ifIndex, DNSServiceErrorType errorCode,
		const char *replyName, const char *replyType, const char *replyDomain)
{
	if(flags & kDNSServiceFlagsAdd)
	{
		DNSServiceRef ref,ref2;
		browse_host_t host;
		timeval timeout;
		fd_set set;
		SOCKET socket,socket2;

		host.port=0;
		DNSServiceResolve(&ref, 0, ifIndex, replyName, replyType, replyDomain, (DNSServiceResolveReply)_resolve_reply, &host);
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		socket = DNSServiceRefSockFD(ref);
		FD_ZERO(&set);
		FD_SET(socket, &set);
		select((int)socket + 1, &set, NULL, NULL, &timeout);
		if(FD_ISSET(socket, &set))
			DNSServiceProcessResult(ref);
		DNSServiceRefDeallocate(ref);

		if(host.port)
		{
			cvs::string fullname;
			cvs::sprintf(fullname,80,"%s.%s%s",replyName,replyType,replyDomain);
			if(fullname.length()>0 && fullname[fullname.length()-1]=='.')
				fullname.resize(fullname.size()-1);
			if(m_callbacks->srv_fn)
			{
				m_callbacks->srv_fn(fullname.c_str(),host.port,host.target.c_str(),m_userdata);
			}
			if(m_callbacks->txt_fn && host.txt.size())
				m_callbacks->txt_fn(fullname.c_str(),host.txt.c_str(),m_userdata);
			if(m_callbacks->ipv4_fn || m_callbacks->ipv6_fn)
			{
				if(m_callbacks->ipv4_fn)
					DNSServiceQueryRecord(&ref, 0, ifIndex, host.target.c_str(), kDNSServiceType_A, kDNSServiceClass_IN, _query_reply, &host);
				if(m_callbacks->ipv6_fn)
					DNSServiceQueryRecord(&ref2, 0, ifIndex, host.target.c_str(), kDNSServiceType_AAAA, kDNSServiceClass_IN, _query_reply, &host);
				timeout.tv_sec = 1;
				timeout.tv_usec = 0;
				if(m_callbacks->ipv4_fn)
					socket = DNSServiceRefSockFD(ref);
				else
					socket = NULL;
				if(m_callbacks->ipv6_fn)
					socket2 = DNSServiceRefSockFD(ref2);
				else
					socket = NULL;
				while(socket || socket2)
				{
					FD_ZERO(&set);
					if(socket)
						FD_SET(socket, &set);
					if(socket2)
						FD_SET(socket2, &set);
					if(select(max((int)socket + 1,(int)socket2+1), &set, NULL, NULL, &timeout)<=0)
						break;
					if(socket && FD_ISSET(socket, &set))
					{
						DNSServiceProcessResult(ref);
						socket = NULL;
						m_callbacks->ipv4_fn(host.target.c_str(), host.ipv4, m_userdata);
					}
					if(socket2 && FD_ISSET(socket2, &set))
					{
						DNSServiceProcessResult(ref2);
						socket2 = NULL;
						m_callbacks->ipv6_fn(host.target.c_str(), host.ipv6, m_userdata);
					}
				}
				if(m_callbacks->ipv4_fn)
					DNSServiceRefDeallocate(ref);
				if(m_callbacks->ipv4_fn)
					DNSServiceRefDeallocate(ref2);
			}
		}
	}
	time(&m_lastreply);
}

void CMdnsHelperApple::_resolve_reply(DNSServiceRef client, DNSServiceFlags flags, uint32_t ifIndex, DNSServiceErrorType errorCode,
	const char *fullname, const char *hosttarget, uint16_t opaqueport, uint16_t txtLen, const unsigned char *txtRecord, void *context)
{
	browse_host_t *host = (browse_host_t*)context;

	host->port = htons(opaqueport);
	host->target = hosttarget;
	host->txt = (const char *)txtRecord;
	if(host->target.length()>0 && host->target[host->target.length()-1]=='.')
		host->target.resize(host->target.size()-1);
}

void CMdnsHelperApple::_query_reply(DNSServiceRef client, DNSServiceFlags flags,
				uint32_t ifIndex, DNSServiceErrorType errorCode, const char *fullname,
				uint16_t rrtype, uint16_t rrclass, uint16_t rdlen, const void *rdata,
				uint32_t ttl, void *context)
{
	browse_host_t *host = (browse_host_t*)context;

	switch(rrtype)
	{
	case kDNSServiceType_A:
		memcpy(host->ipv4,rdata,4);
		break;
	case kDNSServiceType_AAAA:
		memcpy(host->ipv6,rdata,16);
		break;
	}
}

CMdnsHelperApple::CMdnsHelperApple()
{
	m_client = NULL;
}

CMdnsHelperApple::~CMdnsHelperApple()
{
	close();
}

int CMdnsHelperApple::open()
{
	DNSServiceRef ref = NULL;
/*	int err = DNSServiceCreateConnection(&ref);
	if(err)
	{
		CServerIo::error("Couldn't connect to apple mdns service - is responder running?\n");
		return -1;
	}
	if(ref)
		DNSServiceRefDeallocate(ref);
*/
	return 0;
}

int CMdnsHelperApple::publish(const char *instance, const char *service, const char *location, int port, const char *text)
{
	if(text && !*text)
		text = NULL;

	// Sort this out later
	int txtLen = 0;
	void *txtRecord = NULL;

	char tmp[256];
	strncpy(tmp,service,sizeof(tmp));
	char *p=tmp+strlen(tmp)-1;
	if(strlen(tmp)>0 && *p=='.') *(p--)='\0';
	if(strlen(tmp)>6 && !strcmp(p-5,".local")) *(p-5)='\0';

	service = tmp;

	DNSServiceErrorType  err = DNSServiceRegister(&m_client, 0, 0, instance, service, NULL, location, htons(port), txtLen, txtRecord, _reg_reply, this);
	if (!m_client || err != kDNSServiceErr_NoError)
	{
		printf("Unable to register with mDNS responder (%d)\n",err);
		return (-1);
	}
	return 0;
}

int CMdnsHelperApple::step()
{
	if(!m_client)
		return 0;

	int dns_sd_fd  = m_client  ? DNSServiceRefSockFD(m_client) : -1;
	int nfds = dns_sd_fd + 1;
	fd_set readfds;
	struct timeval tv;
	int result;

	// 1. Set up the fd_set as usual here.
	// This example m_client has no file descriptors of its own,
	// but a real application would call FD_SET to add them to the set here
	FD_ZERO(&readfds);

	// 2. Add the fd for our m_client(s) to the fd_set
	if (m_client) FD_SET(dns_sd_fd , &readfds);

	// 3. Set up the timeout.
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	result = select(nfds, &readfds, (fd_set*)NULL, (fd_set*)NULL, &tv);
	if (result > 0)
	{
		DNSServiceErrorType err = kDNSServiceErr_NoError;
		if      (m_client  && FD_ISSET(dns_sd_fd , &readfds)) err = DNSServiceProcessResult(m_client);
		if (err) { CServerIo::trace(3, "DNSServiceProcessResult returned %d", err); }
	}
	else if (result == 0)
	{
	}
	else
	{
		CServerIo::trace(3,"select() returned %d errno %d %s\n", result, errno, strerror(errno));
	}
	return 0;
}

int CMdnsHelperApple::browse(const char *service, MdnsBrowseCallback *callbacks, void *userdata)
{
	m_userdata = userdata;
	m_callbacks = callbacks;

	DNSServiceBrowse(&m_client, 0, 0, service, 0, _browse_reply, this);
	time_t t;
	if(!m_client)
	{
		CServerIo::trace(3,"DNSServiceBrowse() failed.\n");
		return -1;
		}
	time(&m_lastreply);
	do
	{
		step();
		time(&t);
	} while(t<m_lastreply+2);
	DNSServiceRefDeallocate(m_client);
	m_client = NULL;

	return 0;
}

int CMdnsHelperApple::close()
{
	if(m_client)
		DNSServiceRefDeallocate(m_client);
	m_client = NULL;
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
	return new CMdnsHelperApple();
}

#undef MDNS_EXPORT

