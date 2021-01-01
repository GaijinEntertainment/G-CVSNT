#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "cvs.h"
#if defined(_WIN32)
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <fcntl.h>
  #include <io.h>
#else
  #include <sys/socket.h>
  #include <netdb.h>
#endif

void error(int a, int b, const char *cmd, ...)
{
  char line[1024];int line_len = sizeof(line);
  int l;
  va_list va;

  va_start(va,cmd);
  vsnprintf(line,line_len,cmd,va);
  va_end(va);
  fprintf(stderr, "error %d, <%s>\n", b, line);
  if (a)
    exit(1);
}

char *Make_Date (char *rawdate)
{
    time_t unixtime;
	int y,m,d,h,mm,s;

	/* If the date is already in internal format, just return */
	if(sscanf(rawdate,"%04d.%02d.%02d.%02d.%02d.%02d",&y,&m,&d,&h,&mm,&s)==6)
		return xstrdup(rawdate);
    unixtime = get_date (rawdate, (struct timeb *) NULL);
    if (unixtime == (time_t) - 1)
	error (1, 0, "Can't parse date/time: %s", rawdate);
    return date_from_time_t (unixtime);
}

/* Convert a time_t to an RCS format date.  This is mainly for the
   use of "cvs history", because the CVSROOT/history file contains
   time_t format dates; most parts of CVS will want to avoid using
   time_t's directly, and instead use RCS_datecmp, Make_Date, &c.
   Assuming that the time_t is in GMT (as it generally should be),
   then the result will be in GMT too.

   Returns a newly malloc'd string.  */

char *date_from_time_t (time_t unixtime)
{
    struct tm *ftm;
    char date[MAXDATELEN];
    char *ret;

    ftm = gmtime (&unixtime);
    if (ftm == NULL)
	/* This is a system, like VMS, where the system clock is in local
	   time.  Hopefully using localtime here matches the "zero timezone"
	   hack I added to get_date (get_date of course being the relevant
	   issue for Make_Date, and for history.c too I think).  */
	ftm = localtime (&unixtime);

    (void) sprintf (date, DATEFORM,
		    ftm->tm_year + (ftm->tm_year < 100 ? 0 : 1900),
		    ftm->tm_mon + 1, ftm->tm_mday, ftm->tm_hour,
		    ftm->tm_min, ftm->tm_sec);
    ret = xstrdup (date);
    return (ret);
}

/* Convert a date to RFC822/1123 format.  This is used in contexts like
   dates to send in the protocol; it should not vary based on locale or
   other such conventions for users.  We should have another routine which
   does that kind of thing.

   The SOURCE date is in our internal RCS format.  DEST should point to
   storage managed by the caller, at least MAXDATELEN characters.  */
void date_to_internet (char *dest, const char *source)
{
    struct tm date;

    date_to_tm (&date, source);
    tm_to_internet (dest, &date);
}

void date_to_rcsdiff (char *dest, const char *source)
{
    struct tm date;

    date_to_tm (&date, source);
    tm_to_rcsdiff (dest, &date);
}

int date_to_tm (struct tm *dest, const char *source)
{
    if (sscanf (source, SDATEFORM,
		&dest->tm_year, &dest->tm_mon, &dest->tm_mday,
		&dest->tm_hour, &dest->tm_min, &dest->tm_sec)
	    != 6)
	return 1;

    if (dest->tm_year > 100)
	dest->tm_year -= 1900;

    dest->tm_mon -= 1;
    return 0;
}

/* Convert a date to RFC822/1123 format.  This is used in contexts like
   dates to send in the protocol; it should not vary based on locale or
   other such conventions for users.  We should have another routine which
   does that kind of thing.

   The SOURCE date is a pointer to a struct tm.  DEST should point to
   storage managed by the caller, at least MAXDATELEN characters.  */
void tm_to_internet (char *dest, const struct tm *source)
{
    /* Just to reiterate, these strings are from RFC822 and do not vary
       according to locale.  */
    static const char *const month_names[] =
      {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    
    sprintf (dest, "%d %s %d %02d:%02d:%02d -0000", source->tm_mday,
	     source->tm_mon < 0 || source->tm_mon > 11 ? "???" : month_names[source->tm_mon],
	     source->tm_year + 1900, source->tm_hour, source->tm_min, source->tm_sec);
}

void tm_to_rcsdiff(char *dest, const struct tm *source)
{
	sprintf (dest, "%04d/%02d/%02d %02d:%02d:%02d", source->tm_year + 1900,
	     source->tm_mon+1, source->tm_mday,
	     source->tm_hour, source->tm_min, source->tm_sec);
}

int annotate_width = 8;

int trace = 0, noexec = 0, server_active = 0, atomic_checkouts = 0, allow_trace = 0, proxy_active = 0, quiet = 0;
FILE *trace_file_fp = NULL;
const char *CVS_Username = "rcs_cvt";
const char *trace_file = NULL;
int cvs_output(char const *s, uint64_t len){if (len) fprintf(stdout, "%.*s", (int)len, s); else fprintf(stdout, "%s", s); return 0;}
int cvs_outerr(char const *s, uint64_t len){if (len) fprintf(stderr, "%.*s", (int)len, s); else fprintf(stderr, "%s", s); return 0;}
void cvs_flusherr(){fflush(stderr);}


/*
 * Write SIZE bytes at BUF to FP, expanding @ signs into double @
 * signs.  If an error occurs, return a negative value and set errno
 * to indicate the error.  If not, return a nonnegative value.
 */
int expand_at_signs (const char *buf, unsigned int size, FILE *fp)
{
    register const char *cp, *next;

    cp = buf;
    while ((next = (const char *)memchr (cp, '@', size)) != NULL)
    {
	int len;

	++next;
	len = next - cp;
	if (fwrite (cp, 1, len, fp) != len)
	    return EOF;
	if (fputc ('@', fp) == EOF)
	    return EOF;
	cp = next;
	size -= len;
    }

    if (fwrite (cp, 1, size, fp) != size)
	return EOF;

    return 1;
}

void usage (const char *const cpp[]){error(1,0,"no usage");}

mode_t cvsumask = UMASK_DFLT;
char *remote_host_name = NULL; char hostname[] = "localhost";
int start_recursion(FILEPROC fileproc, FILESDONEPROC filesdoneproc, PREDIRENTPROC predirentproc, DIRENTPROC direntproc, DIRLEAVEPROC dirleaveproc, void *callerdat, int argc, char **argv, int local, int which, int aflag, int readlock, const char *update_preload, const char *update_repository, int dosrcs, PERMPROC permproc, const char *tag){return 0;}
/*int do_unlock_file(size_t lockId){return 0;}
size_t do_lock_file(const char *file, const char *repository, int write, int wait){return 1;}
int do_modified(size_t lockId, const char *version, const char *oldversion, const char *branch, char type){return 0;}
int do_lock_version(size_t lockId, const char *branch, char **version){return 0;}*/

cvsroot *current_parsed_root = NULL;
bool is_session_blob_reference_data(const void *data, size_t len){return false;}

const char *Tmpdir = TMPDIR_DFLT;
