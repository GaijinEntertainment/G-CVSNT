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
#ifndef DNSAPI__H
#define DNSAPI__H

#ifdef _WIN32
// Standard DNS RR type defs
#include <windns.h>
#else
/* todo: some unixen have these defines */
#define DNS_TYPE_A	1
#define DNS_TYPE_NS	2
#define DNS_TYPE_PTR 	12
#define DNS_TYPE_TEXT 	16
#define DNS_TYPE_SRV 	33
#endif

class CDnsApi
{
public:
	struct SrvRR
	{
		const char *server;
		unsigned port;
		unsigned priority;
		unsigned weight;
	};

	CVSAPI_EXPORT CDnsApi();
	CVSAPI_EXPORT virtual ~CDnsApi();

	CVSAPI_EXPORT bool Lookup(const char *name, int rrType);
	CVSAPI_EXPORT bool Next();
	CVSAPI_EXPORT bool Close();

	CVSAPI_EXPORT const char *GetRRName();
	CVSAPI_EXPORT const char *GetRRPtr();
	CVSAPI_EXPORT const char *GetRRTxt();
	CVSAPI_EXPORT SrvRR *GetRRSrv();
protected:
	SrvRR tmpSrv;
#ifdef _WIN32
	PDNS_RECORDA m_pdnsBase, m_pdnsCurrent;
#else
	unsigned char *m_pdnsBase, *m_pdnsCurrent, *m_pdnsEnd;
#if defined __HP_aCC
	unsigned char m_dnsName[256],m_dnsTmp[256];
#else
	char m_dnsName[256],m_dnsTmp[256];
#endif
	unsigned short m_type;
	unsigned short m_class;
	int m_ttl;
	unsigned short m_rdlength;
	unsigned char *m_prdata;
	int m_nCount;

	bool GetHeader(bool query);
#endif
};

#endif
