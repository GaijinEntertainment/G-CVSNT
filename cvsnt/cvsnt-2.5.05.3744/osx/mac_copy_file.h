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
 * Author : Jens Miltner <jum@mac.com> --- January 2004
 */
#ifndef __mac_copy_file_H__
#define __mac_copy_file_H__

#ifdef __cplusplus
extern "C" {
#endif

int mac_copy_file(const char* from, const char* to, int force_overwrite, int must_exist);

#ifdef __cplusplus
}
#endif

#endif // __mac_copy_file_H__
