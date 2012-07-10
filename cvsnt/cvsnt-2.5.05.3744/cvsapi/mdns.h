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
#ifndef MDNS__H
#define MDNS__H

struct MdnsBrowseCallback
{
	void (*srv_fn)(const char *name, unsigned short port, const char *target, void *userdata);
	void (*txt_fn)(const char *name, const char *txt, void *userdata);
	void (*ipv4_fn)(const char *name, const unsigned char address[4], void *userdata);
	void (*ipv6_fn)(const char *name, const unsigned char address[16], void *userdata);
};

class CMdnsHelperBase
{
public:
	CMdnsHelperBase() { }
	virtual ~CMdnsHelperBase() { }

	static CVSAPI_EXPORT CMdnsHelperBase* CreateHelper(const char *type, const char *dir);

	virtual int open() =0;
	virtual int publish(const char *instance, const char *service, const char *location, int port, const char *text) =0;
	virtual int step() =0;
	virtual int browse(const char *service, MdnsBrowseCallback *callbacks, void *userdata) =0;
	virtual int close() =0;
};

#endif

