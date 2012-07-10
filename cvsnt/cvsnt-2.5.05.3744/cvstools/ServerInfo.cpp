/*
	CVSNT Helper application API
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
#include <cvsapi.h>
#include "export.h"
#include "ServerInfo.h"

bool CServerInfo::getRemoteServerInfo(const char *server, remoteServerInfo& rsi)
{
	cvs::string current_rep;
	cvs::string remote = server;
	char *p = (char*)strchr(remote.c_str(),':');
	if(p)
		*p='\0';
	CSocketIO sock;
	if(!sock.create(remote.c_str(),p?p+1:"2401",false))
	{
		CServerIo::error("Couldn't create socket: %s",sock.error());
		return false;
	}
	if(p) *p=':';
	if(!sock.connect())
	{
		CServerIo::error("Couldn't connect to remote server: %s",sock.error());
		return false;
	}
	sock.send("BEGIN ENUM\n",11);
	cvs::string line;
	while(sock.getline(line))
	{
		CServerIo::trace(3,"From remote: %s",line.c_str());
		if(!strncmp(line.c_str(),"error ",6) || strstr(line.c_str(),"bad auth protocol start"))
		{
			CServerIo::error("Couldn't enumerate remote server.  Server does not support enum protocol.\n");
			return false;
		}
		if(!strcmp(line.c_str(),"END ENUM"))
			break;
		char *buf = (char*)line.c_str();
		char *p = strstr(buf,": ");
		if(!p)
			continue; // We don't know what this is
		*p='\0';
		p+=2;

		if(!strcmp(buf,"Version"))
			rsi.server_version = p;
		else if(!strcmp(buf,"ServerName"))
			rsi.server_name = p;
		else if(!strcmp(buf,"Repository"))
		{
			current_rep = p;
			rsi.repositories[current_rep]=p;
		}
		else if(!strcmp(buf,"RepositoryDescription"))
		{
			if(current_rep.size() && *p)
				rsi.repositories[current_rep]=p;
		}
		else if(!strcmp(buf,"RepositoryDefault"))
		{
			rsi.default_repository = current_rep;
		}
		else if(!strcmp(buf,"Protocol"))
			rsi.protocols[p]++;
		else if(!strcmp(buf,"AnonymousUsername"))
			rsi.anon_username=p;
		else if(!strcmp(buf,"AnonymousProtocol"))
			rsi.anon_protocol=p;
		else if(!strcmp(buf,"DefaultProtocol"))
			rsi.default_protocol=p;
		else
			continue; // Future or unknown responses
	}
	sock.close();

	// If there's only one repository it's default
	if(rsi.repositories.size()==1 && !rsi.default_repository.size())
		rsi.default_repository=rsi.repositories.begin()->first.c_str();

	// Sourceforge uses some kind of proxy and sends "-!- unable to open session"
	// instead of any standard error message.  In this case all entries will be empty
	if(!rsi.server_name.size() && !rsi.server_version.size() && rsi.protocols.empty() && rsi.repositories.empty())
	{
		CServerIo::error("Couldn't enumerate remote server.  Server does not support enum protocol.\n");
		return false;
	}

	if(!rsi.default_protocol.size())
	{
		cvs::string best_proto = "pserver";
		if(rsi.protocols.find("sspi")!=rsi.protocols.end())
			best_proto="sspi";
		else if(rsi.protocols.find("sserver")!=rsi.protocols.end())
			best_proto="sserver";
		rsi.default_protocol = best_proto;
	}

	if(!rsi.anon_protocol.size())
		rsi.anon_protocol = "pserver";

	return true;
}

const char *CServerInfo::getGlobalServerInfo(const char *server)
{
	CDnsApi dns;
	const char *p;
	cvs::string tmp,tmp2;
	cvs::string srv = server;

	while((p=strrchr(srv.c_str(),'/'))!=NULL)
	{
		cvs::sprintf(tmp2,80,"%s.%s",tmp.c_str(),p+1);
		tmp=tmp2;
		srv.resize(p-srv.c_str());
	}
	cvs::sprintf(tmp2,80,"%s.%s._cvspserver._tcp.cvsnt.org",tmp.substr(1).c_str(),srv.c_str());
	tmp=tmp2;

	// Straight anonymous connection
	if(dns.Lookup(tmp.c_str(), DNS_TYPE_TEXT))
	{
		m_root = dns.GetRRTxt();
		return m_root.c_str();
	}

	// cvsnt-style anonymous mapping
	if(dns.Lookup(tmp.c_str(), DNS_TYPE_SRV))
	{
		CDnsApi::SrvRR *rr = dns.GetRRSrv();
		cvs::sprintf(m_root,80,"::%s",rr->server);
		return m_root.c_str();
	}

	CServerIo::trace(3,"DNS lookup of %s failed",tmp.c_str());
	return NULL;
}
