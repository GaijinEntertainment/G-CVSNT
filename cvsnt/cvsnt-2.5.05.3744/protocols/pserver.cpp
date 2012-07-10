/*	cvsnt pserver auth interface
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

#ifdef _WIN32
// Microsoft braindamage reversal.  
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <netdb.h>
#include <pwd.h>
#include <unistd.h>
#endif

#define MODULE pserver

#include "common.h"
#include "../version.h"

static int pserver_connect(const struct protocol_interface *protocol, int verify_only);
static int pserver_disconnect(const struct protocol_interface *protocol);
static int pserver_login(const struct protocol_interface *protocol, char *password);
static int pserver_logout(const struct protocol_interface *protocol);
static int pserver_auth_protocol_connect(const struct protocol_interface *protocol, const char *auth_string);
static int pserver_get_user_password(const char *username, const char *server, const char *port, const char *directory, char *password, int password_len);
static int pserver_set_user_password(const char *username, const char *server, const char *port, const char *directory, const char *password);
static int pserver_read_data(const struct protocol_interface *protocol, void *data, int length);
static int pserver_write_data(const struct protocol_interface *protocol, const void *data, int length);
static int pserver_flush_data(const struct protocol_interface *protocol);
static int pserver_shutdown(const struct protocol_interface *protocol);

static int init(const struct plugin_interface *plugin);
static int destroy(const struct plugin_interface *plugin);
static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param);

struct protocol_interface pserver_protocol_interface =
{
	{
		PLUGIN_INTERFACE_VERSION,
		":pserver: protocol",CVSNT_PRODUCTVERSION_STRING,"PserverProtocol",
		init,
		destroy,
		get_interface,
	#ifdef _WIN32
		win32config
	#else
		NULL
	#endif
	},
	"pserver",
	"pserver " CVSNT_PRODUCTVERSION_STRING,
	":pserver[;keyword=value...]:[username[:password]@]host[:port][:]/path",

	elemHostname, /* Required elements */
	elemUsername|elemPassword|elemHostname|elemPort|elemTunnel, /* Valid elements */

	NULL, /* validate */
	pserver_connect,
	pserver_disconnect,
	pserver_login,
	pserver_logout,
	NULL, /* start_encryption */
	pserver_auth_protocol_connect,
	pserver_read_data,
	pserver_write_data,
	pserver_flush_data,
	pserver_shutdown,
	NULL, /* impersonate */
	NULL, /* validate_keyword */
	NULL, /* get_keyword_help */
	NULL, /* server_read_data */
	NULL, /* server_write_data */
	NULL, /* server_flush_data */
	NULL, /* server_shutdown */
};

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
	return (void*)&pserver_protocol_interface;
}

plugin_interface *get_plugin_interface()
{
	return &pserver_protocol_interface.plugin;
}

int pserver_connect(const struct protocol_interface *protocol, int verify_only)
{
	char crypt_password[64];
	const char *begin_request = "BEGIN AUTH REQUEST";
	const char *end_request = "END AUTH REQUEST";
	const char *username = NULL;
	CScramble scramble;

	username = get_username(current_server()->current_root);

	if(!username || !current_server()->current_root->hostname || !current_server()->current_root->directory)
		return CVSPROTO_BADPARMS;

	if(tcp_connect(current_server()->current_root))
		return CVSPROTO_FAIL;
	if(current_server()->current_root->password)
		strncpy(crypt_password,scramble.Scramble(current_server()->current_root->password),sizeof(crypt_password));
	else
	{
		if(pserver_get_user_password(username,current_server()->current_root->hostname,current_server()->current_root->port,current_server()->current_root->directory,crypt_password,sizeof(crypt_password)))
		{
			/* Using null password - trace something out here */
			server_error(0,"Empty password used - try 'cvs login' with a real password\n"); 
			strncpy(crypt_password,scramble.Scramble(""),sizeof(crypt_password));
		}
	}

	if(verify_only)
	{
		begin_request = "BEGIN VERIFICATION REQUEST";
		end_request = "END VERIFICATION REQUEST";
	}

	if(tcp_printf("%s\n%s\n%s\n%s\n%s\n",begin_request,current_server()->current_root->directory,username,crypt_password,end_request)<0)
		return CVSPROTO_FAIL;
	return CVSPROTO_SUCCESS;
}

int pserver_disconnect(const struct protocol_interface *protocol)
{
	if(tcp_disconnect())
		return CVSPROTO_FAIL;
	return CVSPROTO_SUCCESS;
}

int pserver_login(const struct protocol_interface *protocol, char *password)
{
	const char *username = NULL;
	CScramble scramble;

	username = get_username(current_server()->current_root);

	/* Store username & encrypted password in password store */
	if(pserver_set_user_password(username,current_server()->current_root->hostname,current_server()->current_root->port,current_server()->current_root->directory,scramble.Scramble(password)))
	{
		server_error(1,"Failed to store password");
	}

	return CVSPROTO_SUCCESS;
}

int pserver_logout(const struct protocol_interface *protocol)
{
	const char *username = NULL;

	username = get_username(current_server()->current_root);
	if(pserver_set_user_password(username,current_server()->current_root->hostname,current_server()->current_root->port,current_server()->current_root->directory,NULL))
	{
		server_error(1,"Failed to delete password");
	}
	return CVSPROTO_SUCCESS;
}

int pserver_auth_protocol_connect(const struct protocol_interface *protocol, const char *auth_string)
{
	char *tmp;
	CScramble scramble;

    if (!strcmp (auth_string, "BEGIN VERIFICATION REQUEST"))
		pserver_protocol_interface.verify_only = 1;
    else if (!strcmp (auth_string, "BEGIN AUTH REQUEST"))
		pserver_protocol_interface.verify_only = 0;
	else
		return CVSPROTO_NOTME;

    /* Get the three important pieces of information in order. */
    /* See above comment about error handling.  */
    server_getline (protocol, &pserver_protocol_interface.auth_repository, PATH_MAX);
    server_getline (protocol, &pserver_protocol_interface.auth_username, PATH_MAX);
    server_getline (protocol, &pserver_protocol_interface.auth_password, PATH_MAX);

    /* ... and make sure the protocol ends on the right foot. */
    /* See above comment about error handling.  */
    server_getline(protocol, &tmp, PATH_MAX);
    if (strcmp (tmp,
		pserver_protocol_interface.verify_only ?
		"END VERIFICATION REQUEST" : "END AUTH REQUEST")
	!= 0)
    {
		server_error (1, "bad auth protocol end: %s", tmp);
		free(tmp);
    }

	const char *unscrambled_password = scramble.Unscramble(pserver_protocol_interface.auth_password);
	if(!unscrambled_password || !*unscrambled_password)
	{
		CServerIo::trace(1,"PROTOCOL VIOLATION: Invalid scrambled password sent by client.  Assuming blank for compatibility.  Report bug to client vendor.");
		unscrambled_password="";
	}
	strcpy(pserver_protocol_interface.auth_password, unscrambled_password);

	free(tmp);
	return CVSPROTO_SUCCESS;
}

int pserver_get_user_password(const char *username, const char *server, const char *port, const char *directory, char *password, int password_len)
{
	char tmp[1024];

	if(port)
		snprintf(tmp,sizeof(tmp),":pserver:%s@%s:%s:%s",username,server,port,directory);
	else
		snprintf(tmp,sizeof(tmp),":pserver:%s@%s:%s",username,server,directory);
	if(!CGlobalSettings::GetUserValue("cvsnt","cvspass",tmp,password,password_len))
		return CVSPROTO_SUCCESS;
	else
		return CVSPROTO_FAIL;
}

int pserver_set_user_password(const char *username, const char *server, const char *port, const char *directory, const char *password)
{
	char tmp[1024];

	if(port)
		snprintf(tmp,sizeof(tmp),":pserver:%s@%s:%s:%s",username,server,port,directory);
	else
		snprintf(tmp,sizeof(tmp),":pserver:%s@%s:%s",username,server,directory);
	if(!CGlobalSettings::SetUserValue("cvsnt","cvspass",tmp,password))
		return CVSPROTO_SUCCESS;
	else
		return CVSPROTO_FAIL;
}

int pserver_read_data(const struct protocol_interface *protocol, void *data, int length)
{
	return tcp_read(data,length);
}

int pserver_write_data(const struct protocol_interface *protocol, const void *data, int length)
{
	return tcp_write(data,length);
}

int pserver_flush_data(const struct protocol_interface *protocol)
{
	return 0; // TCP/IP is always flushed
}

int pserver_shutdown(const struct protocol_interface *protocol)
{
	return tcp_shutdown();
}
