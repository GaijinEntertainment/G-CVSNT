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
#include <config.h>
#include "lib/api_system.h"
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>

#include "ServerIO.h"

#ifdef _WIN32
#include "../pcre/pcreposix.h"
#else
#include <pcreposix.h>
#endif

#include "cvs_string.h"

// Win32 doesn't support template instantiation, however some
// platforms prefer it.
// OSX barfs if we try this also.
// Also HPUX aCC
#if !defined (_WIN32) && !defined(__APPLE__) && !defined(__hpux) && !defined(__digital)
template class std::char_traits<char>;
template class std::char_traits<wchar_t>;
template class std::basic_string<char>;
template class std::basic_string<wchar_t>;
#endif

// Cache for cache_static_string
std::queue<cvs::string> cvs::cache_static_string::global_string_cache;

// Note that if we're using PCRE the string match is always extended...
// for this reason avoid setting extended=false as it'll behave differently on
// different platforms.
bool cvs::wildcard_filename::matches_regexp(const char *regexp, bool extended /*= true*/)
{ 
	regex_t r; 
	if(regcomp(&r,regexp,(FsCaseSensitive?0:REG_ICASE)|REG_NOSUB|(extended?REG_EXTENDED:0)))
		return false;
	int res=regexec(&r,c_str(),0,NULL,0);
	regfree(&r);
	return res?false:true;
}

// Based on BSD vsprintf
// Assert if a null string reference is passed.  Causes
// failure on 'safe' platforms so that unsafe ones eg. solaris
// don't have strange inexplicable crashes.
bool cvs::str_prescan(const char *fmt, va_list va)
{
  char *s;
  int qualifier;
  const char *ofmt = fmt;
  int argno = 0;

  for (; *fmt ; fmt++)
  {
    if (*fmt != '%')
      continue;
                  
    // Process flags
repeat:
    fmt++; // This also skips first '%'
    switch (*fmt)
    {
      case '-':
      case '+':
      case ' ':
      case '#': 
      case '0': 
		  goto repeat;
    }
          
    // Get field width
    if (isdigit((unsigned char)*fmt))
	{
		while(isdigit((unsigned char)*fmt))
			fmt++;
	}
    else if (*fmt == '*')
    {
      fmt++;
      va_arg(va, int);
	  argno++;
    }

    // Get the precision
    if (*fmt == '.')
    {
      ++fmt;    
      if (isdigit((unsigned char)*fmt))
	  {
		while(isdigit((unsigned char)*fmt))
			fmt++;
	  }
      else if (*fmt == '*')
      {
        ++fmt;
        va_arg(va, int);
		argno++;
      }
    }

    // Get the conversion qualifier
	qualifier = -1;
	if(!strncmp(fmt,"I64",3))
	{
		fmt+=3;
		qualifier = 'L';
	}
    else if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L')
	{
	    qualifier = (*fmt++);

		if(*fmt=='l' && qualifier=='l')
		{
			fmt++; // %ll
			qualifier = 'L';
		}
	}

    switch (*fmt)
    {
      case 'c':
        va_arg(va, int);
		argno++;
        continue;

      case 's':
	  case 'S':
        s = va_arg(va, char *);
		argno++;
		if(!s)
		{
			CServerIo::error("Format = %s\n",ofmt);
			CServerIo::error("Argument %d is null\n",argno);
			assert(s);
		}
        continue;

      case 'p':
        va_arg(va, void *);
		argno++;
        continue;

      case 'n':
        va_arg(va, int *);
		argno++;
        continue;

      case 'A':
      case 'a':
          va_arg(va, unsigned char *);
		  argno++;
        continue;

      case 'o':
      case 'X':
      case 'x':
      case 'd':
      case 'i':
      case 'u':
        break;

      case 'E':
      case 'G':
      case 'e':
      case 'f':
      case 'g':
        va_arg(va, double);
		argno++;
        continue;

      default:
        if (!*fmt)
          --fmt;
        continue;
    }

	if (qualifier == 'L')
	{
#ifdef _WIN32
		va_arg(va, __int64);
#else
		va_arg(va, long long);
#endif
		argno++;
	}
    else if (qualifier == 'l')
	{
      va_arg(va, long);
	  argno++;
	}
    else if (qualifier == 'h')
	{
		va_arg(va, int);
		argno++;
	}
    else 
	{
      va_arg(va, int);
	  argno++;
	}
  }
  return true;
}
