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
#ifndef CVSROOT__H
#define CVSROOT__H

enum RootType
{
	RootTypeStandard = 1,
	RootTypeProxyAll = 2,
	RootTypeProxyWrite = 3
};

struct cvsroot {
    const char *original;		/* the complete source CVSroot string */
    char *method;		/* protocol name */
    const char *username;		/* the username or NULL if method == local */
    const char *password;		/* the username or NULL if method == local */
    const char *hostname;		/* the hostname or NULL if method == local */
    const char *port;			/* the port or zero if method == local */
    const char *directory;		/* the directory name */
	const char *proxyport; /* Port number of proxy */
	const char *proxyprotocol; /* Protocol type of proxy (http, SOAP, etc.) */
	const char *proxy; /* Proxy server */
	const char *proxyuser; /* Username for proxy */
	const char *proxypassword; /* Password for proxy */
	const char *unparsed_directory; /* unparsed directory name */
	const char *mapped_directory; /* Original directory, mapped to filesystem */
	const char *optional_1; /* Protocol defined keyword - also text between {...} */
	const char *optional_2; /* Protocol defined keyword */
	const char *optional_3; /* Protocol defined keyword */
	const char *optional_4; /* Protocol defined keyword */
	const char *optional_5; /* Protocol defined keyword */
	const char *optional_6; /* Protocol defined keyword */
	const char *optional_7; /* Protocol defined keyword */
	const char *optional_8; /* Protocol defined keyword */
    bool isremote;	/* true if we are doing remote access */
	bool password_used; /* true if original root had password */
	bool readwrite; /* true if repository is read/write */
	RootType type; /* type of root */
	const char *remote_server;	/* Server to proxy to */
	const char *remote_repository; /* Repository to proxy to */
	const char *remote_passphrase; /* Passphrase to authenticate us to proxy */
	const char *proxy_repository_root; /* the physical directory above the CVSROOT for the proxy server trigger files */
};

#endif
