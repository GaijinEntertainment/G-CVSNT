#ifndef fooqueryhfoo
#define fooqueryhfoo

/* $Id: mdnsclient.h,v 1.1.2.9 2008/11/17 14:29:39 tmh Exp $ */

/***
  This file is part of nss-mdns.
 
  nss-mdns is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.
 
  nss-mdns is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public
  License along with nss-mdns; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _WIN32
 #include <stdarg.h> // hpux needs this
 #include <inttypes.h>
 #if defined(_MDNS_EXPORT) && defined(HAVE_GCC_VISIBILITY)
  #define MDNS_EXPORT __attribute__ ((visibility("default")))
 #endif
#else
 #include "win32/inttypes.h"
#endif

#ifndef MDNS_EXPORT
#define MDNS_EXPORT
#endif

typedef struct ipv4_address_struct_t {
	uint8_t address[4];
} ipv4_address_t;

typedef struct ipv6_address_struct_t {
    uint8_t address[16];
} ipv6_address_t;

typedef void *mdnshandle_t;

MDNS_EXPORT mdnshandle_t mdns_open(void);
MDNS_EXPORT int mdns_close(mdnshandle_t handle);

struct mdns_callback
{
	void (*name_func)(const char *name, void *userdata);
	void (*srv_func)(const char *name, uint16_t port, const char *target, void *userdata);
	void (*txt_func)(const char *name, const char *txt, void *userdata);
	void (*ipv4_func)(const char *name, const ipv4_address_t *ipv4, void *userdata);
	void (*ipv6_func)(const char *name, const ipv6_address_t *ipv6, void *userdata);
};

typedef struct _mdns_service_item_t
{
	const char *Instance;
	const char *Service;
	uint16_t Port;
	const char *Location;
	ipv4_address_t *ipv4;
	ipv6_address_t *ipv6;
	struct _mdns_service_item_t *next;
} mdns_service_item_t;

MDNS_EXPORT int mdns_query_services(mdnshandle_t handle, const char *name, struct mdns_callback *callback, void *userdata, uint64_t timeout);
MDNS_EXPORT int mdns_query_name(mdnshandle_t handle, const char *name, struct mdns_callback *callback, void *userdata, uint64_t timeout);
MDNS_EXPORT int mdns_query_ipv4(mdnshandle_t handle, const ipv4_address_t *ipv4, struct mdns_callback *callback, void *userdata, uint64_t timeout);
MDNS_EXPORT int mdns_query_ipv6(mdnshandle_t handle, const ipv6_address_t *ipv6, struct mdns_callback *callback, void *userdata, uint64_t timeout);
MDNS_EXPORT int mdns_server_step(mdnshandle_t handle);
MDNS_EXPORT int mdns_add_service(mdnshandle_t handle, mdns_service_item_t *item);

#undef MDNS_EXPORT

#ifdef __cplusplus
}
#endif

#endif
