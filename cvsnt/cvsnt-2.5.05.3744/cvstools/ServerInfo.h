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
#ifndef SERVERINFO__H
#define SERVERINFO__H

class CServerInfo
{
public:
	struct remoteServerInfo
	{
		typedef std::map<cvs::string,cvs::string> repositories_t;
		typedef std::map<cvs::string,int> protocols_t;
		repositories_t repositories;
		cvs::string anon_username, anon_protocol;
		cvs::string server_name, server_version, default_repository, default_protocol;
		protocols_t protocols;
	};

	CVSTOOLS_EXPORT CServerInfo() { }
	CVSTOOLS_EXPORT virtual ~CServerInfo() { }
	
	CVSTOOLS_EXPORT bool getRemoteServerInfo(const char *server, remoteServerInfo& rsi);
	CVSTOOLS_EXPORT const char *getGlobalServerInfo(const char *server);

protected:
	cvs::string m_root;
};

#endif
