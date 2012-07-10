/* CVS ssh auth interface
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
#include <time.h>

#define MODULE ssh

#include "common.h"
#include "../version.h"

#include "..\plink\plink_cvsnt.h"

static int ssh_validate(const struct protocol_interface *protocol, cvsroot *newroot);
static int ssh_connect(const struct protocol_interface *protocol, int verify_only);
static int ssh_disconnect(const struct protocol_interface *protocol);
static int ssh_login(const struct protocol_interface *protocol, char *password);
static int ssh_logout(const struct protocol_interface *protocol);
static int ssh_get_user_password(const char *username, const char *server, const char *port, const char *directory, char *password, int password_len);
static int ssh_set_user_password(const char *username, const char *server, const char *port, const char *directory, const char *password);
static int ssh_read_data(const struct protocol_interface *protocol, void *data, int length);
static int ssh_write_data(const struct protocol_interface *protocol, const void *data, int length);
static int ssh_flush_data(const struct protocol_interface *protocol);
static int ssh_shutdown(const struct protocol_interface *protocol);
static int ssh_validate_keyword(const struct protocol_interface *protocol, cvsroot *root, const char *keyword, const char *value);
static const char *ssh_get_keyword_help(const struct protocol_interface *protocol);
static char *ssh_get_default_port();

static int init(const struct plugin_interface *plugin);
static int destroy(const struct plugin_interface *plugin);
static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param);

struct protocol_interface ssh_protocol_interface =
{
	{
		PLUGIN_INTERFACE_VERSION,
		":ssh: protcol",CVSNT_PRODUCTVERSION_STRING,NULL,
		init,
		destroy,
		get_interface,
		NULL
	},

	"ssh",
	"ssh "CVSNT_PRODUCTVERSION_STRING,
	":ssh[;keyword=value...]:[username[:password]@]host[:port][:]/path",

	elemHostname, /* Required elements */
	elemUsername|elemPassword|elemHostname|elemPort|elemTunnel|flagAlwaysEncrypted, /* Valid elements */

	ssh_validate, /* validate */
	ssh_connect,
	ssh_disconnect,
	ssh_login,
	ssh_logout,
	NULL, /* wrap */
	NULL, /* auth_protocol_connect */
	ssh_read_data,
	ssh_write_data,
	ssh_flush_data,
	ssh_shutdown,
	NULL, /* impersonate */
	ssh_validate_keyword,
	ssh_get_keyword_help,
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

static int ssh_yesno(const char *message, const char *title, int withcancel)
{
	return current_server()->yesno(current_server(),message,title,withcancel);
}

static int ssh_getpass(char *password, int max_length, const char *prompt)
{
	return current_server()->getpass(current_server(), password, max_length, prompt);
}

static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param)
{
	if(interface_type!=pitProtocol)
		return NULL;

	set_current_server((const struct server_interface*)param);
	static putty_callbacks cb;
	if(param)
	{
		cb.getpass = ssh_getpass;
		cb.yesno = ssh_yesno;
		plink_set_callbacks(&cb);
	}
	return (void*)&ssh_protocol_interface;
}

plugin_interface *get_plugin_interface()
{
	return &ssh_protocol_interface.plugin;
}

int ssh_validate(const struct protocol_interface *protocol, cvsroot *newroot)
{
	if(!newroot->port)
	{
		/* Replace the default port used with the ssh port */
		newroot->port = ssh_get_default_port();
	}
	if(newroot->optional_2) /* Keyfile was specified */
	{
		free((void*)newroot->password);
		newroot->password = strdup("");
	}
	return 0;
}

int ssh_connect(const struct protocol_interface *protocol, int verify_only)
{
	char crypt_password[4096],command_line[1024];
	const char *username;
	const char *password;
	char *key = NULL;
	const char *server;
	const char *env;
	const char *version;
	CScramble scramble;

	if(!current_server()->current_root->hostname || !current_server()->current_root->directory)
		return CVSPROTO_BADPARMS;

	username=get_username(current_server()->current_root);
	password = current_server()->current_root->password;
	if(password && !*password)
		password=NULL;
	version = current_server()->current_root->optional_1;
	if(!password) // No password supplied
	{
		if(ssh_get_user_password(username,current_server()->current_root->hostname,current_server()->current_root->port,current_server()->current_root->directory,crypt_password,sizeof(crypt_password)))
		{
			/* In the case where we're using an ssh agent such as pageant, it's legal to have no password */
		}
		else
		{
			if(!strncmp(crypt_password,"KEY;",4))
			{
				key=strchr(crypt_password+4,';');
				if(!key || !*key)
				{
					/* Something wrong - ignore password */
					server_error(1,"No password or key set.  Try 'cvs login'\n");
				}
				*key++ = '\0';
				key=strchr(key,';');
				*key++ = '\0';
				if(!key || !*key)
				{
					/* Something wrong - ignore password */
					server_error(1,"No password or key set.  Try 'cvs login'\n");
				}
				version = crypt_password+4;
			}
			else
				password = scramble.Unscramble(crypt_password);
		}
	}
	server=current_server()->current_root->hostname;

	/* Execute correct command */
	if(current_server()->current_root->optional_3)
	{
		strncpy(command_line,current_server()->current_root->optional_3,sizeof(command_line)-sizeof(" server"));
	}
	else if((env=CProtocolLibrary::GetEnvironment("CVS_SERVER"))!=NULL)
		strncpy(command_line,env,sizeof(command_line)-sizeof(" server"));
	else
		strcpy(command_line,"cvs");
	strcat(command_line," server");

	if(current_server()->current_root->optional_2)
		key=(char*)current_server()->current_root->optional_2;

	if(plink_connect(username, password, key, cvs::idn(server), atoi(current_server()->current_root->port), version?atoi(version):0, command_line, current_server()->current_root->proxy, current_server()->current_root->proxyport, current_server()->current_root->proxyuser, current_server()->current_root->proxypassword))
	{
		server_error(0,"Couldn't connect to remote server - plink error");
		return CVSPROTO_FAIL;
	}

	return CVSPROTO_SUCCESS_NOPROTOCOL; /* :ssh: doesn't need login response */
}

int ssh_disconnect(const struct protocol_interface *protocol)
{
	return CVSPROTO_SUCCESS;
}

int ssh_login(const struct protocol_interface *protocol, char *password)
{
	const char *username = NULL;
	CScramble scramble;

	username = get_username(current_server()->current_root);

	if(!current_server()->current_root->optional_2)
	{
		/* Store username & encrypted password in password store */
		if(ssh_set_user_password(username,current_server()->current_root->hostname,current_server()->current_root->port,current_server()->current_root->directory,scramble.Scramble(password)))
		{
			server_error(1,"Failed to store password");
		}
	}
	else
	{
		char buf[4096] = "KEY;";

		if(current_server()->current_root->optional_1)
			strcat(buf,current_server()->current_root->optional_1);
		else
			strcat(buf,"0");
		strcat(buf,";");

		strcat(buf,".;"); // Password field not used any more (use pageant for this kind of thing)

		strcat(buf,current_server()->current_root->optional_2);

		if(ssh_set_user_password(username,current_server()->current_root->hostname,current_server()->current_root->port,current_server()->current_root->directory,buf))
		{
			server_error(1,"Failed to store key");
		}
	}

	free((void*)current_server()->current_root->optional_2);
	current_server()->current_root->optional_2=NULL;
	return CVSPROTO_SUCCESS;
}

int ssh_logout(const struct protocol_interface *protocol)
{
	const char *username = NULL;

	username = get_username(current_server()->current_root);
	if(ssh_set_user_password(username,current_server()->current_root->hostname,current_server()->current_root->port,current_server()->current_root->directory,NULL))
	{
		server_error(1,"Failed to delete password");
	}
	return CVSPROTO_SUCCESS;
}

int ssh_get_user_password(const char *username, const char *server, const char *port, const char *directory, char *password, int password_len)
{
	char tmp[1024];

	if(port)
		snprintf(tmp,sizeof(tmp),":ssh:%s@%s:%s:%s",username,server,port,directory);
	else
		snprintf(tmp,sizeof(tmp),":ssh:%s@%s:%s",username,server,directory);
	if(!CGlobalSettings::GetUserValue("cvsnt","cvspass",tmp,password,password_len))
		return CVSPROTO_SUCCESS;
	else
		return CVSPROTO_FAIL;
}

int ssh_set_user_password(const char *username, const char *server, const char *port, const char *directory, const char *password)
{
	char tmp[1024];

	if(port)
		snprintf(tmp,sizeof(tmp),":ssh:%s@%s:%s:%s",username,server,port,directory);
	else
		snprintf(tmp,sizeof(tmp),":ssh:%s@%s:%s",username,server,directory);
	if(!CGlobalSettings::SetUserValue("cvsnt","cvspass",tmp,password))
		return CVSPROTO_SUCCESS;
	else
		return CVSPROTO_FAIL;
}

int ssh_read_data(const struct protocol_interface *protocol, void *data, int length)
{
	return plink_read_data(data, length);
}

int ssh_write_data(const struct protocol_interface *protocol, const void *data, int length)
{
	CServerIo::trace(4,"SSH(%d): %-*.*s",length,length,length,data);
	return plink_write_data(data, length);
}

int ssh_flush_data(const struct protocol_interface *protocol)
{
	return 0;
}

int ssh_shutdown(const struct protocol_interface *protocol)
{
	return 0;
}

const char *ssh_get_keyword_help(const struct protocol_interface *protocol)
{
	return "privatekey\0Use file as private key (aliases: key, rsakey)\0version\0Force SSH version (alias: ver)\0server\0Remote command (default 'cvs')\0";
}

int ssh_validate_keyword(const struct protocol_interface *protocol, cvsroot *root, const char *keyword, const char *value)
{
	if(!strcasecmp(keyword,"version") || !strcasecmp(keyword,"ver"))
	{
		if(!strcmp(value,"1") || !strcmp(value,"2"))
		{
			root->optional_1 = strdup(value);
			return CVSPROTO_SUCCESS;
		}
		else
			server_error(1,"SSH version should be 1 or 2");
	}
	if(!strcasecmp(keyword,"passphrase")) // Synonym for 'password'
	{
		root->password = strdup(value);
	}
	if(!strcasecmp(keyword,"privatekey") || !strcasecmp(keyword,"key") || !strcasecmp(keyword,"rsakey"))
	{
		root->optional_2 = strdup(value);
		return CVSPROTO_SUCCESS;
	}
	if(!strcasecmp(keyword,"server"))
	{
		root->optional_3 = strdup(value);
		return CVSPROTO_SUCCESS;
	}
	return CVSPROTO_FAIL;
}

char *ssh_get_default_port()
{
	struct servent *ent;
	char p[32];

	if((ent=getservbyname("ssh","tcp"))!=NULL)
	{
		sprintf(p,"%u",ntohs(ent->s_port));
		return strdup(p);
	}

	return strdup("22");
}

