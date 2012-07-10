/*
  Win32 inttypes.h
 
  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 the GNU Lesser General Public License as
  published by the Free Software Foundation.
 
  It is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public
  License along with nss-mdns; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
*/

#ifndef INTTYPES__H
#define INTTYPES__H

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef unsigned char u_int8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef unsigned short u_int16_t;
typedef signed long int32_t; // Note that this is the same for Win64..!
typedef unsigned long uint32_t;
typedef unsigned long u_int32_t;
typedef signed __int64 int64_t;
typedef unsigned __int64 uint64_t;
typedef unsigned __int64 u_int64_t;

#ifdef _WIN64
typedef signed __int64 ssize_t;
#else
typedef __w64 signed long ssize_t;
#endif

#endif