/*
	CVSNT mdns helpers
    Copyright (C) 2005 Tony Hoyle and March-Hare Software Ltd

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
#ifndef MDNS_APPLE__H
#define MDNS_APPLE__H

#include "mdns.h"

#include <dns_sd.h>

#ifndef DNSSD_API
#define DNSSD_API
#endif

class CMdnsHelperApple : public CMdnsHelperBase
{
public:
	CMdnsHelperApple();
	virtual ~CMdnsHelperApple();

	virtual int open();
	virtual int publish(const char *instance, const char *service, const char *location, int port, const char *text);
	virtual int step();
	virtual int browse(const char *service, MdnsBrowseCallback *callbacks, void *userdata);
	virtual int close();

protected:
	void *m_userdata;
	MdnsBrowseCallback *m_callbacks;
	DNSServiceRef m_client;
	time_t m_lastreply;

	static void DNSSD_API _reg_reply(DNSServiceRef client, DNSServiceFlags flags, DNSServiceErrorType errorCode,
		const char *name, const char *regtype, const char *domain, void *context);
	void reg_reply(DNSServiceRef client, DNSServiceFlags flags, DNSServiceErrorType errorCode,
		const char *name, const char *regtype, const char *domain);
	static void DNSSD_API _browse_reply(DNSServiceRef client, DNSServiceFlags flags, uint32_t ifIndex, DNSServiceErrorType errorCode,
		const char *replyName, const char *replyType, const char *replyDomain, void *context);
	void browse_reply(DNSServiceRef client, DNSServiceFlags flags, uint32_t ifIndex, DNSServiceErrorType errorCode,
		const char *replyName, const char *replyType, const char *replyDomain);
	static void DNSSD_API _resolve_reply(DNSServiceRef client, DNSServiceFlags flags, uint32_t ifIndex, DNSServiceErrorType errorCode,
		const char *fullname, const char *hosttarget, uint16_t opaqueport, uint16_t txtLen, const unsigned char *txtRecord, void *context);
	static void DNSSD_API _query_reply(DNSServiceRef client, DNSServiceFlags flags, 
					uint32_t ifIndex, DNSServiceErrorType errorCode, const char *fullname, 
					uint16_t rrtype, uint16_t rrclass, uint16_t rdlen, const void *rdata, 
					uint32_t ttl, void *context);
};

#endif
