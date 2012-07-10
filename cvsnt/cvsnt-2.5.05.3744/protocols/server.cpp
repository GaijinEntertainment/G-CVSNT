/* CVS server (rsh) auth interface
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
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

#define MODULE server

#include "common.h"
#include "../version.h"

static void server_destroy(const struct protocol_interface *protocol);
static int server_connect(const struct protocol_interface *protocol, int verify_only);
static int server_disconnect(const struct protocol_interface *protocol);
static int server_read_data(const struct protocol_interface *protocol, void *data, int length);
static int server_write_data(const struct protocol_interface *protocol, const void *data, int length);
static int server_flush_data(const struct protocol_interface *protocol);
static int server_shutdown(const struct protocol_interface *protocol);

static int init(const struct plugin_interface *plugin);
static int destroy(const struct plugin_interface *plugin);
static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param);

struct protocol_interface server_protocol_interface =
{
	{
		PLUGIN_INTERFACE_VERSION,
		":server: protocol",CVSNT_PRODUCTVERSION_STRING,NULL,
		init,
		destroy,
		get_interface,
		NULL
	},
	"server",
	"server "CVSNT_PRODUCTVERSION_STRING,
	":server[;keyword=value...]:[username[:password]@]host[:]/path",

	elemHostname, /* Required elements */
	elemUsername|elemPassword|elemHostname, /* Valid elements */

	NULL, /* validate */
	server_connect,
	server_disconnect,
	NULL, /* login */
	NULL, /* logout */
	NULL, /* start_encryption */
	NULL, /* auth_protocol_connect */
	server_read_data,
	server_write_data,
	server_flush_data,
	server_shutdown,
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
	return (void*)&server_protocol_interface;
}

plugin_interface *get_plugin_interface()
{
	return &server_protocol_interface.plugin;
}

int server_connect(const struct protocol_interface *protocol, int verify_only)
{
	char current_user[256];
	char remote_user[256];
	char tmp[32];
	unsigned char c;
	int listen_port=0;
	if(!current_server()->current_root->hostname || !current_server()->current_root->directory || current_server()->current_root->port)
		return CVSPROTO_BADPARMS;

	if(tcp_connect_bind(current_server()->current_root->hostname,"514",512,1023)<1)
		return CVSPROTO_FAIL;

#ifdef _WIN32
	{
		DWORD dwLen = 256;
		GetUserNameA(current_user,&dwLen);
	}
#else
	strncpy(current_user,getpwuid(geteuid())->pw_name,sizeof(current_user));
#endif

	if(current_server()->current_root->username)
		strncpy(remote_user,current_server()->current_root->username,sizeof(remote_user));
	else
		strncpy(remote_user,current_user,sizeof(remote_user));

	snprintf(tmp,sizeof(tmp),"%d",listen_port);
	if(tcp_write(tmp,strlen(tmp)+1)<1)
		return CVSPROTO_FAIL;
	if(tcp_write(current_user,strlen(current_user)+1)<1)
		return CVSPROTO_FAIL;
	if(tcp_write(remote_user,strlen(remote_user)+1)<1)
		return CVSPROTO_FAIL;

#define CMD "cvs server"

	if(tcp_write(CMD,sizeof(CMD))<1)
		return CVSPROTO_FAIL;
	
	if(tcp_read(&c,1)<1)
		return CVSPROTO_FAIL;

	if(c)
	{
		char msg[257];
		if((c=tcp_read(msg,256))<1)
			return CVSPROTO_FAIL;
		msg[c]='\0';
		server_error(0,"rsh server reported: %s",msg);
		return CVSPROTO_FAIL;
	}

	return CVSPROTO_SUCCESS_NOPROTOCOL; /* :server: doesn't need login response */
}

int server_disconnect(const struct protocol_interface *protocol)
{
	tcp_disconnect();
	return CVSPROTO_SUCCESS;
}

int server_read_data(const struct protocol_interface *protocol, void *data, int length)
{
	return tcp_read(data,length);
}

int server_write_data(const struct protocol_interface *protocol, const void *data, int length)
{
	return tcp_write(data,length);
}

int server_flush_data(const struct protocol_interface *protocol)
{
	return 0;
}

int server_shutdown(const struct protocol_interface *protocol)
{
	tcp_shutdown();
	return 0;
}
