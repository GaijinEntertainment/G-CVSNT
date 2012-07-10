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
#ifndef DIRECTORYACCESS__H
#define DIRECTORYACCESS__H

struct DirectoryAccessInfo
{
	DirectoryAccessInfo() { isdir = islink = false; }
	cvs::filename filename;
	bool isdir;
	bool islink;
};

class CDirectoryAccess
{
public:
	CVSAPI_EXPORT CDirectoryAccess();
	CVSAPI_EXPORT virtual ~CDirectoryAccess();

	CVSAPI_EXPORT bool open(const char *directory, const char *filter = NULL);
	CVSAPI_EXPORT bool close();
	CVSAPI_EXPORT bool next(DirectoryAccessInfo& info);

	static CVSAPI_EXPORT bool chdir(const char *directory);
	static CVSAPI_EXPORT const char *getcwd();

	static CVSAPI_EXPORT bool mkdir(const char *directory);
	static CVSAPI_EXPORT bool rmdir(const char *directory);

protected:
	static cvs::filename m_lastcwd;
	void *m_dir;
	const char *m_pFilter;
	const char *m_pDirectory;
#ifdef _WIN32
	const char *m_pDir;
#endif
};

#endif
