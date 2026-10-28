/*
	CVSNT Helper application API
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd

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
/* _EXPORT */

#ifndef PROTOCOL_INTERFACE__H
#define PROTOCOL_INTERFACE__H

#ifdef __cplusplus
extern "C" {
#endif

#include "cvsroot.h"

/* Allowed CVSROOT elements */
enum
{
	elemNone	 = 0x0000,
	elemUsername = 0x0001,
	elemPassword = 0x0002,
	elemHostname = 0x0004,
	elemPort	 = 0x0008,
	elemTunnel	 = 0x0010
};

struct protocol_interface
{
	plugin_interface plugin;

	const char *name;	/* Protocol name ('Foobar')*/
	const char *version;	/* Protocol version ('Foobar version 1.0') */
	const char *syntax;	/* Syntax (':foobar:user@server[:port][:]path') */

	unsigned required_elements; /* Required CVSROOT elements */
	unsigned valid_elements;	/* Valid CVSROOT elements */

	void *unused_validate_details; /* unused slots keep the plugin ABI */
	int (*connect)(const struct protocol_interface *protocol, int verify_only);
	int (*disconnect)(const struct protocol_interface *protocol);
	int (*login)(const struct protocol_interface *protocol, char *password);
	int (*logout)(const struct protocol_interface *protocol);
	void *unused_wrap;
	int (*auth_protocol_connect)(const struct protocol_interface *protocol, const char *auth_string);
	int (*read_data)(const struct protocol_interface *protocol, void *data, int length);
	int (*write_data)(const struct protocol_interface *protocol, const void *data, int length);
	int (*flush_data)(const struct protocol_interface *protocol);
	int (*shutdown)(const struct protocol_interface *protocol);
	void *unused_impersonate;
	void *unused_validate_keyword;
	void *unused_get_keyword_help;
	void *unused_server_read_data;
	void *unused_server_write_data;
	void *unused_server_flush_data;
	void *unused_server_shutdown;

	/* The following should be filled in by auth_protocol_connect before it returns SUCCESS */
	int verify_only;
	char *auth_username;
	char *auth_password;
	char *auth_repository;
	char *unused_auth_proxyname;
	char *unused_auth_optional_1;
	char *unused_auth_optional_2;
	char *unused_auth_optional_3;
};

struct server_interface
{
	cvsroot *current_root;
	const char *library_dir;
	const char *config_dir;
	const char *cvs_command;
	int in_fd; /* FD for server input */
	int out_fd; /* FD for server output */

	int (*error)(const struct server_interface *server, int fatal, const char *text);
	void *unused_getpass;
	void *unused_yesno;
	void *unused_set_encrypted_channel;
	void *unused_enumerate_protocols;
};

enum
{
	CVSPROTO_SUCCESS			=  0,
	CVSPROTO_FAIL				= -1, /* Generic failure (errno set) */
	CVSPROTO_BADPARMS			= -2, /* (Usually) wrong parameters from cvsroot string */
	CVSPROTO_AUTHFAIL			= -3, /* Authorization or login failed */
	CVSPROTO_NOTME				= -4, /* Not for this protocol (used by protocol connect) */
	CVSPROTO_NOTIMP				= -5, /* Not implemented */
};

#if !defined(_WIN32) && !defined(__APPLE__)
#ifdef MODULE
/* This needs an extra level of indirection.  gcc bug? */
#define __x11432a(__mod,__func) __mod##_protocol_LTX_##__func
#define __x11432(__mod,__func) __x11432a(__mod,__func)
#define get_protocol_interface __x11432(MODULE,get_protocol_interface)
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
