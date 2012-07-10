#ifndef CONFIG__H
#define CONFIG__H

#ifdef __cplusplus
extern "C" {
#endif

/* config.h --- configuration file for Windows NT
   Jim Blandy <jimb@cyclic.com> --- July 1995

    Modified for Win32 Server support
	Tony Hoyle <tmh@magenta-logic.com> --- December 1999
*/

/* This file lives in the windows-NT subdirectory, which is only included
   in your header search path if you're working under Microsoft Visual C++,
   and use ../cvsnt.mak for your project.  Thus, this is the right place to
   put configuration information for Windows NT.  */



/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#undef _ALL_SOURCE

/* Define to empty if the keyword does not work.  */
/* Const is working.  */
#undef const

/* Define to `int' if <sys/types.h> doesn't define.  */
/* Windows NT doesn't have gid_t.  It doesn't even really have group
   numbers, I think.  This will take more thought to get right, but
   let's get it running first.  */
#define gid_t int

/* Define if you support file names longer than 14 characters.  */
/* Yes.  Woo.  */
#define HAVE_LONG_FILE_NAMES 1

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
/* If POSIX.1 requires this, why doesn't WNT have it?  */
#undef HAVE_SYS_WAIT_H

/* Define if utime(file, NULL) sets file's timestamp to the present.  */
/* Experimentation says yes.  Wish I had the full documentation, but
   I have neither the CD-ROM nor a CD-ROM drive to put it in.  */
#define HAVE_UTIME_NULL 1

/* On Windows NT, when a file is being watched, utime expects a file
   to be writable */
#define UTIME_EXPECTS_WRITABLE

/* Define if on MINIX.  */
/* Hah.  */
#undef _MINIX

#include "../cvsapi/win32/config.h"

/* Define to `int' if <sys/types.h> doesn't define.  */
#define mode_t int

/* Define to `int' if <sys/types.h> doesn't define.  */
/* Under Windows NT, we use the process handle as the pid.
   We could #define pid_t to be HANDLE, but that would require
   us to #include <windows.h>, which I don't trust, and HANDLE
   is a pointer type anyway.  */
#define pid_t void*

/* Define if the system does not provide POSIX.1 features except
   with this defined.  */
/* This string doesn't appear anywhere in the system header files,
   so I assume it's irrelevant.  */
#undef _POSIX_1_SOURCE

/* Define if you need to in order for stat and other things to work.  */
/* Same as for _POSIX_1_SOURCE, above.  */
#undef _POSIX_SOURCE

/* Define as the return type of signal handlers (int or void).  */
/* The manual says they return void.  */
#define RETSIGTYPE void

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* sys/types.h doesn't define it, but stdio.h does, which cvs.h
   #includes, so things should be okay.  */
/* #undef size_t */

/* Define if the `S_IS*' macros in <sys/stat.h> do not work properly.  */
/* We don't seem to have them at all; let ../lib/system.h define them.  */
#define STAT_MACROS_BROKEN 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
/* We don't have <sys/time.h> at all.  Why isn't there a definition
   for HAVE_SYS_TIME_H anywhere in config.h.in?  */
#undef TIME_WITH_SYS_TIME

/* Define if you have MIT Kerberos version 4 available.  */
/* We don't.  Cygnus says they've ported it to Windows 3.1, but
   I don't know if that means that it works under Windows NT as
   well.  */
#undef HAVE_KERBEROS

/* Define if you want CVS to be able to serve repositories to remote
   clients.  */
#define SERVER_SUPPORT

/* Define if creating an NT version */
#define WINNT_VERSION

/* Define if you have the connect function.  */
#define HAVE_CONNECT

/* Define if you have the fchdir function.  */
#undef HAVE_FCHDIR

/* Define if you have the fchmod function.  */
#undef HAVE_FCHMOD

/* Define if you have the fsync function.  */
#undef HAVE_FSYNC

/* Define if you have the ftime function.  */
#define HAVE_FTIME

/* Define if you have the ftruncate function.  */
#undef HAVE_FTRUNCATE

/* Define if you have the getpagesize function.  */
#undef HAVE_GETPAGESIZE

/* Define if you have the krb_get_err_text function.  */
#undef HAVE_KRB_GET_ERR_TEXT

/* Define if you have the putenv function.  */
#define HAVE_PUTENV

/* Define if you have the sigaction function.  */
#undef HAVE_SIGACTION

/* Define if you have the sigblock function.  */
#undef HAVE_SIGBLOCK

/* Define if you have the sigprocmask function.  */
#undef HAVE_SIGPROCMASK

/* Define if you have the sigsetmask function.  */
#undef HAVE_SIGSETMASK

/* Define if you have the sigvec function.  */
#undef HAVE_SIGVEC

/* Define if you have the timezone function.  */
/* Hmm, I actually rather think it's an extern long
   variable; that message was mechanically generated
   by autoconf.  And I don't see any actual uses of
   this function in the code anyway, hmm.  */
#undef HAVE_TIMEZONE

/* Define if you have the vfork function.  */
#undef HAVE_VFORK

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF

/* Define if you have the <direct.h> header file.  */
/* Windows NT wants this for mkdir and friends.  */
#define HAVE_DIRECT_H

/* Define if you have the <dirent.h> header file.  */
/* No, but we have the <direct.h> header file...  */
#undef HAVE_DIRENT_H

/* Define target_cpu */
#undef CVSNT_TARGET_CPU
#ifdef _M_ALPHA
#define CVSNT_TARGET_CPU "alpha"
#endif
#ifdef _M_IX86
#define CVSNT_TARGET_CPU "x86"
#endif
#ifdef _M_IA64
#define CVSNT_TARGET_CPU "ia64"
#endif
#ifdef _M_MPPC
#define CVSNT_TARGET_CPU "macpower"
#endif
#ifdef _M_MRX000
#define CVSNT_TARGET_CPU "mips"
#endif
#ifdef _M_PPC
#define CVSNT_TARGET_CPU "powerpc"
#endif
#ifdef _M_X64
#define CVSNT_TARGET_CPU "x86_64"
#endif

/* Define target_vendor */
#undef CVSNT_TARGET_VENDOR
#define CVSNT_TARGET_VENDOR "microsoft"

/* Define target_os */
#undef CVSNT_TARGET_OS
#define CVSNT_TARGET_OS "windows"

/* Define if you have the <errno.h> header file.  */
#define HAVE_ERRNO_H

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H

/* Define if you have the <io.h> header file.  */
/* Apparently this is where Windows NT declares all the low-level
   Unix I/O routines like open and creat and stuff.  */
#define HAVE_IO_H

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H 1

/* Define if you have the <ndbm.h> header file.  */
#undef HAVE_NDBM_H

/* Define if you have the <ndir.h> header file.  */
#define HAVE_NDIR_H

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H

#define HAVE_MALLOC_H

/* Define to force lib/regex.c to use malloc instead of alloca.  */
#define REGEX_MALLOC 1

/* Define to force lib/regex.c to define re_comp et al.  */
#define _REGEX_RE_COMP 1

/* Define if you have the <sys/bsdtypes.h> header file.  */
#undef HAVE_SYS_BSDTYPES_H

/* Define if you have the <sys/dir.h> header file.  */
#undef HAVE_SYS_DIR_H

/* Define if you have the <sys/ndir.h> header file.  */
#undef HAVE_SYS_NDIR_H

/* Define if you have the <sys/param.h> header file.  */
#undef HAVE_SYS_PARAM_H

/* Define if you have the <sys/select.h> header file.  */
#undef HAVE_SYS_SELECT_H

/* Define if you have the <sys/time.h> header file.  */
#undef HAVE_SYS_TIME_H

/* Define if you have the <sys/timeb.h> header file.  */
#define HAVE_SYS_TIMEB_H

/* Define if you have the <unistd.h> header file.  */
#undef HAVE_UNISTD_H

/* Define if you have the <utime.h> header file.  */
#undef HAVE_UTIME_H
#define HAVE_SYS_UTIME_H

/* Define if you have the inet library (-linet).  */
#undef HAVE_LIBINET

/* Define if you have the nsl library (-lnsl).  */
/* This is not used anywhere in the source code.  */
#undef HAVE_LIBNSL

/* Define if you have the nsl_s library (-lnsl_s).  */
#undef HAVE_LIBNSL_S

/* Define if you have the socket library (-lsocket).  */
/* This isn't ever used either.  */
#undef HAVE_LIBSOCKET

/* Under Windows NT, mkdir only takes one argument.  */
#define CVS_MKDIR wnt_mkdir
extern int wnt_mkdir (const char *PATH, int MODE);
#define CVS_STAT stat
#define CVS_LSTAT stat
#define CVS_RENAME wnt_rename
extern int wnt_rename (const char *, const char *);

extern int wnt_chdir(const char *dir);
#define CVS_CHDIR wnt_chdir

/* This function doesn't exist under Windows NT; we
   provide a stub.  */
extern int readlink (char *path, char *buf, int buf_size);

/* We definitely have prototypes.  */
#define USE_PROTOTYPES 1

/* This is just a call to the Win32 Sleep function.  */
unsigned int sleep (unsigned int);

/* Don't worry, Microsoft, it's okay for these functions to
   be in our namespace.  */
#define popen _popen
#ifndef pclose
#define pclose _pclose
#endif

/* When writing binary data to stdout, we better set
   stdout to binary mode using setmode.  */
#define USE_SETMODE_STDOUT

/* Diff also has an ifdef for setmode, and it is HAVE_SETMODE.  */
#define HAVE_SETMODE

/* Diff needs us to define this.  I think it could always be
   -1 for CVS, because we pass temporary files to diff, but
   config.h seems like the easiest place to put this, so for
   now we put it here.  */
#define same_file(s,t) (-1)

/* This is where old bits go to die under Windows NT.  */
#define DEVNULL "nul:"

/* Don't use an rsh subprocess to connect to the server, because
   the rsh does inappropriate translations on the data (CR-LF/LF).  */
#define RSH_NOT_TRANSPARENT 1
extern void wnt_start_server (int *tofd, int *fromfd,
			      char *client_user,
			      char *server_user,
			      char *server_host,
			      char *server_cvsroot);
extern void wnt_shutdown_server (int fd);
#define START_SERVER wnt_start_server
#define SHUTDOWN_SERVER wnt_shutdown_server

#define SYSTEM_INITIALIZE(mode,pargc,pargv) win32init(mode)
void win32init(int mode);
#define SYSTEM_CLEANUP() wnt_cleanup()
extern void wnt_cleanup (void);

#define HAVE_WINSOCK_H
#define HAVE_SELECT

/* This tells the client that it must use send()/recv() to talk to the
   server if it is connected to the server via a socket; Windows needs
   it because _open_osfhandle doesn't work.  */
#define NO_SOCKET_TO_FD 1

/* This tells the client that, in addition to needing to use
   send()/recv() to do socket I/O, the error codes for send()/recv()
   and other socket operations are not available through errno.
   Instead, this macro should be used to obtain an error code. */
#define SOCK_ERRNO (WSAGetLastError ())

/* This tells the client that, in addition to needing to use
   send()/recv() to do socket I/O, the error codes for send()/recv()
   and other socket operations are not known to strerror.  Instead,
   this macro should be used to convert the error codes to strings. */
#define SOCK_STRERROR sock_strerror
extern char *sock_strerror (int errnum);

/* The internal rsh client uses sockets not file descriptors.  Note
   that as the code stands now, it often takes values from a SOCKET and
   puts them in an int.  This is ugly but it seems like sizeof
   (SOCKET) <= sizeof (int) on win32, even the 64-bit variants.  */
#define START_SERVER_RETURNS_SOCKET 1

/* Is this true on NT?  Seems like I remember reports that NT 3.51 has
   problems with 200K writes (of course, the issue of large writes is
   moot since the use of buffer.c ensures that writes will only be as big
   as the buffers).  */
#define SEND_NEVER_PARTIAL 1

/* Win32 has vsnprintf (sort of) */
#define HAVE_VSNPRINTF

#define SIZEOF_CHAR sizeof(char)
#define SIZEOF_SHORT sizeof(short)

// Disable 'signed/unsigned mismatch'
#pragma warning (disable:4018)

// Disable function parameter differences in function pointers
#pragma warning (disable:4113)

// Disable conversion from long to char
#pragma warning (disable:4244)

#define strcasecmp stricmp
#define strncasecmp strnicmp
#define snprintf _snprintf

#define HAVE_GSSAPI_GSSAPI_H
#define HAVE_GSSAPI_GSSAPI_GENERIC_H
#define HAVE_KRB5_H
#define GSS_C_NT_HOSTBASED_SERVICE gss_nt_service_name

#define HAVE_GETADDRINFO /* Provided by platform SDK */
#define HAVE_INET_ATON

/* Stuff for xdelta */
#define NO_SYS_SIGLIST	1
#define NO_SYS_SIGLIST_DECL	1

/* Iconv requirements */
#define HAVE_ICONV_H
#define HAVE_LIBCHARSET_H
#define HAVE_LOCALE_CHARSET
#define HAVE_LIBICONV

/* We have stddef.h */
#define HAVE_STDDEF_H

/* We have ptrdiff_t */
#define HAVE_PTRDIFF_T

#define STDC_HEADERS

#define iconv_arg2_t const char **

// Debugging
#include <crtdbg.h>
#ifdef _DEBUG
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
/* Standard VC alloc checking */
#define malloc(_mem) _malloc_dbg(_mem, _NORMAL_BLOCK, __FILE__, __LINE__)
#define realloc(_ptr,_mem) _realloc_dbg(_ptr,_mem,_NORMAL_BLOCK,__FILE__,__LINE__)
#endif

#ifdef MAIN_CVS /* If this is a protocol dll stop here */

#define GPL_CODE /* CVSNT core is GPL - enable GPL-Only parts of API */

// Validate a users' password against the NT username database on the
// current machine
int win32_valid_user(const char *username, const char *pasword, const char* domain, void **user_token);

// Helper routines for password stuff
//struct passwd *win32getpwuid(uid_t uid);
struct passwd *win32getpwnam(const char *name);
char *win32getlogin();
char *win32getfileowner(const char *file);

#ifdef SERVER_SUPPORT
int win32setuser(const char *username);
int win32switchtouser(const char *username, void *preauth_user_handle);
#endif
void win32flush(int fd);
#include <stdio.h> // Stops error
#define pipe(_p) _pipe(_p,BUFSIZ,_O_BINARY)
void win32setblock(int h, int block);

/* non-case sensitive filename comparison */
#define filename_cmp(a,b) stricmp(a, b)

/* Stuff to stop inconsistent dll linkage errors */
#define getenv getenv
#define strerror strerror

/* Override so we can delete read-only files */
#define CVS_UNLINK wnt_unlink
int wnt_unlink(const char *file);

int win32_isadmin();
int win32_getgroups_begin(void **grouplist);
const char *win32_getgroup(void *grouplist, int num);
void win32_getgroups_end(void **grouplist);
int win32_openpipe(const char *pipe);
void win32_perror(int quit, const char *prefix, ...); // Print last error from pipe

/* New open funcs, for pre-parsing '\' and '/' */
#include <io.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include <time.h>

#include <sys/types.h>

/* 64bit file offset */
#define off_t _loff_t
typedef __int64 _loff_t;

int wnt_fseek64(FILE *fp, off_t pos, int whence);
off_t wnt_ftell64(FILE *fp);

#define CVS_FSEEK wnt_fseek64
#define CVS_FTELL wnt_ftell64

struct wnt_stat {
        _dev_t st_dev;
        _ino_t st_ino;
        unsigned short st_mode;
        short st_nlink;
        short st_uid;
        short st_gid;
        _dev_t st_rdev;
        _off_t st_size;
        __time64_t st_atime;
        __time64_t st_mtime;
        __time64_t st_ctime;
        };
int wnt_stat(const char *name, struct wnt_stat *buf);
int wnt_fstat (int fildes, struct wnt_stat *buf);
int wnt_lstat (const char *name, struct wnt_stat *buf);

#if !defined(NO_REDIRECTION)
#define fstat wnt_fstat
#define lstat wnt_lstat
#define stat wnt_stat

int wnt_chmod (const char *name, mode_t mode);
FILE *wnt_fopen(const char *filename, const char *mode);
int wnt_open(const char *filename, int mode, int umask = 0);
int wnt_fclose(FILE *file);
int wnt_access(const char *path, int mode);
int wnt_utime(const char *filename, struct utimbuf *uf);

void preparse_filename(char *filename);

#define fopen wnt_fopen
#define fclose wnt_fclose
#define CVS_OPEN wnt_open
#define access wnt_access
#define utime wnt_utime
#define chmod wnt_chmod
#else
#define lstat stat
#endif

#ifdef HAVE_PUTENV
void wnt_putenv(const char *variable, const char *value);
#endif

int wnt_link(const char *oldpath, const char *newpath);
#define link wnt_link

#include <process.h>

#define HAVE_USLEEP
int usleep(unsigned long useconds);

int w32_is_network_share(const char *directory);

extern int bIsWin95, bIsNt4, bIsWin2k;

void win32_sanitize_username(const char **pusername);

void wnt_hide_file(const char *fn);
int win32_makepipe(long hPipe);
void wnt_get_temp_dir(char *tempdir, int tempdir_size);

extern int win32_global_codepage;

#endif /* not PROTOCOL_DLL */

#ifdef __cplusplus
}
#endif

int win32_set_edit_acl(const char *filename);

#endif
