/* backupfile.h -- declarations for making Emacs style backup file names
   Copyright (C) 1990-1992, 1997-1999 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.
   If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef BACKUPFILE_H_
# define BACKUPFILE_H_

extern char const *simple_backup_suffix;

char *find_backup_file_name(char const *);
void addext(char *, char const *, int);

#endif /* ! BACKUPFILE_H_ */
