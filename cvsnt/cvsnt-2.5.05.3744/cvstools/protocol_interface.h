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
	elemTunnel	 = 0x0010,

	flagSystem = 0x4000,
	flagAlwaysEncrypted = 0x8000
};

struct protocol_interface
{
	plugin_interface plugin;

	const char *name;	/* Protocol name ('Foobar')*/
	const char *version;	/* Protocol version ('Foobar version 1.0') */
	const char *syntax;	/* Syntax (':foobar:user@server[:port][:]path') */

	unsigned required_elements; /* Required CVSROOT elements */
	unsigned valid_elements;	/* Valid CVSROOT elements */

	int (*validate_details)(const struct protocol_interface *protocol, cvsroot *newroot);
	int (*connect)(const struct protocol_interface *protocol, int verify_only);
	int (*disconnect)(const struct protocol_interface *protocol);
	int (*login)(const struct protocol_interface *protocol, char *password);
	int (*logout)(const struct protocol_interface *protocol);
	int (*wrap)(const struct protocol_interface *protocol, int unwrap, int encrypt, const void *input, int size, void *output, int *newsize);
	int (*auth_protocol_connect)(const struct protocol_interface *protocol, const char *auth_string);
	int (*read_data)(const struct protocol_interface *protocol, void *data, int length);
	int (*write_data)(const struct protocol_interface *protocol, const void *data, int length);
	int (*flush_data)(const struct protocol_interface *protocol);
	int (*shutdown)(const struct protocol_interface *protocol);
	int (*impersonate)(const struct protocol_interface *protocol, const char *username, void *reserved);
	int (*validate_keyword)(const struct protocol_interface *protocol, cvsroot *root, const char *keyword, const char *value);
	const char *(*get_keyword_help)(const struct protocol_interface *protocol);
	int (*server_read_data)(const struct protocol_interface *protocol, void *data, int length);
	int (*server_write_data)(const struct protocol_interface *protocol, const void *data, int length);
	int (*server_flush_data)(const struct protocol_interface *protocol);
	int (*server_shutdown)(const struct protocol_interface *protocol);

	/* The following should be filled in by auth_protocol_connect before it returns SUCCESS */
	int verify_only;
	char *auth_username;
	char *auth_password;
	char *auth_repository;
	char *auth_proxyname; /* For proxies, override the local repository name */
	char *auth_optional_1;
	char *auth_optional_2;
	char *auth_optional_3;
};

enum proto_type
{
	ptAny,
	ptClient,
	ptServer
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
	int (*getpass)(const struct server_interface *server, char *password, int max_length, const char *prompt); /* return 1 OK, 0 Cancel */
	int (*yesno)(const struct server_interface *server, const char *message, const char *title, int withcancel); /* Return -1 cancel, 0 No, 1 Yes */
	int (*set_encrypted_channel)(const struct server_interface *server, int encrypt); /* Set when plaintext I/O no longer valid */
	const char *(*enumerate_protocols)(const struct server_interface *server, int *context, enum proto_type type);
};

enum
{
	CVSPROTO_SUCCESS			=  0,
	CVSPROTO_FAIL				= -1, /* Generic failure (errno set) */
	CVSPROTO_BADPARMS			= -2, /* (Usually) wrong parameters from cvsroot string */
	CVSPROTO_AUTHFAIL			= -3, /* Authorization or login failed */
	CVSPROTO_NOTME				= -4, /* Not for this protocol (used by protocol connect) */
	CVSPROTO_NOTIMP				= -5, /* Not implemented */
	CVSPROTO_SUCCESS_NOPROTOCOL = -6, /* Connect succeeded, don't wait for 'I LOVE YOU' response */
};

#ifndef _WIN32
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
