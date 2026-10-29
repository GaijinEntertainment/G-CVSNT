/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * Copyright (c) 2001, Tony Hoyle
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Query CVS/Entries from server
 */

#include "cvs.h"

// Global setting from main
extern int no_reverse_dns;

struct protocol_interface local_protocol =
{
	{
		PLUGIN_INTERFACE_VERSION
	},
	"local",
	"(internal)",
	":local:/path",
};

#define ELEM_VALID(_elem) protocol->required_elements&_elem?"Required":protocol->valid_elements&_elem?"Optional":"No"

static const char *const info_usage[] =
{
	"Usage: %s %s [-c|-s|-b] [cvswrappers|cvsignore|config|<protocol>]\n",
	"\t-c\t\tDescribe client (default)\n",
	"\t-s\t\tDescribe server\n",
	"\t-b\t\tBrowse local servers\n",
	"\t-v\t\tMore verbose output\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

#define SERVICE "_cvspserver._tcp"

void show_config()
{
	char buffer[MAX_PATH];
	char token[1024],buffer2[MAX_PATH];
	int n=0;

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
			if(*buffer && ISDIRSEP(buffer[strlen(buffer)-1]))
				buffer[strlen(buffer)-1]='\0';
			snprintf(tmp,sizeof(tmp),"Repository%dDescription",prefixnum);
			if(CGlobalSettings::GetGlobalValue("cvsnt","PServer",tmp,buffer2,sizeof(buffer2)))
				buffer2[0]='\0';
			printf("%-29s %s\n",buffer,buffer2);
		}
	}
}
	

int info(int argc, char **argv)
{
    int c;
	const struct protocol_interface *protocol;
	int use_server = server_active;
	int browse = 0;
	int uibrowse = 0;
	int verbose = 0;
	int test = 0;

    if (argc == -1)
		usage (info_usage);

    optind = 0;
	while ((c = getopt (argc, argv, "+csebI:W:vB")) != -1)
    {
		switch (c)
		{
		case 'c':
			use_server = 0;
			break;
		case 's':
			use_server = 1;
			break;
		case 'b':
			browse = 1;
			break;
		case 'B':
			uibrowse = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'I':
			ign_add (optarg, 0);
			break;
		case 'W':
			wrap_add (optarg, false, false, true, true);
			break;
		case '?':
		default:
			usage (info_usage);
			break;
		}
    }
    argc -= optind;
    argv += optind;

	if(argc>1 || (browse&&((argc>0)||use_server)))
	{
		usage(info_usage);
		return 1;
	}

	if(use_server && !current_parsed_root)
	{
              error (0, 0, "No CVSROOT specified!  Please use the `-d' option");
              error (1, 0, "or set the %s environment variable.", CVSROOT_ENV);
	}

	if(uibrowse)
	{
#ifdef _WIN32
		CBrowserDialog dlg(NULL);
		dlg.ShowDialog(CBrowserDialog::BDShowDefault|CBrowserDialog::BDShowBranches);
		printf("%s*%s\n",dlg.getRoot(),dlg.getModule());
#endif
		return 0;
	}
	if(browse)
	{
		const char *pResp = NULL;
		char szResp[64];

		if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","ResponderName",szResp,sizeof(szResp)))
		{
			pResp = szResp;
			if(!strcmp(pResp,"none"))
			{
				printf("mdns is disabled on this host\n");
				return 0;
			}
			if(!strcmp(pResp,"default"))
				pResp = NULL;
		}

		CZeroconf zc(pResp, CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDMdns));
		const CZeroconf::server_struct_t *serv;

		if(!zc.BrowseForService(SERVICE,CZeroconf::zcAddress))
		{
			printf("Unable to browse for servers.\n");
			return 0;
		}
		bool first = true;
		while((serv=zc.EnumServers(first))!=NULL)
		{
			cvs::string server = serv->server;
			server.resize(server.find_first_of('.'));
			if(verbose)
			{
				addrinfo *fo = serv->addr;
				if(fo)
				{
					while(fo)
					{
						char nhost[256],proto[256];
						if(getnameinfo(fo->ai_addr,fo->ai_addrlen, nhost, sizeof(nhost), proto, sizeof(proto), NI_NUMERICHOST|NI_NUMERICSERV))
						{
							strcpy(nhost,"??");
							strcpy(proto,"??");
						}

						printf("%s(%s):%s\t%s\n",server.c_str(),nhost,proto,serv->servicename.c_str());
						fo = fo->ai_next;
					}
				}
				else
				{
					printf("%s(?):%hu\t%s\n",
						server.c_str(),
						serv->port,
						serv->servicename.c_str());
				}
			}
			else
			{
				printf("%-29s %s\n",server.c_str(),serv->servicename.c_str());
			}
		}
		return 0;
	}

	if(current_parsed_root && current_parsed_root->isremote)
		start_server(0);

	ign_setup();
	wrap_setup();
	wrap_add_file (CVSDOTWRAPPER, true);

	if(use_server && current_parsed_root && current_parsed_root->isremote)
	{
		ign_send();
		wrap_send();

		send_arg("-s");	
		if(argc)
			send_arg(argv[0]);
		send_to_server("info\n", 0);

		return get_responses_and_close();
	}

	CProtocolLibrary lib;
	if(argc==0)
	{
		if(!server_active)
			printf("Available protocols:\n\n");
		else
			printf("Available protocols on server:\n\n");
		TRACE(1,"Interface version is %04x",PLUGIN_INTERFACE_VERSION);
		
		if(!server_active)
			printf("%-20.20s%s\n","local","(internal)");
		protocol=lib.LoadProtocol("pserver");
		if(protocol && ((use_server && protocol->auth_protocol_connect && lib.IsEnabled(protocol)) || (!use_server && protocol->connect)))
			printf("%-20.20s%s\n",protocol->name,protocol->version);
		if(protocol)
			lib.UnloadProtocol(protocol);
	}
	else
	{
		if(!strcmp(argv[0],"cvswrappers"))
		{
		    wrap_display();
		    return 0;
		}
		if(!strcmp(argv[0],"cvsignore"))
		{
		    ign_display();
		    return 0;
		}
		if(!strcmp(argv[0],"config"))
		{
		    show_config();
		    return 0;
		}
		if(!strcmp(argv[0],"local"))
			protocol = &local_protocol;
		else
			protocol = lib.LoadProtocol(argv[0]);
		if(!protocol)
		{
			error(1,0,"Couldn't load protocol '%s'",argv[0]);
			return 1;
		}
		printf("%-20s%s\n","Name:",protocol->name);
		printf("%-20s%s\n","Version:",protocol->version);
		printf("%-20s%s\n","Syntax:",protocol->syntax);
		printf("%-20s%s\n","  Username:",ELEM_VALID(elemUsername));
		printf("%-20s%s\n","  Password:",ELEM_VALID(elemPassword));
		printf("%-20s%s\n","  Hostname:",ELEM_VALID(elemHostname));
		printf("%-20s%s\n","  Port:",ELEM_VALID(elemPort));
		printf("%-20s%s\n","Client:",protocol->connect?"Yes":"No");
		printf("%-20s%s\n","Server:",protocol->auth_protocol_connect?"Yes":"No");
		printf("%-20s%s\n","Login:",protocol->login?"Yes":"No");

		printf("\nKeywords available:\n\n");
		if(protocol->valid_elements&elemTunnel)
		{
			printf("%-20s%s\n","proxy","Proxy server");
			printf("%-20s%s\n","proxyport","Proxy server port (alias: proxy_port)");
			printf("%-20s%s\n","tunnel","Proxy protocol (aliases: proxyprotocol,proxy_protocol)");
			printf("%-20s%s\n","proxyuser","Proxy user (alias: proxy_user)");
			printf("%-20s%s\n","proxypassword","Proxy passsord (alias: proxy_password)");
		}
		if(protocol && protocol!=&local_protocol)
			lib.UnloadProtocol(protocol);
	}

    return 0;
}
