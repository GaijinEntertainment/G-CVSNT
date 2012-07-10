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
#ifndef TOKENLINE__H
#define TOKENLINE__H

#include <vector>
#include "cvs_string.h"

class CTokenLine
{
public:
	CVSAPI_EXPORT CTokenLine();
	CVSAPI_EXPORT CTokenLine(const char *line);
	CVSAPI_EXPORT CTokenLine(int argc, const char *const*argv);
	CVSAPI_EXPORT virtual ~CTokenLine();

	CVSAPI_EXPORT const char *operator[](size_t off) { return m_args[off].c_str(); }
	CVSAPI_EXPORT size_t size() { return m_args.size(); }
	CVSAPI_EXPORT bool shift(size_t count = 0) { if(m_args.empty()) return false; m_args.erase(m_args.begin(),m_args.begin() + count); return true; }
	CVSAPI_EXPORT const char *const*toArgv(size_t off = 0);
	CVSAPI_EXPORT const char *toString(size_t off = 0);

	CVSAPI_EXPORT bool resetArgs();
	CVSAPI_EXPORT bool addArg(const char *arg);
	CVSAPI_EXPORT bool insertArg(size_t pos, const char *arg);
	CVSAPI_EXPORT bool deleteArg(size_t pos);
	CVSAPI_EXPORT bool addArgs(const char *line, int maxArg = 0,const char **argPos = NULL);
	CVSAPI_EXPORT bool addArgs(int argc, const char *const*argv);
	CVSAPI_EXPORT bool setArgs(const char *line);
	CVSAPI_EXPORT bool setArgs(int argc, const char *const*argv);
	CVSAPI_EXPORT bool setSeparators(const char *sep);

private:
	std::vector<cvs::string> m_args;
	const char *const*m_argv;
	cvs::string m_line;
	cvs::string m_separators;
};

#endif

