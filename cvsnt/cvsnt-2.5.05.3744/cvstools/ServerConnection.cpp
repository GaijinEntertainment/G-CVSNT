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
#include "ServerConnection.h"
#include "GlobalSettings.h"
#include "RootSplitter.h"

#include <ctype.h>

bool CServerConnection::Connect(const char *command, ServerConnectionInfo *info, CServerConnectionCallback* callback, int (*debugFn)(int type, const char *,size_t, void *), void *userData)
{
	const char *cvsnt = CGlobalSettings::GetCvsCommand();

	bool repeat = true;
	int pass = 0;

	if(info->level==1)
	{
		info->protocol = info->default_proto;
		if(!info->protocol.size())
			info->protocol = "pserver";

		if(!info->enumerated)
		{
			CRootSplitter split;
			split.Split(info->root.c_str());
			info->protocol = split.m_protocol;
			info->username = split.m_username;
			info->password = split.m_password;
			if(split.m_port.size())
				info->port = split.m_port;
			info->server = split.m_server;
			info->directory = split.m_directory;
			info->keywords = split.m_keywords;
			info->anonymous = false;
		}
	}

	do
	{
		cvs::string keys;
		if(info->keywords.size())
		{
			keys=";"+info->keywords;
		}
		switch(pass)
		{
		case 0: // Default, no password
			if(info->username.size())
				cvs::sprintf(info->root,80,":%s%s:%s%s%s@%s%s%s:%s",info->protocol.c_str(),keys.c_str(),info->username.c_str(),info->password.size()?":":"",info->password.c_str(),info->server.c_str(),info->port.size()?":":"",info->port.c_str(),info->directory.c_str());
			else
				cvs::sprintf(info->root,80,":%s%s:%s%s%s:%s",info->protocol.c_str(),keys.c_str(),info->server.c_str(),info->port.size()?":":"",info->port.c_str(),info->directory.c_str());
			pass++;
			break;
		case 1: // Default, password (loop until cancel)
			cvs::sprintf(info->root,80,":%s%s:%s%s%s:%s",info->protocol.c_str(),keys.c_str(),info->server.c_str(),info->port.size()?":":"",info->port.c_str(),info->directory.c_str());
			if(!callback->AskForPassword(info))
			{
				info->invalid=true;
				return false;
			}
			if(info->username.size())
				cvs::sprintf(info->root,80,":%s%s:%s%s%s@%s%s%s:%s",info->protocol.c_str(),keys.c_str(),info->username.c_str(),info->password.size()?":":"",info->password.c_str(),info->server.c_str(),info->port.size()?":":"",info->port.c_str(),info->directory.c_str());
			else
				cvs::sprintf(info->root,80,":%s%s:%s%s%s:%s",info->protocol.c_str(),keys.c_str(),info->server.c_str(),info->port.size()?":":"",info->port.c_str(),info->directory.c_str());
			break;
		}

#ifdef _WIN32
		HCURSOR hOldCursor = GetCursor();
		SetCursor(LoadCursor(NULL,IDC_WAIT));
#endif

		m_error = -1;
		m_callback = callback;
		CRunFile rf;
		rf.setOutput(_ServerOutput,this);
		rf.setDebug(debugFn,userData);
		rf.resetArgs();
		rf.addArg(cvsnt);
		rf.addArg("--utf8");
		rf.addArg("-z3");
		rf.addArg("-d");
		rf.addArg(info->root.c_str());
		rf.addArgs(command);
		if(!rf.run(NULL))
		{
#ifdef _WIN32
			SetCursor(hOldCursor);
#endif
			callback->Error(info,SCEFailedBadExec);
			info->invalid = true;
			return false ;
		}
		int res;
		rf.wait(res);

#ifdef _WIN32
		SetCursor(hOldCursor);
#endif

		if(m_error)
		{
			if(m_error == -1)
				break; // No output - not necessarily an error

			switch(m_error)
			{
			case 1:
				// Protocol/connect failed...
				if(info->anon_proto.size())
					info->protocol = info->anon_proto;
				continue;
			case 2:
				// Auth failed
				continue;
			case 3:
				// Command not supported
				callback->Error(info,SCEFailedNoSupport);
				info->invalid = true;
				return false;
			case 4:
				// Aborted
				callback->Error(info,SCEFailedCommandAborted);
				info->invalid = true;
				return false;
			}
		}
		else
			repeat = false;
	} while(repeat);

	info->invalid = false;
	return true;
}

int CServerConnection::_ServerOutput(const char *data,size_t len,void *param)
{
	return ((CServerConnection*)param)->ServerOutput(data,len);
}

int CServerConnection::ServerOutput(const char *data,size_t len)
{
	const char *p = data, *q;
	cvs::string str;
	do
	{
		for(q=p; q<data+len; q++)
			if(*q=='\n')
				break;
		if(q>p+1)
		{
			q--;
			str.assign(p,q-p);
			CServerIo::trace(3,"Connection trace: %s\n",str.c_str());
			if(strstr(str.c_str(),"Connection to server failed") || strstr(str.c_str(),"is not installed on this system") || strstr(str.c_str(),"is not available on this system"))
			{
				m_error = 1;
				return -1;
			}
			if(strstr(str.c_str(),"authorization failed") || strstr(str.c_str(),"Rejected access") || strstr(str.c_str(),"no such user"))
			{
				m_error = 2;
				return -1;
			}
			if(strstr(str.c_str(),"server does not support"))
			{
				m_error = 3;
				return -1;
			}
			if(strstr(str.c_str()," aborted]:"))
			{
				m_error = 4;
				return -1;
			}
			if(strncasecmp(str.c_str(),"Empty password used",19))
			{
				m_error = 0;
				m_callback->ProcessOutput(str.c_str());
			}
		}
		p=q;
		while(p<data+len && isspace((unsigned char)*p))
			p++;
	} while(p<data+len);
	return (int)len;
}

