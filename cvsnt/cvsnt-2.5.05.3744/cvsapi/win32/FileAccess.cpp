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
#include <io.h>
#include <tchar.h>
#include <errno.h>

#include <stdio.h>

#include "../cvs_string.h"
#include "../FileAccess.h"
#include "../ServerIO.h"

bool CFileAccess::m_bUtf8Mode = false;

static struct { DWORD err; int dos;} errors[] =
{
        {  ERROR_INVALID_FUNCTION,       EINVAL    },  /* 1 */
        {  ERROR_FILE_NOT_FOUND,         ENOENT    },  /* 2 */
        {  ERROR_PATH_NOT_FOUND,         ENOENT    },  /* 3 */
        {  ERROR_TOO_MANY_OPEN_FILES,    EMFILE    },  /* 4 */
        {  ERROR_ACCESS_DENIED,          EACCES    },  /* 5 */
        {  ERROR_INVALID_HANDLE,         EBADF     },  /* 6 */
        {  ERROR_ARENA_TRASHED,          ENOMEM    },  /* 7 */
        {  ERROR_NOT_ENOUGH_MEMORY,      ENOMEM    },  /* 8 */
        {  ERROR_INVALID_BLOCK,          ENOMEM    },  /* 9 */
        {  ERROR_BAD_ENVIRONMENT,        E2BIG     },  /* 10 */
        {  ERROR_BAD_FORMAT,             ENOEXEC   },  /* 11 */
        {  ERROR_INVALID_ACCESS,         EINVAL    },  /* 12 */
        {  ERROR_INVALID_DATA,           EINVAL    },  /* 13 */
        {  ERROR_INVALID_DRIVE,          ENOENT    },  /* 15 */
        {  ERROR_CURRENT_DIRECTORY,      EACCES    },  /* 16 */
        {  ERROR_NOT_SAME_DEVICE,        EXDEV     },  /* 17 */
        {  ERROR_NO_MORE_FILES,          ENOENT    },  /* 18 */
        {  ERROR_LOCK_VIOLATION,         EACCES    },  /* 33 */
        {  ERROR_BAD_NETPATH,            ENOENT    },  /* 53 */
        {  ERROR_NETWORK_ACCESS_DENIED,  EACCES    },  /* 65 */
        {  ERROR_BAD_NET_NAME,           ENOENT    },  /* 67 */
        {  ERROR_FILE_EXISTS,            EEXIST    },  /* 80 */
        {  ERROR_CANNOT_MAKE,            EACCES    },  /* 82 */
        {  ERROR_FAIL_I24,               EACCES    },  /* 83 */
        {  ERROR_INVALID_PARAMETER,      EINVAL    },  /* 87 */
        {  ERROR_NO_PROC_SLOTS,          EAGAIN    },  /* 89 */
        {  ERROR_DRIVE_LOCKED,           EACCES    },  /* 108 */
        {  ERROR_BROKEN_PIPE,            EPIPE     },  /* 109 */
        {  ERROR_DISK_FULL,              ENOSPC    },  /* 112 */
        {  ERROR_INVALID_TARGET_HANDLE,  EBADF     },  /* 114 */
        {  ERROR_INVALID_HANDLE,         EINVAL    },  /* 124 */
        {  ERROR_WAIT_NO_CHILDREN,       ECHILD    },  /* 128 */
        {  ERROR_CHILD_NOT_COMPLETE,     ECHILD    },  /* 129 */
        {  ERROR_DIRECT_ACCESS_HANDLE,   EBADF     },  /* 130 */
        {  ERROR_NEGATIVE_SEEK,          EINVAL    },  /* 131 */
        {  ERROR_SEEK_ON_DEVICE,         EACCES    },  /* 132 */
        {  ERROR_DIR_NOT_EMPTY,          ENOTEMPTY },  /* 145 */
        {  ERROR_NOT_LOCKED,             EACCES    },  /* 158 */
        {  ERROR_BAD_PATHNAME,           ENOENT    },  /* 161 */
        {  ERROR_MAX_THRDS_REACHED,      EAGAIN    },  /* 164 */
        {  ERROR_LOCK_FAILED,            EACCES    },  /* 167 */
        {  ERROR_ALREADY_EXISTS,         EEXIST    },  /* 183 */
        {  ERROR_FILENAME_EXCED_RANGE,   ENOENT    },  /* 206 */
        {  ERROR_NESTING_NOT_ALLOWED,    EAGAIN    },  /* 215 */
        {  ERROR_NOT_ENOUGH_QUOTA,       ENOMEM    }    /* 1816 */
};

CFileAccess::CFileAccess()
{
	m_file = NULL;
}

CFileAccess::~CFileAccess()
{
	close();
}

bool CFileAccess::open(const char *filename, const char *mode)
{
	m_file = _wfopen(Win32Wide(filename),Win32Wide(mode));
	if(!m_file)
		return false;
	return true;
}

bool CFileAccess::open(FILE *file)
{
	if(m_file)
		return false;
	m_file = file;
	return true;
}

bool CFileAccess::isopen()
{
	if(!m_file)
		return false;
	return true;
}

bool CFileAccess::close()
{
	if(m_file)
		fclose(m_file);
	m_file = NULL;
	return true;
}

bool CFileAccess::getline(cvs::string& line)
{
	int c;

	if(!m_file)
		return false;

	line.reserve(256);
	line="";

	while((c=fgetc(m_file))!=EOF)
	{
		if(c=='\n')
			break;
		if(c=='\r')
			continue;
		line.append(1,c);
	}
	if(c==EOF && line.empty())
		return false;
	return true;
}

bool CFileAccess::getline(char *line, size_t length)
{
	int c;
	size_t len = length;

	if(!m_file)
		return false;

	while(len && (c=fgetc(m_file))!=EOF)
	{
		if(c=='\n')
			break;
		*(line++)=(char)c;
		--len;
	}
	if(c==EOF && len==length)
		return false;
	return true;
}

bool CFileAccess::putline(const char *line)
{
	if(!m_file)
		return false;

	if(fwrite(line,1,strlen(line),m_file)<strlen(line))
		return false;
	if(fwrite("\n",1,1,m_file)<1)
		return false;
	return true;
}

size_t CFileAccess::read(void *buf, size_t length)
{
	if(!m_file)
		return 0;

	return fread(buf,1,length,m_file);
}

size_t CFileAccess::write(const void *buf, size_t length)
{
	if(!m_file)
		return 0;

	return fwrite(buf,1,length,m_file);
}

loff_t CFileAccess::length()
{
	if(!m_file)
		return 0;

	fpos_t pos,len;

	if(fgetpos(m_file,&pos)<0)
		return 0;
	fseek(m_file,0,SEEK_END);
	if(fgetpos(m_file,&len)<0)
		return 0;
	if(fsetpos(m_file,&pos)<0)
		return 0;
	return (loff_t)len;
}

void CFileAccess::_dosmaperr(DWORD dwErr)
{
	int n;
	for(n=0; n<sizeof(errors)/sizeof(errors[0]); n++)
	{
		if(errors[n].err==dwErr)
		{
			errno=errors[n].dos;
			return;
		}
	}
	errno=EFAULT;
}

/*
 * Make a path to the argument directory, printing a message if something
 * goes wrong.
 */
void CFileAccess::_tmake_directories(const char *name, const TCHAR *fn)
{
	DWORD fa;
	TCHAR *dir;
	TCHAR *cp;

	fa = GetFileAttributes(fn);
	if(fa!=0xFFFFFFFF)
	{
		if(!(fa&FILE_ATTRIBUTE_DIRECTORY))
			CServerIo::error (0, 0, "%s already exists but is not a directory", name);
		else
		{
			return;
		}
	}
	if (!CreateDirectory(fn,NULL))
	{
		DWORD dwErr = GetLastError();
		if(dwErr!=ERROR_PATH_NOT_FOUND)
		{
			_dosmaperr(dwErr);
			CServerIo::error (0, errno, "cannot make directory %s", name);
			return;
		}
	}
	else
		return;
	dir = _tcsdup(fn);
	for(cp=dir+_tcslen(dir);cp>dir && !ISDIRSEP(*cp); --cp)
		;
	if(cp==dir)
	{
		free(dir);
		return;
	}
    *cp = '\0';
    _tmake_directories (name,dir);
    *cp++ = '/';
    if (*cp == '\0')
	{
		free(dir);
		return;
	}
	if (!CreateDirectory(dir,NULL))
	{
		_dosmaperr(GetLastError());
		CServerIo::error (0, errno, "cannot make directory %s", name);
		free(dir);
		return;
	}
	free(dir);
}

void CFileAccess::make_directories (const char *name)
{
	Win32Wide fn = name;
	_tmake_directories(name,fn);
}

bool CFileAccess::copyfile(const char *ExistingFileName, const char *NewFileName, bool FailIfExists)
{
	BOOL copyresult;
	copyresult=CopyFileW(Win32Wide(ExistingFileName), Win32Wide(NewFileName), FailIfExists);
	return (copyresult==0)?false:true;
}

loff_t CFileAccess::pos()
{
	if(!m_file)
		return 0;

	fpos_t o;
	if(fgetpos(m_file,&o)<0)
		return 0;
	return (loff_t)o;
}

bool CFileAccess::eof()
{
	if(!m_file)
		return false;

	return feof(m_file)?true:false;
}

bool CFileAccess::seek(loff_t pos, SeekEnum whence)
{
	if(!m_file)
		return false;

	fpos_t p;

	switch(whence)
	{
	case seekBegin:
		p=(fpos_t)pos;
		break;
	case seekCurrent:
		if(fgetpos(m_file,&p)<0)
			return false;
		p+=pos;
		break;
	case seekEnd:
		if(pos>0x7FFFFFFF)
		{
			if(fseek(m_file,0x7FFFFFFF,whence)<0)
				return false;
			pos-=0x7FFFFFFFF;
			while(pos>0x7FFFFFFF && !fseek(m_file,-0x7FFFFFFF,SEEK_CUR))
				pos-=0x7FFFFFFFF;
			if(pos>0x7FFFFFFF)
				return false;
			pos=-pos;
		}
		if(fseek(m_file,(long)pos,SEEK_CUR)<0)
			return false;
		return true;
	default:
		return false;
	}
	if(fsetpos(m_file,&p)<0)
		return false;
	return true;
}

cvs::wstring CFileAccess::wtempdir()
{
	cvs::wstring dir;
	DWORD fa;

	dir.resize(MAX_PATH);
	if(!GetEnvironmentVariableW(_T("TEMP"),(TCHAR*)dir.data(),(DWORD)dir.size()) &&
	     !GetEnvironmentVariableW(_T("TMP"),(TCHAR*)dir.data(),(DWORD)dir.size()))
	{
		// No TEMP or TMP, use default <windir>\TEMP
		GetWindowsDirectoryW((wchar_t*)dir.data(),(UINT)dir.size());
		wcscat((wchar_t*)dir.data(),L"\\TEMP");
	}
	dir.resize(wcslen((wchar_t*)dir.data()));
	if((fa=GetFileAttributesW(dir.c_str()))==0xFFFFFFFF || !(fa&FILE_ATTRIBUTE_DIRECTORY))
	{
		// Last resort, can't find a valid temp.... use C:\...
		dir=L"C:\\";
	}
	return dir;
}

cvs::string CFileAccess::tempdir()
{
	return (const char *)Win32Narrow(wtempdir().c_str());
}

cvs::string CFileAccess::tempfilename(const char *prefix)
{
	cvs::wstring tempfile;
	tempfile.resize(MAX_PATH);
	GetTempFileNameW(wtempdir().c_str(),Win32Wide(prefix),0,(wchar_t*)tempfile.data());
	tempfile.resize(wcslen((wchar_t*)tempfile.data()));
	DeleteFileW(tempfile.c_str());
	return (const char *)Win32Narrow(tempfile.c_str());
}

bool CFileAccess::_remove(cvs::wstring& path)
{
	WIN32_FIND_DATAW wfd;
	HANDLE hFind;

	hFind = FindFirstFileW((path+L"/*.*").c_str(),&wfd);
	if(hFind==INVALID_HANDLE_VALUE)
		return FALSE;
	do
	{
		if(wfd.cFileName[0]!='.' || (wfd.cFileName[1]!='.' && wfd.cFileName[1]!='\0'))
		{
			size_t l = path.length();
			path+=L"/";
			path+=wfd.cFileName;
			if(wfd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
			{
				if(!_remove(path))
				{
					FindClose(hFind);
					return false;
				}
			}
			else
			{
				if(wfd.dwFileAttributes&FILE_ATTRIBUTE_READONLY)
					SetFileAttributesW(path.c_str(),wfd.dwFileAttributes&~FILE_ATTRIBUTE_READONLY);
				if(!DeleteFile(path.c_str()))
				{
					FindClose(hFind);
					return false;
				}
			}
			path.resize(l);
		}
	} while(FindNextFile(hFind,&wfd));
	FindClose(hFind);
	if(!RemoveDirectoryW(path.c_str()))
		return false;
	return true;
}

bool CFileAccess::remove(const char *file, bool recursive /* = false */)
{
	Win32Wide wfile(file);
	DWORD att = GetFileAttributesW(wfile);
	if(att==0xFFFFFFFF)
		return false;
	SetFileAttributesW(wfile,att&~FILE_ATTRIBUTE_READONLY);
	if(att&FILE_ATTRIBUTE_DIRECTORY)
	{
		if(!recursive)
		{
			if(!RemoveDirectoryW(wfile))
				return false;
		}
		else
		{
			cvs::wstring wpath = wfile;
			if(!_remove(wpath))
				return false;
		}
	}
	else
	{
		if(!DeleteFileW(wfile))
			return false;
	}
	return true;
}

bool CFileAccess::rename(const char *from, const char *to)
{
	Win32Wide wfrom(from);
	Win32Wide wto(to);
	DWORD att = GetFileAttributesW(wfrom);
	if(att==0xFFFFFFFF)
		return false;
	SetFileAttributesW(wfrom,att&~FILE_ATTRIBUTE_READONLY);
	att = GetFileAttributesW(wto);
	if(att!=0xFFFFFFFF)
		SetFileAttributesW(wto,att&~FILE_ATTRIBUTE_READONLY);
	if(!MoveFileExW(wfrom,wto,MOVEFILE_COPY_ALLOWED|MOVEFILE_REPLACE_EXISTING))
		return false;
	return true;
}

void CFileAccess::Win32SetUtf8Mode(bool bUtf8Mode)
{
	m_bUtf8Mode = bUtf8Mode;
}

CFileAccess::Win32Wide::Win32Wide(const char *fn)
{
	if(!fn)
	{
		pbuf=NULL;
		return;
	}
	size_t l = strlen(fn)+1;
	if(l>(sizeof(buf)/sizeof(wchar_t)))
		pbuf=new wchar_t[l];
	else
		pbuf=buf;
	MultiByteToWideChar(m_bUtf8Mode?CP_UTF8:CP_ACP,0,fn,(int)l,pbuf,(int)l*sizeof(wchar_t));
}

CFileAccess::Win32Wide::~Win32Wide()
{
	if(pbuf && pbuf!=buf)
		delete[] pbuf;
}

CFileAccess::Win32Narrow::Win32Narrow(const wchar_t *fn)
{
	if(!fn)
	{
		pbuf=NULL;
		return;
	}
	size_t l = wcslen(fn)+1;
	if((l*3)>sizeof(buf))
		pbuf=new char[l*3];
	else
		pbuf=buf;
	WideCharToMultiByte(m_bUtf8Mode?CP_UTF8:CP_ACP,0,fn,(int)l,pbuf,(int)(l*3),NULL,NULL);
}

CFileAccess::Win32Narrow::~Win32Narrow()
{
	if(pbuf && pbuf!=buf)
		delete[] pbuf;
}

bool CFileAccess::exists(const char *file)
{
	if(GetFileAttributesW(Win32Wide(file))==0xFFFFFFFF)
		return false;
	return true;
}

CFileAccess::TypeEnum CFileAccess::type(const char *file)
{
	DWORD dw = GetFileAttributesW(Win32Wide(file));
	if(dw==0xFFFFFFFF)
		return typeNone;
	if(dw&FILE_ATTRIBUTE_DIRECTORY)
		return typeDirectory;
	return typeFile;
}

bool CFileAccess::absolute(const char *file)
{
	return file[0]=='\\' || file[0]=='/' || file[1]==':';
}

int CFileAccess::uplevel(const char *file)
{
	int level = 0;
	for(const char *p=file; *p;)
	{
		size_t l=strcspn(p,"\\/");
		if(l==1 && *p=='.')
			level++; // Compensate for ./
		else if(l==2 && *p=='.' && *(p+1)=='.')
			level+=2; // Compensate for ../
		p+=l;
		if(*p) p++;
		level--;
	}
	return level;
}

cvs::string CFileAccess::mimetype(const char *filename)
{
	HKEY hk;

	const char *p = strrchr(filename,'.');
	if(!p)
		return "";

	if(RegOpenKeyEx(HKEY_CLASSES_ROOT,cvs::wide(p),0,KEY_QUERY_VALUE,&hk))
		return "";

	TCHAR str[256];
	DWORD len = sizeof(str)-sizeof(TCHAR);
	if(RegQueryValueEx(hk,L"Content Type",NULL,NULL,(LPBYTE)str,&len))
	{
		RegCloseKey(hk);
		return "";
	}
	str[len]='\0';
	return (const char *)cvs::narrow(str);
}
