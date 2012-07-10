/*
	CVSNT Generic API
    Copyright (C) 2004 Tony Hoyle and March-Hare Software Ltd

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
#include <config.h>
#include "lib/api_system.h"
#include "cvs_string.h"
#include "ServerIO.h"
#include "SqlConnection.h"
#include "LibraryAccess.h"
#include "DirectoryAccess.h"

CSqlConnection* CSqlConnection::CreateConnection(const char *db, const char *dir)
{
	CLibraryAccess la;
	CSqlConnection* (*pNewSqlConnection)() = NULL;

	cvs::string name;

	name = db;
	name += SHARED_LIBRARY_EXTENSION;

	CServerIo::trace(3,"Connecting to %s",db);
	if(!la.Load(name.c_str(),dir))
		return false;
	pNewSqlConnection = (CSqlConnection*(*)())la.GetProc("CreateConnection");

	if(!pNewSqlConnection)
		return NULL;
	CSqlConnection *conn = pNewSqlConnection();
	la.Detach();
	return conn;
}

bool CSqlConnection::GetConnectionList(connectionList_t& list, const char *library_dir)
{
	CDirectoryAccess dir;
	DirectoryAccessInfo inf;

	if(!dir.open(library_dir,"*"SHARED_LIBRARY_EXTENSION))
		return false;

	list.clear();
	while(dir.next(inf))
	{
		if(inf.isdir)
			continue;
		cvs::string fn = inf.filename.c_str();
		fn.resize(fn.size()-sizeof(SHARED_LIBRARY_EXTENSION)+1);
		list.resize(list.size()+1);
		list[list.size()-1].first = fn; // DLL Name
		list[list.size()-1].second = fn; // Title - to be implemented later
	}
	dir.close();
	return true;
}

