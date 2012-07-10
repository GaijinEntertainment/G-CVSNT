/* Declarations for update.c.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

int do_update (int argc, char *argv[], const char *xoptions, const char *xtag,
	       const char *xdate, int xforce, int local, int xbuild,
	       int xaflag, int xprune, int xpipeout, int which,
	       const char *xjoin_rev1, const char *xjoin_rev2, const char *preload_update_dir,
		   const char *preload_repository);
int joining ();
extern int isemptydir (const char *dir, int might_not_exist);
