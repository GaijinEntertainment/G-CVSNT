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
#ifndef PROTOCOL_LIBRARY__H
#define PROTOCOL_LIBRARY__H

#include "plugin_interface.h"
#include "protocol_interface.h"

class CProtocolLibrary
{
public:
	CVSTOOLS_EXPORT CProtocolLibrary() { }
	CVSTOOLS_EXPORT virtual ~CProtocolLibrary() { }

	CVSTOOLS_EXPORT bool SetupServerInterface(cvsroot *root, int io_socket = 0);
	CVSTOOLS_EXPORT server_interface *GetServerInterface();
	CVSTOOLS_EXPORT const protocol_interface *LoadProtocol(const char *protocol);
	CVSTOOLS_EXPORT bool UnloadProtocol(const protocol_interface *protocol);
	CVSTOOLS_EXPORT const protocol_interface *FindProtocol(const char *tagline, bool& badauth,int io_socket, bool secure, const protocol_interface **temp_protocol);
	CVSTOOLS_EXPORT const char *EnumerateProtocols(int *context);

	CVSTOOLS_EXPORT static bool PromptForPassword(const char *prompt, char *buffer, int max_length);
	CVSTOOLS_EXPORT static char PromptForAnswer(const char *message, const char *title, bool withcancel);
	CVSTOOLS_EXPORT static const char *GetEnvironment(const char *env);

protected:
	CDirectoryAccess m_acc;
	DirectoryAccessInfo m_inf;
	typedef std::map<cvs::string, protocol_interface*> loaded_protocols_t;
	static loaded_protocols_t m_loaded_protocols;

	static const char *__PromptForPassword(const char *prompt);

};

#endif
