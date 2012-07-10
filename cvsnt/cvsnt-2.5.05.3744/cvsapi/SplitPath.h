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
#ifndef SPLITPATH__H
#define SPLITPATH__H

#include <vector>

class CSplitPath
{
public:
	CVSAPI_EXPORT CSplitPath(const char *path);
	virtual CVSAPI_EXPORT ~CSplitPath();

	CVSAPI_EXPORT cvs::string& operator[](size_t off) { return m_pa[off]; }
	CVSAPI_EXPORT size_t size() const { return m_pa.size(); }
	CVSAPI_EXPORT cvs::string& JoinPath(size_t start, size_t end = (size_t)-1 );

protected:
	cvs::string m_tmppath;
	std::vector<cvs::string> m_pa;
};

#endif
