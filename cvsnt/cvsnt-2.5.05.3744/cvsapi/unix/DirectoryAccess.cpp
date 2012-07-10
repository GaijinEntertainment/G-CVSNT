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
/* Unix specific */
#include <config.h>
#include "../lib/api_system.h"

#include "cvs_string.h"
#include "DirectoryAccess.h"

#include <dirent.h>
#include <unistd.h>
#include <glob.h>
#include <sys/stat.h>

cvs::filename CDirectoryAccess::m_lastcwd;

CDirectoryAccess::CDirectoryAccess()
{
	m_dir = NULL;
}

CDirectoryAccess::~CDirectoryAccess()
{
	close();
}

bool CDirectoryAccess::open(const char *directory, const char *filter /* = NULL */)
{
	glob_t *globbuf;
	cvs::filename fn;

	m_pFilter = filter;
	m_pDirectory = directory;
	globbuf = new glob_t;
	memset(globbuf,0,sizeof(glob_t));
	cvs::sprintf(fn,80,"%s/%s",directory,filter?filter:"*");
	globbuf->gl_offs = 0;
	if(glob(fn.c_str(),GLOB_ERR|GLOB_NOSORT,NULL,globbuf) != 0 || globbuf->gl_pathc == 0)
	{
		globfree(globbuf);
		delete globbuf;
		return true;
	}
	m_dir = (void*)globbuf;
	globbuf->gl_offs = 0;
	return true;
}

bool CDirectoryAccess::close()
{
	if(m_dir)
	{
		glob_t *globbuf = (glob_t*)m_dir;
		globbuf->gl_offs = 0;
		globfree(globbuf);
		delete globbuf;
	}
	m_dir = NULL;
	return true;
}

bool CDirectoryAccess::next(DirectoryAccessInfo& info)
{
	glob_t *globbuf = (glob_t*)m_dir;

	if(!m_dir)
		return false;
	
	if(globbuf->gl_offs >= globbuf->gl_pathc)
	{
		close();	
		return false;
	}
	
	info.filename = globbuf->gl_pathv[globbuf->gl_offs++]+strlen(m_pDirectory)+1;
	cvs::filename fn;
	cvs::sprintf(fn,80,"%s/%s",m_pDirectory,info.filename.c_str());
	struct stat st;
	info.isdir=false;
	info.islink=false;
	if(!stat(fn.c_str(),&st))
	{
		info.isdir=S_ISDIR(st.st_mode)?true:false;
		info.islink=S_ISLNK(st.st_mode)?true:false;
	}

	return true;
}

bool CDirectoryAccess::chdir(const char *dir)
{
   return ::chdir(dir)?false:true;
}

const char *CDirectoryAccess::getcwd()
{
	m_lastcwd.resize(PATH_MAX+1);
	::getcwd((char*)m_lastcwd.data(),m_lastcwd.size());
	m_lastcwd.resize(strlen((char*)m_lastcwd.data()));
	return m_lastcwd.c_str();
}

bool CDirectoryAccess::mkdir(const char *directory)
{
   return ::mkdir(directory,0777)?false:true;
}

bool CDirectoryAccess::rmdir(const char *directory)
{
   return ::rmdir(directory)?false:true;
}
