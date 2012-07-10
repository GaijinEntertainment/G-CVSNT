/* CVS enumeration protocol
    Copyright (C) 2005 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA. */

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
#include <winsock2.h>
#else
#include <pwd.h>
#include <unistd.h>
#include <sys/socket.h>
#endif
#include <ctype.h>

#define MODULE enum

#include "common.h"
#include "../version.h"

static int enum_auth_protocol_connect(const struct protocol_interface *protocol, const char *auth_string);
static int enum_wrap(const struct protocol_interface *protocol, int unwrap, int encrypt, const void *input, int size, void *output, int *newsize);

static int init(const struct plugin_interface *plugin);
static int destroy(const struct plugin_interface *plugin);
static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param);

static protocol_interface enum_protocol_interface =
{
	{
		PLUGIN_INTERFACE_VERSION,
		"CVSNT enumeration protocol",CVSNT_PRODUCTVERSION_STRING,"EnumProtocol",
		init,
		destroy,
		get_interface,
	#ifdef _WIN32
		win32config
	#else
		NULL
	#endif
	},
	"enum",
	"enum "CVSNT_PRODUCTVERSION_STRING,
	":enum:",


	0, /* Required elements */
	0, /* Valid elements */

	NULL, /* validate */
	NULL, /* connect */
	NULL, /* disconnect */
	NULL, /* login */
	NULL, /* logout */
	enum_wrap, /* wrap */
	enum_auth_protocol_connect, /* auth_protocol_connect */
	NULL, /* read_data */
	NULL, /* write_data */
	NULL, /* flush_data */
	NULL, /* shutdown */
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
	return 0;
}

static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param)
{
	if(interface_type!=pitProtocol)
		return NULL;

	set_current_server((const struct server_interface*)param);
	return (void*)&enum_protocol_interface;
}

plugin_interface *get_plugin_interface()
{
	return &enum_protocol_interface.plugin;
}

static const char *getHostname()
{
	static char host[1024];
	char *p;

	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","ServerName",host,sizeof(host)))
		return host;

	if(gethostname(host,sizeof(host)))
		return "localhost?";
	p=strchr(host,'.');
	if(p) *p='\0';
	return host;
}

int enum_auth_protocol_connect(const struct protocol_interface *protocol, const char *auth_string)
{
	int context = 0;
	const char *proto;
	char buffer[PATH_MAX];
	char token[1024],buffer2[PATH_MAX];
	int n=0;
	int def = 0;
	int reps = 0;

	if (strcmp (auth_string, "BEGIN ENUM"))
		return CVSPROTO_NOTME;

	server_printf("Version: Concurrent Versions System (CVSNT) "CVSNT_PRODUCTVERSION_STRING"\n");

	if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","ServerName",buffer,sizeof(buffer)))
		strcpy(buffer,getHostname());

	server_printf("ServerName: %s\n",buffer);

	while((proto=enum_protocols(&context,ptServer))!=NULL)
	{
		server_printf("Protocol: %s\n",proto);
	}

	while(!CGlobalSettings::EnumGlobalValues("cvsnt","PServer",n++,token,sizeof(token),buffer,sizeof(buffer)))
	{
		if(!strncasecmp(token,"Repository",10) && isdigit(token[10]) && isdigit(token[strlen(token)-1]))
		{
			char tmp[32];
			int prefixnum = atoi(token+10);
			snprintf(tmp,sizeof(tmp),"Repository%dPublish",prefixnum);
			if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer",tmp,buffer2,sizeof(buffer2)))
			{
				if(!atoi(buffer2))
					continue;
			}
			snprintf(tmp,sizeof(tmp),"Repository%dName",prefixnum);
			if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer",tmp,buffer2,sizeof(buffer2)))
				strcpy(buffer,buffer2);
			if(*buffer && (buffer[strlen(buffer)-1]=='\\' || buffer[strlen(buffer)-1]=='/'))
				buffer[strlen(buffer)-1]='\0';
			server_printf("Repository: %s\n",buffer);

			snprintf(tmp,sizeof(tmp),"Repository%dDescription",prefixnum);
			if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer",tmp,buffer2,sizeof(buffer2)))
				server_printf("RepositoryDescription: %s\n",buffer2);

			snprintf(tmp,sizeof(tmp),"Repository%dDefault",prefixnum);
			if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer",tmp,buffer2,sizeof(buffer2)))
			{
				if(atoi(buffer2))
					server_printf("RepositoryDefault: yes\n");
				def = 1;
			}
			reps++;
		}
	}
	if(reps==1)
		def = 1;

	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AnonymousUsername",buffer,sizeof(buffer)) && *buffer)
	{
		if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","AnonymousProtocol",buffer2,sizeof(buffer2)))
			strcpy(buffer2,"pserver");
		server_printf("AnonymousUsername: %s\n",buffer);
		server_printf("AnonymousProtocol: %s\n",buffer2);
	}
	if(def && !CGlobalSettings::GetGlobalValue("cvsnt","PServer","DefaultProtocol",buffer,sizeof(buffer)) && *buffer)
 		server_printf("DefaultProtocol: %s\n",buffer);

	server_printf("END ENUM\n");

    return CVSPROTO_AUTHFAIL;
}

int enum_wrap(const struct protocol_interface *protocol, int unwrap, int encrypt, const void *input, int size, void *output, int *newsize)
{
	return 0;
}
