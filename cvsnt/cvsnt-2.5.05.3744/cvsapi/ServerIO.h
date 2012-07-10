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
#ifndef SERVERIO__H
#define SERVERIO__H

class CServerIo
{
public:
	static CVSAPI_EXPORT int output(const char *fmt, ...);
	static CVSAPI_EXPORT int output(size_t len, const char *str);
	static CVSAPI_EXPORT int warning(const char *fmt, ...);
	static CVSAPI_EXPORT int warning(size_t len, const char *str);
	static CVSAPI_EXPORT int error(const char *fmt, ...);
	static CVSAPI_EXPORT int error(size_t len, const char *str);
	static CVSAPI_EXPORT int trace(int level, const char *fmt, ...);

	static CVSAPI_EXPORT int init(int (*pOutput)(const char *str, size_t len),int (*pWarning)(const char *str, size_t len),int (*pError)(const char *str, size_t len),int (*pTrace)(int level, const char *msg));
	static CVSAPI_EXPORT void loglevel(int level);

	enum logType
	{
		logNotice,
		logError,
		logAuth
	};
	static CVSAPI_EXPORT void log(logType type, const char *fmt, ...);

private:
	static int (*m_pOutput)(const char *str, size_t len);
	static int (*m_pWarning)(const char *str, size_t len);
	static int (*m_pError)(const char *str, size_t len);
	static int (*m_pTrace)(int level, const char *msg);
	static int m_loglevel;

	static int default_output(const char *str, size_t len);
	static int default_trace(int level, const char *str);

};

#endif
