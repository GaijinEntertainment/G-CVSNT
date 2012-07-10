/*
  Win32 mdnsclient
 
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

#ifndef WIN32__H
#define WIN32__H

/* Some Win32 specific defines */
#define snprintf _snprintf
#define strcasecmp stricmp
#define strncasecmp strnicmp

/* Replacement functions */

int gettimeofday (struct timeval *tv, void* tz);

#endif
