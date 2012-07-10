/* utility functions for `patch' */

/* $Id: util.h,v 1.1 2012/03/04 01:06:58 aliot Exp $ */

/* An upper bound on the print length of a signed decimal line number.
   Add one for the sign.  */
#define LINENUM_LENGTH_BOUND (sizeof (LINENUM) * CHAR_BIT / 3 + 1)

class ThreadSafeProgress;

int ok_to_reverse(ThreadSafeProgress*, char const *, ...) __attribute__ ((format (printf, 2, 3)));
bool ask_yesno(bool yesdefault, char const *, ...) __attribute__ ((format (printf, 1, 2)));
void say(ThreadSafeProgress*, char const *, ...) __attribute__ ((format (printf, 2, 3)));

void fatal PARAMS ((char const *, ...))
   __attribute__ ((noreturn, format (printf, 1, 2)));
void pfatal PARAMS ((char const *, ...))
   __attribute__ ((noreturn, format (printf, 1, 2)));

char *fetchname(ThreadSafeProgress*, char *, int, time_t *);
char *savebuf PARAMS ((char const *, size_t));
char *savestr PARAMS ((char const *));
char const *version_controller PARAMS ((char const *, int, struct stat const *, char **, char **));
int version_get PARAMS ((ThreadSafeProgress* progress, char const *, char const *, int, int, char const *, struct stat *));
int create_file PARAMS ((char const *, int, mode_t));
int systemic(ThreadSafeProgress*, char const *);
char *format_linenum PARAMS ((char[LINENUM_LENGTH_BOUND + 1], LINENUM));
void Fseek PARAMS ((FILE *, file_offset, int));
void copy_file PARAMS ((char const *, char const *, int, mode_t));
void exit_with_signal PARAMS ((int)) __attribute__ ((noreturn));
void init_time PARAMS ((void));
void memory_fatal PARAMS ((void)) __attribute__ ((noreturn));
void move_file (ThreadSafeProgress* progress, char const *, int volatile *, char *, mode_t, int);
void read_fatal PARAMS ((void)) __attribute__ ((noreturn));
void remove_prefix PARAMS ((char *, size_t));
void removedirs(ThreadSafeProgress*, char *);
void write_fatal PARAMS ((void)) __attribute__ ((noreturn));
