/* reading patches */

/* $Id: pch.h,v 1.1 2012/03/04 01:06:58 aliot Exp $ */

LINENUM pch_end PARAMS ((void));
LINENUM pch_first PARAMS ((void));
LINENUM pch_hunk_beg PARAMS ((void));
LINENUM pch_newfirst PARAMS ((void));
LINENUM pch_prefix_context PARAMS ((void));
LINENUM pch_ptrn_lines PARAMS ((void));
LINENUM pch_repl_lines PARAMS ((void));
LINENUM pch_suffix_context PARAMS ((void));
bool pch_swap PARAMS ((void));
bool pch_write_line PARAMS ((LINENUM, FILE *));
bool there_is_another_patch(ThreadSafeProgress*);
char *pfetch PARAMS ((LINENUM));
char pch_char PARAMS ((LINENUM));
int another_hunk(ThreadSafeProgress*, enum diff, int);
int pch_says_nonexistent PARAMS ((int));
size_t pch_line_len PARAMS ((LINENUM));
time_t pch_timestamp PARAMS ((int));
void do_ed_script(ThreadSafeProgress*, FILE *);
void open_patch_file(ThreadSafeProgress*, char const *);
void re_patch PARAMS ((void));
void set_hunkmax PARAMS ((void));
