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

#include "TokenLine.h"
#include "ServerIO.h"

#include <ctype.h>

CTokenLine::CTokenLine()
{
	m_argv = NULL;
}

CTokenLine::CTokenLine(const char *line)
{
	m_argv = NULL;
	setArgs(line);
}

CTokenLine::CTokenLine(int argc, const char *const*argv)
{
	m_argv = NULL;
	setArgs(argc,argv);
}

CTokenLine::~CTokenLine()
{
	if(m_argv)
		delete[] m_argv;
}

const char *const*CTokenLine::toArgv(size_t off)
{
	const char **argv;
	size_t n;

	if(m_argv)
		delete[] m_argv;
	m_argv=NULL;

	if(off>=m_args.size())
		return NULL;

	m_argv = argv = new const char*[(m_args.size()-off)+1];
	for(n=off; n<m_args.size(); n++)
		argv[n-off]=m_args[n].c_str();
	argv[n-off]=NULL;
	return m_argv;
}

const char *CTokenLine::toString(size_t off)
{
	m_line="";

	if(off>=m_args.size())
		return "";

	for(size_t n=off; n<m_args.size(); n++)
	{
	    static const char meta[] = "`\"' ";
		const char *str=m_args[n].c_str();

		if(*str && !strpbrk(str,meta))
			m_line+=str;
		else
		{
			m_line.append("\"",1);
			for(const char *p=str; *p; p++)
			{
				if(*p=='"')
					m_line.append("\\",1);
				m_line.append(p,1);
			}
			m_line.append("\"",1);
		}
		if((n+1)<m_args.size())
			m_line.append(" ",1);
	}
	return m_line.c_str();
}

bool CTokenLine::resetArgs()
{
	m_args.clear();
	return true;
}

bool CTokenLine::addArg(const char *arg)
{
	m_args.push_back(arg);
	return true;
}

bool CTokenLine::insertArg(size_t pos, const char *arg)
{
	if(pos>m_args.size())
	  	return false;

	m_args.insert(m_args.begin()+pos,arg);
	return true;
}

bool CTokenLine::deleteArg(size_t pos)
{
	if(pos>=m_args.size())
		return false;

	m_args.erase(m_args.begin()+pos);
	return true;
}

bool CTokenLine::addArgs(const char *line, int maxArg /* =0 */,const char **argPos /* =NULL */)
{
	char inQuote = 0;
	const char *p = line;
	cvs::string arg;

	arg.reserve(256);

	while(*p)
	{
		arg="";

		while(*p && (isspace((unsigned char)*p) || strchr(m_separators.c_str(),(unsigned char)*p)))
			p++;

		while(*p && (inQuote || (!isspace((unsigned char)*p) && !strchr(m_separators.c_str(),(unsigned char)*p))))
		{
			if(*p=='\\' && *(p+1))
			{
				p++;
				switch(*p)
				{
				case 'n':
					arg.append('\n',1); break;
				case 'r':
					arg.append('\r',1); break;
				case 'b':
					arg.append('\b',1); break;
				case 't':
					arg.append('\t',1); break;
				default:
					if(isspace(*p) || !strchr(m_separators.c_str(),*p) || *p=='%' || *p=='$' || *p==',' || *p=='{' || *p=='}' || *p=='<' || *p=='>' || *p=='\\' || *p=='\'' || *p=='"')
						arg+=*p;
					else
					{
						CServerIo::warning("Unknown escape character '\\%c' ignored.\n",*p);
						arg+='\\';
						arg+=*p;
					}
					break;
				}
			}
			else if(!inQuote && (*p=='"' || *p=='\''))
				inQuote=*p;
			else if(inQuote==*p)
				inQuote=0;
			else
				arg.append(p,1);
			p++;
		}
		if(*p || arg.length())
			m_args.push_back(arg);
		if(maxArg>0 && m_args.size()>=(size_t)maxArg)
			break;
	}
	if(argPos)
		*argPos=p;
	return true;
}

bool CTokenLine::addArgs(int argc, const char *const*argv)
{
	for(int n=0; n<argc; n++)
		m_args.push_back(argv[n]);
	return true;
}

bool CTokenLine::setArgs(const char *line)
{
	m_args.clear();
	return addArgs(line);
}

bool CTokenLine::setArgs(int argc, const char *const*argv)
{
	m_args.clear();
	return addArgs(argc,argv);
}

bool CTokenLine::setSeparators(const char *sep)
{
	m_separators=sep;
	return true;
}
