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
#include "Modules2.h"

bool CModules2::m_bModules2Loaded = false;
CModules2::modlist_t CModules2::m_ModList;

CModules2::CModules2()
{
}

CModules2::~CModules2()
{
}

bool CModules2::ReadModules2()
{
	if(m_bModules2Loaded)
		return true;
	m_bModules2Loaded=true;

	cvs::string path,line,inmodule;
	cvs::sprintf(path,256,"%s/%s/%s",current_parsed_root->directory,CVSROOTADM,CVSROOTADM_MODULES2);
	CFileAccess f;


	if(!f.open(path.c_str(),"r"))
	{
		TRACE(3,"Couldn't open modules2 file %s",path.c_str());
		return false;
	}

	TRACE(3,"Loading modules2 from %s",path.c_str());

	while(f.getline(line))
	{
		line = cvs::trim(line);
		if(!line.size() || line[0]=='#')
			continue;

		if(line[0]=='[') // Start of module
		{
			/* We don't allow [] [r [foo]bar] */
			if(line.find_first_of(']')!=line.length()-1)
			{
				error(0,0,"Malformed line '%s' in modules2 file",line.c_str());
				continue;
			}
			inmodule=line.substr(1,line.length()-1);
			TRACE(3,"Found module %s",inmodule.c_str());
			continue;
		}
		if(!inmodule.length())
		{
			error(0,0,"Malformed line '%s' in modules2 file",line.c_str());
			continue;
		}
		CTokenLine tok;
		tok.setSeparators("=");
		tok.addArgs(line.c_str());
		if(tok.size()<2 || tok.size()>3)
		{
			error(0,0,"Malformed line '%s' in modules2 file",line.c_str());
			continue;
		}
		Module2List_t l;
		l.directory=tok[0];
		line=tok[1];
		l.recurse=true;
		l.no_retranslate=false;
		size_t c = 0;
		while(line[c]=='+' || line[c]=='!')
		{
			if(line[c]=='+')
				l.recurse = false;
			if(line[c]=='!')
				l.no_retranslate=true;
			c++;
		}
		l.translation=line.substr(c);
		if(tok.size()==3)
			l.regexp=tok[2];
		m_ModList[inmodule.c_str()].push_back(l);
	}

	return true;
}

void CModules2::Translate(CvsBasicEntry& item, std::vector<CvsBasicEntry>& list)
{
	if(!m_bModules2Loaded)
		return;

	modlist_t::const_iterator i = m_ModList.find(item.logical_name.c_str());
	if(i==m_ModList.end())
		return;

	// Now what??
}
