/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Difference
 * 
 * Run diff against versions in the repository.  Options that are specified are
 * passed on directly to "rcsdiff".
 * 
 * Without any file arguments, runs diff against all the currently modified
 * files.
 */

#include "cvs.h"

enum diff_file
{
    DIFF_ERROR,
    DIFF_ADDED,
    DIFF_REMOVED,
    DIFF_DIFFERENT,
    DIFF_SAME
};

static Dtype diff_dirproc (void *callerdat, char *dir,
				  char *pos_repos, char *update_dir,
				  List *entries, const char *virtual_repository, Dtype hint);
static int diff_filesdoneproc (void *callerdat, int err,
				      char *repos, char *update_dir,
				      List *entries);
static int diff_dirleaveproc (void *callerdat, char *dir,
				     int err, char *update_dir,
				     List *entries);
static enum diff_file diff_file_nodiff(struct file_info *finfo, Vers_TS *vers, enum diff_file, const char *default_branch);
static int diff_fileproc (void *callerdat, struct file_info *finfo);
static void diff_mark_errors (int err);


/* Global variables.  Would be cleaner if we just put this stuff in a
   struct like log.c does.  */

/* Command line tags, from -r option.  Points into argv.  */
static char *diff_rev1, *diff_rev2;
/* Command line dates, from -D option.  Malloc'd.  */
static char *diff_date1, *diff_date2;
static char *use_rev1, *use_rev2;
static int have_rev1_label, have_rev2_label;

/* Revision of the user file, if it is unchanged from something in the
   repository and we want to use that fact.  */
static char *user_file_rev;

static char *options;
static char *opts;
static size_t opts_allocated = 1;
static int diff_errors;
static int empty_files = 0;
static int is_rcs;

/* FIXME: should be documenting all the options here.  They don't
   perfectly match rcsdiff options (for example, we always support
   --ifdef and --context, but rcsdiff only does if diff does).  */
static const char *const diff_usage[] =
{
    "Usage: %s %s [-lNR] [rcsdiff-options]\n",
    "    [[-r rev1 | -D date1] [-r rev2 | -D date2]] [files...] \n",
    "\t-l\tLocal directory only, not recursive\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-D d1\tDiff revision for date against working file.\n",
    "\t-D d2\tDiff rev1/date1 against date2.\n",
    "\t-N\tinclude diffs for added and removed files.\n",
    "\t-r rev1\tDiff revision for rev1 against working file.\n",
    "\t-r rev2\tDiff rev1/date1 against rev2.\n",
    "\t--ifdef=arg\tOutput diffs in ifdef format.\n",
    "(consult the documentation for your diff program for rcsdiff-options.\n",
    "The most popular is -c for context diffs but there are many more).\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

/* I copied this array directly out of diff.c in diffutils 2.7, after
   removing the following entries, none of which seem relevant to use
   with CVS:
     --help
     --version
     --recursive
     --unidirectional-new-file
     --starting-file
     --exclude
     --exclude-from
     --sdiff-merge-assist

   I changed the options which take optional arguments (--context and
   --unified) to return a number rather than a letter, so that the
   optional argument could be handled more easily.  I changed the
   --paginate and --brief options to return a number, since -l and -q
   mean something else to cvs diff.

   The numbers 129- that appear in the fourth element of some entries
   tell the big switch in `diff' how to process those options. -- Ian

   The following options, which diff lists as "An alias, no longer
   recommended" have been removed: --file-label --entire-new-file
   --ascii --print.  */

static struct option const longopts[] =
{
    {"ignore-blank-lines", 0, 0, 'B'},
    {"context", 2, 0, 143},
    {"ifdef", 1, 0, 131},
    {"show-function-line", 1, 0, 'F'},
    {"speed-large-files", 0, 0, 'H'},
    {"ignore-matching-lines", 1, 0, 'I'},
    {"label", 1, 0, 'L'},
    {"new-file", 0, 0, 'N'},
    {"initial-tab", 0, 0, 'T'},
    {"width", 1, 0, 'W'},
    {"text", 0, 0, 'a'},
    {"ignore-space-change", 0, 0, 'b'},
    {"minimal", 0, 0, 'd'},
    {"ed", 0, 0, 'e'},
    {"forward-ed", 0, 0, 'f'},
    {"ignore-case", 0, 0, 'i'},
    {"paginate", 0, 0, 144},
    {"rcs", 0, 0, 'n'},
    {"show-c-function", 0, 0, 'p'},

    /* This is a potentially very useful option, except the output is so
       silly.  It would be much better for it to look like "cvs rdiff -s"
       which displays all the same info, minus quite a few lines of
       extraneous garbage.  */
    {"brief", 0, 0, 145},

    {"report-identical-files", 0, 0, 's'},
    {"expand-tabs", 0, 0, 't'},
    {"ignore-all-space", 0, 0, 'w'},
    {"side-by-side", 0, 0, 'y'},
    {"unified", 2, 0, 146},
    {"left-column", 0, 0, 129},
    {"suppress-common-lines", 0, 0, 130},
    {"old-line-format", 1, 0, 132},
    {"new-line-format", 1, 0, 133},
    {"unchanged-line-format", 1, 0, 134},
    {"line-format", 1, 0, 135},
    {"old-group-format", 1, 0, 136},
    {"new-group-format", 1, 0, 137},
    {"unchanged-group-format", 1, 0, 138},
    {"changed-group-format", 1, 0, 139},
    {"horizon-lines", 1, 0, 140},
    {"binary", 0, 0, 142},
    {0, 0, 0, 0}
};

/* CVS 1.9 and similar versions seemed to have pretty weird handling
   of -y and -T.  In the cases where it called rcsdiff,
   they would have the meanings mentioned below.  In the cases where it
   called diff, they would have the meanings mentioned in "longopts".
   Noone seems to have missed them, so I think the right thing to do is
   just to remove the options altogether (which I have done).

   In the case of -z and -q, "cvs diff" did not accept them even back
   when we called rcsdiff (at least, it hasn't accepted them
   recently).

   In comparing rcsdiff to the new CVS implementation, I noticed that
   the following rcsdiff flags are not handled by CVS diff:

	   -y: perform diff even when the requested revisions are the
		   same revision number
	   -q: run quietly
	   -T: preserve modification time on the RCS file
	   -z: specify timezone for use in file labels

   I think these are not really relevant.  -y is undocumented even in
   RCS 5.7, and seems like a minor change at best.  According to RCS
   documentation, -T only applies when a RCS file has been modified
   because of lock changes; doesn't CVS sidestep RCS's entire lock
   structure?  -z seems to be unsupported by CVS diff, and has a
   different meaning as a global option anyway.  (Adding it could be
   a feature, but if it is left out for now, it should not break
   anything.)  For the purposes of producing output, CVS diff appears
   mostly to ignore -q.  Maybe this should be fixed, but I think it's
   a larger issue than the changes included here.  */

int diff (int argc, char **argv)
{
    char tmp[50];
    int c, err = 0;
    int local = 0;
    int which;
    int option_index;

	is_rcs = (strcmp (command_name, "rcsfile") == 0);

    if (argc == -1)
		usage (diff_usage);

    have_rev1_label = have_rev2_label = 0;

    /*
     * Note that we catch all the valid arguments here, so that we can
     * intercept the -r arguments for doing revision diffs; and -l/-R for a
     * non-recursive/recursive diff.
     */

    /* Clean out our global variables (multiroot can call us multiple
       times and the server can too, if the client sends several
       diff commands).  */
    if (opts == NULL)
    {
	opts_allocated = 1;
	opts = (char*)xmalloc (opts_allocated);
    }
    opts[0] = '\0';
    diff_rev1 = NULL;
    diff_rev2 = NULL;
    diff_date1 = NULL;
    diff_date2 = NULL;

    optind = 0;
    while ((c = getopt_long (argc, argv,
	       "+abcdefhilnpstuwy0123456789BHNRTC:D:F:I:L:U:V:W:k:r:",
			     longopts, &option_index)) != -1)
    {
	switch (c)
	{
	    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
	    case 'h': case 'i': case 'n': case 'p': case 's': case 't':
	    case 'u': case 'w': case 'y':
            case '0': case '1': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9':
	    case 'B': case 'H': case 'T':
		(void) sprintf (tmp, " -%c", (char) c);
		allocate_and_strcat (&opts, &opts_allocated, tmp);
		break;
	    case 'L':
		if (have_rev1_label++)
		    if (have_rev2_label++)
		    {
			error (0, 0, "extra -L arguments ignored");
			break;
		    }

	        allocate_and_strcat (&opts, &opts_allocated, " -L");
	        allocate_and_strcat (&opts, &opts_allocated, optarg);
		break;
	    case 'C': case 'F': case 'I': case 'U': case 'V': case 'W':
		(void) sprintf (tmp, " -%c", (char) c);
		allocate_and_strcat (&opts, &opts_allocated, tmp);
		allocate_and_strcat (&opts, &opts_allocated, optarg);
		break;
	    case 131:
		/* --ifdef.  */
		allocate_and_strcat (&opts, &opts_allocated, " --ifdef=");
		allocate_and_strcat (&opts, &opts_allocated, optarg);
		break;
	    case 129: case 130:           case 132: case 133: case 134:
	    case 135: case 136: case 137: case 138: case 139: case 140:
	    case 141: case 142: case 143: case 144: case 145: case 146:
		allocate_and_strcat (&opts, &opts_allocated, " --");
		allocate_and_strcat (&opts, &opts_allocated,
				     longopts[option_index].name);
		if (longopts[option_index].has_arg == 1
		    || (longopts[option_index].has_arg == 2
			&& optarg != NULL))
		{
		    allocate_and_strcat (&opts, &opts_allocated, "=");
		    allocate_and_strcat (&opts, &opts_allocated, optarg);
		}
		break;
	    case 'R':
		local = 0;
		break;
	    case 'l':
		local = 1;
		break;
	    case 'k':
		if (options)
		    xfree (options);
		options = RCS_check_kflag (optarg,true,true);
		break;
	    case 'r':
		if (diff_rev2 != NULL || diff_date2 != NULL)
		    error (1, 0,
		       "no more than two revisions/dates can be specified");
		if (diff_rev1 != NULL || diff_date1 != NULL)
		    diff_rev2 = optarg;
		else
		    diff_rev1 = optarg;
		break;
	    case 'D':
		if (diff_rev2 != NULL || diff_date2 != NULL)
		    error (1, 0,
		       "no more than two revisions/dates can be specified");
		if (diff_rev1 != NULL || diff_date1 != NULL)
		    diff_date2 = Make_Date (optarg);
		else
		    diff_date1 = Make_Date (optarg);
		break;
	    case 'N':
		empty_files = 1;
		break;
	    case '?':
	    default:
		usage (diff_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    /* make sure options is non-null */
    if (!options)
		options = xstrdup ("");

    if (!is_rcs && current_parsed_root->isremote)
	{
		if (local)
			send_arg("-l");
		if (empty_files)
			send_arg("-N");
		send_option_string (opts);
		if (options[0] != '\0')
			option_with_arg ("-k", options);
		if (diff_rev1)
			option_with_arg ("-r", diff_rev1);
		if (diff_date1)
			client_senddate (diff_date1);
		if (diff_rev2)
			option_with_arg ("-r", diff_rev2);
		if (diff_date2)
			client_senddate (diff_date2);

		send_arg("--");
		/* Send the current files unless diffing two revs from the archive */
		if (diff_rev2 == NULL && diff_date2 == NULL)
			send_files (argc, argv, local, 0, 0);
		else
			send_files (argc, argv, local, 0, SEND_NO_CONTENTS);

		send_file_names (argc, argv, SEND_EXPAND_WILD);

		send_to_server ("diff\n", 0);
			err = get_responses_and_close ();
		xfree (options);
		options = NULL;
		return (err);
    }

	if(is_rcs)
	{
		int n;

		if(!argc)
			usage(diff_usage);

		for(n=0; n<argc; n++)
		{
			struct file_info finfo = {0};
			const char *name;
			char *tmp = find_rcs_filename(argv[n]);

			if(!tmp)
				error(1,ENOENT,"%s",argv[n]);

			finfo.fullname=fullpathname(tmp, &name);
			finfo.file = xstrdup(name);
			char *ff = xstrdup(finfo.fullname);
			finfo.update_dir = ff;
			ff[(name-finfo.fullname)-1]='\0';

			finfo.rcs = RCS_fopen(finfo.fullname);
			if(finfo.rcs)
			{
				ff = (char*)finfo.fullname;
				ff[strlen(finfo.fullname)-2]='\0';
				ff = (char*)finfo.file;
				ff[strlen(finfo.file)-2]='\0';
				err+=diff_fileproc(NULL,&finfo);
				freercsnode(&finfo.rcs);
			}
			else
			{
				error(1,ENOENT,"%s",tmp);
				err++;
			}
			xfree(finfo.fullname);
			xfree(finfo.file);
			xfree(finfo.update_dir);
			xfree(tmp);
		}
	}
	else
	{
		if (diff_rev1 != NULL)
			tag_check_valid (diff_rev1, argc, argv, local, 0, "");
		if (diff_rev2 != NULL)
			tag_check_valid (diff_rev2, argc, argv, local, 0, "");

		which = W_LOCAL;
		if (diff_rev1 != NULL || diff_date1 != NULL)
			which |= W_REPOS;

		/* start the recursion processor */
		err = start_recursion (diff_fileproc, diff_filesdoneproc, (PREDIRENTPROC) NULL, diff_dirproc,
				diff_dirleaveproc, NULL, argc, argv, local,
				which, 0, 1, (char *) NULL, NULL, 1, verify_read, diff_rev1);
	}

    /* clean up */
    xfree (options);
    options = NULL;

    if (diff_date1 != NULL)
		xfree (diff_date1);
    if (diff_date2 != NULL)
		xfree (diff_date2);

    return (err);
}

/*
 * Do a file diff
 */
/* ARGSUSED */
static int diff_fileproc (void *callerdat, struct file_info *finfo)
{
    int status, err = 2;		/* 2 == trouble, like rcsdiff */
    Vers_TS *vers;
    enum diff_file empty_file = DIFF_DIFFERENT;
    char *tmp;
    char *fname;
    char *label1;
    char *label2;
	char *default_branch;

    /* Initialize these solely to avoid warnings from gcc -Wall about
       variables that might be used uninitialized.  */
    tmp = NULL;
    fname = NULL;

    user_file_rev = 0;
    vers = Version_TS (finfo, NULL, NULL, NULL, 1, 0, 0);
	if(vers->tag && RCS_isbranch(finfo->rcs, vers->tag))
		default_branch = vers->tag;
	else
		default_branch = NULL;

    if (diff_rev2 != NULL || diff_date2 != NULL)
    {
	/* Skip all the following checks regarding the user file; we're
	   not using it.  */
    }
    else if (vers->vn_user == NULL)
    {
	/* The file does not exist in the working directory.  */
	if ((diff_rev1 != NULL || diff_date1 != NULL)
	    && vers->srcfile != NULL)
	{
	    /* The file does exist in the repository.  */
	    if (empty_files)
		empty_file = DIFF_REMOVED;
	    else
	    {
		int exists;

		exists = 0;
		/* special handling for TAG_HEAD */
		if (diff_rev1 && strcmp (diff_rev1, TAG_HEAD) == 0)
		{
		    char *head =
			(vers->vn_rcs == NULL
			 ? NULL
			 : RCS_branch_head (vers->srcfile, vers->vn_rcs));
		    exists = head != NULL;
		    if (head != NULL)
			xfree (head);
		}
		else
		{
		    Vers_TS *xvers;

			xvers = Version_TS (finfo, NULL, diff_rev1?diff_rev1:default_branch, diff_date1,
					1, 0, 0);
		    exists = xvers->vn_rcs != NULL;
		    freevers_ts (&xvers);
		}
		if (exists)
		    error (0, 0,
			   "%s no longer exists, no comparison available",
			   fn_root(finfo->fullname));
		freevers_ts (&vers);
		diff_mark_errors (err);
		return (err);
	    }
	}
	else
	{
	    error (0, 0, "I know nothing about %s", fn_root(finfo->fullname));
	    freevers_ts (&vers);
	    diff_mark_errors (err);
	    return (err);
	}
    }
    else if (vers->vn_user[0] == '0' && vers->vn_user[1] == '\0')
    {
	if (empty_files)
	    empty_file = DIFF_ADDED;
	else
	{
	    error (0, 0, "%s is a new entry, no comparison available",
		   fn_root(finfo->fullname));
	    freevers_ts (&vers);
	    diff_mark_errors (err);
	    return (err);
	}
    }
    else if (vers->vn_user[0] == '-')
    {
	if (empty_files)
	    empty_file = DIFF_REMOVED;
	else
	{
	    error (0, 0, "%s was removed, no comparison available",
		   fn_root(finfo->fullname));
	    freevers_ts (&vers);
	    diff_mark_errors (err);
	    return (err);
	}
    }
    else
    {
	if (vers->vn_rcs == NULL && vers->srcfile == NULL)
	{
	    error (0, 0, "cannot find revision control file for %s",
		   fn_root(finfo->fullname));
	    freevers_ts (&vers);
	    diff_mark_errors (err);
	    return (err);
	}
	else
	{
	    if (vers->ts_user == NULL)
	    {
		error (0, 0, "cannot find %s", fn_root(finfo->fullname));
		freevers_ts (&vers);
		diff_mark_errors (err);
		return (err);
	    }
	    else if (!strcmp (vers->ts_user, vers->ts_rcs)) 
	    {
		/* The user file matches some revision in the repository
		   Diff against the repository (for remote CVS, we might not
		   have a copy of the user file around).  */
		user_file_rev = vers->vn_user;
	    }
	}
    }
    empty_file = diff_file_nodiff (finfo, vers, empty_file, default_branch);
    if (empty_file == DIFF_SAME || empty_file == DIFF_ERROR)
    {
	freevers_ts (&vers);
	if (empty_file == DIFF_SAME)
	{
	    /* In the server case, would be nice to send a "Checked-in"
	       response, so that the client can rewrite its timestamp.
	       server_checked_in by itself isn't the right thing (it
	       needs a server_register), but I'm not sure what is.
	       It isn't clear to me how "cvs status" handles this (that
	       is, for a client which sends Modified not Is-modified to
	       "cvs status"), but it does.  */
	    return (0);
	}
	else
	{
	    diff_mark_errors (err);
	    return (err);
	}
    }

    if (empty_file == DIFF_DIFFERENT)
    {
	int dead1, dead2;

	if (use_rev1 == NULL)
	    dead1 = 0;
	else
	    dead1 = RCS_isdead (vers->srcfile, use_rev1);
	if (use_rev2 == NULL)
	    dead2 = 0;
	else
	    dead2 = RCS_isdead (vers->srcfile, use_rev2);

	if (dead1 && dead2)
	{
	    freevers_ts (&vers);
	    return (0);
	}
	else if (dead1)
	{
	    if (empty_files)
	        empty_file = DIFF_ADDED;
	    else
	    {
		error (0, 0, "%s is a new entry, no comparison available",
		       fn_root(finfo->fullname));
		freevers_ts (&vers);
		diff_mark_errors (err);
		return (err);
	    }
	}
	else if (dead2)
	{
	    if (empty_files)
		empty_file = DIFF_REMOVED;
	    else
	    {
		error (0, 0, "%s was removed, no comparison available",
		       fn_root(finfo->fullname));
		freevers_ts (&vers);
		diff_mark_errors (err);
		return (err);
	    }
	}
    }

    /* Output an "Index:" line for patch to use */
	if(!is_rcs)
	{
		cvs_output ("Index: ", 0);
		cvs_output (fn_root(finfo->fullname), 0);
		cvs_output ("\n", 1);
	}

    /* Set up file labels appropriate for compatibility with the Larry Wall
     * implementation of patch if the user didn't specify.  This is irrelevant
     * according to the POSIX.2 specification.
     */
    label1 = NULL;
    label2 = NULL;
    if (!have_rev1_label)
    {
	if (empty_file == DIFF_ADDED)
	{
	    TRACE(3,"diff_fileproc(); !have_rev1_label; empty_file == DIFF_ADDED make_file_label(is_rcs=%d)",is_rcs);

	    label1 =
		make_file_label (DEVNULL, NULL, NULL, is_rcs);
	}
	else
	{
	    TRACE(3,"diff_fileproc(); !have_rev1_label; empty_file != DIFF_ADDED make_file_label(is_rcs=%d)",is_rcs);

	    label1 =
		make_file_label (is_rcs?fn_root(finfo->file):fn_root(finfo->fullname), use_rev1, vers ? vers->srcfile : NULL, is_rcs);
	}
    }

    if (!have_rev2_label)
    {
	if (empty_file == DIFF_REMOVED)
	{
	    TRACE(3,"diff_fileproc(); !have_rev2_label; empty_file == DIFF_REMOVED make_file_label(is_rcs=%d)",is_rcs);

	    label2 =
		make_file_label (DEVNULL, NULL, NULL, is_rcs);
	}
	else
	{
	    TRACE(3,"diff_fileproc(); !have_rev2_label; empty_file != DIFF_REMOVED make_file_label(is_rcs=%d)",is_rcs);

	    label2 =
		make_file_label (is_rcs?fn_root(finfo->file):fn_root(finfo->fullname), use_rev2, vers ? vers->srcfile : NULL, is_rcs);
	}
    }

    if (empty_file == DIFF_ADDED || empty_file == DIFF_REMOVED)
    {
	/* This is fullname, not file, possibly despite the POSIX.2
	 * specification, because that's the way all the Larry Wall
	 * implementations of patch (are there other implementations?) want
	 * things and the POSIX.2 spec appears to leave room for this.
	 */
	cvs_output ("\
===================================================================\n\
RCS file: ", 0);
	cvs_output (fn_root(finfo->fullname), 0);
	cvs_output ("\n", 1);

	cvs_output ("diff -N ", 0);
	cvs_output (fn_root(finfo->fullname), 0);
	cvs_output ("\n", 1);

	if (empty_file == DIFF_ADDED)
	{
	    if (use_rev2 == NULL)
		status = diff_exec (DEVNULL, finfo->file, label1, label2, opts, RUN_TTY);
	    else
	    {
		int retcode;

		tmp = cvs_temp_name ();
		retcode = RCS_checkout (vers->srcfile, (char *) NULL,
					use_rev2, (char *) NULL,
					vers->options,
					tmp, (RCSCHECKOUTPROC) NULL,
					(void *) NULL, NULL);
		if (retcode != 0)
		{
		    diff_mark_errors (err);
		    return err;
		}

		status = diff_exec (DEVNULL, tmp, label1, label2, opts, RUN_TTY);
	    }
	}
	else
	{
	    int retcode;

	    tmp = cvs_temp_name ();
	    retcode = RCS_checkout (vers->srcfile, (char *) NULL,
				    use_rev1, (char *) NULL,
				    *options ? options : vers->options,
				    tmp, (RCSCHECKOUTPROC) NULL,
				    (void *) NULL, NULL);
	    if (retcode != 0)
	    {
		diff_mark_errors (err);
		return err;
	    }

	    status = diff_exec (tmp, DEVNULL, label1, label2, opts, RUN_TTY);
	}
    }
    else
    {
	status = RCS_exec_rcsdiff (vers->srcfile, opts,
				   *options ? options : vers->options,
				   use_rev1, use_rev2,
				   label1, label2,
				   finfo->file);

	if (label1) xfree (label1);
	if (label2) xfree (label2);
    }

    switch (status)
    {
	case -1:			/* fork failed */
	    error (1, errno, "fork failed while diffing %s",
		   vers->srcfile->path);
	case 0:				/* everything ok */
	    err = 0;
	    break;
	default:			/* other error */
	    err = status;
	    break;
    }

    if (empty_file == DIFF_REMOVED
	|| (empty_file == DIFF_ADDED && use_rev2 != NULL))
    {
	if (CVS_UNLINK (tmp) < 0)
	    error (0, errno, "cannot remove %s", tmp);
	xfree (tmp);
    }

    freevers_ts (&vers);
    diff_mark_errors (err);
    return (err);
}

/*
 * Remember the exit status for each file.
 */
static void diff_mark_errors (int err)
{
    if (err > diff_errors)
	diff_errors = err;
}

/*
 * Print a warm fuzzy message when we enter a dir
 *
 * Don't try to diff directories that don't exist! -- DW
 */
/* ARGSUSED */
static Dtype diff_dirproc (void *callerdat, char *dir, char *repos, char *update_dir, List *entries, const char *virtual_repository, Dtype hint)
{
    /* XXX - check for dirs we don't want to process??? */

    /* YES ... for instance dirs that don't exist!!! -- DW */
	if(hint!=R_PROCESS)
		return hint;

    if (!quiet)
	error (0, 0, "Diffing %s", update_dir);
    return (R_PROCESS);
}

/*
 * Concoct the proper exit status - done with files
 */
/* ARGSUSED */
static int diff_filesdoneproc (void *callerdat, int err, char *repos, char *update_dir, List *entries)
{
    return (diff_errors);
}

/*
 * Concoct the proper exit status - leaving directories
 */
/* ARGSUSED */
static int diff_dirleaveproc (void *callerdat, char *dir, int err, char *update_dir, List *entries)
{
    return (diff_errors);
}

/*
 * verify that a file is different
 */
static enum diff_file diff_file_nodiff(struct file_info *finfo, Vers_TS *vers, enum diff_file empty_file, const char *default_branch)
{
    Vers_TS *xvers;
    int retcode;

    /* free up any old use_rev* variables and reset 'em */
    if (use_rev1)
	xfree (use_rev1);
    if (use_rev2)
	xfree (use_rev2);
    use_rev1 = use_rev2 = (char *) NULL;

    if (diff_rev1 || diff_date1)
    {
		xvers = Version_TS(finfo, NULL, diff_rev1?diff_rev1:default_branch, diff_date1, 1, 0, 0);
	    if (xvers->vn_rcs != NULL)
		use_rev1 = xstrdup (xvers->vn_rcs);
	    freevers_ts (&xvers);
    }
    if (diff_rev2 || diff_date2)
    {
	    xvers = Version_TS(finfo, NULL, diff_rev2?diff_rev2:default_branch, diff_date2, 1, 0, 0);
	    if (xvers->vn_rcs != NULL)
		use_rev2 = xstrdup (xvers->vn_rcs);
	    freevers_ts (&xvers);

	if (use_rev1 == NULL)
	{
	    /* The first revision does not exist.  If EMPTY_FILES is
               true, treat this as an added file.  Otherwise, warn
               about the missing tag.  */
	    if (use_rev2 == NULL)
		/* At least in the case where DIFF_REV1 and DIFF_REV2
		   are both numeric, we should be returning some kind
		   of error (see basicb-8a0 in testsuite).  The symbolic
		   case may be more complicated.  */
		return DIFF_SAME;
	    else if (empty_files)
		return DIFF_ADDED;
	    else if (diff_rev1)
		error (0, 0, "tag %s is not in file %s", diff_rev1,
		       fn_root(finfo->fullname));
	    else
		error (0, 0, "no revision for date %s in file %s",
		       diff_date1, fn_root(finfo->fullname));
	    return DIFF_ERROR;
	}

	if (use_rev2 == NULL)
	{
	    /* The second revision does not exist.  If EMPTY_FILES is
               true, treat this as a removed file.  Otherwise warn
               about the missing tag.  */
	    if (empty_files)
		return DIFF_REMOVED;
	    else if (diff_rev2)
		error (0, 0, "tag %s is not in file %s", diff_rev2,
		       fn_root(finfo->fullname));
	    else
		error (0, 0, "no revision for date %s in file %s",
		       diff_date2, fn_root(finfo->fullname));
	    return DIFF_ERROR;
	}

	/* now, see if we really need to do the diff */
	if (strcmp (use_rev1, use_rev2) == 0)
	    return DIFF_SAME;
	else
	    return DIFF_DIFFERENT;
    }

    if ((diff_rev1 || diff_date1) && use_rev1 == NULL)
    {
	/* The first revision does not exist, and no second revision
           was given.  */
	if (empty_files)
	{
	    if (empty_file == DIFF_REMOVED)
		return DIFF_SAME;
	    else
	    {
		if (user_file_rev && use_rev2 == NULL)
		    use_rev2 = xstrdup (user_file_rev);
		return DIFF_ADDED;
	    }
	}
	else
	{
	    if (diff_rev1)
		error (0, 0, "tag %s is not in file %s", diff_rev1,
		       fn_root(finfo->fullname));
	    else
		error (0, 0, "no revision for date %s in file %s",
		       diff_date1, fn_root(finfo->fullname));
	    return DIFF_ERROR;
	}
    }

    if (user_file_rev)
    {
        /* drop user_file_rev into first unused use_rev */
        if (!use_rev1) 
	    use_rev1 = xstrdup (user_file_rev);
	else if (!use_rev2)
	    use_rev2 = xstrdup (user_file_rev);
	/* and if not, it wasn't needed anyhow */
	user_file_rev = 0;
    }

    /* now, see if we really need to do the diff */
    if (use_rev1 && use_rev2) 
    {
	if (strcmp (use_rev1, use_rev2) == 0)
	    return DIFF_SAME;
	else
	    return DIFF_DIFFERENT;
    }

    if (use_rev1 == NULL
	|| (vers->vn_user != NULL && strcmp (use_rev1, vers->vn_user) == 0))
    {
	if (use_rev1 == NULL
	    && (vers->vn_user[0] != '0' || vers->vn_user[1] != '\0'))
	{
	    if (vers->vn_user[0] == '-')
		use_rev1 = xstrdup (vers->vn_user + 1);
	    else
		use_rev1 = xstrdup (vers->vn_user);
	}

	if (empty_file == DIFF_DIFFERENT
	    && vers->ts_user != NULL
	    && strcmp (vers->ts_rcs, vers->ts_user) == 0)
	{
	    if((!*options) || !strcmp(options, vers->options))
	    {
	        return DIFF_SAME;
	    }

	    if(!isfile(finfo->file))
	    {
	        /* Checkout the file, which wasn't sent by the server, for
	         * checking with different options */


	        retcode = RCS_checkout (vers->srcfile, (char *) NULL,
				    use_rev1, (char *) NULL,
				    vers->options,
				    finfo->file, (RCSCHECKOUTPROC) NULL,
				    (void *) NULL, NULL);
		if (retcode != 0)
		{
		    diff_mark_errors (retcode);
		    return DIFF_DIFFERENT;
		}
	    }
	}
    }

    /* If we already know that the file is being added or removed,
       then we don't want to do an actual file comparison here.  */
    if (empty_file != DIFF_DIFFERENT)
	return empty_file;

    /*
     * with 0 or 1 -r option specified, run a quick diff to see if we
     * should bother with it at all.
     */

	TRACE(3,"RCS_cmp_file() called in diff_file_nodiff()");
    retcode = RCS_cmp_file (vers->srcfile, use_rev1,
			    *options ? options : vers->options,
			    finfo->file, 0);

    return retcode == 0 ? DIFF_SAME : DIFF_DIFFERENT;
}
