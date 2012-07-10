/* global.h --- Global internal include file for libntlm.
 * Copyright (C) 2004, 2005  Frediano Ziglio
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 */

/*
 * Extracted from various source (mainly libmcrypt)
 */

#ifndef NTLM_GLOBAL_H_
#define NTLM_GLOBAL_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <ctype.h>

#ifdef HAVE_STRING_H
# include <string.h>
#endif

#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_SYS_ENDIAN_H
# include <sys/endian.h>
#endif

#ifdef HAVE_MACHINE_ENDIAN_H
# include <machine/endian.h>
#endif

#ifdef HAVE_ENDIAN_H
# include <endian.h>
#endif

#ifdef HAVE_BYTESWAP_H
# include <byteswap.h>
#endif

#define rotl32(x,n)   (((x) << ((word32)(n))) | ((x) >> (32 - (word32)(n))))
#define rotr32(x,n)   (((x) >> ((word32)(n))) | ((x) << (32 - (word32)(n))))
#define rotl16(x,n)   (((x) << ((word16)(n))) | ((x) >> (16 - (word16)(n))))
#define rotr16(x,n)   (((x) >> ((word16)(n))) | ((x) << (16 - (word16)(n))))

/* Use hardware rotations.. when available */
#ifdef swap32
# define byteswap32(x) swap32(x)
#else
# ifdef swap_32
#  define byteswap32(x) swap_32(x)
# else
#  ifdef bswap_32
#   define byteswap32(x) bswap_32(x)
#  else
#   define byteswap32(x)	((rotl32(x, 8) & 0x00ff00ff) | (rotr32(x, 8) & 0xff00ff00))
#  endif
# endif
#endif

#ifdef swap16
# define byteswap16(x) swap16(x)
#else
# ifdef swap_16
#  define byteswap16(x) swap_16(x)
# else
#  ifdef bswap_16
#   define byteswap16(x) bswap_16(x)
#  else
#   define byteswap16(x)	((rotl16(x, 8) & 0x00ff) | (rotr16(x, 8) & 0xff00))
#  endif
# endif
#endif

#ifndef NTLM_STATIC
# define NTLM_STATIC
#endif

#include "ntlm.h"
typedef uint32 word32;

#endif /* NTLM_GLOBAL_H_ */
