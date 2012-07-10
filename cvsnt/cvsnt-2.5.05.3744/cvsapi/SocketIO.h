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
#ifndef SOCKETIO__H
#define SOCKETIO__H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#define SOCKET int
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <vector>

#include "cvs_string.h"
#include "cvs_smartptr.h"

class CSocketIO;

typedef cvs::smartptr<CSocketIO> CSocketIOPtr;

class CSocketIO
{
public:
	CVSAPI_EXPORT CSocketIO();
	CVSAPI_EXPORT CSocketIO(SOCKET s, sockaddr *sin, socklen_t addrlen, bool bUdp);
	CVSAPI_EXPORT CSocketIO(const CSocketIO& other);
	CVSAPI_EXPORT virtual ~CSocketIO();

	CVSAPI_EXPORT static bool init();
	CVSAPI_EXPORT bool close();
	CVSAPI_EXPORT bool create(const char *address, const char *port, bool loopback = true, bool tcp = true);
	CVSAPI_EXPORT bool connect();
	CVSAPI_EXPORT bool bind();
	CVSAPI_EXPORT bool accept(int timeout);
	CVSAPI_EXPORT std::vector<CSocketIOPtr>& accepted_sockets() { return m_accepted_sock; }
	CVSAPI_EXPORT static bool select(int timeout, size_t count, CSocketIO *socks[]);
	CVSAPI_EXPORT int recv(char *buf, int len);
	CVSAPI_EXPORT int send(const char *buf, int len);
	CVSAPI_EXPORT int printf(const char *fmt, ...);
	CVSAPI_EXPORT bool getline(char *&buf, int& buflen);
	CVSAPI_EXPORT bool getline(cvs::string& line);
	CVSAPI_EXPORT const char *error();
	CVSAPI_EXPORT bool setnodelay(bool delay);
	CVSAPI_EXPORT bool setsockopt(int optname, int type, int value);
	CVSAPI_EXPORT SOCKET getsocket() { return m_activeSocket; }
	CVSAPI_EXPORT bool gethostname(cvs::string& host);
	CVSAPI_EXPORT bool blocking(bool block);

protected:
	std::vector<SOCKET> m_sockets;
	std::vector<CSocketIOPtr> m_accepted_sock;
	bool m_bCloseActive;
	SOCKET m_activeSocket;
	addrinfo *m_pAddrInfo;
	struct sockaddr *m_sin;
	int m_addrlen;
	char *m_buffer;
	size_t m_bufpos,m_bufmaxlen,m_buflen;
	bool m_tcp;

	int _recv(char *buf, int len, int flags);
	int _send(const char *buf, int len, int flags);
};

#endif
