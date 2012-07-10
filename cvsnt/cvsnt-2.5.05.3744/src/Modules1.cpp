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
#include "Modules1.h"

bool CModules1::m_bModules1Loaded = false;
CModules1::modlist_t CModules1::m_ModList;

CModules1::CModules1()
{
}

CModules1::~CModules1()
{
}

bool CModules1::ReadModules1()
{
	if(m_bModules1Loaded)
		return true;
	m_bModules1Loaded=true;

	cvs::string path,line,directory,module;
	cvs::sprintf(path,256,"%s/%s/%s",current_parsed_root->directory,CVSROOTADM,CVSROOTADM_MODULES);
	CFileAccess f;

	if(!f.open(path.c_str(),"r"))
	{
		TRACE(3,"Couldn't open modules file %s",path.c_str());
		return false;
	}

	TRACE(3,"Loading modules from %s",path.c_str());

	while(f.getline(line))
	{
		bool alias = false;

		line = cvs::trim(line);
		if(!line.size() || line[0]=='#')
			continue;

		CTokenLine tok;
		tok.addArgs(line.c_str());
		if(tok.size()<2)
		{
			error(0,0,"Malformed line '%s' in modules file",line.c_str());
			continue;
		}

		directory=module=tok[0];
		size_t arg = 1;
		CGetOptions opt(tok, arg, "ad:");
		
		for(size_t n=0; n<opt.count(); n++)
		{
			switch(opt[n].option)
			{
				case 'a':
					alias=true;
					break;
				case 'd':
					directory=opt[n].arg;
					break;
				default:
					error(0,0,"Unrecognised option '%c' in modules file",opt[n].option);
					break;
			}
		}

		if(tok.size()<=arg)
		{
			error(0,0,"Malformed line '%s' in modules file",line.c_str());
			continue;
		}

		Module1List_t mod;
		mod.module=module.c_str();
		if(alias)
		{
			/* Alias modules */
			/* Subdirectories are the arguments */
			/* -a is always on its own... */
			mod.directory="";
			for(;arg<tok.size(); arg++)
			{
				if(tok[arg][0]=='&')
				{
					error(0,0,"Bad module definition '%s'",line.c_str());
					break;
				}
				if(tok[arg][0]=='!')
					mod.ignore.push_back(tok[arg]+1);
				else
					mod.modules.push_back(tok[arg]);
			}
			if(arg<tok.size())
				continue;
		}
		else if(tok[arg][0]=='&')
		{
			/* Ampersand modules */
			/* Subdirectories are the module directory plus the arguments */
			mod.directory=directory.c_str();
			for(;arg<tok.size(); arg++)
			{
				if(tok[arg][0]!='&')
				{
					error(0,0,"Bad module definition '%s'",line.c_str());
					break;
				}
				mod.modules.push_back(tok[arg]+1);
			}
			if(arg<tok.size())
				continue;
		}
		else 
		{
			/* Standard modules */
			/* Subdirectories are the module directory plus argument 1, bounded by the list of files */
			mod.directory=directory.c_str();
			mod.modules.push_back(tok[arg]);
			for(arg++;arg<tok.size();arg++)
				mod.files.push_back(tok[arg]);
		}

		m_ModList[module.c_str()]=mod;
	}

	return true;
}

CvsDirectoryEntry& CModules1::AddDirectory(CvsDirectoryEntry& root, const char *directory)
{
	CvsDirectoryEntry* cur, *parent;
	cvs::string log,phys;

	if(!directory || !*directory)
		return root;

	CSplitPath path(directory);

	log = root.logical_name.c_str();
	phys = root.physical_name.c_str();
	parent = &root;

	for(size_t n=0; n<path.size(); n++)
	{
		if(log.length()) log+='/';
		log+=path[n];
		if(phys.length()) phys+='/';
		phys+=path[n];
		if(parent->children.find(path[n].c_str())!=parent->children.end())
		{
			parent = parent->children[path[n].c_str()];
		}
		else
		{
			cur = new CvsDirectoryEntry;
			cur->logical_name=log.c_str();
			cur->physical_name=phys.c_str();
			parent->children[path[n].c_str()]=cur;
			parent = cur;
		}
	}
	return *parent;
}

void CModules1::Translate(CvsBasicEntry& item, std::vector<CvsBasicEntry>& list)
{
	if(!m_bModules1Loaded)
		return;

	modlist_t::const_iterator i = m_ModList.find(item.logical_name.c_str());
	if(i==m_ModList.end())
		return;

	// Now what??
}

/*bool CModules1::GetModuleTree(const char *module, CvsDirectoryEntry& tree)
{
	if(!m_bModules1Loaded)
		return false;

	CSplitPath path(module);

	for(size_t n=0; n<path.size(); n++)
	{
		cvs::string& tmp = path.JoinPath(0,n);
		if(m_ModList.find(tmp.c_str())!=m_ModList.end())
		{
			Module1List_t mod = m_ModList[tmp.c_str()];
			CvsDirectoryEntry& newtree = AddDirectory(tree,mod.directory.c_str());
			for(size_t m=0; m<mod.modules.size(); m++)
				GetModuleTree(mod.modules[m].c_str(),newtree);
			for(size_t m=0; m<mod.ignore.size(); m++)
			{
				CvsDirectoryEntry& deltree = AddDirectory(newtree,mod.modules[m].c_str());
				deltree.ignored=true;
			}
		}
	}

	return true;
}
*/
