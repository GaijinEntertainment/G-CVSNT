/*	cvsnt fork interface
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
#include <io.h>
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#endif

#define MODULE fork

#include "common.h"
#include "../version.h"

static int fork_connect(const struct protocol_interface *protocol, int verify_only);
static int fork_disconnect(const struct protocol_interface *protocol);
static int fork_read_data(const struct protocol_interface *protocol, void *data, int length);
static int fork_write_data(const struct protocol_interface *protocol, const void *data, int length);
static int fork_flush_data(const struct protocol_interface *protocol);
static int fork_shutdown(const struct protocol_interface *protocol);

static int current_in=-1, current_out=-1;

static int init(const struct plugin_interface *plugin);
static int destroy(const struct plugin_interface *plugin);
static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param);

struct protocol_interface fork_protocol_interface =
{
	{
		PLUGIN_INTERFACE_VERSION,
		":fork: protocol",CVSNT_PRODUCTVERSION_STRING,NULL,
		init,
		destroy,
		get_interface,
		NULL
	},

	"fork",
	"fork "CVSNT_PRODUCTVERSION_STRING,
	":fork[;keyword=value...]:/path",

	elemNone,		/* Required elements */
	elemNone,		/* Valid elements */

	NULL, /* validate */
	fork_connect,
	fork_disconnect,
	NULL, /* login */
	NULL, /* logout */
	NULL, /* start_encryption */
	NULL, /* auth_protocol_connect */
	fork_read_data,
	fork_write_data,
	fork_flush_data,
	fork_shutdown,
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
	return (void*)&fork_protocol_interface;
}

plugin_interface *get_plugin_interface()
{
	return &fork_protocol_interface.plugin;
}

int fork_connect(const struct protocol_interface *protocol, int verify_only)
{
	char command_line[256];

	if(current_server()->current_root->username || current_server()->current_root->hostname || !current_server()->current_root->directory || current_server()->current_root->port)
		return CVSPROTO_BADPARMS;

	snprintf(command_line,sizeof(command_line),"%s server",current_server()->cvs_command);

	if(run_command(command_line,&current_in, &current_out, NULL))
		return CVSPROTO_FAIL;

	return CVSPROTO_SUCCESS_NOPROTOCOL; /* :fork: doesn't need login response */
}

int fork_disconnect(const struct protocol_interface *protocol)
{
	if(current_in>0)
	{
		close(current_in);
		current_in=-1;
	}
	if(current_out>0)
	{
		close(current_out);
		current_in=-1;
	}
	return CVSPROTO_SUCCESS;
}

int fork_read_data(const struct protocol_interface *protocol, void *data, int length)
{
	int res = read(current_out,data,length);
	return res;
}

int fork_write_data(const struct protocol_interface *protocol, const void *data, int length)
{
	int res = write(current_in,data,length);
	return res;
}

int fork_flush_data(const struct protocol_interface *protocol)
{
	return 0;
}

int fork_shutdown(const struct protocol_interface *protocol)
{
	return 0;
}

