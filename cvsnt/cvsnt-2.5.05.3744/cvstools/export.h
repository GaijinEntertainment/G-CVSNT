/*
	CVSNT Helper application API
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd

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
#ifndef CVSTOOLS__EXPORT__H
#define CVSTOOLS__EXPORT__H

#ifdef _WIN32
 #ifndef CVSTOOLS_EXPORT
  #define CVSTOOLS_EXPORT __declspec(dllimport)
 #endif
#elif defined(_CVSTOOLS) && defined(HAVE_GCC_VISIBILITY)
 #define CVSTOOLS_EXPORT __attribute__ ((visibility("default")))
#else
 #define CVSTOOLS_EXPORT
#endif

#endif
