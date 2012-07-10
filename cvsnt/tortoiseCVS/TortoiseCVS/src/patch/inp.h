/* inputting files to be patched */

/* $Id: inp.h,v 1.1 2012/03/04 01:06:58 aliot Exp $ */

XTERN LINENUM input_lines;    /* how long is input file in lines */

char const *ifetch PARAMS ((LINENUM, int, size_t *));
void get_input_file(ThreadSafeProgress*, char const *, char const *);
void re_input PARAMS ((void));
void scan_input(ThreadSafeProgress*, char *);
