/*	cvsnt control service
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
#include "config.h"

#ifdef _WIN32
#include <process.h>
#endif
#include <vector>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "../cvsapi/cvsapi.h"

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#include "CvsControl.h"

typedef std::map<std::string,std::string> header_t;

struct cmd_buf_t
{
	cmd_buf_t() { request_buf=response_buf=NULL; request_len=response_len=0; state=0; }
	~cmd_buf_t() { delete[] request_buf; delete[] response_buf; }
	int state;
	std::string cmd;
	std::string url;
	std::string version;
	header_t request_headers;
	header_t response_headers;
	size_t request_len;
	size_t response_len;
	char *request_buf;
	char *response_buf;
};
std::map<SOCKET,cmd_buf_t> cmd_buf;

bool OpenControlClient(CSocketIOPtr s)
{
	cmd_buf[s->getsocket()].state=0;
	return true;
}

bool CloseControlClient(CSocketIOPtr s)
{
	cmd_buf.erase(cmd_buf.find(s->getsocket()));
	return true;
}

bool result(CSocketIOPtr s, int err)
{
	cmd_buf_t& cbuf = cmd_buf[s->getsocket()];
	char today[64];
	time_t t = time(&t);
	strftime(today,sizeof(today),"%a, %d %b %Y %H:%M:%S GMT",gmtime(&t));

	cbuf.response_headers["Server"]="CvsControl 1.1 (experimental) DAV/1.0";
	cbuf.response_headers["Date"]=today;
	sprintf(today,"%u",cbuf.response_len);
	cbuf.response_headers["Content-Length"]=today;

	const char *post;
	switch(err)
	{
	case 200: post="OK"; break;
	case 404: post="Not Found"; break;
	case 302: post="Moved"; break;
	default:
		post="Error"; break;
	}
	s->printf("%s %d %s\r\n",cbuf.version.length()?cbuf.version.c_str():"HTTP/1.1",err,post);
	printf("%s %d %s\n",cbuf.version.length()?cbuf.version.c_str():"HTTP/1.1",err,post);
	for(header_t::const_iterator i = cbuf.response_headers.begin(); i!=cbuf.response_headers.end(); i++)
	{
		s->printf("%s: %s\r\n",i->first.c_str(),i->second.c_str());
		printf("%s: %s\n",i->first.c_str(),i->second.c_str());
	}
	s->printf("\r\n");
	if(cbuf.response_len)
		s->printf("%s",cbuf.response_buf);
	s->printf("\r\n");
	return true;
}

bool ParseControlCommand(CSocketIOPtr s, const char *command)
{
	cmd_buf_t& cbuf = cmd_buf[s->getsocket()];
	if(!cbuf.state)
	{
		const char *p=strchr(command,' ');
		cbuf.cmd=command;
		if(p)
		{
			cbuf.cmd.resize(p-command);
			while(isspace(*++p))
				;
			cbuf.url=p;
			const char *q=strchr(p,' ');
			if(q)
			{
				cbuf.url.resize(q-p);
				while(isspace(*++q))
					;
				cbuf.version=q;
			}
			cbuf.state=1;
		}
		return true;
	}
	else if(*command && cbuf.state==1)
	{
		std::string cmd,arg;
		const char *p=strchr(command,':');
		cmd = command;
		if(p)
		{
			cmd.resize(p-command);
			while(isspace(*++p))
				;
			arg=p;
		}
		cbuf.request_headers[cmd]=arg;
		return true;
	}
	else if(cbuf.state==1)
	{
		cbuf.request_len = atoi(cbuf.request_headers["Content-Length"].c_str());
		cbuf.state=2;
		if(cbuf.request_len>0)
		{
			cbuf.request_buf=new char[cbuf.request_len+1];
			s->recv(cbuf.request_buf,cbuf.request_len);
			cbuf.request_buf[cbuf.request_len]='\0';
		}

		printf("Command: %s\nUrl=%s\nHTTP Vers=%s\n",cbuf.cmd.c_str(),cbuf.url.c_str(),cbuf.version.c_str());
		for(header_t::const_iterator i = cbuf.request_headers.begin(); i!=cbuf.request_headers.end(); i++)
			printf("%s: %s\n",i->first.c_str(),i->second.c_str());
			
		if(!strcmp(cbuf.cmd.c_str(),"OPTIONS"))
		{
			if(strcmp(cbuf.url.c_str(),"/"))
				result(s,404);
			else
			{
				cbuf.response_headers["DAV"]="1,2";
				cbuf.response_headers["MS-Author-Via"]="DAV";
				cbuf.response_headers["Allow"]="GET, HEAD, POST, DELETE, TRACE, PROPFIND, PROPPATCH, COPY, MOVE, LOCK, UNLOCK";
				result(s,200);
			}
		}
		else if(!strcmp(cbuf.cmd.c_str(),"GET"))
		{
			// DAV minimal, HTML
			if(!strcmp(cbuf.url.c_str(),"/"))
			{
				cbuf.response_buf=strdup("<html><head><title>Hello</title></head><body>Hello world!</body></html>");
				cbuf.response_len=strlen(cbuf.response_buf);
				result(s,200);
			}
			else
				result(s,404);
		}
		else if(!strcmp(cbuf.cmd.c_str(),"PUT"))
		{
			// DAV minimal, HTML
			result(s,404);
		}
		else if(!strcmp(cbuf.cmd.c_str(),"PROPFIND"))
		{
			// DAV minimal
			result(s,207);
		}
		else if(!strcmp(cbuf.cmd.c_str(),"DELETE"))
		{
			// DAV level 1
			result(s,404);
		}
		else if(!strcmp(cbuf.cmd.c_str(),"PROPPATCH"))
		{
			// DAV level 1
			result(s,404);
		}
		else if(!strcmp(cbuf.cmd.c_str(),"MKCOL"))
		{
			// DAV level 1
			result(s,404);
		}
		else if(!strcmp(cbuf.cmd.c_str(),"HEAD"))
		{
			// DAV level 1 optional, HTML
			result(s,404);
		}
		else if(!strcmp(cbuf.cmd.c_str(),"MOVE"))
		{
			// DAV level 1 optional
			result(s,404);
		}
		else if(!strcmp(cbuf.cmd.c_str(),"COPY"))
		{
			// DAV level 1 optional
			result(s,404);
		}
		else if(!strcmp(cbuf.cmd.c_str(),"LOCK"))
		{
			// DAV level 2
			result(s,404);
		}
		else if(!strcmp(cbuf.cmd.c_str(),"UNLOCK"))
		{
			// DAV level 2
			result(s,404);
		}
		else if(!strcmp(cbuf.cmd.c_str(),"POST"))
		{
			//HTML, XML-RPC
			result(s,404);
		}
	}

	return false;
}
