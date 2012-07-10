#ifndef CVS__H
#define CVS__H
/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 *
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS kit.
 */

/*
 * basic information used in all source files
 *
 */

#ifdef _WIN32
#define MAIN_CVS
#include "../windows-NT/config.h"
#else
#ifdef HAVE_CONFIG_H
/* this is stuff found via autoconf */
#include "config.h"
#endif /* HAVE_CONFIG_H */
#endif
#ifndef HAVE_PTRDIFF_T
typedef long ptrdiff_t;
#endif

#include "options.h"		/* these are some larger questions which
				   can't easily be automatically checked
				   for */

/* We link with cvsapi and cvstools*/
#include <cvstools.h>

#define	PTR	void *

#include <stdio.h>

/* Under OS/2, <stdio.h> doesn't define popen()/pclose(). */
#ifdef USE_OWN_POPEN
#include "popen.h"
#endif

#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include <assert.h>

#if defined(SERVER_SUPPORT) && !defined(_WIN32)
/* If the system doesn't provide strerror, it won't be declared in
   string.h.  */
char *strerror(int);
#endif

#include <fnmatch.h> /* This is supposed to be available on Posix systems */

#include <ctype.h>
#include <pwd.h>
#include <signal.h>

#ifdef HAVE_ERRNO_H
#include <errno.h>
#else
#ifndef errno
extern int errno;
#endif /* !errno */
#endif /* HAVE_ERRNO_H */

#include "system.h"

#include "hash.h"
#include "client.h"

#ifdef MY_NDBM
#include "myndbm.h"
#else
#include <ndbm.h>
#endif /* MY_NDBM */

#include <stdarg.h>

#include "getopt1.h"
#include "wait.h"
#include "../cvsapi/lib/getdate.h"

/* User variables */
typedef std::map<cvs::string,cvs::string> variable_list_t;

#include "rcs.h"

/* This actually gets set in system.h.  Note that the _ONLY_ reason for
   this is if various system calls (getwd, getcwd, readlink) require/want
   us to use it.  All other parts of CVS allocate pathname buffers
   dynamically, and we want to keep it that way.  */
#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define	PATH_MAX MAXPATHLEN+2
#else
#define	PATH_MAX 1024+2
#endif
#endif /* PATH_MAX */

/* Definitions for the CVS Administrative directory and the files it contains.
   Here as #define's to make changing the names a simple task.  */

#define	CVSADM		"CVS"
#define CVSDUMMY	"##CVSDUMMY"
#define	CVSADM_ENT	"CVS/Entries"
#define	CVSADM_ENTEXT	"CVS/Entries.Extra"
#define	CVSADM_ENTEXTBAK	"CVS/Entries.Extra.Backup"
#define	CVSADM_ENTOLD	"CVS/Entries.Old"
#define	CVSADM_ENTEXTOLD	"CVS/Entries.Extra.Old"
#define	CVSADM_ENTBAK	"CVS/Entries.Backup"
#define CVSADM_ENTLOG	"CVS/Entries.Log"
#define CVSADM_ENTEXTLOG	"CVS/Entries.Extra.Log"
#define	CVSADM_ENTSTAT	"CVS/Entries.Static"
#define CVSADM_ENTSTATUS "CVS/Entries.Status"
#define	CVSADM_REP	"CVS/Repository"
#define	CVSADM_ROOT	"CVS/Root"
#define	CVSADM_TAG	"CVS/Tag"
#define CVSADM_NOTIFY	"CVS/Notify"
#define CVSADM_NOTIFYTMP "CVS/Notify.tmp"
/* A directory in which we store base versions of files we currently are
   editing with "cvs edit".  */
#define CVSADM_BASE     "CVS/Base"
/* File which contains the template for use in log messages.  */
#define CVSADM_TEMPLATE "CVS/Template"
#define CVSADM_RENAME	"CVS/Rename"
#define CVSADM_VIRTREPOS "CVS/Repository.Virtual"

/* This is the special directory which we use to store various extra
   per-directory information in the repository.  It must be the same as
   CVSADM to avoid creating a new reserved directory name which users cannot
   use, but is a separate #define because if anyone changes it (which I don't
   recommend), one needs to deal with old, unconverted, repositories.

   See fileattr.h for details about file attributes, the only thing stored
   in CVSREP currently.  */
#define CVSREP "CVS"

/*
 * Definitions for the CVSROOT Administrative directory and the files it
 * contains.  This directory is created as a sub-directory of the $CVSROOT
 * environment variable, and holds global administration information for the
 * entire source repository beginning at $CVSROOT.
 */
#define	CVSROOTADM		"CVSROOT"
#define	CVSROOTADM_MODULES	"modules"
#define	CVSROOTADM_MODULES2	"modules2"
#define	CVSROOTADM_LOGINFO	"loginfo"
#define	CVSROOTADM_RCSINFO	"rcsinfo"
#define	CVSROOTADM_KEYWORDS	"keywords"
#define CVSROOTADM_COMMITINFO	"commitinfo"
#define CVSROOTADM_TAGINFO      "taginfo"
#define CVSROOTADM_VERIFYMSG    "verifymsg"
#define	CVSROOTADM_HISTORY	"history"
#define CVSROOTADM_VALTAGS	"val-tags"
#define	CVSROOTADM_IGNORE	"cvsignore"
#define	CVSROOTADM_CHECKOUTLIST "checkoutlist"
#define CVSROOTADM_WRAPPER	"cvswrappers"
#define CVSROOTADM_NOTIFY	"notify"
#define CVSROOTADM_USERS	"users"
#define CVSROOTADM_READERS	"readers"
#define CVSROOTADM_WRITERS	"writers"
#define CVSROOTADM_PASSWD	"passwd"
#define CVSROOTADM_GROUP	"group"
#define CVSROOTADM_CONFIG	"config"
#define CVSROOTADM_POSTCOMMIT "postcommit"
#define CVSROOTADM_PRECOMMAND "precommand"
#define CVSROOTADM_POSTCOMMAND "postcommand"
#define CVSROOTADM_PREMODULE "premodule"
#define CVSROOTADM_POSTMODULE "postmodule"
#define CVSROOTADM_HISTORYINFO "historyinfo"
#define CVSROOTADM_CVSRC	"cvsrc"
#define CVSROOTADM_TRIGGERS	"triggers"

#define CVSNULLREPOS		"Emptydir"	/* an empty directory */

/* Other CVS file names */

/* Files go in the attic if the head main branch revision is dead,
   otherwise they go in the regular repository directories.  The whole
   concept of having an attic is sort of a relic from before death
   support but on the other hand, it probably does help the speed of
   some operations (such as main branch checkouts and updates).  */
#define	CVSATTIC	"Attic"

/* Repository rename/move tracking history file */
#define RCSREPOVERSION ".directory_history"

#define	CVSLCK		"#cvs.lock"
#define	CVSRFL		"#cvs.rfl"
#define	CVSWFL		"#cvs.wfl"
#define CVSRFLPAT	"#cvs.rfl.*"	/* wildcard expr to match read locks */
#define	CVSEXT_LOG	",t"
#define	CVSPREFIX	",,"
#define CVSDOTIGNORE	".cvsignore"
#define CVSDOTWRAPPER   ".cvswrappers"

/* Command attributes -- see function lookup_command_attribute(). */
#define CVS_CMD_IGNORE_ADMROOT        1

/* Set if CVS needs to create a CVS/Root file upon completion of this
   command.  The name may be slightly confusing, because the flag
   isn't really as general purpose as it seems (it is not set for cvs
   release).  */

#define CVS_CMD_USES_WORK_DIR         2

#define CVS_CMD_MODIFIES_REPOSITORY   4

#define CVS_CMD_NO_CONNECT			  8 /* Supress initial start_server() */

/* Set for commands that don't operate on a repository therefore
   don't need a root specified */
#define CVS_CMD_NO_ROOT_NEEDED		  16

/* Set for commands that can work without a root */
#define CVS_CMD_OPTIONAL_ROOT		  32

/* Set for commands that need locks, but might not need a root (RCS commands) */
#define CVS_CMD_LOCKSERVER			  64

/* miscellaneous CVS defines */

/* This is the string which is at the start of the non-log-message lines
   that we put up for the user when they edit the log message.  */
#define	CVSEDITPREFIX	"CVS: "
/* Number of characters in CVSEDITPREFIX to compare when deciding to strip
   off those lines.  We don't check for the space, to accomodate users who
   have editors which strip trailing spaces.  */
#define CVSEDITPREFIXLEN 4

#define	CVSLCKAGE	(60*60)		/* 1-hour old lock files cleaned up */
#define	CVSLCKSLEEP	15		/* wait 15 seconds before retrying */
#define	CVSBRANCH	"1.1.1"		/* RCS branch used for vendor srcs */

#define	BAKPREFIX	".#"		/* when rcsmerge'ing */
#ifndef DEVNULL
#define	DEVNULL		"/dev/null"
#endif

/*
 * Special tags. -rHEAD	refers to the head of an RCS file, regardless of any
 * sticky tags. -rBASE	refers to the current revision the user has checked
 * out This mimics the behaviour of RCS.
 */
#define	TAG_HEAD	"HEAD"
#define	TAG_BASE	"BASE"

/* Environment variable used by CVS */
#define	CVSREAD_ENV	"CVSREAD"	/* make files read-only */
#define	CVSREAD_DFLT	0		/* writable files by default */

#ifndef TMPDIR_ENV
#define	TMPDIR_ENV	"TMPDIR" 	/* Temporary directory */
#endif
/* #define	TMPDIR_DFLT		   Set by options.h */

#define	EDITOR1_ENV	"CVSEDITOR"	/* which editor to use */
#define	EDITOR2_ENV	"VISUAL"	/* which editor to use */
#define	EDITOR3_ENV	"EDITOR"	/* which editor to use */
/* #define	EDITOR_DFLT		   Set by options.h */

#define	CVSROOT_ENV	"CVSROOT"	/* source directory root */
#define	CVSROOT_DFLT	NULL		/* No dflt; must set for checkout */

#define	IGNORE_ENV	"CVSIGNORE"	/* More files to ignore */
#define WRAPPER_ENV     "CVSWRAPPERS"   /* name of the wrapper file */

#define	CVSUMASK_ENV	"CVSUMASK"	/* Effective umask for repository */
/* #define	CVSUMASK_DFLT		   Set by options.h */

#define CVSLIB_ENV	"CVSLIB"	/* Location of protocol libraries */
#define CVSCONF_ENV	"CVSCONF"	/* Location of configuration files */

/*
 * If the beginning of the Repository matches the following string, strip it
 * so that the output to the logfile does not contain a full pathname.
 *
 * If the CVSROOT environment variable is set, it overrides this define.
 */
#define	REPOS_STRIP	"/master/"

/* Large enough to hold DATEFORM.  Not an arbitrary limit as long as
   it is used for that purpose, and not to hold a string from the
   command line, the client, etc.  */
#define MAXDATELEN	50

/* The type of an entnode.  */
enum ent_type
{
    ENT_FILE, ENT_SUBDIR
};

/* structure of a entry record */
struct entnode
{
    enum ent_type type;
    char *user;
    char *version;

    /* Timestamp, or "" if none (never NULL).  */
    char *timestamp;

    /* Keyword expansion options, or "" if none (never NULL).  */
    char *options;

    char *tag;
    char *date;
    char *conflict;

	/* Data in Entries.Extra or "" if none */
	char *merge_from_tag_1;
	char *merge_from_tag_2;

	/* Data associated with current edit */
	char *edit_revision;
	char *edit_tag;
	char *edit_bugid;

    /* Last commit date from RCS file */
	time_t rcs_timestamp;

	/* Checksum from last checkout, if supplied */
	char *md5;
};
typedef struct entnode Entnode;

/* The type of request that is being done in do_module() */
enum mtype
{
    CHECKOUT, TAG, PATCH, EXPORT, mtUPDATE, mtCHECKIN, MISC
};

/*
 * structure used for list-private storage by Entries_Open() and
 * Version_TS() and Find_Directories().
 */
struct stickydirtag
{
    /* These fields pass sticky tag information from Entries_Open() to
       Version_TS().  */
    int aflag;
    char *tag;
    char *date;
    int nonbranch;

    /* This field is set by Entries_Open() if there was subdirectory
       information; Find_Directories() uses it to see whether it needs
       to scan the directory itself.  */
    int subdirs;
};

/* Flags for find_{names,dirs} routines */
#define W_LOCAL			0x01	/* look for files locally */
#define W_REPOS			0x02	/* look for files in the repository */
#define W_QUIET			0x04    /* Don't complain about permission errors */
#define W_FAKE			0x08	/* Fake directory search for -n */

/* Flags for return values of direnter procs for the recursion processor */
enum direnter_type
{
    R_PROCESS = 1,			/* process files and maybe dirs */
    R_SKIP_FILES,			/* don't process files in this dir */
    R_SKIP_DIRS,			/* don't process sub-dirs */
    R_SKIP_ALL,				/* don't process files or dirs */
	R_ERROR					/* An error occurred */
};
#ifdef ENUMS_CAN_BE_TROUBLE
typedef int Dtype;
#else
typedef enum direnter_type Dtype;
#endif

extern const char *program_name, *command_name, *server_command_name;
extern const char *Tmpdir, *Editor;
extern int cvsadmin_root;
extern char *CurDir;
extern int really_quiet, quiet;
extern int use_editor;
extern int cvswrite;
extern int atomic_commits;
extern mode_t cvsumask;
extern int read_only_server;

/* This global variable holds the global -d option.  It is NULL if -d
   was not used, which means that we must get the CVSroot information
   from the CVSROOT environment variable or from a CVS/Root file.  */
extern char *CVSroot_cmdline;

int perms_close();
bool verify_admin();
int verify_read(const char *dir, const char *file, const char *tag, const char **message, const char **type_message);
int verify_write(const char *dir, const char *file, const char *tag, const char **message, const char **type_message);
int verify_create(const char *dir, const char *file, const char *tag, const char **message, const char **type_message);
int verify_tag(const char *dir, const char *file, const char *tag, const char **message, const char **type_message);
int verify_control(const char *dir, const char *file, const char *tag, const char **message, const char **type_message);
int verify_merge (const char *dir, const char *file, const char *tag, const char *merge, const char **message);
int change_owner(const char *user);

/* These variables keep track of all of the CVSROOT directories that
   have been seen by the client and the current one of those selected.  */
extern List *root_directories;
extern cvsroot *current_parsed_root;

extern char *emptydir_name();
extern int safe_location();

extern int trace;		/* Level of tracing */
extern int noexec;		/* Don't modify disk anywhere */
extern int logoff;		/* Don't write history entry */

extern const char *trace_file;
extern FILE *trace_file_fp;

extern int top_level_admin;
extern int acl_mode;

/* List of responses to expand-modules command */
extern std::vector<cvs::filename> client_module_expansion;

extern List *dirs_sent_to_server; /* used to decide which "Argument
				     xxx" commands to send to each
				     server in multiroot mode. */
extern char hostname[];

/* Externs that are included directly in the CVS sources */

int RCS_merge(RCSNode *rcs, const char *path, const char *workfile, const char *options, const char *rev1, const char *rev2, int conflict_3way, mode_t *mode);
/* Flags used by RCS_* functions.  See the description of the individual
   functions for which flags mean what for each function.  */
#define RCS_FLAGS_FORCE 1
#define RCS_FLAGS_DEAD 2
#define RCS_FLAGS_QUIET 4
#define RCS_FLAGS_MODTIME 8
#define RCS_FLAGS_KEEPFILE 16

int RCS_exec_rcsdiff (RCSNode *rcsfile, const char *opts, const char *options, const char *rev1,
                      const char *rev2, const char *label1, const char *label2, const char *workfile);
int diff_exec (const char *file1, const char *file2, const char *label1, const char *label2, const char *options, const char *out);
int diff_execv (const char *file1, const char *file2, const char *label1, const char *label2, const char *options, const char *out);

#include "error.h"

DBM *open_module(void);
FILE *open_file(const char *, const char *);
List *Find_Directories(char *repository, int which, List *entries, const char *virtual_repository);
void Entries_Close(List *entries);
void Entries_Close_Dir(List *entries, const char *dir);
List *Entries_Open(int aflag, const char *update_dir);
List *Entries_Open_Dir(int aflag, const char *dir, const char *update_dir);
void Subdirs_Known(List *entries);
void Subdir_Register(List *, const char *, const char *);
void Subdir_Deregister(List *, const char *, const char *);

char *Make_Date (char *rawdate);
char *date_from_time_t(time_t);
void date_to_internet (char *dest, const char *source);
void date_to_rcsdiff (char *dest, const char *source);
int date_to_tm(struct tm *, const char *);
void tm_to_internet(char *, const struct tm *);
void tm_to_rcsdiff(char *, const struct tm *);

char *Name_Repository(const char *dir, const char *update_dir);
const char *Short_Repository(const char *repository);
void Sanitize_Repository_Name(char **repository);

char *Name_Root (const char *dir, const char *update_dir);
void free_cvsroot_t(cvsroot *root_in);
cvsroot *parse_cvsroot (const char *root_in);
cvsroot *new_cvsroot_t();
cvsroot *local_cvsroot (const char *dir, const char *real_dir, bool readwrite, RootType type, const char *remote_serv, const char *remote_repos, const char *proxy_repos, const char *remote_pass);
void Create_Root(const char *dir, const char *rootdir);

typedef struct
{
	cvs::filename root;
	cvs::filename name;
	bool online;
	bool readwrite;
	bool proxypasswd;
	RootType repotype;
	cvs::string remote_server;
	cvs::string remote_repository;
	cvs::string proxy_repository;
	cvs::string remote_passphrase;
} root_allow_struct;

void root_allow_add(const char *root, const char *name, bool online, bool readwrite, bool proxypasswd, RootType type, const char *remote_serv, const char *remote_repos, const char *proxy_repos, const char *remote_pass);
void root_allow_free();
bool root_allow_ok (const char *root, const root_allow_struct*& found_root, const char *&real_root, bool authuse);

char *normalize_path(char *path);

char *gca (const char *rev1, const char *rev2);
void check_numeric (const char *rev, int argc, char **argv);
const char *getcaller();
char *time_stamp(const char *file, int local);

void expand_string(char **, size_t *, size_t);
void allocate_and_strcat(char **, size_t *, const char *);
#ifndef BCHECK
char *xstrdup (const char *str);
#endif
extern "C" void strip_trailing_newlines(); /* In gnulib */
int pathname_levels (const char *path);

extern bool tf_loaded;
typedef	int (*CALLPROC)	(void *params, const trigger_interface *cb);
int run_trigger(void *params, CALLPROC callproc);
int parse_config (const char *cvsroot);

typedef	RETSIGTYPE (*SIGCLEANUPPROC)	();
extern "C" int SIG_register(int	sig, RETSIGTYPE	(*fn)(int)); /* in gnulib */
char *repository_name();
int isdir(const char *dir);
int isfile(const char *file);
int islink (const char *file);
int isdevice (const char *file);
int isreadable (const char *file);
int iswritable (const char *file);
int isaccessible (const char *file, const int mode);
int isabsolute(const char *directory);
int isabsolute_remote(const char *directory);
char *xreadlink(const char *file);
const char *last_component (const char *path);
char *get_homedir();
char *cvs_temp_name();
FILE *cvs_temp_file(char **filename);

int numdots (const char *s);
char *increment_revnum (const char *rev);
int compare_revnums (const char *rev1, const char *rev2);
int unlink_file (const char *f);
int unlink_file_dir (const char *f);
int update(int argc, char**argv);
int chowner(int argc, char **argv);
int chacl (int argc, char **argv);
int lsacl(int argc, char **argv);
int passwd(int argc, char **argv);
int xcmp (const char *file1, const char *file2);
int Create_Admin (const char *dir, const char *update_dir, const char *repository, const char *tag,
				  const char *date, int nonbranch, int warn);
int expand_at_signs (const char *buf, unsigned int size, FILE *fp);

/* Locking subsystem (implemented in lock.c).  */

int Reader_Lock (const char *xrepository);
void Lock_Cleanup();
void Lock_Cleanup_Directory();

/* Writelock an entire subtree, well the part specified by ARGC, ARGV, LOCAL,
   and AFLAG, anyway.  */
void lock_tree_for_write (int argc, char **argv, int local, int which, int aflag);

/* See lock.c for description.  */
void lock_dir_for_write (const char *repository);

/* Send modified report to lockserver */
int do_modified(size_t lockId, const char *newversion, const char *oldversion, const char *branch, char type);
/* Is there a local lockserver running */
int local_lockserver();

/* Single file full lock/unlock */
size_t do_lock_file(const char *file, const char *repository, int write, int wait);
int do_lock_version(size_t lockId, const char *branch, char **version);
int do_unlock_file(size_t lockId);

void lock_register_client(const char *username, const char *root);

/* LockServer settings from CVSROOT/config.  */
extern char *lock_server;

/* LockDir/LockServer settings from CVSROOT/config.  */
extern char *lock_dir;
extern char *lock_server;

void Scratch_Entry (List *list, const char *fname);
void Rename_Entry (List *list, const char *from, const char *to);
void ParseTag (const char **tagp, const char **datep, int *nonbranchp, const char **version);
void ParseTag_Dir(const char *dir, const char **tagp, const char **datep, int *nonbranchp, const char **version);
void WriteTag(const char *dir, const char *tag, const char *date, int nonbranch, const char *update_dir, const char *repository, const char *vers);
void cat_module (int status);
void check_entries();
void close_module (DBM *db);
int copy_file(const char *from, const char *to, int force_overwrite, int must_exist);
int copy_and_zip_file(const char *from, const char *to, int force_overwrite, int must_exist);
int copy_and_unzip_file(const char *from, const char *to, int force_overwrite, int must_exist);
void fperrmsg(FILE *fp, int status, int errnum, char *message, ...);
void free_names (int *pargc, char **argv);

int ign_name (const char *name);
void ign_add(const char *ign, int hold);
void ign_add_file(const char *file, int hold);
void ign_setup();
int ign_close();
void ign_dir_add (const char *name);
int ignore_directory (const char *name);
void ign_send ();
void ign_display();

typedef void (*Ignore_proc)(char *, char *);
void ignore_files (List *ilist, List *entries, char *update_dir, Ignore_proc proc);

#include "update.h"

void line2argv (int *pargc, char ***argv, const char *line, const char *sepchars);
void make_directories(const char *name);
void make_directory(const char *name);
int mkdir_if_needed (const char *name);
void rename_file (const char *from, const char *to);
/* Expand wildcards in each element of (ARGC,ARGV).  This is according to the
   files which exist in the current directory, and accordingly to OS-specific
   conventions regarding wildcard syntax.  It might be desirable to change the
   former in the future (e.g. "cvs status *.h" including files which don't exist
   in the working directory).  The result is placed in *PARGC and *PARGV;
   the *PARGV array itself and all the strings it contains are newly
   malloc'd.  It is OK to call it with PARGC == &ARGC or PARGV == &ARGV.  */
void expand_wild (int argc, char **argv, int *pargc, char ***pargv);

void strip_trailing_slashes (char *path);
void update_delproc (Node *p);
void usage (const char *const cpp[]);
mode_t modify_mode(mode_t mode, mode_t add, mode_t remove);
void xchmod (const char *fname, int writable);
char *xgetwd(); 
List *Find_Names (const char *repository, int which, int aflag, List **optentries, const char *virtual_repository);
Node *Fast_Register (List *list, const char *fname, const char *vn,  const char *ts, const char *options, const char *tag,
    const char *date, const char *ts_conflict, const char *merge_from_tag_1,
	const char *merge_from_tag_2, time_t rcs_timestamp, 
	const char *edit_revision, const char *edit_tag, const char *edit_bugid, const char *md5);
void Register (List *list, const char *fname, const char *vn,  const char *ts, const char *options, const char *tag,
    const char *date, const char *ts_conflict, const char *merge_from_tag_1,
	const char *merge_from_tag_2, time_t rcs_timestamp,
	const char *edit_revision, const char *edit_tag, const char *edit_bugid, const char *md5);
void Update_Logfile (const char *repository, const char *xmessage, FILE *xlogfp, List *xchanges, const char *xbugid);
void do_editor (const char *dir, char **messagep, const char *repository, List *changes);

extern int reread_log_after_verify; /* Value of RereadLogAfterVerify */

int do_verify(char **message, const char *repository);

typedef	int (*CALLBACKPROC)	(int argc, char *argv[], const char *where,
	const char *mwhere, const char *mfile, int shorten, int local_specified,
	const char *omodule, const char *msg);

/* This is the structure that the recursion processor passes to the
   fileproc to tell it about a particular file.  */
struct file_info
{
    /* Name of the file, without any directory component.  */
    const char *file;

	/* Real RCS path of the file */
	const char *mapped_file;

    /* Name of the directory we are in, relative to the directory in
       which this command was issued.  We have cd'd to this directory
       (either in the working directory or in the repository, depending
       on which sort of recursion we are doing).  If we are in the directory
       in which the command was issued, this is "".  */
    const char *update_dir;

    /* update_dir and file put together, with a slash between them as
       necessary.  This is the proper way to refer to the file in user
       messages.  */
    const char *fullname;

    /* Name of the directory corresponding to the repository which contains
       this file.  */
    const char *repository;

	/* Virtual (client side) name of the directory which contains this file */
	const char *virtual_repository;

    /* The pre-parsed entries for this directory.  */
    List *entries;

    RCSNode *rcs;
};

typedef	int (*FILEPROC)(void *callerdat, struct file_info *finfo);
typedef	int (*FILESDONEPROC) (void *callerdat, int err,
				     char *repository, char *update_dir,
				     List *entries);
typedef	int (*PREDIRENTPROC) (void *callerdat, char *dir,
				    char *repos, char *update_dir,
				    List *entries, const char *virtual_repository, Dtype hint);
typedef	Dtype (*DIRENTPROC) (void *callerdat, char *dir,
				    char *repos, char *update_dir,
				    List *entries, const char *virtual_repository, Dtype hint);
typedef	int (*DIRLEAVEPROC)(void *callerdat, char *dir, int err,
				    char *update_dir, List *entries);
typedef int (*PERMPROC)(const char *dir, const char *name, const char *tag, const char **msg, const char **verif_msg);

int mkmodules (char *dir);
int init(int argc, char **argv);

int do_module (DBM *db, const char *mname, enum mtype m_type,  const char *msg, CALLBACKPROC callback_proc,
    const char *where, int shorten, int local_specified, int run_module_prog, int build_dirs,
    const char *extra_arg);
void history_write (int type, const char *update_dir, const char *revs, const char *name, const char *repository, const char *bugid, const char *message);
int start_recursion(FILEPROC fileproc, FILESDONEPROC filesdoneproc, PREDIRENTPROC predirentproc, DIRENTPROC direntproc, DIRLEAVEPROC dirleaveproc, void *callerdat, int argc, char **argv, int local, int which, int aflag, int readlock, const char *update_preload, const char *update_repository, int dosrcs, PERMPROC permproc, const char *tag);
void read_cvsrc (int *argc, char ***argv, const char *cmdname);
void reset_saved_cvsrc();
int close_cvsrc();

char *make_message_rcslegal (const char *message);
int file_has_markers (const struct file_info *finfo);
extern void get_file (const char *, const char *, const char *,
			     char **, size_t *, size_t *, kflag flags);
char *shell_escape(char **buf, const char *str);
char *shell_escape_backslash(char **buf, const char *str);
char *backup_file (const char *filename, const char *suffix);
void resolve_symlink (char **filename);
void sleep_past (time_t desttime);

void run_arg(const char *s);
void run_print (FILE *fp);
void run_setup(const char *prog);
int run_exec(bool bShow);

#define RUN_TTY ((const char *)0)


/*
 * a struct vers_ts contains all the information about a file including the
 * user and rcs file names, and the version checked out and the head.
 *
 * this is usually obtained from a call to Version_TS which takes a
 * tag argument for the RCS file if desired
 */
struct vers_ts
{
    /* rcs version user file derives from, from CVS/Entries.
       It can have the following special values:

       NULL = file is not mentioned in Entries (this is also used for a
	      directory).
       "" = ILLEGAL!  The comment used to say that it meant "no user file"
	    but as far as I know CVS didn't actually use it that way.
	    Note that according to cvs.texinfo, "" is not legal in the
	    Entries file.
       0 = user file is new
       -vers = user file to be removed.  */
    char *vn_user;

    /* Numeric revision number corresponding to ->vn_tag (->vn_tag
       will often be symbolic).  */
    char *vn_rcs;
    /* If ->tag is a simple tag in the RCS file--a tag which really
       exists which is not a magic revision--and if ->date is NULL,
       then this is a copy of ->tag.  Otherwise, it is a copy of
       ->vn_rcs.  */
    char *vn_tag;

	/* The edit data from the entries.extra, if available */
	char *edit_revision;
	char *edit_tag;
	char *edit_bugid;

    /* This is the timestamp from stating the file in the working directory.
       It is NULL if there is no file in the working directory.  It is
       "Is-modified" if we know the file is modified but don't have its
       contents.  */
    char *ts_user;

    /* Timestamp from CVS/Entries.  For the server, ts_user and ts_rcs
       are computed in a slightly different way, but the fact remains that
       if they are equal the file in the working directory is unmodified
       and if they differ it is modified.  */
    char *ts_rcs;

    /* Timestamp of last checkin stored in CVS/Entries.Extra */
	time_t tt_rcs;

	/* Options from CVS/Entries (keyword expansion), malloc'd.  If none,
       then it is an empty string (never NULL).  */
    char *options;

    /* If non-NULL, there was a conflict (or merely a merge?  See merge_file)
       and the time stamp in this field is the time stamp of the working
       directory file which was created with the conflict markers in it.
       This is from CVS/Entries.  */
    char *ts_conflict;

    /* Tag specified on the command line, or if none, tag stored in
       CVS/Entries.  */
    char *tag;
    /* Date specified on the command line, or if none, date stored in
       CVS/Entries.  */
    char *date;
    /* If this is 1, then tag is not a branch tag.  If this is 0, then
       tag may or may not be a branch tag.  */
    int nonbranch;

	/* If non-NULL, gives the correct filename for this version */
	char *filename;

    /* Pointer to entries file node  */
    Entnode *entdata;

    /* Pointer to parsed src file info */
    RCSNode *srcfile;

	/* Global and local properties */
	List *prop_global;
	List *prop_local;
};
typedef struct vers_ts Vers_TS;

Vers_TS *Version_TS (struct file_info *finfo, const char *options, const char *tag,
    const char *date, int force_tag_match, int set_time, int force_case_match);
void freevers_ts(Vers_TS **versp);
void assign_options(char **existing_options, const char *options);

/* Miscellaneous CVS infrastructure which layers on top of the recursion
   processor (for example, needs struct file_info).  */

int Checkin (int type, struct file_info *finfo, char *rcs, char *rev, char *tag,
    char *options, char *message, const char *merge_from_tag1, const char *merge_from_tag2,
	RCSCHECKINPROC callback, const char *bugid, const char *edit_revision, const char *edit_tag,
	const char *edit_bugid);
int No_Difference (struct file_info *finfo, Vers_TS *vers, int force_nodiff, int ignore_keywords);
/* TODO: can the finfo argument to special_file_mismatch be changed? -twp */
int special_file_mismatch (struct file_info *finfo, const char *rev1, const char *rev2);

/*
 * defines for Classify_File() to determine the current state of a file.
 * These are also used as types in the data field for the list we make for
 * Update_Logfile in commit, import, and add.
 */
enum classify_type
{
    T_UNKNOWN = 1,			/* no old-style analog existed	 */
    T_CONFLICT,				/* C (conflict) list		 */
    T_NEEDS_MERGE,			/* G (needs merging) list	 */
    T_MODIFIED,				/* M (needs checked in) list */
    T_CHECKOUT,				/* O (needs checkout) list	 */
    T_ADDED,				/* A (added file) list		 */
    T_REMOVED,				/* R (removed file) list	 */
    T_REMOVE_ENTRY,			/* W (removed entry) list	 */
    T_UPTODATE,				/* File is up-to-date		 */
    T_PATCH,				/* P Like C, but can patch	 */
    T_TITLE					/* title for node type 		 */
};
typedef enum classify_type Ctype;

Ctype Classify_File (struct file_info *finfo, const char *tag, const char *date,
    const char *options, int force_tag_match, int aflag, Vers_TS **versp,
    int pipeout, int force_time_mismatch, int ignore_keywords);

/*
 * structure used for list nodes passed to Update_Logfile() and
 * do_editor().
 */
struct logfile_info
{
  enum classify_type type;
  char *tag;
  char *bugid;
  char *rev_old;		/* rev number before a commit/modify,
				   NULL for add or import */
  char *rev_new;		/* rev number after a commit/modify,
				   add, or import, NULL for remove */
};

/* Wrappers.  */

typedef enum {
    /* -k wrapper option.  Default keyword expansion options.  */
    WRAP_RCSOPTION,
	WRAP_XDIFFWRAPPER
} WrapMergeHas;

void  wrap_setup();
void wrap_close();
bool wrap_name_has (const char *name,WrapMergeHas has);
char *wrap_rcsoption(const char *filename);
const char *wrap_xdiffwrapper(const char *filename);
char *wrap_tocvs_process_file(const char *fileName);
bool wrap_merge_is_copy (const char *fileName);
void wrap_fromcvs_process_file(const char *fileName);
void wrap_add_file (const char *file, bool temp);
bool wrap_add(const char *line, bool isTemp, bool isRemote, bool Sort, bool fromCommand);
void wrap_send();
bool wrap_unparse_rcs_options (cvs::string& line, bool& first_call);
void wrap_display();

/* Pathname expansion */
char *expand_path (const char *name, const char *file, int line);

/* User variables.  */
extern variable_list_t variable_list;

void variable_set (const char *nameval);
const char *variable_get(const char *nameval);

int watch(int argc, char **argv);
int edit(int argc, char **argv);
int unedit(int argc, char **argv);
int editors(int argc, char **argv);
int watchers(int argc, char **argv);
int check_can_edit(const char *repository, const char *filename, const char *who, const char *tag);

/* Global watcher */
extern const char *global_watcher;

/* Set to 1 to to stop automatic unedit */
extern int commit_keep_edits;

bool bugid_in(const char *bugid, const char *bugs);


int annotate(int argc, char **argv);
int add(int argc, char **argv);
int admin(int argc, char **argv);
int checkout(int argc, char **argv);
int commit(int argc, char **argv);
int diff(int argc, char **argv);
int history(int argc, char **argv);
int import(int argc, char **argv);
int cvslog(int argc, char **argv);
int login(int argc, char **argv);
int logout(int argc, char **argv);
int patch(int argc, char **argv);
int release(int argc, char **argv);
int cvsremove(int argc, char **argv);
int cvsrename(int argc, char **argv);
int rtag(int argc, char **argv);
int cvsstatus(int argc, char **argv);
int cvstag(int argc, char **argv);
int version(int argc, char **argv);
int ls(int argc, char **argv);
int info(int argc, char **argv);
int cvsrcs(int argc, char **argv);
int xdiff(int argc, char **argv);
int cvsswitch(int argc, char **argv);

unsigned long int lookup_command_attribute (const char *cmd_name);

extern const struct protocol_interface *client_protocol;
extern const struct protocol_interface *server_protocol;

char *normalize_cvsroot (const cvsroot *root);

void tag_check_valid (const char *name, int argc, char **argv, int local, int aflag, const char *repository);
void tag_check_valid_join (const char *join_tag, int argc, char **argv, int local, int aflag, const char *repository);
#include "server.h"

/* From server.c and documented there.  */
int cvs_output(const char *, size_t);
int cvs_output_binary(char *, size_t);
int cvs_outerr(const char *, size_t);
void cvs_flusherr();
void cvs_flushout();
void cvs_output_tagged (const char *tag, const char *text);
void server_error_exit();

#ifndef O_BINARY
#define O_BINARY 0
#endif

/* Passwd stuff */
typedef struct
{
	char *username;
	char *password;
	char *real_username;
} passwd_entry;

int read_passwd_list();
int write_passwd_list();
passwd_entry *find_passwd_entry();
int does_passwd_user_exist();
passwd_entry *new_passwd_entry();
void init_passwd_list();
void free_passwd_list();

char *cvs_strtok(char *buffer, const char *tokens);
const char *fn_root(const char *path);
char read_key();
int yesno_prompt(const char *message, const char *title, int withcancel);
char *fullpathname(const char *name, const char **shortname);
char *find_rcs_filename(const char *path);
int case_isfile(const char *name, char **realname);
int get_local_time_offset();
char *xgetwd_mapped();

#ifndef _UNICODE
#define uc_name LPCSTR
#else
class uc_name
{
public:
	uc_name(const char *file);
	~uc_name();
	operator const wchar_t*() { if(_fnp) return _fnp; else return _fn; }
	wchar_t* buf() { if(_fnp) return _fnp; else return _fn; }
private:
	wchar_t _fn[32];
	wchar_t *_fnp;
};
#endif

#ifdef _WIN32
void _dosmaperr(unsigned long dwErr); /* Actually DWORD */
#endif

#if defined(SERVER_SUPPORT) 
/* stdio redirection, the easy way... */
#include <stdio.h>
#define printf cvs_printf
#define putchar cvs_putchar
#define puts cvs_puts
#define fputs cvs_fputs
#define fprintf cvs_fprintf
#define vfprintf cvs_vfprintf
void cvs_printf(const char *fmt, ...);
void cvs_putchar(int c);
void cvs_puts(const char *s);
int cvs_fputs(const char *s, FILE *f);
int cvs_fprintf(FILE *f, const char *fmt, ...);
int cvs_vfprintf(FILE *f, const char *fmt, va_list va);
#endif

/* Certain types of communication input and output data in packets,
   where each packet is translated in some fashion.  The packetizing
   buffer type supports that, given a buffer which handles lower level
   I/O and a routine to translate the data in a packet.

   This code uses two bytes for the size of a packet, so packets are
   restricted to 65536 bytes in total.

   The translation functions should just translate; they may not
   significantly increase or decrease the amount of data.  The actual
   size of the initial data is part of the translated data.  The
   output translation routine may add up to PACKET_SLOP additional
   bytes, and the input translation routine should shrink the data
   correspondingly.  */

/* The protocol wrapper in the worst case (empty file full of linefeeds) can
   add 'M ' after every single byte, tripling the output size */
#define PACKET_SLOP (BUFFER_DATA_SIZE*2)

#ifdef HAVE_PUTENV
	#ifdef _WIN32
		#define cvs_putenv wnt_putenv /* Win32 has its own version of this */
	#else
		void cvs_putenv(const char *variable, const char *value);
	#endif /* Win32 */
#endif /* Have_Putenv */

// Used for connection to lock server
int cvs_tcp_connect(const char *servername, const char *port, int supress_errors);
int cvs_tcp_close(int sock);

/* Hostname of peer, if known */
extern char *remote_host_name;

/* contact agent to get local passwd cache */
int get_cached_password(const char *key, char *buffer, int buffer_len);

/* Return the relative directory for an absolute logical path */
const char *relative_repos(const char *directory);

void cvs_trace(int level, const char *fmt,...);
#define TRACE cvs_trace
#ifdef	sun
/* solaris has a poor implementation of vsnprintf() which is not able to handle null pointers for %s */
# define PATCH_NULL(x) ((x)?(x):"<NULL>")
#else
# define PATCH_NULL(x) x
#endif

#define GLOBAL_SESSION_ID_LENGTH 64
extern char global_session_id[GLOBAL_SESSION_ID_LENGTH];

extern const char *global_session_time;
extern time_t global_session_time_t;

/* from client.c */
int read_line (char **resultp);
size_t try_read_from_server (char *buf, size_t len);

const char *client_where(const char *path);

#ifdef _WIN32
#define MAX_PATH 260
#else
#ifndef MAX_PATH
#ifdef PATH_MAX
#define MAX_PATH PATH_MAX
#elif defined(_MAX_PATH)
#define MAX_PATH _MAX_PATH
#else
#define MAX_PATH 1024
#endif
#endif
#endif

/* Socket for socket i/o otherwise 0 */
extern int server_io_socket;

/* Legacy compatibility */
typedef struct
{
	int return_fake_version;
	int old_checkout_n_behaviour;
	int hide_extended_status;
	int ignore_client_wrappers;
} cvs_compat_t;

extern int compat_level;
extern cvs_compat_t compat[];

/* Location to chroot to after authentication */
extern char *chroot_base;
extern int chroot_done;

/* Force server to a particular user */
extern char *runas_user;

/* Global server trace availablility */
extern int allow_trace;

/* Is client/server locale translation active? */
extern int locale_active;
extern const char *force_locale;
extern int server_stats_enabled;
extern char *server_statistics;
#ifdef _WIN32
extern int force_no_statistics;
#endif

/* Client force mode flag */
extern LineType crlf_mode;

/* atomic checkouts */
extern int atomic_checkouts;

/* Information passed to precommand_proc.  Also called by client in local mode */
struct precommand_args_t
{
	int argc;
	const char **argv;
	const char *command; 
	int retval;
};
int precommand_proc(void *param, const trigger_interface *cb);
int postcommand_proc(void *param, const trigger_interface *cb);

/* Last repository called if any recursion was done */
extern const char *last_repository;

#include "mapping.h"

/* cvs/entries client/server markers.  Were '=','M' and 'D'.  Changed to avoid
   conflicts with date return from CVSNT clients */
#define UNCHANGED_CHAR '='
#define MODIFIED_CHAR '!'
#define DATE_CHAR 'D'
#define UNCHANGED_CHAR_S "="
#define MODIFIED_CHAR_S "!"
#define DATE_CHAR_S "D"

#endif

