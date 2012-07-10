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
#ifndef GETOPTIONS__H
#define GETOPTIONS__H

#include "TokenLine.h"

class CGetOptions
{
public:
	struct Option
	{
		int option;
		const char *arg;
	};

	CVSAPI_EXPORT CGetOptions(CTokenLine& tokens, size_t& argnum, const char *format_string);
	CVSAPI_EXPORT virtual ~CGetOptions();

	CVSAPI_EXPORT bool error() { return m_error; }
	CVSAPI_EXPORT size_t count() { return m_options.size(); }
	CVSAPI_EXPORT const Option& operator[](size_t off) { return m_options[off]; }

private:
	std::vector<Option> m_options;
	bool m_error;
};


#endif

