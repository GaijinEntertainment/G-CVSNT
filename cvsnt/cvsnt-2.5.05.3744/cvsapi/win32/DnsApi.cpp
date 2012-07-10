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
/* Win32 specific */
#include <config.h>
#include "../lib/api_system.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "../ServerIo.h"
#include "../cvs_string.h"

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
	DNS_STATUS ret = DnsQuery_UTF8(name, rrType, DNS_QUERY_TREAT_AS_FQDN, NULL, (PDNS_RECORD*)&m_pdnsBase, NULL);
	if(!ret)
		m_pdnsCurrent = m_pdnsBase;
	return ret?false:true;
}

bool CDnsApi::Next()
{
	if(!m_pdnsCurrent)
		return false;
	m_pdnsCurrent = m_pdnsCurrent->pNext;
	if(!m_pdnsCurrent)
		return false;
	return true;
}

bool CDnsApi::Close()
{
	if(m_pdnsBase)
		DnsRecordListFree((PDNS_RECORD)m_pdnsBase,DnsFreeRecordList);
	m_pdnsBase=m_pdnsCurrent=NULL;
	return true;
}

const char *CDnsApi::GetRRName()
{
	if(m_pdnsBase)
		return m_pdnsBase->pName;
	return NULL;
}

const char *CDnsApi::GetRRPtr()
{
	if(!m_pdnsCurrent)
		return NULL;

	if(m_pdnsCurrent->wType!=DNS_TYPE_PTR)
		return NULL;

	return m_pdnsCurrent->Data.Ptr.pNameHost;
}

const char *CDnsApi::GetRRTxt()
{
	if(!m_pdnsCurrent)
		return NULL;

	if(m_pdnsCurrent->wType!=DNS_TYPE_TEXT)
		return NULL;

	return m_pdnsCurrent->Data.Txt.pStringArray[0];
}

CDnsApi::SrvRR *CDnsApi::GetRRSrv()
{
	if(!m_pdnsCurrent)
		return NULL;

	if(m_pdnsCurrent->wType!=DNS_TYPE_SRV)
		return NULL;

	tmpSrv.server = m_pdnsCurrent->Data.Srv.pNameTarget;
	tmpSrv.port = m_pdnsCurrent->Data.Srv.wPort;
	tmpSrv.priority = m_pdnsCurrent->Data.Srv.wPriority;
	tmpSrv.weight = m_pdnsCurrent->Data.Srv.wWeight;
	return &tmpSrv;
}
