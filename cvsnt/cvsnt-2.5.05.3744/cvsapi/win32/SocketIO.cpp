/*
	CVSNT Generic API
    Copyright (C) 2004 Tony Hoyle and March-Hare Software Ltd

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
/* WIN32 specific */
#include <config.h>
#include "../lib/api_system.h"

#define STRICT
#define WIN32_LEAN_AND_MEAN
#define FD_SETSIZE 1024
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdarg.h>

#include "../ServerIo.h"
#include "../SocketIO.h"

// Only defined by the Win2008 SDK and later
#ifndef IPV6_V6ONLY
#define IPV6_V6ONLY 27
#endif


CSocketIO::CSocketIO()
{
	m_pAddrInfo=NULL;
	m_activeSocket=(SOCKET)0;
	m_bCloseActive=false;
	m_buffer=NULL;
	m_sin=NULL;
	m_addrlen=0;
	m_tcp=false;
}

CSocketIO::CSocketIO(SOCKET s, sockaddr *sin, socklen_t addrlen, bool tcp)
{
	init();

	m_pAddrInfo=NULL;
	m_buffer=NULL;
	m_activeSocket=s;
	m_bCloseActive=tcp;
	if(sin && addrlen)
	{
		m_sin=(sockaddr*)malloc(addrlen);
		memcpy(m_sin,sin,addrlen);
		m_addrlen = addrlen;
	}
	else
	{
		m_sin=NULL;
		m_addrlen=0;
	}
	m_tcp = tcp;
}

CSocketIO::CSocketIO(const CSocketIO& other)
{
//	_asm int 3
}

CSocketIO::~CSocketIO()
{
	close();
}

bool CSocketIO::create(const char *address, const char *port, bool loopback /* = true */, bool tcp /* = true */)
{
	init();

	addrinfo hint = {0}, *addr;
	SOCKET sock;

	close();

	hint.ai_family=PF_UNSPEC;
	hint.ai_socktype=tcp?SOCK_STREAM:SOCK_DGRAM;
	hint.ai_protocol=tcp?IPPROTO_TCP:IPPROTO_UDP;
	hint.ai_flags=loopback?0:AI_PASSIVE;
	m_pAddrInfo=NULL;
	// if NULL address is passed to cvs::idn it will crash (deep in utf82ucs2)
	// for some reason VS.NET 2008 still calls cvs::idn in this case:
	//   (!address)?NULL:(cvs::idn(address))
	int err;
	if (!address)
		err=getaddrinfo(NULL,port,&hint,&m_pAddrInfo);
	else
		err=getaddrinfo(cvs::idn(address),port,&hint,&m_pAddrInfo);
	if(err)
	{
		CServerIo::trace(3,"Socket creation failed: %s",gai_strerrorA(WSAGetLastError()));
		return false;
	}

	for(addr = m_pAddrInfo; addr; addr=addr->ai_next)
	{
		sock = WSASocket(addr->ai_family, addr->ai_socktype, addr->ai_protocol,NULL,0,0);

		// On Win32 you must never set SO_REUSEADDR because its semantics are completely different
		// from the BSD standard - it silently allows multiple apps to bind to the same port,
		// but states the subsequent behaviour is 'undefined'.

		// Note that we don't care if it succeeds (it'll fail on windows <Vista), because
		// the end result is the same.
		int on = 1;
		if(addr->ai_family==PF_INET6)
			::setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&on, sizeof(on));

		m_sockets.push_back(sock);
	}
	m_tcp = tcp;
	return m_sockets.size()?true:false;
}

bool CSocketIO::close()
{
	if(m_pAddrInfo)
		freeaddrinfo(m_pAddrInfo);
	for(size_t n=0; n<m_sockets.size(); n++)
		closesocket(m_sockets[n]);
	if(m_bCloseActive)
		closesocket(m_activeSocket);
	if(m_buffer)
		free(m_buffer);
	if(m_sin)
		free(m_sin);
	m_pAddrInfo=NULL;
	m_bCloseActive=false;
	m_buffer=NULL;
	m_sin=NULL;
	m_addrlen=0;
	m_sockets.clear();

	return true;
}

bool CSocketIO::connect()
{
	addrinfo *addr;
	size_t n;
	for(n=0,addr = m_pAddrInfo; addr; addr=addr->ai_next,n++)
	{
		if(m_sockets[n]!=-1)
		{
			int res = ::connect(m_sockets[n],addr->ai_addr,(int)addr->ai_addrlen);
			if(!res)
			{
				m_activeSocket = m_sockets[n];
				m_bCloseActive=false;
				break;
			}
		}
	}
	if(addr)
		return true;
	return false;
}

bool CSocketIO::bind()
{
	addrinfo *addr;
	size_t n;

	// We don't need the multi-bind checks on Win32
	// On Windows prior to Vista ipv6 was a separate stack, so you don't get collissions
	// Windows after vista supports IPV6_V6ONLY
	for(n=0,addr = m_pAddrInfo; addr; addr=addr->ai_next,n++)
	{
		if(m_sockets[n]!=-1)
		{
			int res = ::bind(m_sockets[n],addr->ai_addr,(int)addr->ai_addrlen);
			if(!res)
			{
				if(m_tcp)
					::listen(m_sockets[n],SOMAXCONN);
			}
			else
			{
				CServerIo::trace( 3, "Socket bind failed: errno %d on socket %d (AF %d)", errno, m_sockets[n], addr->ai_family);
				closesocket(m_sockets[n]);
				m_sockets[n]=-1;
				return false;
			}
		}
	}
	return true;
}

int CSocketIO::recv(char *buf, int len)
{
	if(!m_buffer)
	{
		m_bufmaxlen = BUFSIZ;
		m_buffer = (char*)malloc(m_bufmaxlen);
		m_buflen = 0;
		m_bufpos = 0;
	}
	if(m_bufpos+len<=m_buflen)
	{
		memcpy(buf,m_buffer+m_bufpos,len);
		m_bufpos+=len;
		return len;
	}
	if(m_buflen-m_bufpos)
		memcpy(buf,m_buffer+m_bufpos,m_buflen-m_bufpos);
	m_buflen-=m_bufpos;
	if((len-m_buflen)>=m_bufmaxlen)
	{
		int rd = _recv(buf+m_buflen,len-(int)m_buflen,0);
		len=(int)m_buflen;
		m_buflen=m_bufpos=0;
		if(rd<0)
			return rd;
		return len+rd;
	}
	int rd=_recv(m_buffer,(int)m_bufmaxlen,0);
	size_t oldlen=m_buflen;
	m_bufpos=0;
	if(rd<0)
	{
		m_buflen=0;
		return rd;
	}
	m_buflen = rd;
	if((len-oldlen)<=m_buflen)
	{
		memcpy(buf+oldlen,m_buffer,len-oldlen);
		m_bufpos+=(len-oldlen);
		return len;
	}
	memcpy(buf+oldlen,m_buffer,m_buflen);
	m_bufpos+=m_buflen;
	return (int)(oldlen+m_buflen);
}

int CSocketIO::send(const char *buf, int len)
{
	return _send(buf,len,0);
}

int CSocketIO::printf(const char *fmt, ...)
{
	va_list va;
	cvs::string str;
	va_start(va,fmt);
	cvs::vsprintf(str,128,fmt,va);
	va_end(va);
	return send(str.c_str(),(int)str.length());
}

bool CSocketIO::getline(char *&buf, int& buflen)
{
	char c;
	int l = 0,r;

	while((r=recv(&c,1))==1)
	{
		if(c=='\n')
			break;
		if(c=='\r')
			continue;
		if(l==buflen)
		{
			buflen+=128;
			buf=(char *)realloc(buf,buflen);
		}
		buf[l++]=c;
	}
	return (r<0)?false:true;
}

bool CSocketIO::getline(cvs::string& line)
{
	char c;
	int r;
	line="";
	line.reserve(128);

	while((r=recv(&c,1))==1)
	{
		if(c=='\n')
			break;
		if(c=='\r')
			continue;
		line+=c;
	}
	return (r<0)?false:true;
}

int CSocketIO::_recv(char *buf, int len, int flags)
{
	int ret = ::recv(m_activeSocket,buf,len,flags);
	if(ret!=0) return ret;
	DWORD dw = WSAGetLastError();
	if(dw==WSAEWOULDBLOCK) return 0;
	return -1;
}

int CSocketIO::_send(const char *buf, int len, int flags)
{
	if(m_tcp || !m_sin)
		return ::send(m_activeSocket,buf,len,flags);
	else
		return ::sendto(m_activeSocket,buf,len,flags,m_sin,m_addrlen);
}

bool CSocketIO::select(int timeout, size_t count, CSocketIO *socks[])
{
	if(count<1 || !socks)
		return NULL;

	fd_set rfd;
	SOCKET maxdesc = 0;

	FD_ZERO(&rfd);
	for(size_t n=0; n<count; n++)
	{
		if(!socks[n])
			continue;

		socks[n]->m_accepted_sock.clear();
		for(size_t j=0; j<socks[n]->m_sockets.size(); j++)
		{
			if(socks[n]->m_sockets[j]==-1)
				continue;
			FD_SET(socks[n]->m_sockets[j],&rfd);
			if(socks[n]->m_sockets[j]>maxdesc)
				maxdesc=socks[n]->m_sockets[j];
		}
	}

	struct timeval tv = { timeout/1000, timeout%1000 };
	int sel=::select((int)(maxdesc+1),&rfd,NULL,NULL,&tv);
	if(sel==SOCKET_ERROR)
		return false;

	for(size_t n=0; n<count; n++)
	{
		for(size_t j=0; j<socks[n]->m_sockets.size(); j++)
		{
			if(socks[n]->m_sockets[j]==-1)
				continue;
			if(FD_ISSET(socks[n]->m_sockets[j],&rfd))
			{
				sockaddr_storage sin;
				socklen_t addrlen=sizeof(sockaddr_storage);
				if(socks[n]->m_tcp)
				{
					SOCKET s = ::accept(socks[n]->m_sockets[j],(sockaddr*)&sin,&addrlen);
					if(s!=SOCKET_ERROR)
						socks[n]->m_accepted_sock.push_back(new CSocketIO(s,(sockaddr*)&sin,addrlen,true));
				}
				else
				{
					recvfrom(socks[n]->m_sockets[j],NULL,0,MSG_PEEK,(sockaddr*)&sin,&addrlen);
					socks[n]->m_accepted_sock.push_back(new CSocketIO(socks[n]->m_sockets[j],(sockaddr*)&sin,addrlen,false));
				}
			}
		}
	}
	return true;
}

bool CSocketIO::accept(int timeout)
{
	CSocketIO* s = this;
	return select(timeout,1,&s);
}

const char *CSocketIO::error()
{
	return gai_strerrorA(WSAGetLastError());
}

bool CSocketIO::setnodelay(bool delay)
{
	int v=delay?1:0;
	if(!::setsockopt(m_activeSocket,IPPROTO_TCP,TCP_NODELAY,(const char *)&v,sizeof(v)))
		return true;
	return false;
}

bool CSocketIO::gethostname(cvs::string& host)
{
    host.resize(NI_MAXHOST);
	if(!m_sin || getnameinfo(m_sin,m_addrlen,(char*)host.data(),NI_MAXHOST,NULL,0,0))
		return false;
	host.resize(strlen(host.c_str()));
	return true;
}

bool CSocketIO::init()
{
    static bool bInitCalled = false;
    if(!bInitCalled)
    {
	// Initialisation
	WSADATA data;

	if(WSAStartup (MAKEWORD (1, 1), &data))
		return false;
    	bInitCalled = true;
    }
    return true;
}

bool CSocketIO::blocking(bool block)
{
	u_long v = block?0:1;
	if(ioctlsocket(m_activeSocket,FIONBIO,&v))
		return false;
	return true;
}

bool CSocketIO::setsockopt(int type,int optname, int value)
{
	if(m_activeSocket)
	{
		if(::setsockopt(m_activeSocket,type,optname,(const char *)&value,sizeof(value)))
			return false;
	}
	else
	{
		addrinfo *addr;
		int n;

		for(n=0,addr = m_pAddrInfo; addr; addr=addr->ai_next,n++)
		{
			if(m_sockets[n]!=-1)
			{
				if(::setsockopt(m_sockets[n],type,optname,(const char *)&value,sizeof(value)))
					return false;
			}
		}
	}
	return true;
}


