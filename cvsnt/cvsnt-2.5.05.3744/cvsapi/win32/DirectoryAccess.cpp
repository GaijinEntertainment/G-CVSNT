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
/* Win32 specific */
#include <config.h>
#include "../lib/api_system.h"

#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <windows.h>

#include "../cvs_string.h"
#include "../DirectoryAccess.h"
#include "../FileAccess.h"

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
	DWORD dwAtt = GetFileAttributesW(CFileAccess::Win32Wide(directory));
	if(dwAtt==0xFFFFFFFF || !(dwAtt&FILE_ATTRIBUTE_DIRECTORY))
		return false;
	m_pDir = directory;
	m_pFilter = filter?filter:"*.*";
	return true;
}

bool CDirectoryAccess::close()
{
	if(m_dir)
		FindClose((HANDLE)m_dir);
	m_dir=NULL;
	return true;
}

bool CDirectoryAccess::next(DirectoryAccessInfo& info)
{
	WIN32_FIND_DATAW wfd;
	do
	{
		if(!m_dir && *m_pDir)
		{
			cvs::string str;
			HANDLE hFind;

			cvs::sprintf(str,512,"%s\\%s",m_pDir,m_pFilter);
			hFind = FindFirstFileW(CFileAccess::Win32Wide(str.c_str()),&wfd);
			if(hFind==INVALID_HANDLE_VALUE)
				return false;
			m_dir = (void*)hFind;
		}
		else if(!m_dir)
		{
			return false;
		}
		else if(!FindNextFileW((HANDLE)m_dir,&wfd))
		{
			close();
			return false;
		}
	} while(!wcscmp(wfd.cFileName,L".") || !wcscmp(wfd.cFileName,L".."));

	info.filename = CFileAccess::Win32Narrow(wfd.cFileName);
	info.isdir = wfd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY?true:false;
	info.islink = false;
	return true;
}

bool CDirectoryAccess::chdir(const char *directory)
{
	if(!SetCurrentDirectory(CFileAccess::Win32Wide(directory)))
		return false;
	return true;
}

const char *CDirectoryAccess::getcwd()
{
	TCHAR dir[MAX_PATH];
	if(!GetCurrentDirectory(sizeof(dir),dir))
		return "";
	m_lastcwd=CFileAccess::Win32Narrow(dir);
	return m_lastcwd.c_str();
}

bool CDirectoryAccess::mkdir(const char *directory)
{
	if(!CreateDirectory(CFileAccess::Win32Wide(directory),NULL))
		return false;
	return true;
}

bool CDirectoryAccess::rmdir(const char *directory)
{
	if(!RemoveDirectory(CFileAccess::Win32Wide(directory)))
		return false;
	return true;
}

