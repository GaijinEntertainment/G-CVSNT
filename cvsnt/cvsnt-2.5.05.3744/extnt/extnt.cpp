/* CVS legacy :ext: interface
    Copyright (C) 2004 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include "stdafx.h"

#include <config.h>
#include <cvstools.h>

static void writethread(void *p);

static cvsroot root;
static int trace;
static bool g_bStop;

static int extnt_output(const char *str, size_t len)
{
	return printf("M %-*.*s",len,len,str);
}

static int extnt_error(const char *str, size_t len)
{
	return printf("E %-*.*s",len,len,str);
}

static int extnt_trace(int level, const char *str)
{
	OutputDebugString(str);
	OutputDebugString("\r\n");
	printf("%s\n",str);
	return 0;
}

void usage()
{
	fprintf(stderr,"Usage: extnt.exe hostname [-l username] [-P password] [-p protocol] [-d directory] [command..]\n");
	fprintf(stderr,"Usage: extnt.exe -x [-p default_protocol]\n");
	fprintf(stderr,"\t-l username\tUsername to use (default current user).\n");
	fprintf(stderr,"\t-p protocol\tProtocol to use(default sspi).\n");
	fprintf(stderr,"\t-d directory\tDirectory part of root string.\n");
	fprintf(stderr,"\t-x\tRun in proxy mode.\n");
	fprintf(stderr,"\t-P password\tPassword to use.\n");
	fprintf(stderr,"\nUnspecified parameters are also read from [<hostname>] section in extnt.ini.\n");
	exit(-1);
}

/* Standard :ext: invokes with <hostname> -l <username> cvs server */
CVSNT_EXPORT int main(int argc, char *argv[])
{
	CProtocolLibrary lib;
	char protocol[1024],hostname[1024],directory[1024],password[1024];
	char *section, *username = NULL, *repos = NULL, *prot = NULL, *passw = NULL;
	char buf[8000];
	bool proxy = false;

	if(argc<2)
		usage();

	section = argv[1];

	if(section[0]=='-')
		usage(); /* Options come *after* hostname */

	argv+=2;
	argc-=2;

	while(argc && argv[0][0]=='-')
	{
		if(argc<2)
			usage();
		int a=1;
		switch(argv[0][1])
		{
		case 'l':
			if(proxy) usage();
			username=argv[1]; a++;
			break;
		case 'd':
			if(proxy) usage();
			repos=argv[1]; a++;
			break;
		case 'p':
			prot=argv[1]; a++;
			break;
		case 'P':
			if(proxy) usage();
			passw=argv[1]; a++;
			break;
		case 't':
			trace++;
			CServerIo::loglevel(trace);
			break;
		case 'x':
			if(passw || repos || passw)
				usage();
			proxy=true;
			break;
		default:
			usage();
		}
		argv+=a;
		argc-=a;
	}

	if(proxy && argc)
		usage();

	if(proxy && prot && !strcmp(prot,"pserver"))
	{
		fprintf(stderr, "Unable to proxy pserver to itself\n");
		return -1;
	}
	if(!prot) prot="sspi";

	// Any remaining arguments (usually 'cvs server') are ignored

	CServerIo::init(extnt_output,extnt_output,extnt_error,extnt_trace);

	if (!CSocketIO::init())
    {
		fprintf (stderr, "cvs: unable to initialize sockets driver\n");
		return -1;
    }

	setmode(0,O_BINARY);
	setmode(1,O_BINARY);
	setvbuf(stdin,NULL,_IONBF,0);
	setvbuf(stdout,NULL,_IONBF,0);

	lib.SetupServerInterface(&root,0);

	if(proxy)
	{
		const struct protocol_interface *proto_in = lib.LoadProtocol("pserver");
		if(!proto_in)
		{
			printf("Couldn't load %s protocol\n","pserver");
			return -1;
		}
		const struct protocol_interface *proto_out = lib.LoadProtocol(prot);
		if(!proto_out)
		{
			printf("Couldn't load %s protocol\n",prot?prot:"sspi");
			return -1;
		}
		if(!proto_out->connect)
		{
			printf("%s is not a valid choice for proxy protocol\n",prot);
			return -1;
		}
		CSocketIO listen_sock;

		if(!listen_sock.create(NULL,"2401",true))
		{
			printf("Failed to create listening socket: %s\n",listen_sock.error());
			return -1;
		}
		if(!listen_sock.bind())
		{
			printf("Failed to bind listening socket: %s\n",listen_sock.error());
			return -1;
		}

		CSocketIO* sock_list[1] = { &listen_sock };

		while(!g_bStop && CSocketIO::select(5000,1,sock_list))
		{
			for(size_t n=0; n<listen_sock.accepted_sockets().size(); n++)
			{
//				For testing do everything in the one thread
//				We can only create threads after successful connection anyway, and even
//				then if the protocols can be made threadsafe - they currently aren't.

				cvs::string line;

				CSocketIO* in_sock = listen_sock.accepted_sockets()[n];
				SOCKET sock = in_sock->getsocket();
				// Bit of a hack, since auth_protocol_connect normally takes its input from stdin
				lib.GetServerInterface()->in_fd = sock;
				lib.GetServerInterface()->out_fd = sock;
				((protocol_interface*)proto_in)->server_read_data = proto_in->read_data;
				((protocol_interface*)proto_in)->server_write_data = proto_in->write_data;
				listen_sock.getline(line);
				if(proto_in->auth_protocol_connect(proto_in,line.c_str()))
				{
					listen_sock.printf("error 0 Connection to proxy failed\nI HATE YOU\n");				
					lib.UnloadProtocol(proto_in);
					lib.UnloadProtocol(proto_out);
					return -1;
				}

				char *p=strchr(proto_in->auth_repository+1,'/');
				if(!p)
				{					
					in_sock->printf("error 0 Proxy requires server name in repository root string\nI HATE YOU\n");				
					return -1;
				}

				line.assign(proto_in->auth_repository+1,p-(proto_in->auth_repository+1));

				int verify_only = proto_in->verify_only;
				root.username = proto_in->auth_username;
				root.password = proto_in->auth_password;
				root.directory = p;
				if (strlen(proto_out->name)>20)
				{
					listen_sock.printf("name %s is too long for method\nI HATE YOU\n",proto_out->name);
					return -1;
				}
				strcpy(root.method,proto_out->name);
				root.hostname = line.c_str();
				
				lib.UnloadProtocol(proto_in); // finished with this

				switch(proto_out->connect(proto_out,verify_only))
				{
					case CVSPROTO_SUCCESS: /* Connect succeeded */
					case CVSPROTO_SUCCESS_NOPROTOCOL: /* Connect succeeded, don't wait for 'I LOVE YOU' response */
						{
							char line[1024];
							int r=0,w=0;
							for(;r>=0||w>=0;)
							{
								if(r>=0)
									r=proto_out->read_data(proto_out,line,1024);
								if(r>0)
									in_sock->send(line,r);
								if(w>=0)
									w=in_sock->recv(line,1024);
								if(w>0)
									proto_out->write_data(proto_out,line,w);
								if(!r && !w)
									Sleep(100);
							}
							break;
						}
						break;
					case CVSPROTO_FAIL: /* Generic failure (errno set) */
						in_sock->printf("error 0 Connection failed - generic failure\nI HATE YOU\n");
						return -1;
					case CVSPROTO_BADPARMS: /* (Usually) wrong parameters from cvsroot string */
						in_sock->printf("error 0 Connection failed - Bad parameters\nI HATE YOU\n");
						return -1;
					case CVSPROTO_AUTHFAIL: /* Authorization or login failed */
						in_sock->printf("error 0 Connection failed - Authentication failed\nI HATE YOU\n");
						return -1;
					case CVSPROTO_NOTIMP: /* Not implemented */
						in_sock->printf("error 0 Connection failed - Not implemented\nI HATE YOU\n");
						return -1;
				}
			}
		}
	}

	_snprintf(buf,sizeof(buf),"%s/extnt.ini",CGlobalSettings::GetConfigDirectory());

	if(prot)
		strcpy(protocol,prot);
	else
		GetPrivateProfileString(section,"protocol","sspi",protocol,sizeof(protocol),buf);
	GetPrivateProfileString(section,"hostname",section,hostname,sizeof(hostname),buf);
	if(repos)
		strcpy(directory,repos);
	else
		GetPrivateProfileString(section,"directory","",directory,sizeof(directory),buf);
	
	if(passw)
		strcpy(password,passw);
	else
		GetPrivateProfileString(section,"password","",password,sizeof(password),buf);

	root.method=protocol;
	root.hostname=hostname;
	root.directory=directory;
	root.username=username;
	root.password=password;

	if(!directory[0])
	{
		printf("error 0 [extnt] directory not specified in [%s] section of extnt.ini\n",section);
		return -1;
	}

	const struct protocol_interface *proto = lib.LoadProtocol(protocol);
	if(!proto)
	{
		printf("error 0 [extnt] Couldn't load procotol %s\n",protocol);
		return -1;
	}

	switch(proto->connect(proto, 0))
	{
	case CVSPROTO_SUCCESS: /* Connect succeeded */
		{
			char line[1024];
			int l = proto->read_data(proto,line,1024);
			line[l]='\0';
			if(!strcmp(line,"I HATE YOU\n"))
			{
				printf("error 0 [extnt] connect aborted: server %s rejeced access to %s\n",hostname,directory);
				return -1;
			}
			if(!strncmp(line,"cvs [",5))
			{
				printf("error 0 %s",line);
				return -1;
			}
			if(strcmp(line,"I LOVE YOU\n"))
			{
				printf("error 0 [extnt] Unknown response '%s' from protocol\n", line);
				return -1;
			}
			break;
		}
	case CVSPROTO_SUCCESS_NOPROTOCOL: /* Connect succeeded, don't wait for 'I LOVE YOU' response */
		break;
	case CVSPROTO_FAIL: /* Generic failure (errno set) */
		printf("error 0 [extnt] Connection failed\n");
		return -1;
	case CVSPROTO_BADPARMS: /* (Usually) wrong parameters from cvsroot string */
		printf("error 0 [extnt] Bad parameters\n");
		return -1;
	case CVSPROTO_AUTHFAIL: /* Authorization or login failed */
		printf("error 0 [extnt] Authentication failed\n");
		return -1;
	case CVSPROTO_NOTIMP: /* Not implemented */
		printf("error 0 [extnt] Not implemented\n");
		return -1;
	}

	_beginthread(writethread,0,(void*)proto);
	do
	{
		int len = proto->read_data(proto,buf,sizeof(buf));
		if(len==-1)
		{
			exit(-1); /* dead bidirectionally */
		}
		if(len)
			write(1,buf,len);
		Sleep(50);
	} while(1);

    return 0;
}

void writethread(void *p)
{
	const struct protocol_interface *proto = (const struct protocol_interface *)p;
	char buf[8000];

	do
	{
		int len = read(0,buf,sizeof(buf));
		/* stdin returns 0 when its input pipe goes dead */
		if(len<1)
		{
			exit(0); /* Dead bidirectionally */
		}
		proto->write_data(proto,buf,len);
		Sleep(50);
	} while(1);
}
