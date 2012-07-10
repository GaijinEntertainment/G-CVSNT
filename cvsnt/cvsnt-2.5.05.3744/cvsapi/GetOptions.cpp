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

#include "GetOptions.h"

CGetOptions::CGetOptions(CTokenLine& tokens, size_t& argnum, const char *format_string)
{
	m_error = false;

	if(format_string && format_string[0]=='+') /* We always behave in the posixly_correct manner */
		format_string++;

	while(argnum<tokens.size())
	{
		Option opt;
		const char *p = tokens[argnum];

		if(*(p++)!='-')
			return;
		if(*p=='-')
		{
			p++;
			if(!*p)
				return; /* '--' */

			/* long option... --foo[=]bar] */
			/* not yet implemented */
			m_error = true;
			return;
		}
		else
		{
			if(!format_string)
			{
				m_error = true;
				return;
			}
			const char *q=strchr(format_string, p[0]);
			if(!q)
			{
				m_error = true;
				return;
			}
			opt.option=q[0];
			if(q[1]==':' && q[2]==':')
			{
				/* optional argument */
				if(p[1])
					opt.arg=p+1;
				else
					opt.arg = NULL;
				argnum++;
			}
			else if(q[1]==':')
			{
				/* Mandatory argument */
				if(p[1])
				{
					opt.arg=p+1;
					argnum++;
				}
				else
				{
					if(++argnum>=tokens.size())
					{
						m_error = true;
						return;
					}
					opt.arg=tokens[argnum++];
				}
			}
			else
				argnum++;
		}
		m_options.push_back(opt);
	}
}

CGetOptions::~CGetOptions()
{
}
