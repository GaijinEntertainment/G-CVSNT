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
#include "../../lib/system.h"
#include "../lib/api_system.h"

#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

#include "cvs_string.h"
#include "FileAccess.h"
#include "ServerIO.h"

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
	m_file = fopen(filename,mode);
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

bool CFileAccess::close()
{
	if(m_file)
		fclose(m_file);
	m_file = NULL;
	return true;
}

bool CFileAccess::isopen()
{
	if(m_file)
		return true;
	return false;
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
		line.append(1,(char)c);
	}
	if(c==EOF && line.empty())
		return false;
	return true;
}

bool CFileAccess::getline(char *line, size_t length)
{
	int c;
	int len = length;

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

/*
 * Make a path to the argument directory, printing a message if something
 * goes wrong.
 */
/*
 * Make a path to the argument directory, printing a message if something
 * goes wrong.
 */
void make_directories (const char *name)
{
    char *cp;
    char *dir;

    if (CVS_MKDIR (name, 0777) == 0 || errno == EEXIST)
		return;
    if (errno != ENOENT)
    {
		CServerIo::error (0, errno, "cannot make path to %s", name);
		return;
    }
    dir = strdup(name);
    for(cp=dir+strlen(dir);cp>dir && !ISDIRSEP(*cp); --cp)
	;
    if(cp==dir)
    {
	free(dir);
	return;
    }
    *cp = '\0';
    make_directories (dir);
    *cp++ = '/';
    if (*cp == '\0')
    {
	free(dir);
	return;
    }
    free(dir);
    CVS_MKDIR (name, 0777);
}

loff_t CFileAccess::length()
{
	if(!m_file)
		return 0;

	loff_t pos = ftell(m_file);
	fseek(m_file,0,SEEK_END);
	loff_t len = ftell(m_file);
	fseek(m_file,pos,SEEK_SET);
	return len;
}

loff_t CFileAccess::pos()
{
	if(!m_file)
		return 0;

#ifdef HAVE_FTELLO
	return ftello(m_file);
#elif defined(HAVE_FGETPOS)
	fpos_t pos;
	fgetpos(m_file,&pos);
	return (loff_t)pos;
#else
	return (loff_t)ftell(m_file);
#endif
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

	switch(whence)
	{
	case seekBegin:
		if(fseek(m_file,pos,0)<0)
			return false;
		return true;
	case seekCurrent:
		if(fseek(m_file,pos,0)<0)
			return false;
		return true;
	case seekEnd:
		if(fseek(m_file,pos,0)<0)
			return false;
		return true;
	default:
		return false;
	}
}

cvs::string CFileAccess::tempdir()
{
	return "/tmp";
}

cvs::string CFileAccess::tempfilename(const char *prefix)
{
	return tempnam(tempdir().c_str(),prefix);
}

bool CFileAccess::remove(const char *file, bool recursive /* = false */)
{
	struct stat sb;
	if(stat(file,&sb)<0)
		return true;
	if(S_ISDIR(sb.st_mode))
	{
		if(!recursive)
			return false;

		DIR *dp = opendir(file);
		struct dirent *dpp;
		if(!dp)
			return false;
		while((dpp=readdir(dp))!=NULL)
		{
			if(!strcmp(dpp->d_name,".") || !strcmp(dpp->d_name,".."))
				continue;
			
			chdir(file);
			if(!remove(dpp->d_name,recursive))
					return false;
			chdir("..");
		}
		closedir(dp);
		if(rmdir(file)<0)
			return false;
		return true;
	}
	else
	{
		if(::remove(file)<0)
			return false;
		return true;
	}
}

bool CFileAccess::absolute(const char *file)
{
        return file[0]=='/';
}

int CFileAccess::uplevel(const char *file)
{
	int level = 0;
	for(const char *p=file; *p;)
	{
		size_t l=strcspn(p,"/");
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

bool CFileAccess::exists(const char *file)
{
	struct stat st;
	if(stat(file,&st))
		return false;
	return true;
}

CFileAccess::TypeEnum CFileAccess::type(const char *file)
{
	struct stat st;
	if(stat(file,&st))
		return typeNone;
	if(S_ISLNK(st.st_mode))
		return typeSymlink;
	if(S_ISDIR(st.st_mode))
		return typeDirectory;
	if(S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))
		return typeDevice;
	if(S_ISREG(st.st_mode))
		return typeFile;
	return typeOther;
}

cvs::string CFileAccess::mimetype(const char *filename)
{
	return "";
	// TODO: Implement for unix.  Read from /etc/mime.types works on Linux..
	// don't know if there's even a standard for other platforms.
}
