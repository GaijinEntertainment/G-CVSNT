/*
	CVSNT Helper application API
    Copyright (C) 2004-6 Tony Hoyle and March-Hare Software Ltd

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
#include "EntriesParser.h"

CEntriesParser::CEntriesParser()
{
}

CEntriesParser::~CEntriesParser()
{
}

bool CEntriesParser::IsCvsControlledDirectory(const char *Directory)
{
	cvs::filename fn = Directory;

	if(!CFileAccess::exists((fn+"/CVS").c_str()))
		return false;
	if(!CFileAccess::exists((fn+"/CVS/Root").c_str()))
		return false;
	if(!CFileAccess::exists((fn+"/CVS/Repository").c_str()))
		return false;
	if(!CFileAccess::exists((fn+"/CVS/Entries").c_str()))
		return false;
	return true;
}

bool CEntriesParser::Load(const char *Directory)
{
	cvs::filename fn = Directory;

	m_fileMap.clear();

	if(!CFileAccess::exists((fn+"/CVS").c_str()))
		return false;
	if(!CFileAccess::exists((fn+"/CVS/Root").c_str()))
		return false;
	if(!CFileAccess::exists((fn+"/CVS/Repository").c_str()))
		return false;

	CFileAccess fa;
	cvs::string line;
	entries_t ent;

	if(!fa.open((fn+"/CVS/Entries").c_str(),"r"))
		return false;

	while(fa.getline(line))
	{
		if(ParseEntriesLine(line,ent))
			m_fileMap[ent.filename]=ent;
	}

	return true;
}

bool CEntriesParser::Exists(const char *filename)
{
	if(m_fileMap.find(filename)!=m_fileMap.end())
		return true;
	return false;
}

bool CEntriesParser::GetEntry(const char *filename, const entries_t*& entry)
{
	if(!Exists(filename))
		return false;
	entry=&m_fileMap[filename];
	return true;
}

bool CEntriesParser::ParseEntriesLine(cvs::string& line, entries_t& ent)
{
	const char *p=line.c_str(), *q;

	q=strchr(p,'/');
	if(!q)
		return false;
	ent.type=*p=='D'?'D':'F';
	p=q+1;

	q=strchr(p,'/');
	if(!q)
		return false;
	ent.filename.assign(p,q-p);
	p=q+1;

	q=strchr(p,'/');
	if(!q)
		return false;
	ent.version.assign(p,q-p);
	p=q+1;

	q=strchr(p,'/');
	if(!q)
		return false;
	ent.date.assign(p,q-p);
	p=q+1;

	q=strchr(p,'/');
	if(!q)
		return false;
	ent.expansion.assign(p,q-p);
	p=q+1;

	ent.tag=p;

	return true;
}

bool CEntriesParser::Unload()
{
	m_fileMap.clear();
	return true;
}
