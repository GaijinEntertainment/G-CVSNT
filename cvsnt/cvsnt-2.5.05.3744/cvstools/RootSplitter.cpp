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
#include "RootSplitter.h"

#include <ctype.h>

bool CRootSplitter::Split(const char *root)
{
	if(!root || !*root)
		return false;

	m_root = root;

	const char *p = root,*q;
	const char *base = p;

	p=q=root;
	if(*p!=':') 
		return false;
	
	m_port="";

	q=++p;
	for(; *p && (*p!=':' && *p!=';'); p++)
		;
	if(!*p)
		return false;
	m_protocol.assign(q,p-q);
	if(*p==';')
	{
		char InQuote = 0;
		q=++p;
		for(; *p && (!InQuote && *p!=':'); p++)
		{
			if(InQuote && *p==InQuote)
			{
				InQuote=0;
				continue;
			}
			if(*p=='"' || *p=='\'')
			{
				InQuote=*p;
				continue;
			}
		}
		if(*p!=':' || InQuote)
			return false;
		m_keywords.assign(q,p-q);
	}
	if(strchr(p,'@'))
	{
		q=++p;
		for(;*p && *p!=':' && *p!='@'; p++)
			;
		if(!*p)
			return false;
		m_username.assign(q,p-q);
		if(*p==':')
		{
			q=++p;
			for(;*p && *p!='@'; p++)
				;
			if(!*p)
				return false;
			m_password.assign(q,p-q);
		}
	}
	q=++p;
	for(;*p && *p!='/' && *p!=':'; p++)
		;
	m_server.assign(q,p-q);
	if(*p==':')
	{
		if(isdigit((int)(unsigned)*(p+1)))
		{
			q=++p;
			while(isdigit((int)(unsigned)*p))
				p++;
			m_port.assign(q,p-q);
			if(*p==':')
				p++;
		}
		else
			p++;
	}
	if(*p!='/')
		return false;
	if(strchr(p,'*'))
	{
		q=p;
		for(;*p && *p!='*'; p++)
			;
		if(!*p)
			return false;
		m_directory.assign(q,p-q);
		m_module=++p;
	}
	else
		m_directory=p;
	return true;
}

const char *CRootSplitter::Join(bool password)
{
	if(password && m_username.size())
		cvs::sprintf(m_root,80,":%s%s:%s%s%s@%s%s%s:%s",m_protocol.c_str(),m_keywords.c_str(),m_username.c_str(),m_password.size()?":":"",m_password.c_str(),m_server.c_str(),m_port.size()?":":"",m_port.c_str(),m_directory.c_str());
	else if(m_username.size())
		cvs::sprintf(m_root,80,":%s%s:%s@%s%s%s:%s",m_protocol.c_str(),m_keywords.c_str(),m_username.c_str(),m_server.c_str(),m_port.size()?":":"",m_port.c_str(),m_directory.c_str());
	else
		cvs::sprintf(m_root,80,":%s%s:%s%s%s:%s",m_protocol.c_str(),m_keywords.c_str(),m_server.c_str(),m_port.size()?":":"",m_port.c_str(),m_directory.c_str());
	return m_root.c_str();
}
