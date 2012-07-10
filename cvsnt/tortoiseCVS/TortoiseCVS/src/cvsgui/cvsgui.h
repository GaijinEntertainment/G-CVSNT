/*
** The cvsgui protocol used by WinCvs
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License as published by the Free Software Foundation; either
** version 2.1 of the License, or (at your option) any later version.
** 
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Lesser General Public License for more details.
** 
** You should have received a copy of the GNU Lesser General Public
** License along with this library; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*!
	\file cvsgui.h
	\brief Glue code header for communicating with CVS over pipes

	Glue code signatures and defines to intercept some low level I/O calls and tunnel them inside the communication pipes

	\author Alexandre Parenteau <aubonbeurre@hotmail.com> --- November 1999
	\note To be used by CVS client
*/

#ifndef CVSGUI_H
#define CVSGUI_H

#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif

#ifdef WIN32
#	include <process.h> // exit
#endif

#ifdef __MACH__
#	include <CoreServices/CoreServices.h>
#endif /* __MACH__ */

#ifdef __cplusplus
extern "C" {
#endif

/// RCH - extra define for Solaris
#define getpassphrase cvsguiglue_getpass

#define getenv cvsguiglue_getenv
#define getpass cvsguiglue_getpass
#define main cvsguiglue_main
#define exit cvsguiglue_exit

extern char* cvsguiglue_getenv(const char* env);
extern char* cvsguiglue_getpass(const char* prompt);
extern int cvsguiglue_main(int argc, char* argv[]);
extern void cvsguiglue_exit(int code);
extern void cvsguiglue_flushconsole(int closeit);

#ifdef __cplusplus
}
#endif

#endif /* CVSGUI_H */
