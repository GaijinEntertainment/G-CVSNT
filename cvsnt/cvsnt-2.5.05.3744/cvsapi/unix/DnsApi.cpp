/*
	CVSNT Generic API
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
/* Unix specific */
#define BIND_8_COMPAT /* OSX otherwise it uses its own internal definitions */
#include <sys/types.h>

#include <config.h>
#include "../lib/api_system.h"

#ifdef HAVE_MDNS
#if defined __HP_aCC
#include <sys/types.h>
#endif
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <errno.h>
#include <netdb.h>
#endif

#include "../ServerIO.h"
#include "../cvs_string.h"

#if defined __HP_aCC

ssize_t res_query(
 char *dname,
 int c,
 int type,
 u_char *answer,
 int anslen
);

ssize_t res_search(
 char *dname,
 int c,
 int type,
 u_char *answer,
 int anslen
);

ssize_t res_mkquery(
 int op,
 const char *dname,
 int c,
 int type,
 const char *data,
 int datalen,
 const char *newrr,
 char *buf,
 int buflen
);

ssize_t res_send(
 const char *msg,
 ssize_t msglen,
 char *answer,
 int anslen
);

int res_init();

ssize_t dn_comp(
 u_char *comp_dn,
 ssize_t ssize_length,
 u_char **dnptrs,
 u_char *exp_dn,
 int  i_length
);

ssize_t dn_expand(
 const u_char *msg,
 const u_char *eomorig,
 const u_char *comp_dn,
 u_char *exp_dn,
 int  i_length
);


int set_resfield(
 int i_field,
 void *ptr_value
);

int get_resfield(
 int i_field,
 void *ptr_value,
 size_t i_value
);

void herror(char *s);
#endif

#include "../DnsApi.h"

CDnsApi::CDnsApi()
{
	m_pdnsBase=m_pdnsCurrent=NULL;
}

CDnsApi::~CDnsApi()
{
	Close();
}

bool CDnsApi::Lookup(const char *name, int rrType)
{
	Close();
#ifndef HAVE_MDNS
	return false;
#else
	m_pdnsBase = new u_char[16384];
	HEADER *h = (HEADER*)m_pdnsBase;
	int ret = res_query((char *)name, C_IN, rrType, m_pdnsBase, 16384);
	if(ret>0)
	{
		/* qdcount should always be 1 for an answer */
		if(ntohs(h->qdcount)>1)
			return false;
		m_nCount = ntohs(h->ancount);
		printf("ancount=%d\n",m_nCount);
		m_pdnsEnd = m_pdnsBase + ret;
		m_pdnsCurrent = m_pdnsBase+sizeof(HEADER);
		if(h->qdcount)
		{
			if(!GetHeader(true))
			{
				printf("getheader failed\n");
				m_pdnsCurrent = NULL;
				return false;
			}
			if(!Next())
			{	
				printf("next failed\n");
				return false;
			}
		}
	}
	return (ret<=0)?false:true;
#endif
}

bool CDnsApi::GetHeader(bool query)
{
#ifndef HAVE_MDNS
	return false;
#else
	u_char *p = m_pdnsCurrent;
	int n=dn_expand(m_pdnsCurrent, m_pdnsEnd, p, m_dnsName, sizeof(m_dnsName));
	if(n<1)
	{
		printf("dn_expand failed\n");
		return false;
	}
	p+=n;
	GETSHORT(m_type, p); 
	GETSHORT(m_class, p);
	/* The first response record (query) does *not* appear to have
	 * these fields - this appears to violate the RFC */
	if(!query)
	{
		GETLONG(m_ttl, p); 
		GETSHORT(m_rdlength, p);
	}
	else
	{
		m_ttl = 0;
		m_rdlength = 0;
	}
	m_prdata = p;
	m_class &=0x7FFF;
	printf("name=%s\n",m_dnsName);
	printf("type=%d\n",m_type);
	printf("class=%d\n",m_class);
	printf("ttl=%d\n",m_ttl);
	printf("rdlength=%d\n",m_rdlength);
	return true;
#endif
}

bool CDnsApi::Next()
{
	if(!m_pdnsCurrent)
		return false;

	if(!m_nCount--)
	{
		printf("count=0\n");
		m_pdnsCurrent = NULL;
		return false;
	}

	m_pdnsCurrent = m_prdata + m_rdlength;

	if(!GetHeader(false))
	{
		printf("getheader failed\n");
		m_pdnsCurrent = NULL;
		return false;
	}

	return true;
}

bool CDnsApi::Close()
{
	if(m_pdnsBase)
		delete m_pdnsBase;
	m_pdnsBase=m_pdnsCurrent=NULL;
	return true;
}

const char *CDnsApi::GetRRName()
{
	if(!m_pdnsCurrent)
		return NULL;
	return (const char *)m_dnsName;
}

const char *CDnsApi::GetRRPtr()
{
#ifndef HAVE_MDNS
	return NULL;
#else
	printf("GetRRPtr\n");
	if(!m_pdnsCurrent)
		return NULL;

	if(m_type!=DNS_TYPE_PTR)
		return NULL;

	if(dn_expand(m_pdnsCurrent, m_pdnsEnd, m_prdata, m_dnsTmp, sizeof(m_dnsTmp))<1)
		return NULL;
	return (const char *)m_dnsTmp;
#endif
}

const char *CDnsApi::GetRRTxt()
{
#ifndef HAVE_MDNS
	return NULL;
#else
	printf("GetRRTxt\n");
	if(!m_pdnsCurrent)
		return NULL;

	if(m_type!=DNS_TYPE_TEXT)
		return NULL;

	if(dn_expand(m_pdnsCurrent, m_pdnsEnd, m_prdata, m_dnsTmp, sizeof(m_dnsTmp))<1)
		return NULL;
	return (const char *)m_dnsTmp;
#endif
}

CDnsApi::SrvRR *CDnsApi::GetRRSrv()
{
#ifndef HAVE_MDNS
	return NULL;
#else
	printf("GetRRSrv\n");
	if(!m_pdnsCurrent)
		return NULL;

	if(m_type!=DNS_TYPE_SRV)
		return NULL;

	u_char *p = m_prdata;
	GETSHORT(tmpSrv.priority, p);
	GETSHORT(tmpSrv.weight, p); 
	GETSHORT(tmpSrv.port, p); 
	int n = dn_expand(m_pdnsCurrent, m_pdnsEnd, p, m_dnsTmp, sizeof(m_dnsTmp));
	if(n<1)
		return NULL;
	p+=n;
	tmpSrv.server = (const char *)m_dnsTmp;
	return &tmpSrv;
#endif
}
