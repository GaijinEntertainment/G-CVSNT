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
#ifndef MDNS_MINI__H
#define MDNS_MINI__H

#include "mdns.h"

#include <vector>

#include "../mdnsclient/mdnsclient.h"

#ifndef PATCH_NULL
#ifdef  sun
/* solaris has a poor implementation of vsnprintf() which is not able to handle null pointers for %s */
# define PATCH_NULL(x) ((x)?(x):"<NULL>")
#else
# define PATCH_NULL(x) x
#endif
#endif

class CMdnsHelperMini : public CMdnsHelperBase
{
public:
	CMdnsHelperMini();
	virtual ~CMdnsHelperMini();

	virtual int open();
	virtual int publish(const char *instance, const char *service, const char *location, int port, const char *text);
	virtual int step();
	virtual int browse(const char *service, MdnsBrowseCallback *callbacks, void *userdata);
	virtual int close();

protected:
	void *m_userdata;
	MdnsBrowseCallback *m_callbacks;
	mdnshandle_t m_handle;
	std::vector<mdns_service_item_t *>m_services;

	static void _browse_srv_func(const char *name, uint16_t port, const char *target, void *userdata);
	void browse_srv_func(const char *name, uint16_t port, const char *target);
	static void _browse_txt_func(const char *name, const char *txt, void *userdata);
	void browse_txt_func(const char *name, const char *txt);
	static void _browse_ipv4_func(const char *name, const ipv4_address_t *ipv4, void *userdata);
	void browse_ipv4_func(const char *name, const ipv4_address_t *ipv4);
	static void _browse_ipv6_func(const char *name, const ipv6_address_t *ipv6, void *userdata);
	void browse_ipv6_func(const char *name, const ipv6_address_t *ipv6);
};

#endif
