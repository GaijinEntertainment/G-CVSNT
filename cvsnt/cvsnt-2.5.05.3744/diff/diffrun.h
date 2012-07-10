/* Interface header file for GNU DIFF library.
   Copyright (C) 1998 Free Software Foundation, Inc.

This file is part of GNU DIFF.

GNU DIFF is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU DIFF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef DIFFRUN_H
#define DIFFRUN_H

/* This header file defines the interfaces used by the diff library.
   It should be included by programs which use the diff library.  */

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The diff_callbacks structure is used to handle callbacks from the
   diff library.  All output goes through these callbacks.  When a
   pointer to this structure is passed in, it may be NULL.  Also, any
   of the individual callbacks may be NULL.  This means that the
   default action should be taken.  */

struct diff_callbacks
{
  /* Write output.  This function just writes a string of a given
     length to the output file.  The default is to fwrite to OUTFILE.
     If this callback is defined, flush_output must also be defined.
     If the length is zero, output zero bytes.  */
  void (*write_output)(char const *, size_t);
  /* Flush output.  The default is to fflush OUTFILE.  If this
     callback is defined, write_output must also be defined.  */
  void (*flush_output)();
  /* Write a '\0'-terminated string to stdout.
     This is called for version and help messages.  */
  void (*write_stdout)(char const *);
  /* Print an error message.  The first argument is a printf format,
     and the next two are parameters.  The default is to print a
     message on stderr.  */
  void (*error)(char const *, char const *, char const *);
  void (*cvs_trace)(int level, const char *fmt,...);

  FILE *(*fopen)(const char *fn, const char *mode);
  int (*open)(const char *fn, int mode, ...);
  int (*stat)(const char *fn, struct stat *st);
};

/* Run a diff.  */

int diff_run (int argc, char *argv[], const char *out, const struct diff_callbacks *callbacks_arg);

/* Run a diff3.  */

int diff3_run (int argc, char **argv, const char *out, const struct diff_callbacks *callbacks_arg);

#ifdef __cplusplus
}
#endif

#endif /* DIFFRUN_H */
