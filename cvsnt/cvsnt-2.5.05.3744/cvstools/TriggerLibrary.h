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
#ifndef TRIGGER_LIBRARY__H
#define TRIGGER_LIBRARY__H

#include "plugin_interface.h"
#include "trigger_interface.h"

#ifdef _WIN32
	struct ICvsInfo4;
	struct ICvsInfo5;
	struct ICvsInfo6;
#endif 

class CTriggerLibrary
{
public:
#ifdef _WIN32
	struct InfoStruct
	{
			ICvsInfo4 *i4;
			ICvsInfo5 *i5;
			ICvsInfo6 *i6;
	};
#endif

	struct trigger_info_t
	{
		trigger_info_t() { pLib=NULL; delete_trig = false; }
#ifdef _WIN32
		InfoStruct is;
#endif
		void *pLib;

		std::vector<void*> to_free;
		bool delete_trig;
	};

	CVSTOOLS_EXPORT CTriggerLibrary() { }
	CVSTOOLS_EXPORT virtual ~CTriggerLibrary() { }

	CVSTOOLS_EXPORT const trigger_interface *LoadTrigger(const char *name, const char *command, const char *date, const char *hostname, const char *username, const char *virtual_repository, const char *physical_repository, const char *sessionid, const char *editor, int count_uservar, const char **uservar, const char **userval, const char *client_version, const char *character_set);
	CVSTOOLS_EXPORT bool CloseTrigger(trigger_interface *trg);
	CVSTOOLS_EXPORT bool CloseAllTriggers();
	CVSTOOLS_EXPORT const trigger_interface *EnumLoadedTriggers(bool& first, const char *& name);

protected:
	typedef std::map<cvs::string, trigger_interface*> trigger_list_t;

	static trigger_list_t trigger_list;
	trigger_list_t::const_iterator m_it;
};

#endif
