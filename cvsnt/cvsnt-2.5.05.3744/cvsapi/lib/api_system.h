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
/* _EXPORT */

#ifndef API_SYSTEM__H
#define API_SYSTEM__H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
/* Win32 is both username and filename case insensitive, with multiple path separators */
#define IsSeparator(c) ((c)=='\\' || (c)=='/')
#define FsCaseSensitive __case_sensitive()
#define CVS_CASEFOLD (FsCaseSensitive?0:FNM_CASEFOLD)
#define CompareFileChar(c,d) __cfc(c,d,FsCaseSensitive)
#define CompareUserChar(c,d) __cfc(c,d,0)
#define fncmp __fncmp
#define fnncmp __fnncmp
#define usercmp stricmp
#define userncmp strnicmp

/* Make Win32 use 64bit times, and fix a VC compatibility bug */
#include <time.h>
#define time_t __time64_t
#define gmtime _gmtime64
#define localtime _localtime64
#define mktime _mktime64
#define time _time64
#define ctime wnt_ctime
#define asctime wnt_asctime
#define TIME_T_SPRINTF "I64"

#define vsnwprintf _vsnwprintf
#define snprintf _snprintf
#define snwprintf _snwprintf
#define strcasecmp stricmp
#define strncasecmp strnicmp

#if !defined(CVSAPI_EXPORT) && !defined(CVSAPI_STATIC)
	#define CVSAPI_EXPORT __declspec(dllimport)
#elif !defined(CVSAPI_EXPORT)
	#define CVSAPI_EXPORT
#endif

#ifdef __cplusplus
#define CVSNT_EXPORT extern "C" __declspec(dllexport)
#else
#define CVSNT_EXPORT __declspec(dllexport)
#endif

#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif

#define CRLF_DEFAULT ltCrLf

#define SHARED_LIBRARY_EXTENSION ".dll"

CVSAPI_EXPORT int __case_sensitive();
CVSAPI_EXPORT void __set_case_sensitive(int cs);

CVSAPI_EXPORT char *wnt_asctime(const struct tm *tm);
CVSAPI_EXPORT char *wnt_ctime(const time_t *t);

#endif

#ifdef __APPLE__
/* Mac OSX can have multiple concepts of case insensitivity, but we just
   define as insensitive here (HFS default).  It would be relatively
   trivial to replace this with a routine to detect it dynamically */
#define FsCaseSensitive 0
#define CVS_CASEFOLD FNM_CASEFOLD
#define CompareFileChar(c,d) (((c)==(d))?0:tolower(c)-tolower(d))
#define fncmp strcasecmp
#define fnncmp strncasecmp

/* OSX has a utf8 filesystem so we need to use that always */
#define UTF8_CLIENT 1

/* We use the apple responder by default */
#define MDNS_DEFAULT "apple"

#endif /* __APPLE__ */

#ifdef __OS400__
/* AS400 is case insensitive.. sort of... */
#define FsCaseSensitive 0
#define CVS_CASEFOLD FNM_CASEFOLD
#define CompareFileChar(c,d) (((c)==(d))?0:tolower(c)-tolower(d))
#define fncmp strcasecmp
#define fnncmp strncasecmp
#endif

#ifndef IsSeparator
#define IsSeparator(c) ((c)=='/')
#endif /* __OS400__ */

#ifndef FsCaseSensitive
#define FsCaseSensitive 1
#endif

#ifndef CVS_CASEFOLD
#define CVS_CASEFOLD 0
#endif

#ifndef CompareFileChar
#define CompareFileChar(c,d) ((c)-(d))
#endif

#ifndef CompareUserChar
#define CompareUserChar(c,d) ((c)-(d))
#endif

#ifndef fncmp
#define fncmp strcmp
#endif

#ifndef fnncmp
#define fnncmp strncmp
#endif

#ifndef usercmp
#define usercmp strcmp
#endif

#ifndef userncmp
#define userncmp strncmp
#endif

#if !defined(CVSAPI_EXPORT) && defined(_CVSAPI) && defined(HAVE_GCC_VISIBILITY)
#define CVSAPI_EXPORT __attribute__ ((visibility("default")))
#endif

#ifndef CVSAPI_EXPORT
#define CVSAPI_EXPORT
#endif

/* Does platform use CRLF by default.  To be replaced by a more generic mechanism once the
   core is rewritten */
#ifndef CRLF_DEFAULT
#define CRLF_DEFAULT ltLf
#endif

#ifndef TIME_T_SPRINTF
#define TIME_T_SPRINTF "l"
#endif

/* Some platforms (HPUX!) use a non-standard name for the character set */
#ifndef UTF8_CHARSET
#define UTF8_CHARSET "UTF-8"
#endif

#ifndef UTF8_CLIENT
#define UTF8_CLIENT 0
#endif

#ifndef MDNS_DEFAULT
//#ifdef HAVE_AvAHI
//#define MDNS_DEFAULT "avahi"
#if defined HAVE_DNS_SD
#define MDNS_DEFAULT "apple"
#else
#define MDNS_DEFAULT "mini"
#endif
#endif

#ifndef TIME_T_SPRINTF
#define TIME_T_SPRINTF "l"
#endif

#ifndef CVSNT_EXPORT
#define CVSNT_EXPORT
#endif

/* Default is to search for libtool (.la) extenstions - this should work on all platforms except win32 */
#ifndef SHARED_LIBRARY_EXTENSION
#define SHARED_LIBRARY_EXTENSION ".la"
#endif

#include <stdarg.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

/* Define va_copy if it isn't defined already */
#ifndef va_copy
# ifdef __va_copy
#  define va_copy(_d,s) ((void)__va_copy((_d),(_s)))
# else
#  ifdef HAVE_VA_LIST_AS_ARRAY
#   define va_copy(_d,_s) ((void)(*(_d) = *(_s)))
#  else
#   define va_copy(_d,_s) ((void)((_d) = (_s)))
#  endif
# endif
#endif

#include "fncmp.h"

#ifdef __cplusplus
}
#endif

#endif
