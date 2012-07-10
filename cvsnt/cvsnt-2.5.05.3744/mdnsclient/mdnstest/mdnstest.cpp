// mdnstest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <map>
#include <vector>
#include <string>
#include "../mdnsclient.h"

std::map<std::string, std::map<std::string, std::vector<std::string> > > servers;

void serv(const char *service, struct sockaddr_storage*, char *txt,void*)
{
	servers[service].size(); // Just reference it to get an array entry
	if(txt)
	{
		char *p=txt;
		while(*p)
		{
			char *tag=strchr(p,'=');
			char *val = strchr(p,'\n');
			if(tag)
			{
				if(val) *val='\0';
				*tag='\0';
				servers[service][p].push_back(tag+1);
				if(val) *val='\n';
				*tag='=';
			}
			if(val) p=val+1;
			else p+=strlen(p);
		}
	}
}

int main(int argc, char* argv[])
{
	WSADATA wsa = {0};
	WSAStartup(MAKEWORD(2,0),&wsa);

	sock_t sock = mdns_open_socket();
	mdns_query_services(sock,"_cvspserver._tcp",serv,NULL);
	printf("finished\n");
	getchar();

	return 0;
}

