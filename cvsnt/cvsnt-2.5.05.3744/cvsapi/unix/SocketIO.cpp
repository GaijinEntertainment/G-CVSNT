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
/* Unix specific */
#include <config.h>
#include "../lib/api_system.h"
#include <sys/types.h>
#include <sys/socket.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

#include "../ServerIO.h"
#include "../SocketIO.h"

#if defined(sun) || defined( __HP_aCC ) || defined(hpux)
/* solaris has a poor implementation of vsnprintf() which is not able to handle
null pointers for %s */
# define PATCH_NULL(x) ((x)?(x):"<NULL>")
#else
# define PATCH_NULL(x) x
#endif

static bool v6only_set = false;

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

CSocketIO::~CSocketIO()
{
	close();
}

bool CSocketIO::create(const char *address, const char *port, bool loopback /* = true */, bool tcp /* = true */)
{
	static bool v6_checked = false, v6_available = false;
	addrinfo hint = {0}, *addr;
	SOCKET sock;

	// I'm still not convinced this actually does anything useful, but at least it should only run once otherwise
	// you've simply added extra overhead.
	if(!v6_checked)
	{
	// check for installed IPv6 stack
	// this may speed up getaddrinfo() dramaticaly if no IPv6 is installed
	sock = socket(PF_INET6, SOCK_DGRAM, 0);
	if( -1 != sock) {
		// IPv6 seams to be installed
		hint.ai_family=PF_UNSPEC;
		v6_checked = true;
		v6_available = true;
		::close(sock);
	} else {
		// IPv6 is not installed
		hint.ai_family=PF_INET;
		v6_checked = true;
		v6_available = false;
	}
	}
	else
		hint.ai_family=v6_available?PF_UNSPEC:PF_INET;

	hint.ai_socktype=tcp?SOCK_STREAM:SOCK_DGRAM;
	hint.ai_protocol=tcp?IPPROTO_TCP:IPPROTO_UDP;
	hint.ai_flags=loopback?0:AI_PASSIVE;
	m_pAddrInfo=NULL;
	int err=getaddrinfo(address,port,&hint,&m_pAddrInfo);
	if(err)
	{
		CServerIo::trace(3,"Socket creation failed %s for:",gai_strerror(errno));
		CServerIo::trace(3,"   address %s, port %s family %s flags %s protocol %s",PATCH_NULL(address),PATCH_NULL(port),(hint.ai_family&PF_INET)?"IPv4":"Unspecified",(hint.ai_flags&AI_PASSIVE)?"AI_PASSIVE":"",(hint.ai_protocol&IPPROTO_TCP)?"TCP":"UDP");
#if defined( __HP_aCC )
		CServerIo::trace(3,"HP: getaddrinfo is not documented in the man pages, please refer to an HP engineer.");
		CServerIo::trace(3," see http://docs.hp.com/en/B2355-60127/getaddrinfo.3N.html");
		CServerIo::trace(3,"");
		switch (err) {
		case EAI_ADDRFAMILY:
			CServerIo::trace(3,"Address family for hostname not supported.");
			break;
		case EAI_AGAIN:
			CServerIo::trace(3,"Temporary failure in name resolution.");
			break;
		case EAI_BADFLAGS:
			CServerIo::trace(3,"Invalid value for ai_flags.");
			break;
		case EAI_FAIL:
			CServerIo::trace(3,"Non-recoverable failure in name resolution.");
			break;
		case EAI_FAMILY:
			CServerIo::trace(3,"ai_family not supported.");
			break;
		case EAI_MEMORY:
			CServerIo::trace(3,"Memory allocation failure.");
			break;
		case EAI_NODATA:
			CServerIo::trace(3,"No address associated with hostname.");
			break;
		case EAI_NONAME:
			CServerIo::trace(3,"No hostname nor servname provided, or not known.");
			break;
		case EAI_SERVICE:
			CServerIo::trace(3,"The servname is not supported for ai_socktype.");
			break;
		case EAI_SOCKTYPE:
			CServerIo::trace(3,"ai_socktype not supported.");
			break;
		case EAI_SYSTEM:
			CServerIo::trace(3,"System error returned in errno. \"%s\". ",strerror(errno));
			break;
		default:
			CServerIo::trace(3,"Unknown HP error.");
		}
#endif
		return false;
	}

	for(addr = m_pAddrInfo; addr; addr=addr->ai_next)
	{
		sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if(sock!=-1)
		{
			int on = 1;

#ifdef SO_REUSEADDR
			::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));
#endif

#if defined(IPV6_V6ONLY) && defined(IPPROTO_IPV6)
			if(v6_available && addr->ai_family==PF_INET6)
				if(!::setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&on, sizeof(on)))
					v6only_set = true;
#endif
		}
		m_sockets.push_back(sock); // even push (-1) to keep order of m_sockets and m_pAddrInfo  
	}
	m_tcp = tcp;
	return m_sockets.size()?true:false;
}

bool CSocketIO::close()
{
	if(m_pAddrInfo)
		freeaddrinfo(m_pAddrInfo);
	for(size_t n=0; n<m_sockets.size(); n++)
		::close(m_sockets[n]);
	if(m_bCloseActive)
		::close(m_activeSocket);
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
	bool bound = false;

	for(n=0,addr = m_pAddrInfo; addr; addr=addr->ai_next,n++)
	{
		if(m_sockets[n]!=-1)
		{
			int res = ::bind(m_sockets[n],addr->ai_addr,(int)addr->ai_addrlen);
			if(!res)
			{
				::listen(m_sockets[n],SOMAXCONN);
				bound=true;
			}
			else 
			{
				if(v6only_set || errno!=EADDRINUSE)
				{
					CServerIo::trace( 3, "Socket bind failed: errno %d on socket %d (AF %d)", errno, m_sockets[n], addr->ai_family);
					return false;
				}

				::close(m_sockets[n]);
				m_sockets[n] = -1;
			}
		}
	}
	if(!bound)
	{
		CServerIo::trace(3,"All binds failed");
	}
	return bound;
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
		m_bufpos+=len;
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
#ifdef EAGAIN
	if(errno==EAGAIN) return 0;
#endif
#ifdef EWOULDBLOCK
	if(errno==EWOULDBLOCK) return 0;
#endif
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
	size_t n;

	if(count<1 || !socks)
		return false;

	fd_set rfd;
	SOCKET maxdesc = 0;

	FD_ZERO(&rfd);
	for(n=0; n<count; n++)
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
	if(sel<0)
		return false;

	for(n=0; n<count; n++)
	{
		for(size_t j=0; j<socks[n]->m_sockets.size(); j++)
		{
			if(socks[n]->m_sockets[j]==-1)
				continue;
			if(FD_ISSET(socks[n]->m_sockets[j],&rfd))
			{
				sockaddr_storage sin;
#ifdef __hpux
				int addrlen=(int)sizeof(sockaddr_storage);
#else
				socklen_t addrlen=sizeof(sockaddr_storage);
#endif
				if(socks[n]->m_tcp)
				{
					SOCKET s = ::accept(socks[n]->m_sockets[j],(sockaddr*)&sin,&addrlen);
					if(s>0)
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
	return strerror(errno);
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
	return true;
}

bool CSocketIO::blocking(bool block)
{
#ifdef FIONBIO
	u_long v = block?0:1;
	if(ioctl(m_activeSocket,FIONBIO,&v))
		return false;
	return true;
#else
	int mode = fcntl (m_activeSocket, F_GETFL);
	if (mode == -1) 
		return false;
	if( block)
		mode &= ~O_NONBLOCK;
	else
		mode |= O_NONBLOCK;
	if( -1 == fcntl (m_activeSocket, F_SETFL, mode))
		return false;
	return true;
#endif
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
