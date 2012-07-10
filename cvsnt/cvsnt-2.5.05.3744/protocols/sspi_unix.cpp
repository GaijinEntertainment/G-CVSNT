/* CVS sspi auth interface - unix (client only)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MODULE sspi

#include "common.h"
#include "ntlm/ntlm.h"
#include "../version.h"

static int sspi_connect(const struct protocol_interface *protocol, int verify_only);
static int sspi_disconnect(const struct protocol_interface *protocol);
static int sspi_login(const struct protocol_interface *protocol, char *password);
static int sspi_logout(const struct protocol_interface *protocol);
static int sspi_auth_protocol_connect(const struct protocol_interface *protocol, const char *auth_string);
static int sspi_read_data(const struct protocol_interface *protocol, void *data, int length);
static int sspi_write_data(const struct protocol_interface *protocol, const void *data, int length);
static int sspi_flush_data(const struct protocol_interface *protocol);
static int sspi_shutdown(const struct protocol_interface *protocol);
static int ClientAuthenticate(const char *protocol, const char *name, const char *pwd, const char *domain, const char *hostname);
static int sspi_get_user_password(const char *username, const char *server, const char *port, const char *directory, char *password, int password_len);
static int sspi_set_user_password(const char *username, const char *server, const char *port, const char *directory, const char *password);

static int init(const struct plugin_interface *plugin);
static int destroy(const struct plugin_interface *plugin);
static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param);

static char winbindwrapper[1024];

struct protocol_interface sspi_protocol_interface =
{
        {
                PLUGIN_INTERFACE_VERSION,
                ":sspi: protocol",CVSNT_PRODUCTVERSION_STRING,"SspiProtocol",
                init,
                destroy,
                get_interface,
                NULL
        },

	"sspi",
	"sspi "CVSNT_PRODUCTVERSION_STRING,
	":sspi[;keyword=value...]:[username[:password]@]host[:port][:]/path",

	elemHostname, /* Required elements */
	elemUsername|elemPassword|elemHostname|elemPort|elemTunnel, /* Valid elements */

	NULL, /* validate */
	sspi_connect,
	sspi_disconnect,
	sspi_login,
	sspi_logout,
	NULL, /* wrap */
	sspi_auth_protocol_connect, 
	sspi_read_data,
	sspi_write_data,
	sspi_flush_data,
	sspi_shutdown,
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
        if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","WinbindWrapper",winbindwrapper,sizeof(winbindwrapper)) || !winbindwrapper[0])
            sspi_protocol_interface.auth_protocol_connect = NULL;
	return 0;
}

static int destroy(const struct plugin_interface *plugin)
{
        protocol_interface *protocol = (protocol_interface*)plugin;

        free(protocol->auth_username);
        free(protocol->auth_repository);

        return 0;
}

static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param)
{
        if(interface_type!=pitProtocol)
                return NULL;

        set_current_server((const struct server_interface*)param);
        return (void*)&sspi_protocol_interface;
}

plugin_interface *get_plugin_interface()
{
        return &sspi_protocol_interface.plugin;
}

int sspi_connect(const struct protocol_interface *protocol, int verify_only)
{
	char crypt_password[64];
	const char *password;
	char domain_buffer[128],*domain;
	char user_buffer[128],*p;
	const char *user;
	const char *begin_request = "BEGIN SSPI";
	char protocols[1024];
	const char *proto;
	CScramble scramble;

	if(!current_server()->current_root->hostname || !current_server()->current_root->directory)
		return CVSPROTO_BADPARMS;

	if(tcp_connect(current_server()->current_root))
		return CVSPROTO_FAIL;

	user = get_username(current_server()->current_root);
	password = current_server()->current_root->password;
	domain = NULL;

	if(!current_server()->current_root->password)
	{
		if(!sspi_get_user_password(user,current_server()->current_root->hostname,current_server()->current_root->port,current_server()->current_root->directory,crypt_password,sizeof(crypt_password)))
		{
			password = scramble.Unscramble(crypt_password);
		}
	}

	if(strchr(user,'\\'))
	{
		strncpy(domain_buffer,user,sizeof(domain_buffer));
		domain_buffer[sizeof(domain_buffer)-1]='\0';
		domain=strchr(domain_buffer,'\\');
		if(domain)
		{
			*domain = '\0';
			strncpy(user_buffer,domain+1,sizeof(user_buffer));
			domain = domain_buffer;
			user = user_buffer;
		}
	}

	if(tcp_printf("%s\nNTLM\n",begin_request)<0)
		return CVSPROTO_FAIL;

	tcp_readline(protocols, sizeof(protocols));
	if((p=strstr(protocols,"[server aborted"))!=NULL)
	{
		server_error(1, p);
	}

	if(strstr(protocols,"NTLM"))
		proto="NTLM";
	else
	    server_error(1, "Can't authenticate - server and client cannot agree on an authentication scheme (got '%s')\n",protocols);

	if(!ClientAuthenticate(proto,user,password,domain,current_server()->current_root->hostname))
	{
		/* Actually we never get here... NTLM seems to allow the client to
		   authenticate then fails at the server end.  Wierd huh? */

		if(user)
			server_error(1, "Can't authenticate - Username, Password or Domain is incorrect\n");
		else
			server_error(1, "Can't authenticate - perhaps you need to login first?\n");

		return CVSPROTO_FAIL;
	}

	if(tcp_printf("%s\n",current_server()->current_root->directory)<0)
		return CVSPROTO_FAIL;
	return CVSPROTO_SUCCESS;
}

int sspi_disconnect(const struct protocol_interface *protocol)
{
	if(tcp_disconnect())
		return CVSPROTO_FAIL;
	return CVSPROTO_SUCCESS;
}

int sspi_login(const struct protocol_interface *protocol, char *password)
{
	CScramble scramble;
	const char *user = get_username(current_server()->current_root);
	
	/* Store username & encrypted password in password store */
	if(sspi_set_user_password(user,current_server()->current_root->hostname,current_server()->current_root->port,current_server()->current_root->directory,scramble.Scramble(password)))
	{
		server_error(1,"Failed to store password");
	}

	return CVSPROTO_SUCCESS;
}

int sspi_logout(const struct protocol_interface *protocol)
{
	const char *user = get_username(current_server()->current_root);
	
	if(sspi_set_user_password(user,current_server()->current_root->hostname,current_server()->current_root->port,current_server()->current_root->directory,NULL))
	{
		server_error(1,"Failed to delete password");
	}
	return CVSPROTO_SUCCESS;
}

int sspi_auth_protocol_connect(const struct protocol_interface *protocol, const char *auth_string)
{
        char *protocols;
        const char *proto;
	int fdin, fdout, fderr;
	int l;
	short len;
	char line[1024];
	char buf[1024];
	int first;

	if (!strcmp (auth_string, "BEGIN SSPI"))
           sspi_protocol_interface.verify_only = 0;
        else
           return CVSPROTO_NOTME;

        server_getline(protocol, &protocols, 1024);

        if(!protocols)
        {
                server_printf("Nope!\n");
                return CVSPROTO_FAIL;
        }
        else if(strstr(protocols,"Negotiate"))
                proto="Negotiate";
        else if(strstr(protocols,"NTLM"))
                proto="NTLM";
        else
        {
                server_printf("Nope!\n");
                return CVSPROTO_FAIL;
        }
        free(protocols);

        server_printf("%s\n",proto); /* We have negotiated NTLM */

	if(run_command(winbindwrapper, &fdin, &fdout, &fderr))
	  return CVSPROTO_FAIL;

        first=1;	
	do
	{
	read(current_server()->in_fd,&len,2);
	len=ntohs(len);
	l=read(current_server()->in_fd,buf,len);
	if(l<0)
	  return CVSPROTO_FAIL;
	if(first)
	  	strcpy(line,"YR ");
	else
		strcpy(line,"KK ");
	first = 0;
	l=base64enc((unsigned char *)buf,(unsigned char *)line+3,len);
	strcat(line,"\n");
	write(fdin, line, strlen(line));
	l=read(fdout, line, sizeof(line));
	if(l<0)
	  return CVSPROTO_FAIL;
	line[l]='\0';
	if(line[0]=='T' && line[1]=='T')
	{
	  len=base64dec((unsigned char *)line+3,(unsigned char *)buf,l-4);
	  base64enc((unsigned char *)buf,(unsigned char *)line+3,len);
	  len=htons(len);
	  write(current_server()->out_fd,&len,2);
	  write(current_server()->out_fd,buf,ntohs(len));
	}
	} while(line[0]=='T' && line[1]=='T');
	if(line[0]!='A' || line[1]!='F')
	   return CVSPROTO_FAIL;
	close(fdin);
	close(fdout);
	close(fderr);

	line[strlen(line)-1]='\0';
        sspi_protocol_interface.auth_username = strdup(line+3);

        /* Get the repository details for checking */
        server_getline (protocol, &sspi_protocol_interface.auth_repository, 4096);
        return CVSPROTO_SUCCESS;
}

int sspi_read_data(const struct protocol_interface *protocol, void *data, int length)
{
	return tcp_read(data,length);
}

int sspi_write_data(const struct protocol_interface *protocol, const void *data, int length)
{
	return tcp_write(data,length);
}

int sspi_flush_data(const struct protocol_interface *protocol)
{
	return 0; // TCP/IP is always flushed
}

int sspi_shutdown(const struct protocol_interface *protocol)
{
	return tcp_shutdown();
}

int ClientAuthenticate(const char *protocol, const char *name, const char *pwd, const char *domain, const char *hostname)
{
	tSmbNtlmAuthRequest request;
	tSmbNtlmAuthChallenge challenge;
	tSmbNtlmAuthResponse response;
	short len;
	
	buildSmbNtlmAuthRequest(&request,name?(char*)name:(char*)"",domain?(char*)domain:(char*)"");
	len=htons(SmbLength(&request));
	if(tcp_write(&len,sizeof(len))<0)
	  return 0;
	if(tcp_write(&request,SmbLength(&request))<0)
	  return 0;
	if(tcp_read(&len,2)!=2)
	  return 0;
	if(!len)
	  return 0;
	if(tcp_read(&challenge,ntohs(len))!=ntohs(len))
	  return 0;
	buildSmbNtlmAuthResponse(&challenge, &response, name?(char*)name:(char*)"", pwd?(char*)pwd:(char*)"");
	len=htons(SmbLength(&response));
	if(tcp_write(&len,sizeof(len))<0)
	  return 0;
	if(tcp_write(&response, SmbLength(&response))<0)
	  return 0;
	return 1;
}

int sspi_get_user_password(const char *username, const char *server, const char *port, const char *directory, char *password, int password_len)
{
	char tmp[1024];

	if(port)
		snprintf(tmp,sizeof(tmp),":sspi:%s@%s:%s:%s",username,server,port,directory);
	else
		snprintf(tmp,sizeof(tmp),":sspi:%s@%s:%s",username,server,directory);
	if(!CGlobalSettings::GetUserValue("cvsnt","cvspass",tmp,password,password_len))
		return CVSPROTO_SUCCESS;
	else
		return CVSPROTO_FAIL;
}

int sspi_set_user_password(const char *username, const char *server, const char *port, const char *directory, const char *password)
{
	char tmp[1024];

	if(port)
		snprintf(tmp,sizeof(tmp),":sspi:%s@%s:%s:%s",username,server,port,directory);
	else
		snprintf(tmp,sizeof(tmp),":sspi:%s@%s:%s",username,server,directory);
	if(!CGlobalSettings::SetUserValue("cvsnt","cvspass",tmp,password))
		return CVSPROTO_SUCCESS;
	else
		return CVSPROTO_FAIL;
}

