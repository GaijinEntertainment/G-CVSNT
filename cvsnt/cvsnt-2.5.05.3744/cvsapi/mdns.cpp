/*
	CVSNT mdns helpers
    Copyright (C) 2005 Tony Hoyle and March-Hare Software Ltd

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
#include <config.h>
#include "lib/api_system.h"
#include "cvs_string.h"
#include "ServerIO.h"
#include "mdns.h"
#include "LibraryAccess.h"

CMdnsHelperBase* CMdnsHelperBase::CreateHelper(const char *type, const char *dir)
{
	CLibraryAccess la;
	CMdnsHelperBase* (*pNewMdnsHelper)() = NULL;

	if(!type)
		type=MDNS_DEFAULT;

	CServerIo::trace(3,"Loading MDNS helper %s",type);
	cvs::string tmp = type;
	tmp+=SHARED_LIBRARY_EXTENSION;
	if(!la.Load(tmp.c_str(),dir))
		return false;
	pNewMdnsHelper = (CMdnsHelperBase*(*)())la.GetProc("CreateHelper");
	if(!pNewMdnsHelper)
		return NULL;
	CMdnsHelperBase *mdns = pNewMdnsHelper();
	la.Detach(); 
	return mdns;
}
