/* CVS ext auth interface
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

#define MODULE ext

#include "common.h"
#include "../version.h"

static void ext_destroy(const struct protocol_interface *protocol);
static int ext_connect(const struct protocol_interface *protocol, int verify_only);
static int ext_disconnect(const struct protocol_interface *protocol);
static int ext_read_data(const struct protocol_interface *protocol, void *data, int length);
static int ext_write_data(const struct protocol_interface *protocol, const void *data, int length);
static int ext_flush_data(const struct protocol_interface *protocol);
static int ext_shutdown(const struct protocol_interface *protocol);
static int ext_validate_keyword(const struct protocol_interface *protocol, cvsroot *root, const char *keyword, const char *value);
static const char *ext_get_keyword_help(const struct protocol_interface *protocol);

static int expand_command_line(char *result, int length, const char *command, const cvsroot* root);

static int current_in=-1, current_out=-1;

static int init(const struct plugin_interface *plugin);
static int destroy(const struct plugin_interface *plugin);
static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param);

struct protocol_interface ext_protocol_interface =
{
	{
		PLUGIN_INTERFACE_VERSION,
		":ext: protocol",CVSNT_PRODUCTVERSION_STRING,NULL,
		init,
		destroy,
		get_interface,
		NULL
	},

	"ext",
	"ext "CVSNT_PRODUCTVERSION_STRING,
	":ext[{program}][;keyword=value...]:[user@]host[:]/path",

	elemHostname,				/* Required elements */
	elemUsername|elemHostname,	/* Valid elements */

	NULL, /* validate_details */
	ext_connect,
	ext_disconnect,
	NULL, /* login */
	NULL, /* logout */
	NULL, /* start_encryption */
	NULL, /* auth_protocol_connect */
	ext_read_data,
	ext_write_data,
	ext_flush_data,
	ext_shutdown,
	NULL, /* impersonate */
	ext_validate_keyword,
	ext_get_keyword_help,
	NULL, /* read_data */
	NULL, /* write_data */
	NULL, /* flush_data */
	NULL, /* shutdown */
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
	return (void*)&ext_protocol_interface;
}

plugin_interface *get_plugin_interface()
{
	return &ext_protocol_interface.plugin;
}

int ext_connect(const struct protocol_interface *protocol, int verify_only)
{
	char command[256],command_line[1024];
	const char *env;

	if(!current_server()->current_root->hostname || !current_server()->current_root->directory || current_server()->current_root->port)
		return CVSPROTO_BADPARMS;

	if(current_server()->current_root->optional_1)
		expand_command_line(command_line,sizeof(command_line),current_server()->current_root->optional_1,current_server()->current_root); // CVSROOT parameter
	else if(!CGlobalSettings::GetUserValue("cvsnt","ext","command",command,sizeof(command)))
		expand_command_line(command_line,sizeof(command_line),command,current_server()->current_root); // CVSROOT parameter
	else if((env = CProtocolLibrary::GetEnvironment("CVS_EXT"))!=NULL) // cvsnt environment
		expand_command_line(command_line,sizeof(command_line),env,current_server()->current_root);
	else if((env = CProtocolLibrary::GetEnvironment("CVS_RSH"))!=NULL) // legacy cvs environment
	{
		//  People are amazingly anal about the handling of this env. so we try to make
		//  it as unixy as possible.  Personally I'd rather ditch it as it's so limiting.
		if(env[0]=='"') // Strip quotes.  It's incorrect to put quotes around a CVS_RSH but it does happen
			env++;
		int l=strlen(env);
		if(env[l-1]=='"')
			l--;
		if(current_server()->current_root->username)
			snprintf(command_line,sizeof(command_line),"\"%-*.*s\" %s -l \"%s\"",l,l,env,current_server()->current_root->hostname,get_username(current_server()->current_root));
		else
			snprintf(command_line,sizeof(command_line),"\"%-*.*s\" %s",l,l,env,current_server()->current_root->hostname);
	}
	else
	{
		if(current_server()->current_root->username)
			expand_command_line(command_line,sizeof(command_line),"ssh -l \"%u\" %h",current_server()->current_root);
		else
			expand_command_line(command_line,sizeof(command_line),"ssh %h",current_server()->current_root);
	}

	strcat(command_line," ");
	if(current_server()->current_root->optional_2)
		strcat(command_line,current_server()->current_root->optional_2);
	else if((env=CProtocolLibrary::GetEnvironment("CVS_SERVER"))!=NULL)
		strcat(command_line,env);
	else
		strcat(command_line,"cvs");
	strcat(command_line," server");
	if(run_command(command_line,&current_in, &current_out, NULL))
		return CVSPROTO_FAIL;

	return CVSPROTO_SUCCESS_NOPROTOCOL; /* :ext: doesn't need login response */
}

int ext_disconnect(const struct protocol_interface *protocol)
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

int ext_read_data(const struct protocol_interface *protocol, void *data, int length)
{
	return read(current_out,data,length);
}

int ext_write_data(const struct protocol_interface *protocol, const void *data, int length)
{
	return write(current_in,data,length);
}

int ext_flush_data(const struct protocol_interface *protocol)
{
	return 0;
}

int ext_shutdown(const struct protocol_interface *protocol)
{
	return 0;
}

int expand_command_line(char *result, int length, const char *command, const cvsroot* root)
{
	const char *p;
	char *q;

	q=result;
	for(p=command; *p && (q-result)<length; p++)
	{
		if(*p=='%')
		{
			switch(*(p+1))
			{
			case 0:
				p--; // Counteract p++ below, so we end on NULL
				break;
			case '%':
				*(q++)='%';
				break;
			case 'u':
				strcpy(q, get_username(root));
				q+=strlen(q);
				break;
			case 'h':
				strcpy(q, root->hostname);
				q+=strlen(q);
				break;
			case 'd':
				strcpy(q, root->directory);
				q+=strlen(q);
				break;
			}
			p++;
		}
		else
			*(q++)=*p;
	}		
	*(q++)='\0';
	return 0;
}

int ext_validate_keyword(const struct protocol_interface *protocol, cvsroot *root, const char *keyword, const char *value)
{
	if(!strcasecmp(keyword,"command"))
	{
		root->optional_1 = strdup(value);
		return CVSPROTO_SUCCESS;
	}
	if(!strcasecmp(keyword,"server")) 
	{
		root->optional_2 = strdup(value);
		return CVSPROTO_SUCCESS;
	}
	return CVSPROTO_FAIL;
}

const char *ext_get_keyword_help(const struct protocol_interface *protocol)
{
	return "command\0Command to execute\0server\0Remote command (default 'cvs')\0";
}
