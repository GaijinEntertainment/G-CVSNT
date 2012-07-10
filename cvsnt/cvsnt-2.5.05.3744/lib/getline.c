/* getline.c -- Replacement for GNU C library function getline

Copyright (C) 1993 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.  */

/* Written by Jan Brittenson, bson@gnu.ai.mit.edu.  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "getline.h"
#include "getdelim.h"

int getline(char **lineptr, size_t *n, FILE *stream)
{
  return getdelim(lineptr, n, '\n', stream);
}
