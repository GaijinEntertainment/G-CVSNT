/*
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 1, or (at your option)
** any later version.

** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.

** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
 * Author : Jens Miltner <jum@mac.com> -- October 2004
 */

#include "config.h"
#include "system.h"
#include <Carbon/Carbon.h>
#include "mac_hfs_stuff.h"
#include "cvs_hqx.h"


void mac_decode_file (const char *infile, char *outfile, int binary)
{
	mac_convert_file(infile, 0, outfile, binary);
}

void mac_encode_file (const char *infile, char *outfile, int binary)
{
	mac_convert_file(infile, 1, outfile, binary);
}

