/*	CVS Recursion processor
    Copyright (C) 2004 Tony Hoyle and March-Hare Software Ltd

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
#define NO_REDIRECTION
#include "cvs.h"
#include "RecurseRepository.h"
#include "Modules1.h"
#include "Modules2.h"

CRecurseRepository::CRecurseRepository()
{
}

CRecurseRepository::~CRecurseRepository()
{
}

// Start the recursion
// Module may be a directory or a file, and will be correct
// only from the users' point of view.  
bool CRecurseRepository::BeginRecursion(const char *module)
{
	typedef std::vector<CvsBasicEntry> dirlist_t;
	dirlist_t dirlist;
	CvsBasicEntry first;

	CModules1 mod1;
	CModules2 mod2;

	mod1.ReadModules1();
	mod2.ReadModules2();

	first.logical_name=module;
	first.physical_name=module;
	dirlist.push_back(first);
	for(size_t i=0; i<dirlist.size(); i++)
	{
		mod1.Translate(dirlist[i],dirlist);
		mod2.Translate(dirlist[i],dirlist);
		GetPhysTree(dirlist[i],dirlist);

		/* In theory we can process dirlist[i] at this point I think */
		/* If we can maybe sort into the right order, and use that as the basis for recursion....
		   might slow things down though.  Need to test */
	}

/*
	user module = foo{foo}
	translate via modules
	foo/bar{foo/bar},foo/baz{foo/baz}
	translate via modules2
	foo/bar{repo/bar},foo/ban{repo/baz}
	Add physical
	foo/bar{repo/bar} foo/ban{repo/baz} foo/bar/a{repo/bar/a} foo/bar/b{repo/bar/b}
	Add to tree

	// Directory rename?  This is tracked in both directories so in theory can go later
	// File rename is similar.

	user module = 	foo/bar{repo/bar} foo/ban{repo/baz} foo/bar/a{repo/bar/a},foo/bar/b{repo/bar/b}
	translate via modules
	foo/bar{repo/bar} foo/ban{repo/baz} foo/bar/a{repo/bar/a},foo/bar/b{repo/bar/b}
	translate via modules2
	bar{repo/bar},ban{repo/ban}
*/

	return true;
}

void CRecurseRepository::GetPhysTree(CvsBasicEntry& item, std::vector<CvsBasicEntry>& list)
{
	CDirectoryAccess dir;
	DirectoryAccessInfo inf;

	if(!dir.open(item.physical_name.c_str()))
	{
		CServerIo::trace(3,"Physical directory '%s' does not exist",item.physical_name.c_str());
		return;
	}
	while(dir.next(inf))
	{
		if(!inf.isdir)
			continue;

		// Handle islink here like a separate mapping possibly

		CvsBasicEntry ent;
		cvs::sprintf(ent.logical_name,256,"%s/%s",item.logical_name.c_str(),"/",inf.filename.c_str());
		cvs::sprintf(ent.physical_name,256,"%s/%s",item.physical_name.c_str(),"/",inf.filename.c_str());
		list.push_back(ent);
	}
}

