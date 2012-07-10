/* CVS gserver auth interface

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

// LICENSE NOTE:  Inherits code from old CVS auth, so cannot be LGPL.
// Only error handling and some krb5 stuff though

#ifdef _WIN32
// Microsoft braindamage reversal.  
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h> // we don't use these, but hpux needs them as its crap
#include <unistd.h>
#define GSS_MIT /* Unix always uses MIT */
#endif
#include <ctype.h>

#ifdef _WIN32
#define socket_errno WSAGetLastError()
#define sock_strerror gai_strerror
#else
#define closesocket close
#define socket_errno errno
#define sock_strerror strerror
#endif

#define MODULE gserver

#include "common.h"
#include "../version.h"

#if !defined(GSS_AD) && !defined(GSS_MIT)
#error "Must define GSS_AD or GSS_MIT"
#endif

#if !defined(_WIN32) || !defined(GSS_AD)
#undef HAVE_STRING_H /* For Kerberos */

#ifdef HAVE_GSSAPI_H
#include <gssapi.h>
#endif
#ifdef HAVE_GSSAPI_GSSAPI_H
#include <gssapi/gssapi.h>
#endif /* freebsd needs 'else' but this breaks on other platforms so is incorrect for the main distribution.  Fix manually if building on that platform */
#ifdef HAVE_GSSAPI_GSSAPI_GENERIC_H
#include <gssapi/gssapi_generic.h>
#endif
#include <krb5.h>

#else
#include "../windows-NT/gss-ad/gssapi.h"
#include "../windows-NT/gss-ad/krb5.h"
#endif

#if defined(_WIN32) && !defined(GSS_AD)
#pragma comment(lib,"krb5_32.lib")
#pragma comment(lib,"gssapi32.lib")
#endif

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 50
#endif

static int gserver_connect(const struct protocol_interface *protocol, int verify_only);
static int gserver_disconnect(const struct protocol_interface *protocol);
static int gserver_wrap(const struct protocol_interface *protocol, int unwrap, int encrypt, const void *input, int size, void *output, int *newsize);
static int gserver_auth_protocol_connect(const struct protocol_interface *protocol, const char *auth_string);
static int gserver_read_data(const struct protocol_interface *protocol, void *data, int length);
static int gserver_write_data(const struct protocol_interface *protocol, const void *data, int length);
static int gserver_flush_data(const struct protocol_interface *protocol);
static void gserver_error(int error, OM_uint32 stat_min, OM_uint32 stat_maj, const char *msg);
static int gssapi_unwrap(const void *buffer, int size, void *output, int *newsize);
static int gssapi_wrap(int encrypt, const void *buffer, int size, void *output, int *newsize);
static int gserver_shutdown(const struct protocol_interface *protocol);
#ifdef GSS_AD
static int gserver_impersonate(const struct protocol_interface *protocol, const char *username, void *reserved);
#endif
static int gserver_validate_keyword(const struct protocol_interface *protocol, cvsroot *root, const char *keyword, const char *value);
static const char *gserver_get_keyword_help(const struct protocol_interface *protocol);

static int init(const struct plugin_interface *plugin);
static int destroy(const struct plugin_interface *plugin);
static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param);

struct protocol_interface gserver_protocol_interface =
{
	{
		PLUGIN_INTERFACE_VERSION,
		":gserver: protocol",CVSNT_PRODUCTVERSION_STRING,"GserverProtocol",
		init,
		destroy,
		get_interface,
	#ifdef _WIN32
		win32config
	#else
		NULL
	#endif
	},

	"gserver",
#ifdef GSS_AD
	"gserver "CVSNT_PRODUCTVERSION_STRING" (Active Directory)",
#else
	"gserver "CVSNT_PRODUCTVERSION_STRING" (MIT)",
#endif
	":gserver[;keyword=value...]:host[:port][:]/path",

	elemHostname,	/* Required elements */
	elemHostname|elemPort|elemTunnel,	/* Valid elements */

	NULL, /* validate */
	gserver_connect,
	gserver_disconnect,
	NULL, /* login */
	NULL, /* logout */
	gserver_wrap, 
	gserver_auth_protocol_connect,
	gserver_read_data,
	gserver_write_data,
	gserver_flush_data,
	gserver_shutdown,
#ifdef GSS_AD
	gserver_impersonate, 
#else
	NULL, /* impersonate */
#endif
	gserver_validate_keyword,
	gserver_get_keyword_help,
	NULL, /* server_read_data */
	NULL, /* server_write_data */
	NULL, /* server_flush_data */
	NULL, /* server_shutdown */
};

/* This is needed for GSSAPI encryption.  */
static gss_ctx_id_t gcontext;

static int init(const struct plugin_interface *plugin)
{
	return 0;
}

static int destroy(const struct plugin_interface *plugin)
{
	protocol_interface *protocol = (protocol_interface*)plugin;

	free(protocol->auth_username);
	free(protocol->auth_password);
	free(protocol->auth_repository);
	return 0;
}

static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param)
{
	if(interface_type!=pitProtocol)
		return NULL;

	set_current_server((const struct server_interface*)param);
	return (void*)&gserver_protocol_interface;
}

plugin_interface *get_plugin_interface()
{
	return &gserver_protocol_interface.plugin;
}

int gserver_connect(const struct protocol_interface *protocol, int verify_only)
{
    char buf[12288];
    gss_buffer_desc *tok_in_ptr, tok_in, tok_out;
    OM_uint32 stat_min, stat_maj;
    gss_name_t server_name;

	if(current_server()->current_root->username || !current_server()->current_root->hostname || !current_server()->current_root->directory)
		return CVSPROTO_BADPARMS;

	if(tcp_connect(current_server()->current_root))
		return CVSPROTO_FAIL;

	tcp_printf("BEGIN GSSAPI REQUEST\n");

	{
		struct addrinfo *ai;
		struct addrinfo hint={0};
		hint.ai_flags=AI_CANONNAME;
		if(!getaddrinfo(cvs::idn(current_server()->current_root->hostname),NULL,&hint,&ai))
		{
			if(isdigit(ai->ai_canonname[0]))
			{
				char host[256];
				if(!getnameinfo(ai->ai_addr,ai->ai_addrlen,host,sizeof(host),NULL,0,0))
					sprintf (buf, "cvs@%s", host);
				else
					sprintf (buf, "cvs@%s", (const char *)cvs::decode_idn(ai->ai_canonname));
			}
			else
				sprintf (buf, "cvs@%s", (const char *)cvs::decode_idn(ai->ai_canonname));

			freeaddrinfo(ai);
		}
		else
			sprintf (buf, "cvs@%s", current_server()->current_root->hostname);
	}

#ifdef GSS_AD
	if(current_server()->current_root->optional_1)
	{
		strcat(buf,"@");
		strcat(buf,current_server()->current_root->optional_1);
	}
#endif

    tok_in.length = strlen (buf);
    tok_in.value = buf;
    gss_import_name (&stat_min, &tok_in, GSS_C_NT_HOSTBASED_SERVICE,
		     &server_name);

    tok_in_ptr = GSS_C_NO_BUFFER;
    gcontext = GSS_C_NO_CONTEXT;

    do
    {
	stat_maj = gss_init_sec_context (&stat_min, GSS_C_NO_CREDENTIAL,
					 &gcontext, server_name,
					 GSS_C_NULL_OID,
					 (GSS_C_MUTUAL_FLAG
					  | GSS_C_REPLAY_FLAG | GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG),
					 0, NULL, tok_in_ptr, NULL, &tok_out,
					 NULL, NULL);
	if (stat_maj != GSS_S_COMPLETE && stat_maj != GSS_S_CONTINUE_NEEDED)
	{
		gserver_error(1,stat_min,stat_maj,"GSSAPI authentication failed");
		return CVSPROTO_FAIL;
	}

	if (tok_out.length == 0)
	{
	    tok_in.length = 0;
	}
	else
	{
	    short len;
	    int need;

	    len = htons((short)tok_out.length);
	    if (tcp_write (&len, 2) < 0)
			server_error (1, "cannot send: %s", sock_strerror(socket_errno));
	    if (tcp_write (tok_out.value, tok_out.length) < 0)
			server_error (1, "cannot send: %s", sock_strerror(socket_errno));

	    tcp_read (&len, 2);
	    need = ntohs(len);

	    if (need > sizeof buf)
	    {
		int got;

		/* This usually means that the server sent us an error
		   message.  Read it byte by byte and print it out.
		   FIXME: This is a terrible error handling strategy.
		   However, even if we fix the server, we will still
		   want to do this to work with older servers.  */
		*(short*)buf=len;
		got = tcp_read (buf + 2, sizeof buf - 2);
		if (got < 0)
		    server_error (1, "receive from server %s: %s",
			   current_server()->current_root->hostname, sock_strerror(socket_errno));
		buf[got + 2] = '\0';
		if (buf[got + 1] == '\n')
		    buf[got + 1] = '\0';
		server_error (1, "error from server %s: %s", current_server()->current_root->hostname,
		       buf);
	    }

	    tcp_read (buf, need);
	    tok_in.length = need;
	}

	tok_in.value = buf;
	tok_in_ptr = &tok_in;
    }
    while (stat_maj == GSS_S_CONTINUE_NEEDED);


	return CVSPROTO_SUCCESS;
}

int gserver_disconnect(const struct protocol_interface *protocol)
{
	if(tcp_disconnect())
		return CVSPROTO_FAIL;
	return CVSPROTO_SUCCESS;
}

int gserver_auth_protocol_connect(const struct protocol_interface *protocol, const char *auth_string)
{
    char hostname[MAXHOSTNAMELEN];
    struct addrinfo *ai,hints={0};
    gss_buffer_desc tok_in, tok_out;
    char buf[4096], *gssreadstr=NULL;
    OM_uint32 stat_min, stat_maj, ret;
    gss_name_t server_name, client_name;
    gss_cred_id_t server_creds;
    size_t nbytes;
    gss_OID mechid;

	if (strcmp (auth_string, "BEGIN GSSAPI REQUEST"))
		return CVSPROTO_NOTME;

	gethostname (hostname, sizeof hostname);
	hints.ai_flags=AI_CANONNAME;
	if(getaddrinfo(cvs::idn(hostname),NULL,&hints,&ai))
		server_error (1, "can't get canonical hostname");

	sprintf (buf, "cvs@%s", (const char *)cvs::decode_idn(ai->ai_canonname));
    tok_in.value = buf;
    tok_in.length = strlen (buf);

	freeaddrinfo(ai);

    if (gss_import_name (&stat_min, &tok_in, GSS_C_NT_HOSTBASED_SERVICE,
			 &server_name) != GSS_S_COMPLETE)
		server_error (1, "could not import GSSAPI service name %s", buf);

	/* Acquire the server credential to verify the client's
       authentication.  */
    stat_maj = gss_acquire_cred (&stat_min, server_name, 0, GSS_C_NULL_OID_SET,
			  GSS_C_ACCEPT, &server_creds,
			  NULL, NULL);
	if(stat_maj != GSS_S_COMPLETE)
	{
		gserver_error (1, stat_min, stat_maj, "could not acquire GSSAPI server credentials");
		return CVSPROTO_FAIL;
	}

    gss_release_name (&stat_min, &server_name);

	memset(buf,'\0',sizeof(buf)-1);
	do
	{
		/* The client will send us a two byte length followed by that many
		bytes.  */
		if (read (current_server()->in_fd, buf, 2) != 2)
			server_error (1, "read of length failed");

		// the packet size of Kerberos on Windows 2003 Server is 12,000 bytes
		nbytes = ntohs(*(short*)buf);

		gssreadstr = (char *)malloc(sizeof(char)*((((unsigned long)nbytes)<12188)?12288:((unsigned long)nbytes)+100));
		if (gssreadstr==NULL)
		{
			CServerIo::trace(99,"gserver packet too large %lu (malloc failed)",(unsigned long)nbytes);
			server_error (1, "gserver packet too large (cannot malloc)");
			return CVSPROTO_FAIL;
		}
		if (read (current_server()->in_fd, gssreadstr, nbytes) != nbytes)
			server_error (1, "read of data failed");

		gcontext = GSS_C_NO_CONTEXT;
		tok_in.length = nbytes;
		tok_in.value = gssreadstr;
		tok_out.length=0;
		tok_out.value=NULL;

		stat_maj = gss_accept_sec_context (&stat_min,
									&gcontext,	/* context_handle */
									server_creds,	/* verifier_cred_handle */
									&tok_in,	/* input_token */
									NULL,		/* channel bindings */
									&client_name,	/* src_name */
									&mechid,	/* mech_type */
									&tok_out,	/* output_token */
									&ret,
									NULL,	 	/* ignore time_rec */
									NULL);		/* ignore del_cred_handle */
		if (stat_maj != GSS_S_COMPLETE && stat_maj != GSS_S_CONTINUE_NEEDED)
		{
			gserver_error (1, stat_min, stat_maj, "could not verify credentials");
			return CVSPROTO_FAIL;
		}
		if (tok_out.length != 0)
		{
			short len;

		    len = htons((short)tok_out.length);
			if (write(current_server()->out_fd, &len, 2) < 0)
				server_error (1, "cannot send: %s", sock_strerror(socket_errno));
			if (write(current_server()->out_fd, tok_out.value, tok_out.length) < 0)
				server_error (1, "cannot send: %s", sock_strerror(socket_errno));
		}
		free(gssreadstr);
		gssreadstr=NULL;
    }
    while (stat_maj == GSS_S_CONTINUE_NEEDED);

#ifdef GSS_AD
	{
		gss_buffer_desc desc;
		if(gss_display_name(&stat_min, client_name, &desc, &mechid) != GSS_S_COMPLETE)
		{
			server_error (1, "access denied by kerberos name check");
		}
		else
		{
       		gserver_protocol_interface.auth_username = strdup((char*)desc.value);
		}
	}
#else
    /* FIXME: Use Kerberos v5 specific code to authenticate to a user.
       We could instead use an authentication to access mapping.  */
    {
	krb5_context kc;
	krb5_principal p;
	gss_buffer_desc desc;

	krb5_init_context (&kc);
	if (gss_display_name (&stat_min, client_name, &desc,
			      &mechid) != GSS_S_COMPLETE
	    || krb5_parse_name (kc, (char*)((gss_buffer_t) &desc)->value, &p) != 0
	    || krb5_aname_to_localname (kc, p, sizeof buf, buf) != 0
        || (krb5_kuserok (kc, p, buf) != TRUE))
	{
	    server_error (1, "access denied by kerberos name check");
	}
        else
	{
       	gserver_protocol_interface.auth_username = strdup(buf);
	}
	krb5_free_principal (kc, p);
	krb5_free_context (kc);
    }
#endif

	return CVSPROTO_SUCCESS;
}

int gserver_read_data(const struct protocol_interface *protocol, void *data, int length)
{
	return tcp_read(data,length);
}

int gserver_write_data(const struct protocol_interface *protocol, const void *data, int length)
{
	return tcp_write(data,length);
}

int gserver_flush_data(const struct protocol_interface *protocol)
{
	return 0; // TCP/IP is always flushed
}

void gserver_error(int error, OM_uint32 stat_min, OM_uint32 stat_maj, const char *msg)
{
    OM_uint32 message_context;
    OM_uint32 new_stat_min;
	gss_buffer_desc tok_out;

	if(stat_maj)
	{
		message_context = 0;
		gss_display_status (&new_stat_min, stat_maj, GSS_C_GSS_CODE,
					  GSS_C_NULL_OID, &message_context, &tok_out);
		server_error (stat_min?0:error, "%s: %s\n", msg, (char *) tok_out.value);
	}

	if(stat_min)
	{
		message_context = 0;
		gss_display_status (&new_stat_min, stat_min, GSS_C_MECH_CODE,
					GSS_C_NULL_OID, &message_context, &tok_out);
		server_error (error, "%s: %s\n", msg, (char *) tok_out.value);
	}
}

int gserver_wrap(const struct protocol_interface *protocol, int unwrap, int encrypt, const void *input, int size, void *output, int *newsize)
{
	if(unwrap)
		return gssapi_unwrap(input,size,output,newsize);
	else
		return gssapi_wrap(encrypt,input,size,output,newsize);
	return 0;
}

/* Unwrap data using GSSAPI.  */

static int gssapi_unwrap(const void *buffer, int size, void *output, int *newsize)
{
    gss_buffer_desc inbuf, outbuf;
    OM_uint32 stat_min, stat_maj;
    int conf;

    inbuf.value = (void *) buffer;
    inbuf.length = size;

    if ((stat_maj = gss_unwrap (&stat_min, gcontext, &inbuf, &outbuf, &conf, NULL)) != GSS_S_COMPLETE)
	{
		gserver_error (1, stat_min, stat_maj, "gss_unwrap failed");
	}

    if ((int)outbuf.length > size)
	{
		server_error (1, "GSSAPI Assertion failed: outbuf.length > size");
	}

    memcpy (output, outbuf.value, outbuf.length);
	*newsize = outbuf.length;

    return 0;
}

/* Wrap data using GSSAPI.  */

static int gssapi_wrap(int encrypt, const void *buffer, int size, void *output, int *newsize)
{
    gss_buffer_desc inbuf, outbuf;
    OM_uint32 stat_min, stat_maj;
    int conf;

    inbuf.value = (void *)buffer;
    inbuf.length = size;

    if ((stat_maj = gss_wrap (&stat_min, gcontext, encrypt, (gss_qop_t)GSS_C_QOP_DEFAULT, &inbuf, &conf, &outbuf)) != GSS_S_COMPLETE)
	{
		gserver_error (1, stat_min, stat_maj, "gss_wrap failed");
	}

    memcpy (output, outbuf.value, outbuf.length);
	*newsize = outbuf.length;

    gss_release_buffer (&stat_min, &outbuf);

    return 0;
}

#ifdef GSS_AD
int gserver_impersonate(const struct protocol_interface *protocol, const char *username, void *reserved)
{
	if(ImpersonateSecurityContext(&gcontext)==SEC_E_OK)
		return CVSPROTO_SUCCESS;
	return CVSPROTO_FAIL;
}
#endif

int gserver_shutdown(const struct protocol_interface *protocol)
{
	return tcp_shutdown();
}

int gserver_validate_keyword(const struct protocol_interface *protocol, cvsroot *root, const char *keyword, const char *value)
{
#ifdef GSS_AD
   if(!strcasecmp(keyword,"realm"))
   {
      root->optional_1 = strdup(value);
      return CVSPROTO_SUCCESS;
   }
#endif
   return CVSPROTO_FAIL;
}

const char *gserver_get_keyword_help(const struct protocol_interface *protocol)
{
#ifdef GSS_AD
	return "realm\0Override default realm\0";
#else
	return "";
#endif
}

