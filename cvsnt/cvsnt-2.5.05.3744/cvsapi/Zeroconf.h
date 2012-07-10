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
#ifndef ZEROCONF__H
#define ZEROCONF__H

struct addrinfo;

#include <map>
#include <vector>

#include "mdns.h"

class CZeroconf
{
public:
	enum
	{
		zcText	  = 0x01,	// Return text entries
		zcAddress = 0x02,	// Return addrinfo structure
		zcHost	  = 0x08	// Fill in host name
	};
	struct server_struct_t
	{
		server_struct_t();
		~server_struct_t();
		cvs::string servicename;
		cvs::string server; // mdns name (xx.local)
		cvs::string host; // host from gethostname zcHost set
		unsigned short port;
		cvs::string text;
		addrinfo *addr;
		bool bogus;
	};

	CVSAPI_EXPORT CZeroconf(const char *type, const char *library_dir);
	CVSAPI_EXPORT virtual ~CZeroconf();

	CVSAPI_EXPORT bool BrowseForService(const char *service, unsigned flags);
	CVSAPI_EXPORT const server_struct_t *EnumServers(bool& first);

protected:
	typedef std::map<cvs::string,server_struct_t> servers_t;
	servers_t m_servers;
	unsigned m_flags;
	cvs::string m_service;
	servers_t::const_iterator m_it;
	const char *m_mdns_type;
	const char *m_library_dir;

	struct name_lookup_struct_t
	{
		name_lookup_struct_t() { ipv4_set=ipv6_set=false; }
		bool ipv4_set, ipv6_set;
		unsigned char ipv4[4];
		unsigned char ipv6[16];
};

	typedef std::map<cvs::string,name_lookup_struct_t> name_lookup_t;
	name_lookup_t m_name_lookup;

	static void service_srv_func(const char *name, unsigned short port, const char *target, void *userdata);
	static void service_txt_func(const char *name, const char *txt, void *userdata);
	static void service_ipv4_func(const char *name, const unsigned char address[4], void *userdata);
	static void service_ipv6_func(const char *name, const unsigned char address[16], void *userdata);

	void _service_srv_func(const char *name, unsigned short port, const char *target);
	void _service_txt_func(const char *name, const char *txt);
	void _service_ipv4_func(const char *name, const unsigned char address[4]);
	void _service_ipv6_func(const char *name, const unsigned char address[16]);
};

#endif

