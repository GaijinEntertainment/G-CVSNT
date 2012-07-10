/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS kit.  */

#include "cvs.h"
#include "savecwd.h"
#include "getline.h"
#include "fileattr.h"

#ifndef DBLKSIZ
#define	DBLKSIZ	4096			/* since GNU ndbm doesn't define it */
#endif

static int checkout_file (const char *file, const char *directory, const char *temp, mode_t *mode);
static char *make_tempfile ();
static void rename_rcsfile (char *temp, char *real);

#ifndef MY_NDBM
static void rename_dbmfile (char *temp);
static void write_dbmfile (char *temp);
#endif				/* !MY_NDBM */

/* Set to a valid non_root directory to enable remote init commands */
extern const char *remote_init_root;

/* Structure which describes an administrative file.  */
struct admin_file {
   /* Name of the file, within the CVSROOT directory.  */
   char *filename;

   /* This is a one line description of what the file is for.  It is not
      currently used, although one wonders whether it should be, somehow.
      If NULL, then don't process this file in mkmodules (FIXME?: a bit of
      a kludge; probably should replace this with a flags field).  */
   char *errormsg;

   /* Contents which the file should have in a new repository.  To avoid
      problems with brain-dead compilers which choke on long string constants,
      this is a pointer to an array of char * terminated by NULL--each of
      the strings is concatenated.

      If this field is NULL, the file is not created in a new
      repository, but it can be added with "cvs add" (just as if one
      had created the repository with a version of CVS which didn't
      know about the file) and the checked-out copy will be updated
      without having to add it to checkoutlist.  */
   const char * const *contents;
};

static const char *const loginfo_contents[] = {
    "# The \"loginfo\" file controls where \"cvs commit\" log information\n",
    "# is sent.  The first entry on a line is a regular expression which must match\n",
    "# the directory that the change is being made to, relative to the\n",
    "# $CVSROOT.  If a match is found, then the remainder of the line is a filter\n",
    "# program that should expect log information on its standard input.\n",
    "#\n",
    "# If the repository name does not match any of the regular expressions in this\n",
    "# file, the \"DEFAULT\" line is used, if it is specified.\n",
    "#\n",
    "# If the name ALL appears as a regular expression it is always used\n",
    "# in addition to the first matching regex or DEFAULT.\n",
    "#\n",
    "# You may specify a format string as part of the\n",
    "# filter.  The string is composed of a '%' followed\n",
    "# by a single format character, or followed by a set of format\n",
    "# characters surrounded by '{' and '}' as separators.  The format\n",
    "# characters are:\n",
    "#\n",
    "#   s = file name\n",
    "#   V = old version number (pre-checkin)\n",
    "#   v = new version number (post-checkin)\n",
    "#\n",
    NULL
};

static const char *const rcsinfo_contents[] = {
    "# The \"rcsinfo\" file is used to control templates with which the editor\n",
    "# is invoked on commit and import.\n",
    "#\n",
    "# The first entry on a line is a regular expression which is tested\n",
    "# against the directory that the change is being made to, relative to the\n",
    "# $CVSROOT.  For the first match that is found, then the remainder of the\n",
    "# line is the name of the file that contains the template.\n",
    "#\n",
    "# If the repository name does not match any of the regular expressions in this\n",
    "# file, the \"DEFAULT\" line is used, if it is specified.\n",
    NULL
};

static const char *const keywords_contents[] = {
    "# The \"keywords\" file is used to modify the standard set of RCS keywords\n",
	"# or define entirely new ones.\n",
    "#\n",
    "# The first entry on a line is a regular expression which is tested\n",
    "# against the directory that the change is being made to, relative to the\n",
    "# $CVSROOT.  Subsequent lines contain keyword definitions, indented by a space\n",
	"# to separate them from module definitions.\n",
    "#\n",
    "# If the repository name does not match any of the definitions in this\n",
    "# file, the \"ALL\" section is used, if it is specified.\n",
	"#\n",
	"# Any keyword with an empty definition is ignored.  This can be used to selectively\n",
	"# disable individual RCS keywords.\n",
	"#\n",
	"# The default defintions are:\n",
	"#\n",
	"#ALL\n",
	"#  Author    %a\n",
	"#  Date      %d\n",
	"#  Header    %r/%p/%f %v %d %a %s %l\n",
	"#  CVSHeader %p/%f %v %d %a %s %l\n",
	"#  Id        %f %v %d %a %s %l\n",
	"#  Locker    %l\n",
	"#  Log		  %f\n",
	"#  Name      %N\n",
	"#  RCSfile   %f\n",
	"#  Revision  %v\n",
	"#  Source    %r/%p/%f\n",
	"#  State     %s\n",
	"#  CommitId  %C\n",
    NULL
};

static const char *const verifymsg_contents[] = {
    "# The \"verifymsg\" file is used to allow verification of logging\n",
    "# information.  It works best when a template (as specified in the\n",
    "# rcsinfo file) is provided for the logging procedure.  Given a\n",
    "# template with locations for, a bug-id number, a list of people who\n",
    "# reviewed the code before it can be checked in, and an external\n",
    "# process to catalog the differences that were code reviewed, the\n",
    "# following test can be applied to the code:\n",
    "#\n",
    "#   Making sure that the entered bug-id number is correct.\n",
    "#   Validating that the code that was reviewed is indeed the code being\n",
    "#       checked in (using the bug-id number or a seperate review\n",
    "#       number to identify this particular code set.).\n",
    "#\n",
    "# If any of the above test failed, then the commit would be aborted.\n",
    "#\n",
    "# Actions such as mailing a copy of the report to each reviewer are\n",
    "# better handled by an entry in the loginfo file.\n",
    NULL
};

static const char *const commitinfo_contents[] = {
    "# The \"commitinfo\" file is used to control pre-commit checks.\n",
    "# The filter on the right is invoked with the repository name.  A list\n",
    "# of files to check is passed to the standard input of the script.  A non-zero\n",
	"# exit of the filter program will cause the commit to be aborted.\n",
    "#\n",
    "# The first entry on a line is a regular expression which is tested\n",
    "# against the directory that the change is being committed to, relative\n",
    "# to the $CVSROOT.  For the first match that is found, then the remainder\n",
    "# of the line is the name of the filter to run.\n",
    "#\n",
    "# If the repository name does not match any of the regular expressions in this\n",
    "# file, the \"DEFAULT\" line is used, if it is specified.\n",
    "#\n",
    "# If the name \"ALL\" appears as a regular expression it is always used\n",
    "# in addition to the first matching regex or \"DEFAULT\".\n",
    NULL
};

static const char *const taginfo_contents[] = {
    "# The \"taginfo\" file is used to control pre-tag checks.\n",
    "# The filter on the right is invoked with the following arguments:\n",
    "#\n",
    "# $1 -- tagname\n",
    "# $2 -- operation \"add\" for tag, \"mov\" for tag -F, and \"del\" for tag -d\n",
    "# $3 -- repository\n",
	"#\n",
	"# The filter is passed a series of filename/version pairs on its standard input\n",
    "#\n",
    "# A non-zero exit of the filter program will cause the tag to be aborted.\n",
    "#\n",
    "# The first entry on a line is a regular expression which is tested\n",
    "# against the directory that the change is being committed to, relative\n",
    "# to the $CVSROOT.  For the first match that is found, then the remainder\n",
    "# of the line is the name of the filter to run.\n",
    "#\n",
    "# If the repository name does not match any of the regular expressions in this\n",
    "# file, the \"DEFAULT\" line is used, if it is specified.\n",
    "#\n",
    "# If the name \"ALL\" appears as a regular expression it is always used\n",
    "# in addition to the first matching regex or \"DEFAULT\".\n",
    NULL
};

static const char *const checkoutlist_contents[] = {
    "# The \"checkoutlist\" file is used to support additional version controlled\n",
    "# administrative files in $CVSROOT/CVSROOT, such as template files.\n",
    "#\n",
    "# The first entry on a line is a filename which will be checked out from\n",
    "# the corresponding RCS file in the $CVSROOT/CVSROOT directory.\n",
    "# The remainder of the line is an error message to use if the file cannot\n",
    "# be checked out.\n",
    "#\n",
    "# File format:\n",
    "#\n",
    "#	[<whitespace>]<filename><whitespace><error message><end-of-line>\n",
    "#\n",
    "# comment lines begin with '#'\n",
    NULL
};

static const char *const cvswrappers_contents[] = {
    "# This file affects handling of files based on their names.\n",
    "#\n",
    "# The -m option specifies whether CVS attempts to merge files.\n",
    "#\n",
    "# The -k option specifies keyword expansion (e.g. -kb for binary).\n",
    "#\n",
    "# The -t option overrides the default mime type.\n",
    "#\n",
    "# Format of wrapper file ($CVSROOT/CVSROOT/cvswrappers or .cvswrappers)\n",
    "#\n",
    "#  wildcard	[option value][option value]...\n",
    "#\n",
    "#  where option is one of\n",
    "#  -k		expansion mode		value: b, o, kkv, etc.\n",
    "#\n",
    "#  and value is a single-quote delimited value.\n",
    "# For example:\n",
    "#*.gif -kb\n",
    NULL
};

static const char *const notify_contents[] = {
    "# The \"notify\" file controls where notifications from watches set by\n",
    "# \"cvs watch add\" or \"cvs edit\" are sent.  The first entry on a line is\n",
    "# a regular expression which is tested against the directory that the\n",
    "# change is being made to, relative to the $CVSROOT.  If it matches,\n",
    "# then the remainder of the line is a filter program that should contain\n",
    "# one occurrence of %s for the user to notify, and information on its\n",
    "# standard input.\n",
    "#\n",
    "# \"ALL\" or \"DEFAULT\" can be used in place of the regular expression.\n",
    "#\n",
    "# You may specify a format string as part of the\n",
    "# filter.  The format characters are:\n",
    "#\n",
    "#   s = user being notified\n",
    "#   b = Bug identifier\n",
    "#   m = Message supplied on command line\n",
    "#   d = Date of action\n",
    "#   u = User performing the unedit\n",
	"#   t = tag or branch being edited\n",
    "#\n",
    "# For example:\n",
    "#ALL mail %s -s \"CVS notification for bug %b\"\n",
    NULL
};

static const char *const modules_contents[] = {
    "# Three different line formats are valid:\n",
    "#	key	-a    aliases...\n",
    "#	key [options] directory\n",
    "#	key [options] directory files...\n",
    "#\n",
    "# Where \"options\" are composed of:\n",
    "#	-i prog		Run \"prog\" on \"cvs commit\" from top-level of module.\n",
    "#	-o prog		Run \"prog\" on \"cvs checkout\" of module.\n",
    "#	-e prog		Run \"prog\" on \"cvs export\" of module.\n",
    "#	-t prog		Run \"prog\" on \"cvs rtag\" of module.\n",
    "#	-u prog		Run \"prog\" on \"cvs update\" of module.\n",
    "#	-d dir		Place module in directory \"dir\" instead of module name.\n",
    "#	-l		Top-level directory only -- do not recurse.\n",
    "#\n",
    "# NOTE:  If you change any of the \"Run\" options above, you'll have to\n",
    "# release and re-checkout any working directories of these modules.\n",
    "#\n",
    "# And \"directory\" is a path to a directory relative to $CVSROOT.\n",
    "#\n",
    "# The \"-a\" option specifies an alias.  An alias is interpreted as if\n",
    "# everything on the right of the \"-a\" had been typed on the command line.\n",
    "#\n",
    "# You can encode a module within a module by using the special '&'\n",
    "# character to interpose another module into the current module.  This\n",
    "# can be useful for creating a module that consists of many directories\n",
    "# spread out over the entire source repository.\n",
    NULL
};

static const char *const modules2_contents[] = {
	"# *** modules2 currently has 'experimental' status.  Testing is encouraged but.\n",
	"# for greatest stability use the modules file. ***\n",
	"#\n",
    "# This file describes the layout of virtual directory structures\n",
    "# within the repository.\n",
    "#\n",
	"# The layout is similar to a Windows .ini file.  For example:\n",
    "#\n",
    "# [foo]\n",
    "# dir1/dir2/dir3 = realdir1/realdir2\n",
    "# dir1/dir3 = !realdir1/realdir3 (^*js$|^*cpp$)\n",
    "#\n",
    "# [bar]\n",
    "# / = realdir4\n",
	"# dir_to_delete =\n",
	"# foo = foo\n",
	"#\n",
	"# The special character '!' stops recursion to directories below the one specified (-l option).\n",
	"# The special character '+' stops parsing of that line, so that you can avoid infinte loops.\n",
	"#\n",
	"# Items in (...) are an extended regular expression applied to the filenames.  All files which do not.\n",
	"# match are ignored.\n",
    NULL
};

static const char *const config_contents[] = {
    "# Set this to 'no' if pserver shouldn't check system users/passwords\n",
    "#SystemAuth=yes\n",
    "\n",
    "# Set the Acl parsing type (none,compat,normal).\n",
    "#AclMode=compat\n",
    "\n",
    "# Alternate location of CVS LockServer.  Set to 'none' to disable..\n",
	"#LockServer=localhost:2402\n",
    "\n",
    "# Set 'TopLevelAdmin' to 'yes' to create a CVS directory at the top\n",
    "# level of the new working directory when using the 'cvs checkout'\n",
    "# command.\n",
    "#TopLevelAdmin=no\n",
    "\n",
    "# Set 'LogHistory' to 'all' or 'TOFEWGCMAR' to log all transactions to the\n",
    "# history file, or a subset as needed (ie 'TMAR' logs all write operations)\n",
    "#LogHistory=TOFEWGCMAR\n",
    "\n",
    "# Set 'RereadLogAfterVerify' to control rereading of the log file after a verifymsg\n",
    "#   'always' or 'yes' to always reread the log regardless\n",
    "#   'never' or 'no' (default) to never reread the log\n",
    "#RereadLogAfterVerify=no\n",
	"\n",
	"# Set 'Watcher' to set a user who gets all notify events within the repository whether\n",
	"# or not the ifle is watched.\n",
	"#Watcher=watch_user",
    NULL
};

static const char *const precommand_contents[] = {
    "# The \"precommand\" file is run before a cvs command is executed.\n",
    "# The filter on the right is invoked with the repository name and\n",
    "# the name of the command that has been executed.  A non-zero return\n",
	"# value with abort the command with an error.\n",
    "#\n",
    "# The standard input of the filter receives each command argument,\n",
    "# separated by linefeeds.\n",
    "#\n",
    "# The first entry on a line is a regular expression which is tested\n",
    "# against the directory that the change is being committed to, relative\n",
    "# to the $CVSROOT.  For the first match that is found, then the remainder\n",
    "# of the line is the name of the filter to run.\n",
    "#\n",
    "# If the repository name does not match any of the regular expressions in this\n",
    "# file, the \"DEFAULT\" line is used, if it is specified.\n",
    "#\n",
    "# If the name \"ALL\" appears as a regular expression it is always used\n",
    "# in addition to the first matching regex or \"DEFAULT\".\n",
    NULL
};

static const char *const postcommand_contents[] = {
    "# The \"postcommand\" file is run after a cvs command has finished.\n",
    "# The filter on the right is invoked with the repository name and\n",
    "# the name of the command that has been executed.\n",
    "#\n",
    "# The first entry on a line is a regular expression which is tested\n",
    "# against the directory that the change is being committed to, relative\n",
    "# to the $CVSROOT.  For the first match that is found, then the remainder\n",
    "# of the line is the name of the filter to run.\n",
    "#\n",
    "# If the repository name does not match any of the regular expressions in this\n",
    "# file, the \"DEFAULT\" line is used, if it is specified.\n",
    "#\n",
    "# If the name \"ALL\" appears as a regular expression it is always used\n",
    "# in addition to the first matching regex or \"DEFAULT\".\n",
    NULL
};

static const char *const premodule_contents[] = {
    "# The \"premodule\" file is run before cvs module is processed.\n",
    "# The filter on the right is invoked with the repository name, \n",
    "# the name of the command that has been executed, and the module name.\n",
	"# A non-zero return value with abort the command with an error.\n",
    "#\n",
    "# The first entry on a line is a regular expression which is tested\n",
    "# against the directory that the change is being committed to, relative\n",
    "# to the $CVSROOT.  For the first match that is found, then the remainder\n",
    "# of the line is the name of the filter to run.\n",
    "#\n",
    "# If the repository name does not match any of the regular expressions in this\n",
    "# file, the \"DEFAULT\" line is used, if it is specified.\n",
    "#\n",
    "# If the name \"ALL\" appears as a regular expression it is always used\n",
    "# in addition to the first matching regex or \"DEFAULT\".\n",
    NULL
};

static const char *const postmodule_contents[] = {
    "# The \"postmodule\" file is run after a cvs module is processed.\n",
    "# The filter on the right is invoked with the repository name, \n",
    "# the name of the command that has been executed, and the module name.\n",
    "#\n",
    "# The first entry on a line is a regular expression which is tested\n",
    "# against the directory that the change is being committed to, relative\n",
    "# to the $CVSROOT.  For the first match that is found, then the remainder\n",
    "# of the line is the name of the filter to run.\n",
    "#\n",
    "# If the repository name does not match any of the regular expressions in this\n",
    "# file, the \"DEFAULT\" line is used, if it is specified.\n",
    "#\n",
    "# If the name \"ALL\" appears as a regular expression it is always used\n",
    "# in addition to the first matching regex or \"DEFAULT\".\n",
    NULL
};

static const char *const historyinfo_contents[] = {
    "# The \"historyinfo\" file is used to log the history file output.\n",
    "# The filter on the right is invoked with the repository name.  Its \n",
	"# standard input contains the history line that has just been written \n",
	"# to the history file (if it exists)\n",
    "#\n",
    "# The first entry on a line is a regular expression which is tested\n",
    "# against the directory that the change is being committed to, relative\n",
    "# to the $CVSROOT.  For the first match that is found, then the remainder\n",
    "# of the line is the name of the filter to run.\n",
    "#\n",
    "# If the repository name does not match any of the regular expressions in this\n",
    "# file, the \"DEFAULT\" line is used, if it is specified.\n",
    "#\n",
    "# If the name \"ALL\" appears as a regular expression it is always used\n",
    "# in addition to the first matching regex or \"DEFAULT\".\n",
    NULL
};

static const char *const commit_email_contents[] = {
    "# The \"commit_email\" file is used to control templates for emails sent\n",
    "# during commit and import.\n",
    "#\n",
    "# The first entry on a line is a regular expression which is tested\n",
    "# against the directory that the change is being made to, relative to the\n",
    "# $CVSROOT.  For the first match that is found, then the remainder of the\n",
    "# line is the name of the file that contains the template.\n",
    "#\n",
    "# If the repository name does not match any of the regular expressions in this\n",
    "# file, the \"DEFAULT\" line is used, if it is specified.\n",
    NULL
};

static const char *const tag_email_contents[] = {
    "# The \"tag_email\" file is used to control templates for emails sent\n",
    "# during tagging operations.\n",
    "#\n",
    "# The first entry on a line is a regular expression which is tested\n",
    "# against the directory that the change is being made to, relative to the\n",
    "# $CVSROOT.  For the first match that is found, then the remainder of the\n",
    "# line is the name of the file that contains the template.\n",
    "#\n",
    "# If the repository name does not match any of the regular expressions in this\n",
    "# file, the \"DEFAULT\" line is used, if it is specified.\n",
    NULL
};

static const char *const notify_email_contents[] = {
    "# The \"notify_email\" file is used to control templates for emails sent\n",
    "# when notifying users.\n",
    "#\n",
    "# The first entry on a line is a regular expression which is tested\n",
    "# against the directory that the change is being made to, relative to the\n",
    "# $CVSROOT.  For the first match that is found, then the remainder of the\n",
    "# line is the name of the file that contains the template.\n",
    "#\n",
    "# If the repository name does not match any of the regular expressions in this\n",
    "# file, the \"DEFAULT\" line is used, if it is specified.\n",
    NULL
};

static const char *const shadow_contents[] = {
    "# The \"shadow\" file is used to control automatic checkouts.\n",
    "#\n",
	"# Each line has 3 parts:\n",
	"# <module> <tag> <directory>\n",
	"#\n",
	"# In common with other commit support files, use forward slashes\n",
	"# and escape any spaces in filenames.\n",
    NULL
};

static const char *const cvsrc_contents[] = {
    NULL
};

static const char *const triggers_contents[] = {
    "# The \"triggers\" file lists libraries which handle the events for each\n",
    "# module.\n",
    "#\n",
#ifdef _WIN32
	"# A line enclosed in {....} will be treaded as a COM object.  You can use either\n",
	"# the ClassId or the ProgId\n",
	"#\n",
#endif
	"# In addition to the lines here, the default_trigger library is loaded, if available.\n",
    "#\n",
    NULL
};

static const struct admin_file filelist[] = {
    {CVSROOTADM_LOGINFO, 
	"no logging of 'cvs commit' messages is done without a %s file",
	&loginfo_contents[0]},
    {CVSROOTADM_RCSINFO,
	"a %s file can be used to configure 'cvs commit' templates",
	rcsinfo_contents},
    {CVSROOTADM_KEYWORDS,
	"a %s file can be used to define rcs keywords",
	keywords_contents},
    {CVSROOTADM_VERIFYMSG,
	"a %s file can be used to validate log messages",
	verifymsg_contents},
    {CVSROOTADM_COMMITINFO,
	"a %s file can be used to configure 'cvs commit' checking",
	commitinfo_contents},
    {CVSROOTADM_TAGINFO,
	"a %s file can be used to configure 'cvs tag' checking",
	taginfo_contents},
    {CVSROOTADM_IGNORE,
	"a %s file can be used to specify files to ignore",
	NULL},
    {CVSROOTADM_CHECKOUTLIST,
	"a %s file can specify extra CVSROOT files to auto-checkout",
	checkoutlist_contents},
    {CVSROOTADM_WRAPPER,
	"a %s file can be used to specify files to treat as wrappers",
	cvswrappers_contents},
    {CVSROOTADM_NOTIFY,
	"a %s file can be used to specify where notifications go",
	notify_contents},
    {CVSROOTADM_MODULES,
	/* modules is special-cased in mkmodules.  */
	NULL,
	modules_contents},
    {CVSROOTADM_MODULES2,
	"a %s file specifies logical directory mappings",
	modules2_contents},
    {CVSROOTADM_READERS,
	"a %s file specifies read-only users",
	NULL},
    {CVSROOTADM_WRITERS,
	"a %s file specifies read/write users",
	NULL},

    /* Some have suggested listing CVSROOTADM_PASSWD here too.  This
       would mean that CVS commands which operate on the
       CVSROOTADM_PASSWD file would transmit hashed passwords over the
       net.  This might seem to be no big deal, as pserver normally
       transmits cleartext passwords, but the difference is that
       CVSROOTADM_PASSWD contains *all* passwords, not just the ones
       currently being used.  For example, it could be too easy to
       accidentally give someone readonly access to CVSROOTADM_PASSWD
       (e.g. via anonymous CVS or cvsweb), and then if there are any
       guessable passwords for read/write access (usually there will be)
       they get read/write access.

       Another worry is the implications of storing old passwords--if
       someone used a password in the past they might be using it
       elsewhere, using a similar password, etc, and so saving old
       passwords, even hashed, is probably not a good idea.  */

    {CVSROOTADM_CONFIG,
	 "a %s file configures various behaviors",
	 config_contents},
	{CVSROOTADM_POSTCOMMIT,
	"a %s file is run after each commit has completed",
	NULL},
	{CVSROOTADM_PRECOMMAND,
	"a %s file is run before each command is executed",
	precommand_contents},
	{CVSROOTADM_POSTCOMMAND,
	"a %s file is run after each command has completed",
	postcommand_contents},
	{CVSROOTADM_PREMODULE,
	"a %s file is run before each module is processed",
	premodule_contents},
	{CVSROOTADM_POSTMODULE,
	"a %s file is run after each module is processed",
	postmodule_contents},
	{CVSROOTADM_HISTORYINFO,
	"a %s file is run each time the history file is updated",
	historyinfo_contents},
	{CVSROOTADM_CVSRC,
	"a %s file configures default cvs parameters",
	cvsrc_contents},
	{CVSROOTADM_TRIGGERS,
	"a %s file containts links to the cvs trigger libraries",
	triggers_contents},
	{"script.js",
	"a %s file contains javascript script hooks",
	NULL},
	{"script.vbs",
	"a %s file contains vbscript script hooks",
	NULL},
	{"script.pl",
	"a %s file contains perlscript script hooks",
	NULL},
	{"script.py",
	"a %s file contains pythonscript script hooks",
	NULL},
	{"script.rb",
	"a %s file contains rubyscript script hooks",
	NULL},
	{"commit_email",
	"a %s file contains the list of templates for commit emails",
	commit_email_contents},
	{"tag_email",
	"a %s file contains the list of templates for tag emails",
	tag_email_contents},
	{"notify_email",
	"a %s file contains the list of templates for notify emails",
	notify_email_contents},
	{"shadow",
	"a %s file contains the list of files to checkout after a successful commit",
	shadow_contents},
//	{"passwd",
//	"a %s file contains the list of users and passwords",
//	NULL},
	{"group",
	"a %s file contains the list of groups",
	NULL},
	{"admin",
	"a %s file contains the list of administrators",
	NULL},
	{0}
};

/* Rebuild the checked out administrative files in directory DIR.  */
int mkmodules (char *dir)
{
    struct saved_cwd cwd;
    char *temp;
    char *cp, *last, *fname;
#ifdef MY_NDBM
    DBM *db;
#endif
    FILE *fp;
    char *line = NULL;
    size_t line_allocated = 0;
    const struct admin_file *fileptr;
    mode_t mode;

    if (noexec)
	return 0;

    if (save_cwd (&cwd))
	error_exit ();

    if ( CVS_CHDIR (dir) < 0)
	error (1, errno, "cannot chdir to %s", dir);

    /*
     * First, do the work necessary to update the "modules" database.
     */
    temp = make_tempfile ();
    switch (checkout_file (CVSROOTADM_MODULES, dir, temp, NULL))
    {

	case 0:			/* everything ok */
#ifdef MY_NDBM
	    /* open it, to generate any duplicate errors */
	    if ((db = dbm_open (temp, O_RDONLY, 0666)) != NULL)
		dbm_close (db);
#else
	    write_dbmfile (temp);
	    rename_dbmfile (temp);
#endif
	    rename_rcsfile (temp, CVSROOTADM_MODULES);
	    break;

	default:
	    error (0, 0,
		"'cvs checkout' is less functional without a %s file",
		CVSROOTADM_MODULES);
	    break;
    }					/* switch on checkout_file() */

    if (unlink_file (temp) < 0
	&& !existence_error (errno))
	error (0, errno, "cannot remove %s", temp);
    xfree (temp);

    /* Checkout the files that need it in CVSROOT dir */
    for (fileptr = filelist; fileptr && fileptr->filename; fileptr++) {
	if (fileptr->errormsg == NULL)
	    continue;
	temp = make_tempfile ();
	if (checkout_file (fileptr->filename, dir, temp, NULL) == 0)
	    rename_rcsfile (temp, fileptr->filename);

	if (unlink_file (temp) < 0
	    && !existence_error (errno))
	    error (0, errno, "cannot remove %s", temp);
	xfree (temp);
    }

    fp = CVS_FOPEN (CVSROOTADM_CHECKOUTLIST, "r");
    if (fp)
    {
	/*
	 * File format:
	 *  [<whitespace>]<filename><whitespace><error message><end-of-line>
	 *
	 * comment lines begin with '#'
	 */
	while (getline (&line, &line_allocated, fp) >= 0)
	{
	    /* skip lines starting with # */
	    if (line[0] == '#')
		continue;

	    if ((last = strrchr (line, '\n')) != NULL)
		*last = '\0';			/* strip the newline */

	    /* Skip leading white space. */
	    for (fname = line;
		 *fname && isspace ((unsigned char) *fname);
		 fname++)
		;

	    /* Find end of filename. */
	    for (cp = fname; *cp && !isspace ((unsigned char) *cp); cp++)
		;
	    *cp = '\0';

	    if(isabsolute(fname) || pathname_levels(fname)>0 || !fncmp(fname,"checkoutlist"))
		{
			error(0,0,"Invalid filename '%s' in checkoutlist", fname);
			continue;
		}

	    temp = make_tempfile ();
	    if (checkout_file (fname, dir, temp, &mode) == 0)
	    {
	        chmod(temp,mode);
		rename_rcsfile (temp, fname);
	    }
	    else
	    {
		for (cp++;
		     cp < last && *last && isspace ((unsigned char) *last);
		     cp++)
		    ;
		if (cp < last && *cp)
		    error (0, 0, cp, fname);
	    }
	    if (unlink_file (temp) < 0
		&& !existence_error (errno))
		error (0, errno, "cannot remove %s", temp);
	    xfree (temp);
	}
	if (line)
	    xfree (line);
	if (ferror (fp))
	    error (0, errno, "cannot read %s", CVSROOTADM_CHECKOUTLIST);
	if (fclose (fp) < 0)
	    error (0, errno, "cannot close %s", CVSROOTADM_CHECKOUTLIST);
    }
    else
    {
	/* Error from CVS_FOPEN.  */
	if (!existence_error (errno))
	    error (0, errno, "cannot open %s", CVSROOTADM_CHECKOUTLIST);
    }

    if (restore_cwd (&cwd, NULL))
	error_exit ();
    free_cwd (&cwd);

    return (0);
}

/*
 * Yeah, I know, there are NFS race conditions here.
 */
static char *
make_tempfile ()
{
    static int seed = 0;
    int fd;
    char *temp;

    if (seed == 0)
	seed = (int)getpid ();
    temp = (char*)xmalloc (sizeof (BAKPREFIX) + 40);
    while (1)
    {
	(void) sprintf (temp, "%s%d", BAKPREFIX, seed++);
	if ((fd = CVS_OPEN (temp, O_BINARY|O_CREAT|O_EXCL|O_RDWR, 0666)) != -1)
	    break;
	if (errno != EEXIST)
	    error (1, errno, "cannot create temporary file %s", temp);
    }
    if (close(fd) < 0)
	error(1, errno, "cannot close temporary file %s", temp);
    return temp;
}

/* Get a file.  If the file does not exist, return 1 silently.  If
   there is an error, print a message and return 1 (FIXME: probably
   not a very clean convention).  On success, return 0.  */

static int checkout_file (const char *file, const char *directory, const char *temp, mode_t *mode)
{
    char *rcs;
    RCSNode *rcsnode;
    int retcode = 0;

    if (noexec)
		return 0;

    rcs = (char*)xmalloc (strlen (file) + strlen(directory) + 5);
	sprintf(rcs,"%s/%s%s",directory,file,RCSEXT);
    if (!isfile (rcs))
    {
		xfree (rcs);
		return (1);
    }
    rcsnode = RCS_parsercsfile (rcs);
	if(rcsnode)
		retcode = RCS_checkout (rcsnode, NULL, NULL, NULL, "b", (char*)temp, 
			    (RCSCHECKOUTPROC) NULL, (void *) NULL, mode);
    if (rcsnode && retcode != 0)
    {
		/* Probably not necessary (?); RCS_checkout already printed a
		message.  */
		error (0, 0, "failed to check out %s file",
			file);
    }
    freercsnode (&rcsnode);
    xfree (rcs);
    return (retcode);
}

#ifndef MY_NDBM

static void
write_dbmfile (temp)
    char *temp;
{
    char line[DBLKSIZ], value[DBLKSIZ];
    FILE *fp;
    DBM *db;
    char *cp, *vp;
    datum key, val;
    int len, cont, err = 0;

    fp = open_file (temp, "r");
    if ((db = dbm_open (temp, O_RDWR | O_CREAT | O_TRUNC, 0666)) == NULL)
	error (1, errno, "cannot open dbm file %s for creation", temp);
    for (cont = 0; fgets (line, sizeof (line), fp) != NULL;)
    {
	if ((cp = strrchr (line, '\n')) != NULL)
	    *cp = '\0';			/* strip the newline */

	/*
	 * Add the line to the value, at the end if this is a continuation
	 * line; otherwise at the beginning, but only after any trailing
	 * backslash is removed.
	 */
	vp = value;
	if (cont)
	    vp += strlen (value);

	/*
	 * See if the line we read is a continuation line, and strip the
	 * backslash if so.
	 */
	len = strlen (line);
	if (len > 0)
	    cp = &line[len - 1];
	else
	    cp = line;
	if (*cp == '\\')
	{
	    cont = 1;
	    *cp = '\0';
	}
	else
	{
	    cont = 0;
	}
	(void) strcpy (vp, line);
	if (value[0] == '#')
	    continue;			/* comment line */
	vp = value;
	while (*vp && isspace ((unsigned char) *vp))
	    vp++;
	if (*vp == '\0')
	    continue;			/* empty line */

	/*
	 * If this was not a continuation line, add the entry to the database
	 */
	if (!cont)
	{
	    key.dptr = vp;
	    while (*vp && !isspace ((unsigned char) *vp))
		vp++;
	    key.dsize = vp - key.dptr;
	    *vp++ = '\0';		/* NULL terminate the key */
	    while (*vp && isspace ((unsigned char) *vp))
		vp++;			/* skip whitespace to value */
	    if (*vp == '\0')
	    {
		error (0, 0, "warning: NULL value for key '%s'", key.dptr);
		continue;
	    }
	    val.dptr = vp;
	    val.dsize = strlen (vp);
	    if (dbm_store (db, key, val, DBM_INSERT) == 1)
	    {
		error (0, 0, "duplicate key found for '%s'", key.dptr);
		err++;
	    }
	}
    }
    dbm_close (db);
    if (fclose (fp) < 0)
	error (0, errno, "cannot close %s", temp);
    if (err)
    {
	/* I think that the size of the buffer needed here is
	   just determined by sizeof (CVSROOTADM_MODULES), the
	   filenames created by make_tempfile, and other things that won't
	   overflow.  */
	char dotdir[50], dotpag[50], dotdb[50];

	(void) sprintf (dotdir, "%s.dir", temp);
	(void) sprintf (dotpag, "%s.pag", temp);
	(void) sprintf (dotdb, "%s.db", temp);
	if (unlink_file (dotdir) < 0
	    && !existence_error (errno))
	    error (0, errno, "cannot remove %s", dotdir);
	if (unlink_file (dotpag) < 0
	    && !existence_error (errno))
	    error (0, errno, "cannot remove %s", dotpag);
	if (unlink_file (dotdb) < 0
	    && !existence_error (errno))
	    error (0, errno, "cannot remove %s", dotdb);
	error (1, 0, "DBM creation failed; correct above errors");
    }
}

static void
rename_dbmfile (temp)
    char *temp;
{
    /* I think that the size of the buffer needed here is
       just determined by sizeof (CVSROOTADM_MODULES), the
       filenames created by make_tempfile, and other things that won't
       overflow.  */
    char newdir[50], newpag[50], newdb[50];
    char dotdir[50], dotpag[50], dotdb[50];
    char bakdir[50], bakpag[50], bakdb[50];

    int dir1_errno = 0, pag1_errno = 0, db1_errno = 0;
    int dir2_errno = 0, pag2_errno = 0, db2_errno = 0;
    int dir3_errno = 0, pag3_errno = 0, db3_errno = 0;

    (void) sprintf (dotdir, "%s.dir", CVSROOTADM_MODULES);
    (void) sprintf (dotpag, "%s.pag", CVSROOTADM_MODULES);
    (void) sprintf (dotdb, "%s.db", CVSROOTADM_MODULES);
    (void) sprintf (bakdir, "%s%s.dir", BAKPREFIX, CVSROOTADM_MODULES);
    (void) sprintf (bakpag, "%s%s.pag", BAKPREFIX, CVSROOTADM_MODULES);
    (void) sprintf (bakdb, "%s%s.db", BAKPREFIX, CVSROOTADM_MODULES);
    (void) sprintf (newdir, "%s.dir", temp);
    (void) sprintf (newpag, "%s.pag", temp);
    (void) sprintf (newdb, "%s.db", temp);

    (void) chmod (newdir, 0666);
    (void) chmod (newpag, 0666);
    (void) chmod (newdb, 0666);

    /* rm .#modules.dir .#modules.pag */
    if (unlink_file (bakdir) < 0)
	dir1_errno = errno;
    if (unlink_file (bakpag) < 0)
	pag1_errno = errno;
    if (unlink_file (bakdb) < 0)
	db1_errno = errno;

    /* mv modules.dir .#modules.dir */
    if (CVS_RENAME (dotdir, bakdir) < 0)
	dir2_errno = errno;
    /* mv modules.pag .#modules.pag */
    if (CVS_RENAME (dotpag, bakpag) < 0)
	pag2_errno = errno;
    /* mv modules.db .#modules.db */
    if (CVS_RENAME (dotdb, bakdb) < 0)
	db2_errno = errno;

    /* mv "temp".dir modules.dir */
    if (CVS_RENAME (newdir, dotdir) < 0)
	dir3_errno = errno;
    /* mv "temp".pag modules.pag */
    if (CVS_RENAME (newpag, dotpag) < 0)
	pag3_errno = errno;
    /* mv "temp".db modules.db */
    if (CVS_RENAME (newdb, dotdb) < 0)
	db3_errno = errno;

    /* I didn't want to call error() when we had signals blocked
       (unnecessary?), but do it now.  */
    if (dir1_errno && !existence_error (dir1_errno))
	error (0, dir1_errno, "cannot remove %s", bakdir);
    if (pag1_errno && !existence_error (pag1_errno))
	error (0, pag1_errno, "cannot remove %s", bakpag);
    if (db1_errno && !existence_error (db1_errno))
	error (0, db1_errno, "cannot remove %s", bakdb);

    if (dir2_errno && !existence_error (dir2_errno))
	error (0, dir2_errno, "cannot remove %s", bakdir);
    if (pag2_errno && !existence_error (pag2_errno))
	error (0, pag2_errno, "cannot remove %s", bakpag);
    if (db2_errno && !existence_error (db2_errno))
	error (0, db2_errno, "cannot remove %s", bakdb);

    if (dir3_errno && !existence_error (dir3_errno))
	error (0, dir3_errno, "cannot remove %s", bakdir);
    if (pag3_errno && !existence_error (pag3_errno))
	error (0, pag3_errno, "cannot remove %s", bakpag);
    if (db3_errno && !existence_error (db3_errno))
	error (0, db3_errno, "cannot remove %s", bakdb);
}

#endif				/* !MY_NDBM */

static void rename_rcsfile (char *temp, char *real)
{
    char *bak;

	xchmod(temp,0);

	bak = (char*)xmalloc (strlen (real) + sizeof (BAKPREFIX) + 10);
    sprintf (bak, "%s%s", BAKPREFIX, real);

    /* rm .#loginfo */
    if (unlink_file (bak) < 0 && !existence_error (errno))
		error (0, errno, "cannot remove %s", bak);

    /* mv loginfo .#loginfo */
    if (CVS_RENAME (real, bak) < 0 && !existence_error (errno))
	error (0, errno, "cannot rename %s to %s", real, bak);

    /* mv "temp" loginfo */
    if (CVS_RENAME (temp, real) < 0 && !existence_error (errno))
	error (0, errno, "cannot rename %s to %s", temp, real);

    xfree (bak);
}

const char *const init_usage[] = {
    "Usage: %s %s [-f] [-n] [-r remote_root] [-a alias] [-d description]\n",
	"\t-a alias\tNew repository alias\n",
	"\t-d description\tNew repository description\n",
	"\t-f\t\tForce overwrite existing repository\n",
	"\t-r repository\tRemote repository location (remote init only)\n",
	"\t-n\t\tDo not register repository\n",
	"\t-u\t\tUnregister repository\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

int init (int argc, char **argv)
{
    /* Name of CVSROOT directory.  */
    char *adm;
    /* Name of this administrative file.  */
    char *info;
    /* Name of ,v file for this administrative file.  */
    char *info_v;
    /* Exit status.  */
    int err;
	int c;
	const char *root = NULL;
	const char *alias = NULL;
	const char *description = NULL;
	int force = 0, unregister = 0, reg = 1;
	char tmp[64];
	char buffer[MAX_PATH];
	int n;

    const struct admin_file *fileptr;

    umask (cvsumask);

    if (argc == -1)
		usage (init_usage);

    optind = 0;
	while ((c = getopt (argc, argv, "a:r:d:fun")) != -1)
    {
		switch (c)
		{
			case 'r':
				root = optarg;
				break;
			case 'a':
				alias = optarg;
				break;
			case 'd':
				description = optarg;
				break;
			case 'f':
				force = 1;
				break;
			case 'n':
				reg = 0;
				break;
			case 'u':
				unregister = 1;
				break;
		    default:
				usage (init_usage);
				break;
		}
    }
    argc -= optind;
    argv += optind;

	if(argc)
		usage(init_usage);

#ifdef SERVER_SUPPORT
    if (current_parsed_root->isremote)
    {
		if(force)
			send_arg("-f");
		if(unregister)
			send_arg("-u");
		option_with_arg("-r",root);
		option_with_arg("-a",alias);
		option_with_arg("-d",description);
		send_to_server("init\n",0);
		return get_responses_and_close ();
    }

	if(server_active)
	{
		if(!verify_admin())
			error(1,0,"Only repository administrators can run a remote cvs init command");
		if(!remote_init_root)
			error(1,0,"Remote init commands have been disabled by the server administrator");
		if(!isabsolute(remote_init_root))
			error(1,0,"Remote init root directory is not an absolute path - remote init disabled");
		if(!root && !unregister)
			error(1,0,"Must specify root for remote init command");

		if(root && (root[0]!='/' || strstr(root,"..")))
			error(1,0,"Root has incorrect format (unix style absolute path required)");

		if(alias && alias[0]!='/')
			error(1,0,"Alias has incorrect format (unix style absolute path required)");
		if(root)
		{
			free_cvsroot_t(current_parsed_root);
			current_parsed_root = new_cvsroot_t();
			current_parsed_root->directory = (const char*)xmalloc(strlen(root)+strlen(remote_init_root)+10);
			sprintf((char*)current_parsed_root->directory,"%s%s",remote_init_root,root);
			current_parsed_root->original = xstrdup(alias?alias:root);
			current_parsed_root->unparsed_directory = xstrdup(alias?alias:root);

			Lock_Cleanup();
			lock_register_client(CVS_Username,current_parsed_root->directory);
		}
		else if(alias)
		{
			xfree(current_parsed_root->unparsed_directory);
			current_parsed_root->unparsed_directory = xstrdup(alias);
		}
	}
	else
#endif
	{
		if(root)
			error(1,0,"The remote -r command is not used for local init commands");

		if(alias)
		{
			xfree(current_parsed_root->unparsed_directory);
			current_parsed_root->unparsed_directory = xstrdup(alias);
		}
	}

	if(unregister)
	{
		n=0;
		bool done = false;
		while(!CGlobalSettings::EnumGlobalValues("cvsnt","PServer",n++,tmp,sizeof(tmp),buffer,sizeof(buffer)))
		{
			if(!strncasecmp(tmp,"Repository",10) && isdigit(tmp[10]) && isdigit(tmp[strlen(tmp)-1]))
			{
				if(fncmp(buffer,current_parsed_root->directory))
					continue;
				int prefixnum = atoi(tmp+10);
				if(alias)
				{
					snprintf(tmp,sizeof(tmp),"Repository%dName",prefixnum);
					if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer",tmp,buffer,sizeof(buffer)))
					{
						if(fncmp(alias,buffer))
							continue;
					}
				}
				snprintf(tmp,sizeof(tmp),"Repository%d",prefixnum);
				if(CGlobalSettings::SetGlobalValue("cvsnt","PServer",tmp,(char*)NULL))
				{
					error(0,0,"Unable to unregister repository.");
					error(0,0,"Your login does not have sufficient permissions to modify the");
					error(0,0,"global server settings.");
					return 1;
				}
				snprintf(tmp,sizeof(tmp),"Repository%dName",prefixnum);
				CGlobalSettings::SetGlobalValue("cvsnt","PServer",tmp,(char*)NULL);
				snprintf(tmp,sizeof(tmp),"Repository%dDescription",prefixnum);
				CGlobalSettings::SetGlobalValue("cvsnt","PServer",tmp,(char*)NULL);
				snprintf(tmp,sizeof(tmp),"Repository%dPublish",prefixnum);
				CGlobalSettings::SetGlobalValue("cvsnt","PServer",tmp,(char*)NULL);
				done = true;
			}
		}
		if(done)
		{
			/* We may be unregistering ourselves, so can't use if(alias) test here */
			error(0,0,"Repository %s (%s) unregistered",current_parsed_root->unparsed_directory,current_parsed_root->directory);
			return 0;
		}
		else
		{
			if(alias)
				error(0,0,"No repository registered for %s (%s)",alias,current_parsed_root->directory);
			else
				error(0,0,"No repository registered for %s",current_parsed_root->directory);
			return 1;
		}
	}

	if(reg && alias)
	{
		n=0;
		while(!CGlobalSettings::EnumGlobalValues("cvsnt","PServer",n++,tmp,sizeof(tmp),buffer,sizeof(buffer)))
		{
			if(!strncasecmp(tmp,"Repository",10) && isdigit(tmp[10]) && isdigit(tmp[strlen(tmp)-1]))
			{
				int prefixnum = atoi(tmp+10);
				snprintf(tmp,sizeof(tmp),"Repository%dName",prefixnum);
				if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer",tmp,buffer,sizeof(buffer)))
				{
					if(!fncmp(alias,buffer))
						error(1,0,"Repository with alias '%s' already exists.",alias);
				}
			}
		}
	}

	adm = (char*)xmalloc (strlen (current_parsed_root->directory) + sizeof (CVSROOTADM) + 2);
    sprintf (adm, "%s/%s", current_parsed_root->directory, CVSROOTADM);

	if(!force)
	{
		cvs::filename fn;
		cvs::sprintf(fn,80,"%s/config",adm);
		if(CFileAccess::exists(fn.c_str()))
		{
			error(1,0,"Repository at %s already exists.", current_parsed_root->directory);
		}
	}

	if(reg)
	{
		for(n=0; n<1000000; n++)
		{
			snprintf(tmp,sizeof(tmp),"Repository%d",n);
			if(CGlobalSettings::GetGlobalValue("cvsnt","PServer",tmp,buffer,sizeof(buffer)))
				break;
		}
		if(n==1000000)
			error(1,0,"1000000 repositories?  Are you mad?");

		if(CGlobalSettings::SetGlobalValue("cvsnt","PServer",tmp,current_parsed_root->directory))
		{
			error(0,0,"Unable to register repository.");
			error(0,0,"Your login may not have sufficient permissions to modify the");
			error(0,0,"global server settings.");
			return 1;
		}
		else
		{
			snprintf(tmp,sizeof(tmp),"Repository%dName", n);
			CGlobalSettings::SetGlobalValue("cvsnt","PServer",tmp,current_parsed_root->unparsed_directory);
			if(description)
			{
				snprintf(tmp,sizeof(tmp),"Repository%dDescription", n);
				CGlobalSettings::SetGlobalValue("cvsnt","PServer",tmp,description);
			}
		}
	}

    /* Note: we do *not* create parent directories as needed like the
       old cvsinit.sh script did.  Few utilities do that, and a
       non-existent parent directory is as likely to be a typo as something
       which needs to be created.  */
    mkdir_if_needed (current_parsed_root->directory);
    mkdir_if_needed (adm);

    /* This is needed because we pass "fileptr->filename" not "info"
       to add_rcs_file below.  I think this would be easy to change,
       thus nuking the need for CVS_CHDIR here, but I haven't looked
       closely (e.g. see wrappers calls within add_rcs_file).  */
    if ( CVS_CHDIR (adm) < 0)
		error (1, errno, "cannot change to directory %s", fn_root(adm));

    /* Make Emptydir so it's there if we need it */
    mkdir_if_needed (CVSNULLREPOS);

    /* 80 is long enough for all the administrative file names, plus
       "/" and so on.  */
    info = (char*)xmalloc (strlen (adm) + 80);
    info_v = (char*)xmalloc (strlen (adm) + 80);
    for (fileptr = filelist; fileptr && fileptr->filename; ++fileptr)
    {
	if (fileptr->contents == NULL)
	    continue;
	strcpy (info, adm);
	strcat (info, "/");
	strcat (info, fileptr->filename);
	strcpy (info_v, info);
	strcat (info_v, RCSEXT);
	if (isfile (info_v))
	    /* We will check out this file in the mkmodules step.
	       Nothing else is required.  */
	    ;
	else
	{
	    int retcode;

	    if (!isfile (info))
	    {
		FILE *fp;
		const char * const *p;

		fp = open_file (info, "wb");
		for (p = fileptr->contents; *p != NULL; ++p)
		    if (fputs (*p, fp) < 0)
			error (1, errno, "cannot write %s", info);
		if (fclose (fp) < 0)
		    error (1, errno, "cannot close %s", info);
	    }
	    /* The message used to say " of " and fileptr->filename after
	       "initial checkin" but I fail to see the point as we know what
	       file it is from the name.  */
	    retcode = add_rcs_file ("initial checkin", info_v,
				    fileptr->filename, "1.1", NULL,

				    /* No vendor branch.  */
				    NULL, NULL, 0, NULL,

				    NULL, 0, NULL, NULL);
	    if (retcode != 0)
		/* add_rcs_file already printed an error message.  */
		err = 1;
	}
    }

    /* Turn on history logging by default.  The user can remove the file
       to disable it.  */
	/* Don't do this... a big linear file is a lame way of tracking repository usage,
	   and it just grows without any end. */
#if 0
    strcpy (info, adm);
    strcat (info, "/");
    strcat (info, CVSROOTADM_HISTORY);
    if (!isfile (info))
    {
		FILE *fp;

		fp = open_file (info, "w");
		if (fclose (fp) < 0)
			error (1, errno, "cannot close %s", info);
 
        /* Make the new history file world-writeable, since every CVS
           user will need to be able to write to it.  We use chmod()
           because xchmod() is too shy. */
#ifndef _WIN32
        chmod (info, 0666);
#endif
    }
#endif

    /* Make an empty val-tags file to prevent problems creating it later.  */
    strcpy (info, adm);
    strcat (info, "/");
    strcat (info, CVSROOTADM_VALTAGS);
    if (!isfile (info))
    {
	FILE *fp;

	fp = open_file (info, "w");
	if (fclose (fp) < 0)
	    error (1, errno, "cannot close %s", info);
 
        /* Make the new val-tags file world-writeable, since every CVS
           user will need to be able to write to it.  We use chmod()
           because xchmod() is too shy. */
#ifndef _WIN32
        chmod (info, 0666);
#else
		xchmod (info,1);
#endif
    }

    xfree (info);
    xfree (info_v);

	fileattr_startdir(adm);
	fileattr_modified();
	CXmlNodePtr curdir = fileattr_getroot();
	curdir->NewNode("directory");
	curdir->NewNode("owner",CVS_Username,false);
	curdir->NewNode("acl");
	curdir->NewAttribute("user","owner");
	curdir->NewNode("all",NULL,false);
	curdir->GetParent();
	curdir->NewNode("acl");
	curdir->NewAttribute("user","admin");
	curdir->NewNode("all",NULL,false);
	curdir->GetParent();
	curdir->NewNode("acl");
	curdir->NewNode("all");
	curdir->NewAttribute("deny","1");
	curdir->GetParent();
	curdir->GetParent();
	fileattr_write();
	fileattr_free();

    mkmodules (adm);

    xfree (adm);

	if(alias)
		error(0,0,"Repository %s (%s) initialised",alias,current_parsed_root->directory);
	else
		error(0,0,"Repository %s initialised",current_parsed_root->directory);
    return 0;
}
