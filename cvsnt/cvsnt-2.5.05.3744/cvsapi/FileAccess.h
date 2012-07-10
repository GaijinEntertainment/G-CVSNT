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
#ifndef FILEACCESS__H
#define FILEACCESS__H

#include <sys/types.h>


#ifdef _WIN32
#include <windows.h>
#ifndef loff_t
#define loff_t __int64
#endif
#endif

#include "cvs_string.h"

#ifdef _WIN32
#include <windows.h>
#define ISDIRSEP(c) (c=='/' || c=='\\')
#else
#ifndef ISDIRSEP
#define ISDIRSEP(c) ((c) == '/')
#endif
#endif

class CFileAccess
{
public:
	enum SeekEnum
	{
		seekBegin,
		seekCurrent,
		seekEnd
	};

	enum TypeEnum
	{
		typeNone,
		typeFile,
		typeDirectory,
		typeDevice,
		typeSymlink,
		typeOther
	};

	CVSAPI_EXPORT CFileAccess();
	CVSAPI_EXPORT virtual ~CFileAccess();

	CVSAPI_EXPORT bool open(const char *filename, const char *mode);
	CVSAPI_EXPORT bool open(FILE *file);
	CVSAPI_EXPORT bool close();
	CVSAPI_EXPORT bool isopen();

	CVSAPI_EXPORT bool getline(cvs::string& line);
	CVSAPI_EXPORT bool getline(char *line, size_t length);
	CVSAPI_EXPORT bool putline(const char *line);

	CVSAPI_EXPORT size_t read(void *buf, size_t length);
	CVSAPI_EXPORT size_t write(const void *buf, size_t length);

	CVSAPI_EXPORT loff_t length();
	static CVSAPI_EXPORT void make_directories (const char *name);
#ifdef _WIN32
	CVSAPI_EXPORT bool copyfile(const char *ExistingFileName, const char *NewFileName, bool FailIfExists);
#endif
	CVSAPI_EXPORT loff_t pos();
	CVSAPI_EXPORT bool eof();
	CVSAPI_EXPORT bool seek(loff_t pos, SeekEnum whence);

	static CVSAPI_EXPORT cvs::string tempdir();
	static CVSAPI_EXPORT cvs::string tempfilename(const char *prefix);
	static CVSAPI_EXPORT bool remove(const char *file, bool recursive = false);
	static CVSAPI_EXPORT bool rename(const char *from, const char *to);

	static CVSAPI_EXPORT bool exists(const char *file);
	static CVSAPI_EXPORT TypeEnum type(const char *file);
	static CVSAPI_EXPORT bool absolute(const char *file);
	static CVSAPI_EXPORT int uplevel(const char *file);

	static CVSAPI_EXPORT cvs::string mimetype(const char *filename);

#ifdef _WIN32
	static CVSAPI_EXPORT void Win32SetUtf8Mode(bool bUtf8Mode);
	struct CVSAPI_EXPORT Win32Wide
	{
		Win32Wide(const char *fn);
		~Win32Wide();
		operator const wchar_t *() { return pbuf; }

	private:
		wchar_t *pbuf, buf[32];
	};
	struct CVSAPI_EXPORT Win32Narrow
	{
		Win32Narrow(const wchar_t *fn);
		~Win32Narrow();
		operator const char *() { return pbuf; };

	private:
		char *pbuf, buf[32];
	};
#endif

protected:
	FILE *m_file;

#ifdef _WIN32
	static void _tmake_directories(const char *name, const TCHAR *fn);
	static void _dosmaperr(DWORD dwErr);
	static cvs::wstring wtempdir();
	static bool _remove(cvs::wstring& path);

	static bool m_bUtf8Mode;
#endif
};

#endif
