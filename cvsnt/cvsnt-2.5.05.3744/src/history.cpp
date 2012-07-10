/*
 *
 *    You may distribute under the terms of the GNU General Public License
 *    as specified in the README file that comes with the CVS 1.0 kit.
 *
 * **************** History of Users and Module ****************
 *
 * LOGGING:  Append record to "${CVSROOT}/CVSROOTADM/CVSROOTADM_HISTORY".
 *
 * On For each Tag, Add, Checkout, Commit, Update or Release command,
 * one line of text is written to a History log.
 *
 *	X date | user | CurDir | special | rev(s) | argument | bugid '\n'
 *
 * where: [The spaces in the example line above are not in the history file.]
 *
 *  X		is a single character showing the type of event:
 *		T	"Tag" cmd.
 *		O	"Checkout" cmd.
 *              E       "Export" cmd.
 *		F	"Release" cmd.
 *		W	"Update" cmd - No User file, Remove from Entries file.
 *		U	"Update" cmd - File was checked out over User file.
 *		G	"Update" cmd - File was merged successfully.
 *		C	"Update" cmd - File was merged and shows overlaps.
 *		M	"Commit" cmd - "Modified" file.
 *		A	"Commit" cmd - "Added" file.
 *		R	"Commit" cmd - "Removed" file.
 *		e	"edit" cmd - file edited
 *		u	"unedit" cmd - file unedited
 *
 *  date  is a minimum 8-char hex representation of a Unix time_t.
 *		[Starting here, variable fields are delimited by '|' chars.]
 *
 *  user	is the username of the person who typed the command.
 *
 *  CurDir	The directory where the action occurred.  This should be the
 *		absolute path of the directory which is at the same level as
 *		the "Repository" field (for W,U,G,C & M,A,R,e,u).
 *
 *  Repository	For record types [W,U,G,C,M,A,R,e,u] this field holds the
 *		repository read from the administrative data where the
 *		command was typed.
 *		T	"A" --> New Tag, "D" --> Delete Tag
 *			Otherwise it is the Tag or Date to modify.
 *		O,F,E	A "" (null field)
 *
 *  rev(s)	Revision number or tag.
 *		T	The Tag to apply.
 *		O,E	The Tag or Date, if specified, else "" (null field).
 *		F	"" (null field)
 *		W	The Tag or Date, if specified, else "" (null field).
 *		U	The Revision checked out over the User file.
 *		G,C	The Revision(s) involved in merge.
 *		M,A,R,e,u	RCS Revision affected.
 *
 *  argument	The module (for [TOEUF]) or file (for [WUGCMAReu]) affected.
 *
 *
 *** Report categories: "User" and "Since" modifiers apply to all reports.
 *			[For "sort" ordering see the "sort_order" routine.]
 *
 *   Extract list of record types
 *
 *	-e, -x [TOEFWUCGMAReu]
 *
 *		Extracted records are simply printed, No analysis is performed.
 *		All "field" modifiers apply.  -e chooses all types.
 *
 *   Checked 'O'ut modules
 *
 *	-o, -w
 *		Checked out modules.  'F' and 'O' records are examined and if
 *		the last record for a repository/file is an 'O', a line is
 *		printed.  "-w" forces the "working dir" to be used in the
 *		comparison instead of the repository.
 *
 *   Committed (Modified) files
 *
 *	-c, -l, -w
 *		All 'M'odified, 'A'dded and 'R'emoved records are examined.
 *		"Field" modifiers apply.  -l forces a sort by file within user
 *		and shows only the last modifier.  -w works as in Checkout.
 *
 *		Warning: Be careful with what you infer from the output of
 *			 "cvs hi -c -l".  It means the last time *you*
 *			 changed the file, not the list of files for which
 *			 you were the last changer!!!
 *
 *   Module history for named modules.
 *	-m module, -l
 *
 *		This is special.  If one or more modules are specified, the
 *		module names are remembered and the files making up the
 *		modules are remembered.  Only records matching exactly those
 *		files and repositories are shown.  Sorting by "module", then
 *		filename, is implied.  If -l ("last modified") is specified,
 *		then "update" records (types WUCG), tag and release records
 *		are ignored and the last (by date) "modified" record.
 *
 *   TAG history
 *
 *	-T	All Tag records are displayed.
 *
 *** Modifiers.
 *
 *   Since ...		[All records contain a timestamp, so any report
 *			 category can be limited by date.]
 *
 *	-D date		- The "date" is parsed into a Unix "time_t" and
 *			  records with an earlier time stamp are ignored.
 *	-r rev/tag	- A "rev" begins with a digit.  A "tag" does not.  If
 *			  you use this option, every file is searched for the
 *			  indicated rev/tag.
 *	-t tag		- The "tag" is searched for in the history file and no
 *			  record is displayed before the tag is found.  An
 *			  error is printed if the tag is never found.
 *	-b string	- Records are printed only back to the last reference
 *			  to the string in the "module", "file" or
 *			  "repository" fields.
 *
 *   Field Selections	[Simple comparisons on existing fields.  All field
 *			 selections are repeatable.]
 *
 *	-a		- All users.
 *	-u user		- If no user is given and '-a' is not given, only
 *			  records for the user typing the command are shown.
 *			  ==> If -a or -u is not specified, just use "self".
 *
 *	-f filematch	- Only records in which the "file" field contains the
 *			  string "filematch" are considered.
 *
 *	-p repository	- Only records in which the "repository" string is a
 *			  prefix of the "repos" field are considered.
 *
 *	-n modulename	- Only records which contain "modulename" in the
 *			  "module" field are considered.
 *  -n bugid		- Only records which contain "bugid" in the "bugid" field are considered.
 *
 *
 * EXAMPLES: ("cvs history", "cvs his" or "cvs hi")
 *
 *** Checked out files for username.  (default self, e.g. "dgg")
 *	cvs hi			[equivalent to: "cvs hi -o -u dgg"]
 *	cvs hi -u user		[equivalent to: "cvs hi -o -u user"]
 *	cvs hi -o		[equivalent to: "cvs hi -o -u dgg"]
 *
 *** Committed (modified) files from the beginning of the file.
 *	cvs hi -c [-u user]
 *
 *** Committed (modified) files since Midnight, January 1, 1990:
 *	cvs hi -c -D 'Jan 1 1990' [-u user]
 *
 *** Committed (modified) files since tag "TAG" was stored in the history file:
 *	cvs hi -c -t TAG [-u user]
 *
 *** Committed (modified) files since tag "TAG" was placed on the files:
 *	cvs hi -c -r TAG [-u user]
 *
 *** Who last committed file/repository X?
 *	cvs hi -c -l -[fp] X
 *
 *** Modified files since tag/date/file/repos?
 *	cvs hi -c {-r TAG | -D Date | -b string}
 *
 *** Tag history
 *	cvs hi -T
 *
 *** History of file/repository/module X.
 *	cvs hi -[fpn] X
 *
 *** History of user "user".
 *	cvs hi -e -u user
 *
 *** Dump (eXtract) specified record types
 *	cvs hi -x [TOFWUGCMAReu]
 *
 *
 * FUTURE:		J[Join], I[Import]  (Not currently implemented.)
 *
 */

#include "cvs.h"
#include "savecwd.h"

struct hrec_t
{
    char type;		/* Type of record (In history record) */
    cvs::username user;		/* Username (In history record) */
    cvs::filename dir;		/* "Compressed" Working dir (In history record) */
    cvs::filename repos;	/* (Tag is special.) Repository (In history record) */
    cvs::string rev;		/* Revision affected (In history record) */
    cvs::filename file;		/* Filename (In history record) */
    cvs::filename end;		/* Ptr into repository to copy at end of workdir */
    cvs::filename mod;		/* The module within which the file is contained */
    time_t date;	/* Calculated from date stored in record */
    long idx;		/* Index of record, for "stable" sort. */
	cvs::string bugid;	/* Associated bugid */
};

std::vector<hrec_t> hrec_list;

std::pair<bool,std::vector<hrec_t>::iterator> last_since_tag;
std::pair<bool,std::vector<hrec_t>::iterator> last_backto;

static long hrec_idx;

static void fill_hrec (char *line, hrec_t * hr);
static int accept_hrec (const hrec_t * hr, const hrec_t * lr);
static int select_hrec (hrec_t * hr, int since, int backto);
static bool sort_order (const hrec_t& l, const hrec_t& r);
static void read_hrecs (const char *fname);
static void report_hrecs ();
static void save_file (const char *dir, const char *name);
static void save_module (const char *module);
static void save_user(const char *name);
static void save_bugid(const char *name);

#define ALL_REC_TYPES "TOEFWUCGMAReu"

char *logHistory = ALL_REC_TYPES;

static short report_count;
static short extract;
static short v_checkout;
static short modified;
static short tag_report;
static short module_report;
static short working;
static short last_entry;
static short all_users;

static short user_sort;
static short repos_sort;
static short file_sort;
static short module_sort;

static short tz_local;
static time_t tz_seconds_east_of_GMT;
static const char *tz_name = "+0000";

/* -r, -t, or -b options.  These are NULL if the option in
   question is not specified or is overridden by another option.  Together
   with since_date, these are a mutually exclusive set; one overrides the
   others.  */
static const char *since_rev;
static const char *since_tag;
static const char *backto;
/* -D option, or NULL if not specified.  RCS format.  */
static const char *since_date;

/* Record types to look for, malloc'd.  Probably could be statically
   allocated, but only if we wanted to check for duplicates more than
   we do.  */
static cvs::string rec_types;

static std::vector<cvs::username> user_list; /* Array of user names */
static std::vector<cvs::string> bug_list;
std::vector<cvs::filename> file_list; /* Array of file names */

static std::vector<cvs::filename> mod_list; /* Array of module names */

struct historyproc_param_t
{
	char type;
	const char *workdir;
	const char *revs;
	const char *name;
	const char *bugid;
	const char *message;
};

/* This is pretty unclear.  First of all, separating "flags" vs.
   "options" (I think the distinction is that "options" take arguments)
   is nonstandard, and not something we do elsewhere in CVS.  Second of
   all, what does "reports" mean?  I think it means that you can only
   supply one of those options, but "reports" hardly has that meaning in
   a self-explanatory way.  */
static const char *const history_usg[] =
{
    "Usage: %s %s [-report] [-flags] [-options args] [files...]\n\n",
    "   Reports:\n",
    "        -T              Produce report on all TAGs\n",
    "        -c              Committed (Modified) files\n",
    "        -o              Checked out modules\n",
    "        -m <module>     Look for specified module (repeatable)\n",
    "        -x [TOEFWUCGMAReu] Extract by record type\n",
    "        -e              Everything (same as -x, but all record types)\n",
    "   Flags:\n",
    "        -a              All users (Default is self)\n",
    "        -l              Last modified (committed or modified report)\n",
    "        -w              Working directory must match\n",
    "   Options:\n",
    "        -D <date>       Since date (Many formats)\n",
    "        -b <str>        Back to record with str in module/file/repos field\n",
    "        -f <file>       Specified file (same as command line) (repeatable)\n",
    "        -n <modulename> In module (repeatable)\n",
    "        -p <repos>      In repository (repeatable)\n",
    "        -r <rev/tag>    Since rev or tag (looks inside RCS files!)\n",
    "        -t <tag>        Since tag record placed in history file (by anyone).\n",
    "        -u <user>       For user name (repeatable)\n",
    "        -Z              Output for local time zone\n",
    "        -z <tz>         Output for time zone <tz> (e.g. -z -0700)\n",
	"        -B <bugid>      Containing bug <bugid>\n",
    NULL};

/*
 * Callback proc for historyinfo
 */
static int historyinfo_proc (void *params, const trigger_interface *cb)
{
	historyproc_param_t *args = (historyproc_param_t*)params;
	int ret = 0;
	if(cb->history)
	{
		ret = cb->history(cb,args->type,args->workdir,args->revs,args->name, args->bugid, args->message);
	}
	return ret;
}

/* Sort routine for qsort:
   - If a user is selected at all, sort it first. User-within-file is useless.
   - If a module was selected explicitly, sort next on module.
   - Then sort by file.  "File" is "repository/file" unless "working" is set,
     then it is "workdir/file".  (Revision order should always track date.)
   - Always sort timestamp last.
*/
static bool sort_order (const hrec_t& left, const hrec_t& right)
{
    int i;

    if (user_sort)	/* If Sort by username, compare users */
    {
		if ((i = strcmp (left.user.c_str(), right.user.c_str())) != 0)
			return i<0;
    }
    if (module_sort)	/* If sort by modules, compare module names */
    {
		if ((i = strcmp (left.mod.c_str(), right.mod.c_str())) != 0)
			return i<0;
    }
    if (repos_sort)	/* If sort by repository, compare them. */
    {
		if ((i = strcmp (left.repos.c_str(), right.repos.c_str())) != 0)
		    return i<0;
    }
    if (file_sort)	/* If sort by filename, compare files, NOT dirs. */
    {
		if ((i = strcmp (left.file.c_str(), right.file.c_str())) != 0)
			return i<0;

		if (working)
		{
			if ((i = strcmp (left.dir.c_str(), right.dir.c_str())) != 0)
				return i<0;

			if ((i = strcmp (left.end.c_str(), right.end.c_str())) != 0)
				return i<0;
		}
	}

    /*
     * By default, sort by date, time
     * XXX: This fails after 2030 on 32bit machines when date slides into sign bit
     */
    if ((i = left.date - right.date) != 0)
		return i<0;

    /* For matching dates, keep the sort stable by using record index */
    return (left.idx - right.idx)<0;
}

int history (int argc, char **argv)
{
    int i, c;
	cvs::filename fname;

    if (argc == -1)
		usage (history_usg);

    optind = 0;
	while ((c = getopt (argc, argv, "+Tacelow?D:b:f:m:n:p:r:t:u:x:z:ZB:")) != -1)
    {
	switch (c)
	{
	    case 'T':			/* Tag list */
		report_count++;
		tag_report++;
		break;
	    case 'a':			/* For all usernames */
		all_users++;
		break;
	    case 'c':
		report_count++;
		modified = 1;
		break;
	    case 'e':
		report_count++;
		extract++;
		rec_types = ALL_REC_TYPES;
		break;
	    case 'l':			/* Find Last file record */
		last_entry = 1;
		break;
	    case 'o':
		report_count++;
		v_checkout = 1;
		break;
	    case 'w':			/* Match Working Dir (CurDir) fields */
		working = 1;
		break;
	    case 'D':			/* Since specified date */
		if (since_rev || since_tag || backto)
		{
		    error (0, 0, "date overriding rev/tag/backto");
		    since_rev = since_tag = backto = NULL;
		}
		since_date = Make_Date (optarg);
		break;
	    case 'b':			/* Since specified file/Repos */
		if (since_date || since_rev || since_tag)
		{
		    error (0, 0, "backto overriding date/rev/tag");
		    since_rev = since_tag = NULL;
			xfree (since_date);
		    since_date = NULL;
		}
		backto = optarg;
		break;
		case 'B':
			save_bugid(optarg);
			break;
	    case 'f':			/* For specified file */
		save_file ("", optarg);
		break;
	    case 'm':			/* Full module report */
		if (!module_report++) report_count++;
		/* fall through */
	    case 'n':			/* Look for specified module */
		save_module (optarg);
		break;
	    case 'p':			/* For specified directory */
		save_file (optarg, "");
		break;
	    case 'r':			/* Since specified Tag/Rev */
		if (since_date || since_tag || backto)
		{
		    error (0, 0, "rev overriding date/tag/backto");
		    since_tag = backto = NULL;
			xfree (since_date);
		}
		since_rev = optarg;
		break;
	    case 't':			/* Since specified Tag/Rev */
		if (since_date || since_rev || backto)
		{
		    error (0, 0, "tag overriding date/marker/file/repos");
		    since_rev = backto = NULL;
			xfree (since_date);
		}
		since_tag = optarg;
		break;
	    case 'u':			/* For specified username */
		save_user (optarg);
		break;
	    case 'x':
		report_count++;
		extract++;
	    for (char *cp = optarg; *cp; cp++)
			if (!strchr (logHistory, *cp))
				error (1, 0, "%c is not a valid report type", *cp);
		rec_types = optarg;
		break;
	    case 'Z':
		tz_local = 0; // static short
		tz_seconds_east_of_GMT = get_local_time_offset();
		tz_name = (char*)xmalloc(10);
		i = tz_seconds_east_of_GMT / 60;
		snprintf((char*)tz_name,10,"%+05d",((i/60)*100)+(i%60));
		// Of course this suffers from the DST bug... as do all the other -T commands now I come to
		// think about it..  not so critical for status and log since you're generally looking at stuff
		// done in the last couple of days anyway - but for history it may be more obvious.

		// The solution for Unix is probably to pass real timezone information (Europe/London), use localtime
		// and modify TZ temporarily.  Needs some thought maybe for 2.6.
		break;
	    case 'z':
		tz_local = 
		    (optarg[0] == 'l' || optarg[0] == 'L')
		    && (optarg[1] == 't' || optarg[1] == 'T')
		    && !optarg[2];
		if (tz_local)
		    tz_name = optarg;
		else
		{
		    /*
		     * Convert a known time with the given timezone to time_t.
		     * Use the epoch + 23 hours, so timezones east of GMT work.
		     */
		    static char f[] = "1/1/1970 23:00 %s";
		    char *buf = (char*)xmalloc (sizeof (f) - 2 + strlen (optarg));
		    time_t t;
		    sprintf (buf, f, optarg);
		    t = get_date (buf, (struct timeb *) NULL);
		    xfree (buf);
		    if (t == (time_t) -1)
			error (0, 0, "%s is not a known time zone", optarg);
		    else
		    {
			/*
			 * Convert to seconds east of GMT, removing the
			 * 23-hour offset mentioned above.
			 */
			tz_seconds_east_of_GMT = (time_t)23 * 60 * 60  -  t;
			tz_name = optarg;
		    }
		}
		break;
	    case '?':
	    default:
		usage (history_usg);
		break;
	}
    }
    argc -= optind;
    argv += optind;
    for (i = 0; i < argc; i++)
		save_file ("", argv[i]);


    /* ================ Now analyze the arguments a bit */
    if (!report_count)
		v_checkout++;
    else if (report_count > 1)
		error (1, 0, "Only one report type allowed from: \"-Tcomxe\".");

    if (current_parsed_root->isremote)
    {
		if (tag_report)
		    send_arg("-T");
		if (all_users)
		    send_arg("-a");
		if (modified)
			send_arg("-c");
		if (last_entry)
		    send_arg("-l");
		if (v_checkout)
			send_arg("-o");
		if (working)
			send_arg("-w");
		if (since_date)
			client_senddate (since_date);
		if (backto)
			option_with_arg ("-b", backto);
		for (std::vector<cvs::filename>::const_iterator i = file_list.begin(); i!=file_list.end(); ++i)
		{
			if (i->c_str()[0] == '*')
				option_with_arg ("-p", i->substr(1).c_str());
			else
				option_with_arg ("-f", i->c_str());
		}
		if (module_report)
			send_arg("-m");
		for (std::vector<cvs::filename>::const_iterator i = mod_list.begin(); i!=mod_list.end(); ++i)
			option_with_arg ("-n", i->c_str());
		if (since_rev)
			option_with_arg ("-r", since_rev);
		if (since_tag)
			option_with_arg ("-t", since_tag);
		for (std::vector<cvs::username>::const_iterator i = user_list.begin(); i!=user_list.end(); ++i)
			option_with_arg ("-u", i->c_str());
		for (std::vector<cvs::string>::const_iterator i = bug_list.begin(); i!=bug_list.end(); ++i)
			option_with_arg ("-B", i->c_str());
		if (extract)
			option_with_arg ("-x", rec_types.c_str());
		option_with_arg ("-z", tz_name);

		send_to_server ("history\n", 0);
        return get_responses_and_close ();
    }

    if (all_users)
		save_user ("");

    if (tag_report)
    {
		if (!strchr (rec_types.c_str(), 'T'))
		{
			rec_types+="T";
		}
    }
    else if (extract)
    {
		if (user_list.size())
			user_sort++;
    }
    else if (modified)
    {
		rec_types = "MAR";
	/*
	 * If the user has not specified a date oriented flag ("Since"), sort
	 * by Repository/file before date.  Default is "just" date.
	 */
	if (last_entry || (!since_date && !since_rev && !since_tag && !backto))
	{
	    repos_sort++;
	    file_sort++;
	    /*
	     * If we are not looking for last_modified and the user specified
	     * one or more users to look at, sort by user before filename.
	     */
	    if (!last_entry && user_list.size())
			user_sort++;
	}
    }
    else if (module_report)
    {
		rec_types = last_entry ? "OMAR" : ALL_REC_TYPES;
		module_sort++;
		repos_sort++;
		file_sort++;
		working = 0;			/* User's workdir doesn't count here */
    }
    else
	/* Must be "checkout" or default */
    {
		rec_types = "OF";
	/* See comments in "modified" above */
		if (!last_entry && user_list.size())
			user_sort++;
		if (last_entry || (!since_date && !since_rev && !since_tag && !backto))
			file_sort++;
    }

    /* If no users were specified, use self (-a saves a universal ("") user) */
    if (!user_list.size())
		save_user (getcaller ());

    /* If we're looking back to a Tag value, must consider "Tag" records */
    if (since_tag && !strchr (rec_types.c_str(), 'T'))
    {
		rec_types+="T";
    }

	cvs::sprintf(fname, 80, "%s/%s/%s", current_parsed_root->directory, CVSROOTADM, CVSROOTADM_HISTORY);

	if(!CFileAccess::exists(fname.c_str()))
	{
		error(1,0,"History logging is not enabled on this repository");
		return 0;
	}

    read_hrecs (fname.c_str());

    if(hrec_list.size())
		std::sort(hrec_list.begin(), hrec_list.end(), sort_order);
    report_hrecs ();
	xfree (since_date);

    return 0;
}

void history_write (int type, const char *update_dir, const char *revs, const char *name, const char *repository, const char *bugid, const char *message)
{
	cvs::string fname;
	cvs::string workdir;
	cvs::string real_workdir;
    const char *username = getcaller ();
    CFileAccess acc;
	cvs::string line;
    char *slash = "", *cp;
    const char *repos,*cp2;
    int i;
    static char *tilde = "";
    static char *PrCurDir = NULL;

    TRACE(3,"history_write(%c,%s,%s,%s,%s,%s,%s)",type,PATCH_NULL(update_dir),PATCH_NULL(revs),PATCH_NULL(name),PATCH_NULL(repository),PATCH_NULL(bugid),PATCH_NULL(message));

    if ( strchr(ALL_REC_TYPES, type) == NULL )	
		return;

	if (noexec)
		return;

	cvs::sprintf(fname, 80, "%s/%s/%s", current_parsed_root->directory, CVSROOTADM, CVSROOTADM_HISTORY);

	if (CFileAccess::exists(fname.c_str()))
    {
		TRACE(1,"fopen(%s,a)",fname.c_str());
		if(!acc.open(fname.c_str(),"a+"))
		{
			if (! really_quiet)
			{
				error (0, errno, "warning: cannot write to history file %s", fn_root(fname.c_str()));
			}
			return;
		}
	}
    repos = Short_Repository (repository);
    if (!PrCurDir)
    {
		char *pwdir;

		pwdir = get_homedir ();
		PrCurDir = CurDir;
		if (pwdir != NULL)
		{
		    /* Assumes neither CurDir nor pwdir ends in '/' */
		    i = strlen (pwdir);
			if (!strncmp (CurDir, pwdir, i))
			{
				PrCurDir += i;		/* Point to '/' separator */
				tilde = "~";
			}
			else
			{
				/* Try harder to find a "homedir" */
				struct saved_cwd cwd;
				char *homedir;

				if (save_cwd (&cwd))
					error_exit ();

				if ( CVS_CHDIR (pwdir) >= 0)
				{
					homedir = xgetwd_mapped ();
					if (homedir == NULL)
						error (1, errno, "can't getwd in %s", pwdir);

					if (restore_cwd (&cwd, NULL))
						error_exit ();
					free_cwd (&cwd);

					i = strlen (homedir);
					if (!strncmp (CurDir, homedir, i))
					{
						PrCurDir += i;	/* Point to '/' separator */
						tilde = "~";
					}
					xfree (homedir);
				}
		    }
		}
    }

    if (type == 'T')
    {
		repos = update_dir;
		update_dir = "";
    }
    else if (update_dir && *update_dir)
		slash = "/";
    else
		update_dir = "";
	cvs::sprintf(workdir, 80, "%s%s%s%s", tilde, PrCurDir, slash, update_dir);

    /*
     * "workdir" is the directory where the file "name" is. ("^~" == $HOME)
     * "repos"	is the Repository, relative to $CVSROOT where the RCS file is.
     *
     * "$workdir/$name" is the working file name.
     * "$CVSROOT/$repos/$name,v" is the RCS file in the Repository.
     *
     * First, note that the history format was intended to save space, not
     * to be human readable.
     *
     * The working file directory ("workdir") and the Repository ("repos")
     * usually end with the same one or more directory elements.  To avoid
     * duplication (and save space), the "workdir" field ends with
     * an integer offset into the "repos" field.  This offset indicates the
     * beginning of the "tail" of "repos", after which all characters are
     * duplicates.
     *
     * In other words, if the "workdir" field has a '*' (a very stupid thing
     * to put in a filename) in it, then every thing following the last '*'
     * is a hex offset into "repos" of the first character from "repos" to
     * append to "workdir" to finish the pathname.
     *
     * It might be easier to look at an example:
     *
     *  M273b3463|dgg|~/work*9|usr/local/cvs/examples|1.2|loginfo
     *
     * Indicates that the workdir is really "~/work/cvs/examples", saving
     * 10 characters, where "~/work*d" would save 6 characters and mean that
     * the workdir is really "~/work/examples".  It will mean more on
     * directories like: usr/local/gnu/emacs/dist-19.17/lisp/term
     *
     * "workdir" is always an absolute pathname (~/xxx is an absolute path)
     * "repos" is always a relative pathname.  So we can assume that we will
     * never run into the top of "workdir" -- there will always be a '/' or
     * a '~' at the head of "workdir" that is not matched by anything in
     * "repos".  On the other hand, we *can* run off the top of "repos".
     *
     * Only "compress" if we save characters.
     */

    if (!repos)
		repos = "";

	real_workdir = fn_root(workdir.c_str());

    cp = (char*)workdir.c_str() + workdir.length() - 1;
    cp2 = repos + strlen (repos) - 1;
    for (i = 0; cp2 >= repos && cp > workdir && *cp == *cp2--; cp--)
		i++;

    if (i > 2)
    {
		i = strlen (repos) - i;
		sprintf ((cp + 1), "*%x", i);
    }

    if (!revs)
		revs = "";


	if(acc.isopen())
	{
		cvs::sprintf(line,80,"%c%08"TIME_T_SPRINTF"x|%s|%s|%s|%s|%s|%s\n",
	     		type, global_session_time_t, username, workdir.c_str(), repos, revs, name, bugid?bugid:"");
		if(!acc.write(line.c_str(),line.length()))
			error (1, errno, "cannot write to history file: %s", fn_root(fname.c_str()));
		if(!acc.close())
			error (1, errno, "cannot close history file: %s", fn_root(fname.c_str()));
	}

	historyproc_param_t args;
	args.type = type;
	args.workdir = real_workdir.c_str();
	args.bugid = bugid;
	args.message = message;
	args.revs=revs;
	args.name=name;

	TRACE(3,"run history trigger");
	if (run_trigger (&args, historyinfo_proc) > 0)
	{
		error (0, 0, "Historyinfo logging failed");
	}
}

/*
 * save_user() adds a user name to the user list to select.  Zero-length
 *		username ("") matches any user.
 */
static void save_user (const char *name)
{
	user_list.push_back(name);
}

static void save_bugid (const char *bugid)
{
	char *bug = xstrdup(bugid);
	char *p = strtok(bug,",");
	while(p)
	{
		bug_list.push_back(p);
		p=strtok(NULL,",");
	}
}

/*
 * save_file() adds file name and associated module to the file list to select.
 *
 * If "dir" is null, store a file name as is.
 * If "name" is null, store a directory name with a '*' on the front.
 * Else, store concatenated "dir/name".
 *
 * Later, in the "select" stage:
 *	- if it starts with '*', it is prefix-matched against the repository.
 *	- if it has a '/' in it, it is matched against the repository/file.
 *	- else it is matched against the file name.
 */
static void save_file (const char *dir, const char *name)
{
	cvs::filename fn;

    if (dir && *dir)
    {
		if (name && *name)
			cvs::sprintf(fn,80,"%s/%s",dir,name);
		else
			cvs::sprintf(fn,80,"*%s",dir);
    }
    else
    {
		if (name && *name)
			fn = name;
		else
			error (0, 0, "save_file: null dir and file name");
    }
	file_list.push_back(fn);
}

static void save_module (const char *module)
{
	mod_list.push_back(module);
}

/* fill_hrec
 *
 * Take a ptr to 7 or 8-part history line, ending with a newline, for example:
 *
 *	M273b3463|dgg|~/work*9|usr/local/cvs/examples|1.2|loginfo|mybug
 *
 * Split it into 7 parts and drop the parts into a "struct hrec".
 * Return a pointer to the character following the newline.
 * 
 */

#define NEXT_BAR(here,what) do { \
	while (isspace(*line)) line++; \
	const char *cpp = line; \
	while ((c = *line++) && c != '|') ; \
	if (!c) { --line; what; }; line[-1] = '\0'; \
	hr->here = cpp; \
	} while (0)

static void fill_hrec (char *line, hrec_t *hr)
{
    char *cp;
    int c;

    hr->date = -1;
    hr->idx = ++hrec_idx;

    while (isspace ((unsigned char) *line))
		line++;

    hr->type = *(line++);
	sscanf(line,"%"TIME_T_SPRINTF"x",&hr->date);
	cp=line;
	while(*cp && (isdigit((unsigned char)*cp) || (tolower((unsigned char)*cp)>='a' && tolower((unsigned char)*cp)<='f')))
		cp++;
    if (cp == line || *cp != '|')
		return;
    line = cp + 1;
    NEXT_BAR (user,return);
	if(!strcmp(hr->user.c_str(),"(null)")) // Old bug
	    NEXT_BAR (user,return);
	NEXT_BAR (dir,return);
    if ((cp = (char*)strrchr (hr->dir.c_str(), '*')) != NULL)
    {
		*cp++ = '\0';
		hr->end = line + strtoul (cp, NULL, 16);
    }
    else
		hr->end = line - 1;		/* A handy pointer to '\0' */
    NEXT_BAR (repos,return);
	NEXT_BAR (rev,{hr->rev=cpp;break;});
	if(!*line)
		return;
    if (strchr ("FOET", hr->type))
		hr->mod = line;
    NEXT_BAR (file,break);
	if(*line)
		NEXT_BAR (bugid,break);
}

/* read_hrecs's job is to read the history file and fill in all the "hrec"
 * (history record) array elements with the ones we need to print.
 *
 * Logic:
 * - Read a block from the file. 
 * - Walk through the block parsing line into hr records. 
 * - if the hr isn't used, free its strings, if it is, bump the hrec counter
 * - at the end of a block, copy the end of the current block to the start 
 * of space for the next block, then read in the next block.  If we get less
 * than the whole block, we're done. 
 */
static void read_hrecs (const char *fname)
{
    CFileAccess acc;
	cvs::string line;

	if(!acc.open(fname,"r"))
		error (1, errno, "cannot open history file: %s", fname);

	hrec_list.clear();
	last_since_tag.first = false;
	last_backto.first = false;
	while(acc.getline(line))
	{
		hrec_t hr;
		int  since = 0, backto = 0;

		fill_hrec ((char*)line.c_str(), &hr);
		if (select_hrec (&hr, since, backto))
		{
			hrec_list.push_back(hr);
			if(since == 2)
			{
				last_since_tag.first = true;
				last_since_tag.second = hrec_list.end() - 1;
				since = 1;
			}
			if(backto == 2)
			{
				last_backto.first = true;
				last_backto.second = hrec_list.end() - 1;
				backto = 1;
			}
		}
    }
    acc.close();

    /* Special selection problem: If "since_tag" is set, we have saved every
     * record from the 1st occurrence of "since_tag", when we want to save
     * records since the *last* occurrence of "since_tag".  So what we have
     * to do is bump hrec_head forward and reduce hrec_count accordingly.
     */
   
    if (last_since_tag.first)
    {
		hrec_list.erase(hrec_list.begin(),last_since_tag.second);
    }

    /* Much the same thing is necessary for the "backto" option. */
    if (last_backto.first)
    {
		hrec_list.erase(hrec_list.begin(),last_backto.second);
    }
}

/* The purpose of "select_hrec" is to apply the selection criteria based on
 * the command arguments and defaults and return a flag indicating whether
 * this record should be remembered for printing.
 */
static int select_hrec (hrec_t *hr, int s_since, int s_backto)
{
    /* "Since" checking:  The argument parser guarantees that only one of the
     *			  following four choices is set:
     *
     * 1. If "since_date" is set, it contains the date specified on the
     *    command line. hr->date fields earlier than "since_date" are ignored.
     * 2. If "since_rev" is set, it contains either an RCS "dotted" revision
     *    number (which is of limited use) or a symbolic TAG.  Each RCS file
     *    is examined and the date on the specified revision (or the revision
     *    corresponding to the TAG) in the RCS file (CVSROOT/repos/file) is
     *    compared against hr->date as in 1. above.
     * 3. If "since_tag" is set, matching tag records are saved.  The field
     *    "last_since_tag" is set to the last one of these.  Since we don't
     *    know where the last one will be, all records are saved from the
     *    first occurrence of the TAG.  Later, at the end of "select_hrec"
     *    records before the last occurrence of "since_tag" are skipped.
     * 4. If "backto" is set, all records with a module name or file name
     *    matching "backto" are saved.  In addition, all records with a
     *    repository field with a *prefix* matching "backto" are saved.
     *    The field "last_backto" is set to the last one of these.  As in
     *    3. above, "select_hrec" adjusts to include the last one later on.
     */
    if (since_date)
    {
		char *ourdate = date_from_time_t (hr->date);
		int diff = RCS_datecmp (ourdate, since_date);
		xfree (ourdate);
		if (diff < 0)
			return 0;
    }
    else if (since_rev)
    {
		Vers_TS *vers;
		time_t t;
		struct file_info finfo;

		memset (&finfo, 0, sizeof finfo);
		finfo.file = hr->file.c_str();
		/* Not used, so don't worry about it.  */
		finfo.update_dir = NULL;
		finfo.fullname = finfo.file;
		finfo.repository = hr->repos.c_str();
		finfo.entries = NULL;
		finfo.rcs = NULL;

		vers = Version_TS (&finfo, (char *) NULL, since_rev, (char *) NULL, 1, 0, 0);
		if (vers->vn_rcs)
		{
			if ((t = RCS_getrevtime (vers->srcfile, vers->vn_rcs, (char *) 0, 0)) != (time_t) 0)
			{
				if (hr->date < t)
				{
					freevers_ts (&vers);
					return 0;
				}
			}
		}
		freevers_ts (&vers);
    }
    else if (since_tag)
    {
		if (hr->type == 'T')
		{
			/*
			* A 'T'ag record, the "rev" field holds the tag to be set,
			* while the "repos" field holds "D"elete, "A"dd or a rev.
			*/
			if (strstr (since_tag, hr->rev.c_str()))
			{
				s_since = 2;
				return 1;
			}
			else
				return 0;
		}
		if(!s_since)
			return false;
    }
    else if (backto)
    {
		if (strstr (backto, hr->file.c_str()) || strstr (backto, hr->mod.c_str()) || strstr (backto, hr->repos.c_str()))
			s_backto = 2;
		else
		    return 0;
    }

    /* User checking:
     *
     * Run down "user_list", match username ("" matches anything)
     * If "" is not there and actual username is not there, return failure.
     */
    if (user_list.size() && hr->user.size())
    {
		bool found = false;
		for (size_t n=0; n<user_list.size(); n++)
		{
			if(!user_list[n].size() || !usercmp(hr->user.c_str(),user_list[n].c_str()))
			{
				found = true;
				break;
			}
		}
		if(!found)
			return 0;
    }

    /* Record type checking:
     *
     * 1. If Record type is not in rec_types field, skip it.
     * 2. If mod_list is null, keep everything.  Otherwise keep only modules
     *    on mod_list.
     * 3. If neither a 'T', 'F' nor 'O' record, run through "file_list".  If
     *    file_list is null, keep everything.  Otherwise, keep only files on
     *    file_list, matched appropriately.
     */
    if (!strchr (rec_types.c_str(), hr->type))
		return 0;

    if (!strchr ("TFOE", hr->type) && file_list.size())	/* Don't bother with "file" if "TFOE" */
    {
		bool found = false;

		for(std::vector<cvs::filename>::const_iterator i = file_list.begin(); i!=file_list.end(); ++i)
		{
			const char *cp, *cp2;
			/* 1. If file_list entry starts with '*', skip the '*' and
			*    compare it against the repository in the hrec.
			* 2. If file_list entry has a '/' in it, compare it against
			*    the concatenation of the repository and file from hrec.
			* 3. Else compare the file_list entry against the hrec file.
			*/

			if (*(cp = i->c_str()) == '*')
			{
			    cp++;
				/* if argument to -p is a prefix of repository */
				if (!fnncmp (cp, hr->repos.c_str(), strlen (cp)))
				{
					found = true;
					break;
				}
			}
			else
			{
				cvs::string cmpfile;
				if (strchr (cp, '/'))
				{
					cvs::sprintf(cmpfile,80,"%s/%s",hr->repos.c_str(),hr->file.c_str());
					cp2 = cmpfile.c_str();
			    }
			    else
			    {
					cp2 = hr->file.c_str();
			    }

			    /* if requested file is found within {repos}/file fields */
			    if (*cp2 && *cp && strstr (cp2, cp))
				{
					found = true;
					break;
			    }
			}
	    }
	    if (!found)
			return 0;		/* String specified and no match */
    }

    if (mod_list.size() && hr->mod.size())
    {
		if(std::find(mod_list.begin(),mod_list.end(),hr->mod)==mod_list.end())
			return 0;/* Module specified & this record is not one of them. */
    }

    return 1;		/* Select this record unless rejected above. */
}

/* The "sort_order" routine (when handed to qsort) has arranged for the
 * hrecs files to be in the right order for the report.
 *
 * Most of the "selections" are done in the select_hrec routine, but some
 * selections are more easily done after the qsort by "accept_hrec".
 */
static void report_hrecs ()
{
    struct tm *tm;
    int ty;
    char *cp;
    int user_len, file_len, rev_len, mod_len, repos_len;

    if (since_tag && !last_since_tag.first)
    {
		printf ("No tag found: %s\n", since_tag);
		return;
    }
    else if (backto && !last_backto.first)
    {
		printf ("No module, file or repository with: %s\n", backto);
		return;
    }
    else if (hrec_list.size() < 1)
    {
		printf ("No records selected.\n");
		return;
    }

    user_len = file_len = rev_len = mod_len = repos_len = 0;

    /* Run through lists and find maximum field widths */
	std::vector<hrec_t>::iterator lr = hrec_list.begin();
	for(std::vector<hrec_t>::iterator hr = hrec_list.begin(); lr!=hrec_list.end(); lr = hr++)
    {
		if(hr == hrec_list.begin())
			continue;

		if (!accept_hrec (&(*lr), (hr==hrec_list.end())?NULL:&(*hr)))
		    continue;

		ty = lr->type;
		cvs::filename repos = lr->repos;
		if ((cp = (char*)strrchr (repos.c_str(), '/')) != NULL)
		{
			if (lr->mod.size() && !fncmp (++cp, lr->mod.c_str()))
				strcpy (cp, "*");
		}

		if(lr->user.length()>user_len)
			user_len=lr->user.length();
		if(lr->file.length()>file_len)
			file_len=lr->file.length();
		if(ty != 'T' && repos.length()>repos_len)
			repos_len=repos.length();
		if(ty != 'T' && lr->rev.length()>rev_len)
			rev_len=lr->rev.length();
		if(lr->mod.size() && lr->mod.length()>mod_len)
			mod_len=lr->mod.length();
    }

    /* Walk through hrec array setting "lr" (Last Record) to each element.
     * "hr" points to the record following "lr" -- It is NULL in the last
     * pass.
     *
     * There are two sections in the loop below:
     * 1. Based on the report type (e.g. extract, checkout, tag, etc.),
     *    decide whether the record should be printed.
     * 2. Based on the record type, format and print the data.
     */
	lr = hrec_list.begin();
	for(std::vector<hrec_t>::iterator hr = hrec_list.begin(); lr!=hrec_list.end(); lr = hr++)
    {
		if(hr == hrec_list.begin())
			continue;

		if (!accept_hrec (&(*lr), (hr==hrec_list.end())?NULL:&(*hr)))
		    continue;

		ty = lr->type;
		if (!tz_local)
		{
			time_t t = lr->date + tz_seconds_east_of_GMT;
			tm = gmtime (&t);
		}
		else
			tm = localtime (&(lr->date));

		printf ("%c %04d-%02d-%02d %02d:%02d %s %-*s", ty,
		  tm->tm_year+1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
		  tm->tm_min, tz_name, user_len, lr->user.c_str());

		cvs::filename workdir,repos;
		cvs::sprintf(workdir,80, "%s%s", lr->dir.c_str(), lr->end.c_str());

		if ((cp = (char*)strrchr (workdir.c_str(), '/')) != NULL)
		{
			if (lr->mod.size() && !fncmp (++cp, lr->mod.c_str()))
				strcpy (cp, "*");
		}

		repos = lr->repos;
		if ((cp = (char*)strrchr (repos.c_str(), '/')) != NULL)
		{
			if (lr->mod.size() && !fncmp (++cp, lr->mod.c_str()))
				strcpy (cp, "*");
	    }

		switch (ty)
		{
			case 'T':
				/* 'T'ag records: repository is a "tag type", rev is the tag */
				printf (" %-*s [%s:%s]", mod_len, lr->mod.c_str(), lr->rev.c_str(), repos.c_str());
				if (working)
					printf (" {%s}", workdir.c_str());
				break;
			case 'F':
			case 'E':
			case 'O':
				if (lr->rev.size())
					printf (" [%s]", lr->rev.c_str());
				printf (" %-*s =%s%-*s %s", repos_len, repos.c_str(), lr->mod.c_str(), 
						mod_len + 1 - (int) strlen (lr->mod.c_str()), "=", workdir.c_str());
				break;
			case 'W':
			case 'U':
			case 'C':
			case 'G':
			case 'M':
			case 'A':
			case 'R':
			case 'e':
			case 'u':
				printf (" %-*s %-*s %-*s =%s= %s", rev_len, lr->rev.c_str(),
						file_len, lr->file.c_str(), repos_len, repos.c_str(), lr->mod.c_str(), workdir.c_str());
			break;
			default:
				printf ("Hey! What is this junk? RecType[0x%2.2x]", ty);
			break;
		}
		putchar ('\n');
    }
}

static int accept_hrec (const hrec_t *lr, const hrec_t *hr)
{
    int ty;

    ty = lr->type;

    if (last_since_tag.first && ty == 'T')
		return 1;

    if (v_checkout)
    {
		if (ty != 'O')
		    return 0;			/* Only interested in 'O' records */

		/* We want to identify all the states that cause the next record
		* ("hr") to be different from the current one ("lr") and only
		* print a line at the allowed boundaries.
		*/

		if (!hr ||			/* The last record */
			usercmp (hr->user.c_str(), lr->user.c_str()) ||	/* User has changed */
			fncmp (hr->mod.c_str(), lr->mod.c_str()) || /* Module has changed */
		    strcmp (hr->bugid.c_str(), lr->bugid.c_str()) || /* Bugid has changed */
			(working &&			/* If must match "workdir" */
			(fncmp (hr->dir.c_str(), lr->dir.c_str()) ||	/*    and the 1st parts or */
			fncmp (hr->end.c_str(), lr->end.c_str()))))	/*    the 2nd parts differ */

	    return 1;
    }
    else if (modified)
    {
		if (!last_entry ||		/* Don't want only last rec */
			!hr ||			/* Last entry is a "last entry" */
			fncmp (hr->repos.c_str(), lr->repos.c_str()) ||	/* Repository has changed */
			fncmp (hr->file.c_str(), lr->file.c_str()) || /* File has changed */
		    strcmp (hr->bugid.c_str(), lr->bugid.c_str()))/* Bugid has changed */
			return 1;

		if (working)
		{				/* If must match "workdir" */
			if (fncmp (hr->dir.c_str(), lr->dir.c_str()) ||	/*    and the 1st parts or */
			fncmp (hr->end.c_str(), lr->end.c_str()))	/*    the 2nd parts differ */
			return 1;
		}
    }
    else if (module_report)
    {
	if (!last_entry ||		/* Don't want only last rec */
	    !hr ||			/* Last entry is a "last entry" */
	    fncmp (hr->mod.c_str(), lr->mod.c_str()) ||/* Module has changed */
	    fncmp (hr->repos.c_str(), lr->repos.c_str()) ||	/* Repository has changed */
	    fncmp (hr->file.c_str(), lr->file.c_str()) || /* File has changed */
	    strcmp (hr->bugid.c_str(), lr->bugid.c_str()))/* Bugid has changed */
	    return 1;
    }
    else
    {
		/* "extract" and "tag_report" always print selected records. */
		return 1;
    }

    return 0;
}
