/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Author : Jonathan M. Gilligan <jonathan.gilligan@vanderbilt.edu> --- 23 June 2001
 */

/*
 * version_no.h -- Definitions for version numbering in cvsnt,
 *                 detailed documentation is below, see also, version_fu.h
 */

#ifndef VERSION_NO__H
#define VERSION_NO__H

// cvsnt version
#define CVSNT_PRODUCT_MAJOR 2
#define CVSNT_PRODUCT_MINOR 5
#define CVSNT_PRODUCT_PATCHLEVEL 05
#define CVSNT_PRODUCT_PATCHLEVELS 5
#include "build.h"
#define CVSNT_PRODUCT_NAME " (Gan + [Gaijin -kB/-kBz patch])"

// usually something like "RC 9"
//#define CVSNT_SPECIAL_BUILD "RC 2"

#ifdef RC_INVOKED
//#define CVSNT_SPECIAL_BUILD "Prerelease"
#else
//#define CVSNT_SPECIAL_BUILD "Prerelease "__DATE__
#endif

#endif
