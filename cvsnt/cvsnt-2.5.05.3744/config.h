/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.in by autoheader.  */

/* Define to 1 if the `closedir' function returns void instead of `int'. */
/* #undef CLOSEDIR_VOID */

/* Define target_cpu */
#define CVSNT_TARGET_CPU "x86_64"

/* Define target_os */
#define CVSNT_TARGET_OS "linux-gnu"

/* Define target_vendor */
#define CVSNT_TARGET_VENDOR "pc"

/* Define to OS specific seek routine */
#define CVS_FSEEK fseeko

/* Define to OS specific tell routine */
#define CVS_FTELL ftello

/* Support for remote enumerationi protocol */
#define ENUM_PROTOCOL_SUPPORT /**/

/* Support for :ext: */
#define EXT_PROTOCOL_SUPPORT /**/

/* Support for :fork: */
/* #undef FORK_PROTOCOL_SUPPORT */

/* Support for :gserver: */
/* #undef GSERVER_PROTOCOL_SUPPORT */

/* Define to an alternative value if GSS_C_NT_HOSTBASED_SERVICE isn't defined
   in the gssapi.h header file. MIT Kerberos 1.2.1 requires this. Only
   relevant when using GSSAPI. */
/* #undef GSS_C_NT_HOSTBASED_SERVICE */

/* Define to 1 if you have the `argz_add' function. */
#define HAVE_ARGZ_ADD 1

/* Define to 1 if you have the `argz_append' function. */
#define HAVE_ARGZ_APPEND 1

/* Define to 1 if you have the `argz_count' function. */
#define HAVE_ARGZ_COUNT 1

/* Define to 1 if you have the `argz_create_sep' function. */
#define HAVE_ARGZ_CREATE_SEP 1

/* Define to 1 if you have the <argz.h> header file. */
#define HAVE_ARGZ_H 1

/* Define to 1 if you have the `argz_insert' function. */
#define HAVE_ARGZ_INSERT 1

/* Define to 1 if you have the `argz_next' function. */
#define HAVE_ARGZ_NEXT 1

/* Define to 1 if you have the `argz_stringify' function. */
#define HAVE_ARGZ_STRINGIFY 1

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define HAVE_ARPA_INET_H 1

/* Use Avahi/DNS-SD registration */
/* #undef HAVE_AVAHI */

/* Define to 1 if you have the `closedir' function. */
#define HAVE_CLOSEDIR 1

/* Define if connect is supported */
#define HAVE_CONNECT /**/

/* Define to 1 if you have the `daemon' function. */
#define HAVE_DAEMON 1

/* Define to 1 if you have the declaration of `cygwin_conv_path', and to 0 if
   you don't. */
/* #undef HAVE_DECL_CYGWIN_CONV_PATH */

/* Define to 1 if you have the <direct.h> header file. */
/* #undef HAVE_DIRECT_H */

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
#define HAVE_DIRENT_H 1

/* Define if you have the GNU dld library. */
/* #undef HAVE_DLD */

/* Define to 1 if you have the <dld.h> header file. */
/* #undef HAVE_DLD_H */

/* Define to 1 if you have the `dlerror' function. */
#define HAVE_DLERROR 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <dl.h> header file. */
/* #undef HAVE_DL_H */

/* Use Bonjour/DNS-SD registration */
/* #undef HAVE_DNS_SD */

/* Define to 1 if you have the <dns_sd.h> header file. */
/* #undef HAVE_DNS_SD_H */

/* Define to 1 if you have the `dup2' function. */
#define HAVE_DUP2 1

/* Define if you have the _dyld_func_lookup function. */
/* #undef HAVE_DYLD */

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define to 1 if the system has the type `error_t'. */
#define HAVE_ERROR_T 1

/* Define to 1 if you have the `fchdir' function. */
#define HAVE_FCHDIR 1

/* Define to 1 if you have the `fchmod' function. */
#define HAVE_FCHMOD 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if your system has a working POSIX `fnmatch' function. */
#define HAVE_FNMATCH 1

/* Define to 1 if you have the `fork' function. */
#define HAVE_FORK 1

/* Define to 1 if you have the `fseeko' function. */
#define HAVE_FSEEKO 1

/* Define to 1 if you have the `fsync' function. */
#define HAVE_FSYNC 1

/* Define to 1 if you have the `ftello' function. */
#define HAVE_FTELLO 1

/* Define to 1 if you have the `ftime' function. */
#define HAVE_FTIME 1

/* Define to 1 if you have the `ftruncate' function. */
#define HAVE_FTRUNCATE 1

/* supports GCC visibility attributes */
/* #undef HAVE_GCC_VISIBILITY */

/* Define to 1 if you have the `getaddrinfo' function. */
#define HAVE_GETADDRINFO 1

/* Define to 1 if you have the `getdelim' function. */
#define HAVE_GETDELIM 1

/* Define to 1 if you have the `getgroups' function. */
#define HAVE_GETGROUPS 1

/* Define to 1 if you have the `getline' function. */
#define HAVE_GETLINE 1

/* Define to 1 if you have the `getnameinfo' function. */
#define HAVE_GETNAMEINFO 1

/* Define to 1 if you have the `getopt' function. */
#define HAVE_GETOPT 1

/* Define to 1 if you have the <getopt.h> header file. */
#define HAVE_GETOPT_H 1

/* Define to 1 if you have the `getopt_long' function. */
#define HAVE_GETOPT_LONG 1

/* Define to 1 if you have the `getpagesize' function. */
#define HAVE_GETPAGESIZE 1

/* Define to 1 if you have the `getpass' function. */
#define HAVE_GETPASS 1

/* Define to 1 if you have the `getpassphrase' function. */
/* #undef HAVE_GETPASSPHRASE */

/* Define if getspnam is supported */
#define HAVE_GETSPNAM /**/

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the <glob.h> header file. */
#define HAVE_GLOB_H 1

/* Define to 1 if you have the <grp.h> header file. */
#define HAVE_GRP_H 1

/* Define if you have GSSAPI with Kerberos version 5 available. */
/* #undef HAVE_GSSAPI */

/* Define to 1 if you have the <gssapi/gssapi_generic.h> header file. */
/* #undef HAVE_GSSAPI_GSSAPI_GENERIC_H */

/* Define to 1 if you have the <gssapi/gssapi.h> header file. */
/* #undef HAVE_GSSAPI_GSSAPI_H */

/* Define to 1 if you have the <gssapi.h> header file. */
/* #undef HAVE_GSSAPI_H */

/* Define to 1 if you have the <iconv.h> header file. */
#define HAVE_ICONV_H 1

/* Define to 1 if you have the `iconv_open' function. */
#define HAVE_ICONV_OPEN 1

/* Define to 1 if you have the <idna.h> header file. */
/* #undef HAVE_IDNA_H */

/* Define to 1 if you have the `inet_aton' function. */
#define HAVE_INET_ATON 1

/* Define to 1 if you have the `initgroups' function. */
#define HAVE_INITGROUPS 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <io.h> header file. */
/* #undef HAVE_IO_H */

/* Define to 1 if you have the <langinfo.h> header file. */
#define HAVE_LANGINFO_H 1

/* Define to 1 if you have the `avahi-client' library (-lavahi-client). */
/* #undef HAVE_LIBAVAHI_CLIENT */

/* Define to 1 if you have the <libcharset.h> header file. */
/* #undef HAVE_LIBCHARSET_H */

/* Define to 1 if you have the `crypt' library (-lcrypt). */
#define HAVE_LIBCRYPT 1

/* Define if you have the libdl library or equivalent. */
#define HAVE_LIBDL 1

/* Define if libdlloader will be built on this platform */
#define HAVE_LIBDLLOADER 1

/* Define to 1 if you have the `dns_sd' library (-ldns_sd). */
/* #undef HAVE_LIBDNS_SD */

/* Define to 1 if you have the `libiconv_open' function. */
/* #undef HAVE_LIBICONV_OPEN */

/* Define to 1 if you have the `idn' library (-lidn). */
/* #undef HAVE_LIBIDN */

/* Define to 1 if you have the <libpq-fe.h> header file. */
/* #undef HAVE_LIBPQ_FE_H */

/* Define to 1 if you have the `socket' library (-lsocket). */
/* #undef HAVE_LIBSOCKET */

/* Define to 1 if you have the `xml2' library (-lxml2). */
#define HAVE_LIBXML2 1

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the <localcharset.h> header file. */
/* #undef HAVE_LOCALCHARSET_H */

/* Define to 1 if you have the `locale_charset' function. */
/* #undef HAVE_LOCALE_CHARSET */

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define to 1 if you support file names longer than 14 characters. */
#define HAVE_LONG_FILE_NAMES 1

/* Define this if a modern libltdl is already installed */
#define HAVE_LTDL 1

/* Define to 1 if you have the <mach-o/dyld.h> header file. */
/* #undef HAVE_MACH_O_DYLD_H */

/* Define to 1 if you have the <malloc.h> header file. */
#define HAVE_MALLOC_H 1

/* Defined if MDNS is available */
#define HAVE_MDNS /**/

/* Define to 1 if you have the `memchr' function. */
#define HAVE_MEMCHR 1

/* Define to 1 if you have the `memmove' function. */
#define HAVE_MEMMOVE 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `mkdir' function. */
#define HAVE_MKDIR 1

/* Define to 1 if you have the `mknod' function. */
#define HAVE_MKNOD 1

/* Define to 1 if you have the `mkstemp' function. */
#define HAVE_MKSTEMP 1

/* Define to 1 if you have the `mktemp' function. */
#define HAVE_MKTEMP 1

/* Defined if Mysql 3.23 is available */
/* #undef HAVE_MYSQL */

/* Define to 1 if you have the <mysql.h> header file. */
/* #undef HAVE_MYSQL_H */

/* Define to 1 if you have the `nanosleep' function. */
#define HAVE_NANOSLEEP 1

/* Define to 1 if you have the <ndbm.h> header file. */
/* #undef HAVE_NDBM_H */

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have the `nl_langinfo' function. */
#define HAVE_NL_LANGINFO 1

/* Defined if ODBC is available */
/* #undef HAVE_ODBC */

/* Define to 1 if you have the `opendir' function. */
#define HAVE_OPENDIR 1

/* Support for PAM authentication */
/* #undef HAVE_PAM */

/* Define to 1 if you have the <pam/pam_appl.h> header file. */
/* #undef HAVE_PAM_PAM_APPL_H */

/* Define to 1 if you have the <pam/pam_misc.h> header file. */
/* #undef HAVE_PAM_PAM_MISC_H */

/* Define to 1 if you have the <paths.h> header file. */
#define HAVE_PATHS_H 1

/* Defined if Postgres is available */
/* #undef HAVE_POSTGRES */

/* Define if libtool can extract symbol lists from object files. */
#define HAVE_PRELOADED_SYMBOLS 1

/* Define if you have POSIX threads libraries and header files. */
#define HAVE_PTHREAD 1

/* Define to 1 if you have the <pthread.h> header file. */
#define HAVE_PTHREAD_H 1

/* Define to 1 if the system has the type `ptrdiff_t'. */
#define HAVE_PTRDIFF_T 1

/* Define to 1 if you have the `putenv' function. */
#define HAVE_PUTENV 1

/* Define to 1 if you have the `readdir' function. */
#define HAVE_READDIR 1

/* Define to 1 if you have the `readlink' function. */
#define HAVE_READLINK 1

/* Define to 1 if you have the `regcomp' function. */
#define HAVE_REGCOMP 1

/* Define to 1 if you have the <regex.h> header file. */
#define HAVE_REGEX_H 1

/* Define to 1 if you have the `rename' function. */
#define HAVE_RENAME 1

/* Define to 1 if you have the `res_query' function. */
/* #undef HAVE_RES_QUERY */

/* Define to 1 if you have the <security/pam_appl.h> header file. */
/* #undef HAVE_SECURITY_PAM_APPL_H */

/* Define to 1 if you have the <security/pam_misc.h> header file. */
/* #undef HAVE_SECURITY_PAM_MISC_H */

/* Define to 1 if you have the `select' function. */
/* #undef HAVE_SELECT */

/* Define to 1 if you have the `setlocale' function. */
#define HAVE_SETLOCALE 1

/* Define if you have the shl_load function. */
/* #undef HAVE_SHL_LOAD */

/* Define to 1 if you have the `sigaction' function. */
#define HAVE_SIGACTION 1

/* Define to 1 if you have the `sigblock' function. */
#define HAVE_SIGBLOCK 1

/* Define to 1 if you have the `sigprocmask' function. */
#define HAVE_SIGPROCMASK 1

/* Define to 1 if you have the `sigsetmask' function. */
#define HAVE_SIGSETMASK 1

/* Define to 1 if you have the `sigvec' function. */
/* #undef HAVE_SIGVEC */

/* Define to 1 if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define to 1 if you have the <sqlext.h> header file. */
/* #undef HAVE_SQLEXT_H */

/* Defined if SQLite is available */
/* #undef HAVE_SQLITE */

/* Define to 1 if you have the <sqlite3.h> header file. */
/* #undef HAVE_SQLITE3_H */

/* Define to 1 if you have the <sql.h> header file. */
/* #undef HAVE_SQL_H */

/* Define to 1 if you have the <stdarg.h> header file. */
#define HAVE_STDARG_H 1

/* Define to 1 if you have the <stddef.h> header file. */
#define HAVE_STDDEF_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strchr' function. */
#define HAVE_STRCHR 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strlcat' function. */
/* #undef HAVE_STRLCAT */

/* Define to 1 if you have the `strlcpy' function. */
/* #undef HAVE_STRLCPY */

/* Define to 1 if you have the `strstr' function. */
#define HAVE_STRSTR 1

/* Define to 1 if you have the `strtoul' function. */
#define HAVE_STRTOUL 1

/* Define to 1 if `st_blksize' is a member of `struct stat'. */
#define HAVE_STRUCT_STAT_ST_BLKSIZE 1

/* Define to 1 if `st_rdev' is a member of `struct stat'. */
#define HAVE_STRUCT_STAT_ST_RDEV 1

/* Define to 1 if you have the <syslog.h> header file. */
#define HAVE_SYSLOG_H 1

/* Define to 1 if you have the <sys/bsdtypes.h> header file. */
/* #undef HAVE_SYS_BSDTYPES_H */

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/dl.h> header file. */
/* #undef HAVE_SYS_DL_H */

/* Define to 1 if you have the <sys/file.h> header file. */
#define HAVE_SYS_FILE_H 1

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/resource.h> header file. */
#define HAVE_SYS_RESOURCE_H 1

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/termios.h> header file. */
#define HAVE_SYS_TERMIOS_H 1

/* Define to 1 if you have the <sys/timeb.h> header file. */
#define HAVE_SYS_TIMEB_H 1

/* Define to 1 if you have the <sys/times.h> header file. */
#define HAVE_SYS_TIMES_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/uio.h> header file. */
#define HAVE_SYS_UIO_H 1

/* Define to 1 if you have the <sys/un.h> header file. */
#define HAVE_SYS_UN_H 1

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
#define HAVE_SYS_WAIT_H 1

/* Define to 1 if you have the `tempnam' function. */
#define HAVE_TEMPNAM 1

/* Define to 1 if you have the <termios.h> header file. */
#define HAVE_TERMIOS_H 1

/* Define to 1 if you have the `timegm' function. */
#define HAVE_TIMEGM 1

/* Define to 1 if you have the `timezone' function. */
#define HAVE_TIMEZONE 1

/* Define to 1 if you have the `tzset' function. */
#define HAVE_TZSET 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `usleep' function. */
/* #undef HAVE_USLEEP */

/* Define to 1 if you have the <utime.h> header file. */
#define HAVE_UTIME_H 1

/* Define to 1 if `utime(file, NULL)' sets file's timestamp to the present. */
#define HAVE_UTIME_NULL 1

/* Defined if va_list is an array */
#define HAVE_VA_LIST_AS_ARRAY /**/

/* Define to 1 if you have the `vfork' function. */
#define HAVE_VFORK 1

/* Define to 1 if you have the <vfork.h> header file. */
/* #undef HAVE_VFORK_H */

/* Define to 1 if you have the `vprintf' function. */
#define HAVE_VPRINTF 1

/* Define to 1 if you have the `wait3' function. */
#define HAVE_WAIT3 1

/* Define to 1 if you have the `waitpid' function. */
#define HAVE_WAITPID 1

/* Define to 1 if you have the <wchar.h> header file. */
#define HAVE_WCHAR_H 1

/* This value is set to 1 to indicate that the system argz facility works */
#define HAVE_WORKING_ARGZ 1

/* Define to 1 if `fork' works. */
#define HAVE_WORKING_FORK 1

/* Define to 1 if `vfork' works. */
#define HAVE_WORKING_VFORK 1

/* Define if we have standard wstring */
#define HAVE_WSTRING /**/

/* Define if the OS needs help to load dependent libraries for dlopen(). */
/* #undef LTDL_DLOPEN_DEPLIBS */

/* Define to the system default library search path. */
#define LT_DLSEARCH_PATH "/lib:/usr/lib:/usr/lib/x86_64-linux-gnu/libfakeroot:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu:/lib32:/usr/lib32:/libx32:/usr/libx32"

/* The archive extension */
#define LT_LIBEXT "a"

/* The archive prefix */
#define LT_LIBPREFIX "lib"

/* Define to the extension used for runtime loadable modules, say, ".so". */
#define LT_MODULE_EXT ".so"

/* Define to the name of the environment variable that determines the run-time
   module search path. */
#define LT_MODULE_PATH_VAR "LD_LIBRARY_PATH"

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Define to the shared library suffix, say, ".dylib". */
/* #undef LT_SHARED_EXT */

/* Define to the shared archive member specification, say "(shr.o)". */
/* #undef LT_SHARED_LIB_MEMBER */

/* Support for Mac HFS filesystem */
/* #undef MAC_HFS_STUFF */

/* Define if dlsym() requires a leading underscore in symbol names. */
/* #undef NEED_USCORE */

/* Operating system version */
#define OS_VERSION "linux-gnu"

/* Name of package */
#define PACKAGE "cvsnt"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "tmh@nodomain.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "cvsnt"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "cvsnt 0"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "cvsnt"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "0"

/* Support for :pserver: */
#define PSERVER_PROTOCOL_SUPPORT /**/

/* Define to necessary symbol if this constant uses a non-standard name on
   your system. */
/* #undef PTHREAD_CREATE_JOINABLE */

/* Define to force lib/regex.c to use malloc instead of alloca. */
#define REGEX_MALLOC /**/

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* Support for :server: */
#define SERVER_PROTOCOL_SUPPORT /**/

/* Support CVS remote server */
#define SERVER_SUPPORT /**/

/* The size of `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of `long', as computed by sizeof. */
#define SIZEOF_LONG 8

/* The size of `short', as computed by sizeof. */
#define SIZEOF_SHORT 2

/* Support for :sserver: */
#define SSERVER_PROTOCOL_SUPPORT /**/

/* Support for :sspi: */
#define SSPI_PROTOCOL_SUPPORT /**/

/* Define to 1 if the `S_IS*' macros in <sys/stat.h> do not work properly. */
/* #undef STAT_MACROS_BROKEN */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Version number of package */
#define VERSION "2.5.05.7607"

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Defined to enable GNU extensions */
#define _GNU_SOURCE 1

/* Defined to enable large files on some systems */
#define _LARGEFILE_SOURCE 1

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

/* Define so that glibc/gnulib argp.h does not typedef error_t. */
/* #undef __error_t_defined */

/* Defined to string_char_traits on gcc 2.95 */
/* #undef char_traits */

/* Define to a type to use for 'error_t' if it is not otherwise available. */
/* #undef error_t */

/* Define to `int' if <sys/types.h> doesn't define. */
/* #undef gid_t */

/* Second argument of iconv() */
#define iconv_arg2_t char **

/* Define to `unsigned' if <sys/types.h> does not define. */
/* #undef ino_t */

/* Define to `off_t' if <sys/types.h> does not define. */
/* #undef loff_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef mode_t */

/* Define to `long int' if <sys/types.h> does not define. */
/* #undef off_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to `int' if neither <sys/types.h> nor <sys/socket.h> define. */
/* #undef socklen_t */

/* Define to `int' if <sys/types.h> doesn't define. */
/* #undef uid_t */

/* Define as `fork' if `vfork' does not work. */
/* #undef vfork */
