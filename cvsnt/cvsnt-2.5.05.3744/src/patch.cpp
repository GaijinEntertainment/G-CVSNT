/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Patch
 * 
 * Create a Larry Wall format "patch" file between a previous release and the
 * current head of a module, or between two releases.  Can specify the
 * release as either a date or a revision number.
 */

#include "cvs.h"
#include "getline.h"

static RETSIGTYPE patch_cleanup(int sig);
static Dtype patch_dirproc (void *callerdat, char *dir,
				   char *repos, char *update_dir,
				   List *entries, const char *virtual_repository, Dtype hint);
static int patch_fileproc (void *callerdat, struct file_info *finfo);
static int patch_proc (int argc, char **argv, const char *xwhere,
		       const char *mwhere, const char *mfile, int shorten,
		       int local_specified, const char *mname, const char *msg);

static int force_tag_match = 1;
static int patch_short = 0;
static int toptwo_diffs = 0;
static int local = 0;
static char *options = NULL;
static char *rev1 = NULL;
static int rev1_validated = 0;
static char *rev2 = NULL;
static int rev2_validated = 0;
static char *date1 = NULL;
static char *date2 = NULL;
static char *tmpfile1 = NULL;
static char *tmpfile2 = NULL;
static char *tmpfile3 = NULL;
static int unidiff = 0;

static const char *const patch_usage[] =
{
    "Usage: %s %s [-flR] [-c|-u] [-s|-t] [-V %%d]\n",
    "    -r rev|-D date [-r rev2 | -D date2] modules...\n",
    "\t-f\tForce a head revision match if tag/date not found.\n",
    "\t-l\tLocal directory only, not recursive\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-c\tContext diffs (default)\n",
    "\t-u\tUnidiff format.\n",
    "\t-s\tShort patch - one liner per file.\n",
    "\t-t\tTop two diffs - last change made to the file.\n",
    "\t-D date\tDate.\n",
    "\t-r rev\tRevision - symbolic or numeric.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

int patch (int argc, char **argv)
{
    register int i;
    int c;
    int err = 0;
    DBM *db;

    if (argc == -1)
	usage (patch_usage);

    optind = 0;
    while ((c = getopt (argc, argv, "+:k:cuftsQqlRD:r:")) != -1)
    {
	switch (c)
	{
	    case 'Q':
	    case 'q':
		/* The CVS 1.5 client sends these options (in addition to
		   Global_option requests), so we must ignore them.  */
		if (!server_active)
		    error (1, 0,
			   "-q or -Q must be specified before \"%s\"",
			   command_name);
		break;
	    case 'f':
		force_tag_match = 0;
		break;
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case 't':
		toptwo_diffs = 1;
		break;
	    case 's':
		patch_short = 1;
		break;
	    case 'D':
		if (rev2 != NULL || date2 != NULL)
		    error (1, 0,
		       "no more than two revisions/dates can be specified");
		if (rev1 != NULL || date1 != NULL)
		    date2 = Make_Date (optarg);
		else
		    date1 = Make_Date (optarg);
		break;
	    case 'r':
		if (rev2 != NULL || date2 != NULL)
		    error (1, 0,
		       "no more than two revisions/dates can be specified");
		if (rev1 != NULL || date1 != NULL)
		    rev2 = optarg;
		else
		    rev1 = optarg;
		break;
	    case 'k':
		if (options)
		    xfree (options);
		options = RCS_check_kflag (optarg,true,true);
		break;
	    case 'u':
		unidiff = 1;		/* Unidiff */
		break;
	    case 'c':			/* Context diff */
		unidiff = 0;
		break;
	    case '?':
	    default:
		usage (patch_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    /* Sanity checks */
    if (argc < 1)
	usage (patch_usage);

    if (toptwo_diffs && patch_short)
	error (1, 0, "-t and -s options are mutually exclusive");
    if (toptwo_diffs && (date1 != NULL || date2 != NULL ||
			 rev1 != NULL || rev2 != NULL))
	error (1, 0, "must not specify revisions/dates with -t option!");

    if (!toptwo_diffs && (date1 == NULL && date2 == NULL &&
			  rev1 == NULL && rev2 == NULL))
	error (1, 0, "must specify at least one revision/date!");
    if (date1 != NULL && date2 != NULL)
	if (RCS_datecmp (date1, date2) >= 0)
	    error (1, 0, "second date must come after first date!");

    /* if options is NULL, make it a NULL string */
    if (options == NULL)
	options = xstrdup ("");

    if (current_parsed_root->isremote)
    {
	if (local)
	    send_arg("-l");
	if (!force_tag_match)
	    send_arg("-f");
	if (toptwo_diffs)
	    send_arg("-t");
	if (patch_short)
	    send_arg("-s");
	if (unidiff)
	    send_arg("-u");

	if (rev1)
	    option_with_arg ("-r", rev1);
	if (date1)
	    client_senddate (date1);
	if (rev2)
	    option_with_arg ("-r", rev2);
	if (date2)
	    client_senddate (date2);
	if (options[0] != '\0')
	    option_with_arg ("-k", options);

	{
	    int i;
	    for (i = 0; i < argc; ++i)
		send_arg (argv[i]);
	}

	send_to_server ("rdiff\n", 0);
        return get_responses_and_close ();
    }

    /* clean up if we get a signal */
#ifdef SIGABRT
    SIG_register (SIGABRT, patch_cleanup);
#endif
#ifdef SIGHUP
    SIG_register (SIGHUP, patch_cleanup);
#endif
#ifdef SIGINT
    SIG_register (SIGINT, patch_cleanup);
#endif
#ifdef SIGQUIT
    SIG_register (SIGQUIT, patch_cleanup);
#endif
#ifdef SIGPIPE
    SIG_register (SIGPIPE, patch_cleanup);
#endif
#ifdef SIGTERM
    SIG_register (SIGTERM, patch_cleanup);
#endif

    db = open_module ();
    for (i = 0; i < argc; i++)
	err += do_module (db, argv[i], PATCH, "Patching", patch_proc,
			  (char *) NULL, 0, 0, 0, 0, (char *) NULL);
    close_module (db);
    xfree (options);
    patch_cleanup (0);
    return (err);
}

/*
 * callback proc for doing the real work of patching
 */
/* ARGSUSED */
static int patch_proc (int argc, char **argv, const char *xwhere,
		       const char *mwhere, const char *mfile, int shorten,
		       int local_specified, const char *mname, const char *msg)
{
    char *myargv[2];
    int err = 0;
    int which;
    char *repository;
    char *where;
//	const char *message;

    repository = (char*)xmalloc (strlen (current_parsed_root->directory) + strlen (argv[0])
			  + (mfile == NULL ? 0 : strlen (mfile) + 1) + 2);
    (void) sprintf (repository, "%s/%s", current_parsed_root->directory, argv[0]);
    where = (char*)xmalloc (strlen (argv[0]) + (mfile == NULL ? 0 : strlen (mfile) + 1)
		     + 1);
    (void) strcpy (where, argv[0]);

    /* if mfile isn't null, we need to set up to do only part of the module */
    if (mfile != NULL)
    {
	char *cp;
	char *path;

	/* if the portion of the module is a path, put the dir part on repos */
	if ((cp = (char*)strrchr (mfile, '/')) != NULL)
	{
	    *cp = '\0';
	    (void) strcat (repository, "/");
	    (void) strcat (repository, mfile);
	    (void) strcat (where, "/");
	    (void) strcat (where, mfile);
	    mfile = cp + 1;
	}

	/* take care of the rest */
	path = (char*)xmalloc (strlen (repository) + strlen (mfile) + 2);
	(void) sprintf (path, "%s/%s", repository, mfile);
	if (isdir (path))
	{
	    /* directory means repository gets the dir tacked on */
	    (void) strcpy (repository, path);
	    (void) strcat (where, "/");
	    (void) strcat (where, mfile);
	}
	else
	{
	    myargv[0] = argv[0];
	    myargv[1] = (char*)mfile;
	    argc = 2;
	    argv = myargv;
	}
	xfree (path);
    }

    /* cd to the starting repository */
    if ( CVS_CHDIR (repository) < 0)
    {
	error (0, errno, "cannot chdir to %s", repository);
	xfree (repository);
	return (1);
    }

	which = W_REPOS;

    if (rev1 != NULL && !rev1_validated)
    {
	tag_check_valid (rev1, argc - 1, argv + 1, local, 0, NULL);
	rev1_validated = 1;
    }
    if (rev2 != NULL && !rev2_validated)
    {
	tag_check_valid (rev2, argc - 1, argv + 1, local, 0, NULL);
	rev2_validated = 1;
    }

    /* start the recursion processor */
    err = start_recursion (patch_fileproc, (FILESDONEPROC) NULL, (PREDIRENTPROC) NULL, patch_dirproc,
			   (DIRLEAVEPROC) NULL, NULL,
			   argc - 1, argv + 1, local,
			   which, 0, 1, where, repository, 1, verify_read, rev1);
    xfree (where);
    xfree (repository);

    return (err);
}

/*
 * Called to examine a particular RCS file, as appropriate with the options
 * that were set above.
 */
/* ARGSUSED */
static int patch_fileproc (void *callerdat, struct file_info *finfo)
{
    struct utimbuf t;
    char *vers_tag, *vers_head;
    char *rcs = NULL;
    RCSNode *rcsfile;
    FILE *fp1, *fp2, *fp3;
    int ret = 0;
    int retcode = 0;
    char *file1;
    char *file2;
    char *strippath;
    char *line1, *line2;
    size_t line1_chars_allocated;
    size_t line2_chars_allocated;
    char *cp1, *cp2;
    FILE *fp;
    int line_length;
    char *nrev2;

    line1 = NULL;
    line1_chars_allocated = 0;
    line2 = NULL;
    line2_chars_allocated = 0;

    /* find the parsed rcs file */
    if ((rcsfile = finfo->rcs) == NULL)
    {
	ret = 1;
	goto out2;
    }

    rcs = (char*)xmalloc (strlen (finfo->file) + sizeof (RCSEXT) + 5);
    (void) sprintf (rcs, "%s%s", finfo->file, RCSEXT);
 
    /* Keep 'on branch' so we don't diff against an arbitrary HEAD */
    if(!rev2 && !date2)
    {
	nrev2 = RCS_branch_head(rcsfile, rev1);
	if(!nrev2)
		nrev2 = xstrdup(rev1);
    }
    else
	nrev2=xstrdup(rev2);

    /* if vers_head is NULL, may have been removed from the release */
	vers_head = RCS_getversion (rcsfile, nrev2, date2, force_tag_match,
				    (int *) NULL);
	if (vers_head != NULL && RCS_isdead (rcsfile, vers_head))
	{
	    xfree (vers_head);
	    vers_head = NULL;
	}

    if (toptwo_diffs)
    {
	if (vers_head == NULL)
	{
	    ret = 1;
	    goto out2;
	}

	if (!date1)
	    date1 = (char*)xmalloc (MAXDATELEN);
	*date1 = '\0';
	if (RCS_getrevtime (rcsfile, vers_head, date1, 1) == -1)
	{
	    if (!really_quiet)
		error (0, 0, "cannot find date in rcs file %s revision %s",
		       rcs, vers_head);
	    ret = 1;
	    goto out2;
	}
    }
    vers_tag = RCS_getversion (rcsfile, rev1, date1, force_tag_match,
			       (int *) NULL);
    if (vers_tag != NULL && RCS_isdead (rcsfile, vers_tag))
    {
        xfree (vers_tag);
	vers_tag = NULL;
    }

    if (vers_tag == NULL && vers_head == NULL)
    {
	/* Nothing known about specified revs.  */
	ret = 0;
	goto out2;
    }

    if (vers_tag && vers_head && strcmp (vers_head, vers_tag) == 0)
    {
	/* Not changed between releases.  */
	ret = 0;
	goto out2;
    }

    if (patch_short)
    {
	cvs_output ("File ", 0);
	cvs_output (fn_root(finfo->fullname), 0);
	if (vers_tag == NULL)
	{
	    cvs_output (" is new; current revision ", 0);
	    cvs_output (vers_head, 0);
	    cvs_output ("\n", 1);
	}
	else if (vers_head == NULL)
	{
	    cvs_output (" is removed; not included in ", 0);
	    if (rev2 != NULL)
	    {
		cvs_output ("release tag ", 0);
		cvs_output (rev2, 0);
	    }
	    else if (date2 != NULL)
	    {
		cvs_output ("release date ", 0);
		cvs_output (date2, 0);
	    }
	    else
		cvs_output ("current release", 0);
	    cvs_output ("\n", 1);
	}
	else
	{
	    cvs_output (" changed from revision ", 0);
	    cvs_output (vers_tag, 0);
	    cvs_output (" to ", 0);
	    cvs_output (vers_head, 0);
	    cvs_output ("\n", 1);
	}
	ret = 0;
	goto out2;
    }

    /* Create 3 empty files.  I'm not really sure there is any advantage
     * to doing so now rather than just waiting until later.
     *
     * There is - cvs_temp_file opens the file so that it can guarantee that
     * we have exclusive write access to the file.  Unfortunately we spoil that
     * by closing it and reopening it again.  Of course any better solution
     * requires that the RCS functions accept open file pointers rather than
     * simple file names.
     */
    if ((fp1 = cvs_temp_file (&tmpfile1)) == NULL)
    {
	error (0, errno, "cannot create temporary file %s", tmpfile1);
	ret = 1;
	goto out;
    }
    else
	if (fclose (fp1) < 0)
	    error (0, errno, "warning: cannot close %s", tmpfile1);
    if ((fp2 = cvs_temp_file (&tmpfile2)) == NULL)
    {
	error (0, errno, "cannot create temporary file %s", tmpfile2);
	ret = 1;
	goto out;
    }
    else
	if (fclose (fp2) < 0)
	    error (0, errno, "warning: cannot close %s", tmpfile2);
    if ((fp3 = cvs_temp_file (&tmpfile3)) == NULL)
    {
	error (0, errno, "cannot create temporary file %s", tmpfile3);
	ret = 1;
	goto out;
    }
    else
	if (fclose (fp3) < 0)
	    error (0, errno, "warning: cannot close %s", tmpfile3);

    if (vers_tag != NULL)
    {
	retcode = RCS_checkout (rcsfile, (char *) NULL, vers_tag,
				rev1, options, tmpfile1,
				(RCSCHECKOUTPROC) NULL, (void *) NULL, NULL);
	if (retcode != 0)
	{
	    error (0, 0,
		   "cannot check out revision %s of %s", vers_tag, rcs);
	    ret = 1;
	    goto out;
	}
	memset ((char *) &t, 0, sizeof (t));
	if ((t.actime = t.modtime = RCS_getrevtime (rcsfile, vers_tag,
						    (char *) 0, 0)) != -1)
	    /* I believe this timestamp only affects the dates in our diffs,
	       and therefore should be on the server, not the client.  */
	    (void) utime (tmpfile1, &t);
    }
    else if (toptwo_diffs)
    {
	ret = 1;
	goto out;
    }
    if (vers_head != NULL)
    {
	retcode = RCS_checkout (rcsfile, (char *) NULL, vers_head,
				nrev2, options, tmpfile2,
				(RCSCHECKOUTPROC) NULL, (void *) NULL, NULL);
	if (retcode != 0)
	{
	    error (0, 0,
		   "cannot check out revision %s of %s", vers_head, rcs);
	    ret = 1;
	    goto out;
	}
	if ((t.actime = t.modtime = RCS_getrevtime (rcsfile, vers_head,
						    (char *) 0, 0)) != -1)
	    /* I believe this timestamp only affects the dates in our diffs,
	       and therefore should be on the server, not the client.  */
	    (void) utime (tmpfile2, &t);
    }

    switch (diff_exec (tmpfile1, tmpfile2, NULL, NULL, unidiff ? "-u" : "-c", tmpfile3))
    {
	case -1:			/* fork/wait failure */
	    error (1, errno, "fork for diff failed on %s", rcs);
	    break;
	case 0:				/* nothing to do */
	    break;
	case 1:
	    /*
	     * The two revisions are really different, so read the first two
	     * lines of the diff output file, and munge them to include more
	     * reasonable file names that "patch" will understand.
	     */

	    /* Output an "Index:" line for patch to use */
	    cvs_output ("Index: ", 0);
	    cvs_output (fn_root(finfo->fullname), 0);
	    cvs_output ("\n", 1);

	    fp = open_file (tmpfile3, "r");
	    if (getline (&line1, &line1_chars_allocated, fp) < 0 ||
		getline (&line2, &line2_chars_allocated, fp) < 0)
	    {
		if (feof (fp))
		    error (0, 0, "\
failed to read diff file header %s for %s: end of file", tmpfile3, rcs);
		else
		    error (0, errno,
			   "failed to read diff file header %s for %s",
			   tmpfile3, rcs);
		ret = 1;
		if (fclose (fp) < 0)
		    error (0, errno, "error closing %s", tmpfile3);
		goto out;
	    }
	    if (!unidiff)
	    {
		if (strncmp (line1, "*** ", 4) != 0 ||
		    strncmp (line2, "--- ", 4) != 0 ||
		    (cp1 = strchr (line1, '\t')) == NULL ||
		    (cp2 = strchr (line2, '\t')) == NULL)
		{
		    error (0, 0, "invalid diff header for %s", rcs);
		    ret = 1;
		    if (fclose (fp) < 0)
			error (0, errno, "error closing %s", tmpfile3);
		    goto out;
		}
	    }
	    else
	    {
		if (strncmp (line1, "--- ", 4) != 0 ||
		    strncmp (line2, "+++ ", 4) != 0 ||
		    (cp1 = strchr (line1, '\t')) == NULL ||
		    (cp2 = strchr  (line2, '\t')) == NULL)
		{
		    error (0, 0, "invalid unidiff header for %s", rcs);
		    ret = 1;
		    if (fclose (fp) < 0)
			error (0, errno, "error closing %s", tmpfile3);
		    goto out;
		}
	    }
	    assert (current_parsed_root != NULL);
	    assert (current_parsed_root->directory != NULL);
	    {
		strippath = (char*)xmalloc (strlen (current_parsed_root->directory) + 2);
		(void) sprintf (strippath, "%s/", current_parsed_root->directory);
	    }
	    /*else
		strippath = xstrdup (REPOS_STRIP); */
	    if (strncmp (rcs, strippath, strlen (strippath)) == 0)
		rcs += strlen (strippath);
	    xfree (strippath);
	    if (vers_tag != NULL)
	    {
		file1 = (char*)xmalloc (strlen (fn_root(finfo->fullname))
				 + strlen (vers_tag)
				 + 10);
		(void) sprintf (file1, "%s:%s", fn_root(finfo->fullname), vers_tag);
	    }
	    else
	    {
		file1 = xstrdup (DEVNULL);
	    }
	    file2 = (char*)xmalloc (strlen (fn_root(finfo->fullname))
			     + (vers_head != NULL ? strlen (vers_head) : 10)
			     + 10);
	    (void) sprintf (file2, "%s:%s", fn_root(finfo->fullname),
			    vers_head ? vers_head : "removed");

	    /* Note that the string "diff" is specified by POSIX (for -c)
	       and is part of the diff output format, not the name of a
	       program.  */
	    if (unidiff)
	    {
		cvs_output ("diff -u ", 0);
		cvs_output (file1, 0);
		cvs_output (" ", 1);
		cvs_output (file2, 0);
		cvs_output ("\n", 1);

		cvs_output ("--- ", 0);
		cvs_output (file1, 0);
		cvs_output (cp1, 0);
		cvs_output ("+++ ", 0);
	    }
	    else
	    {
		cvs_output ("diff -c ", 0);
		cvs_output (file1, 0);
		cvs_output (" ", 1);
		cvs_output (file2, 0);
		cvs_output ("\n", 1);

		cvs_output ("*** ", 0);
		cvs_output (file1, 0);
		cvs_output (cp1, 0);
		cvs_output ("--- ", 0);
	    }

	    cvs_output (fn_root(finfo->fullname), 0);
	    cvs_output (cp2, 0);

	    /* spew the rest of the diff out */
	    while ((line_length
		    = getline (&line1, &line1_chars_allocated, fp))
		   >= 0)
		cvs_output (line1, 0);
	    if (line_length < 0 && !feof (fp))
		error (0, errno, "cannot read %s", tmpfile3);

	    if (fclose (fp) < 0)
		error (0, errno, "cannot close %s", tmpfile3);
	    xfree (file1);
	    xfree (file2);
	    break;
	default:
	    error (0, 0, "diff failed for %s", fn_root(finfo->fullname));
    }
  out:
    if (line1)
        xfree (line1);
    if (line2)
        xfree (line2);
    if (CVS_UNLINK (tmpfile1) < 0)
	error (0, errno, "cannot unlink %s", tmpfile1);
    if (CVS_UNLINK (tmpfile2) < 0)
	error (0, errno, "cannot unlink %s", tmpfile2);
    if (CVS_UNLINK (tmpfile3) < 0)
	error (0, errno, "cannot unlink %s", tmpfile3);
    xfree (tmpfile1);
    xfree (tmpfile2);
    xfree (tmpfile3);
    tmpfile1 = tmpfile2 = tmpfile3 = NULL;

 out2:
    xfree(nrev2);
    xfree (vers_tag);
    xfree (vers_head);
    xfree (rcs);
    return (ret);
}

/*
 * Print a warm fuzzy message
 */
/* ARGSUSED */
static Dtype patch_dirproc (void *callerdat, char *dir, char *repos, char *update_dir, List *entries, const char *virtual_repository, Dtype hint)
{
	if(hint!=R_PROCESS)
		return hint;

    if (!quiet)
	error (0, 0, "Diffing %s", update_dir);
    return (R_PROCESS);
}

/*
 * Clean up temporary files
 */
static RETSIGTYPE patch_cleanup (int sig)
{
    /* Note that the checks for existence_error are because we are
       called from a signal handler, without SIG_begincrsect, so
       we don't know whether the files got created.  */

    if (tmpfile1 != NULL)
    {
	if (unlink_file (tmpfile1) < 0
	    && !existence_error (errno))
	    error (0, errno, "cannot remove %s", tmpfile1);
	xfree (tmpfile1);
    }
    if (tmpfile2 != NULL)
    {
	if (unlink_file (tmpfile2) < 0
	    && !existence_error (errno))
	    error (0, errno, "cannot remove %s", tmpfile2);
	xfree (tmpfile2);
    }
    if (tmpfile3 != NULL)
    {
	if (unlink_file (tmpfile3) < 0
	    && !existence_error (errno))
	    error (0, errno, "cannot remove %s", tmpfile3);
	xfree (tmpfile3);
    }
    tmpfile1 = tmpfile2 = tmpfile3 = NULL;
}
