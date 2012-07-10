/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 *
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Show last revision where each line modified
 * 
 * Prints the specified files with each line annotated with the revision
 * number where it was last modified.  With no argument, annotates all
 * all the files in the directory (recursive by default).
 */

#include "cvs.h"

/* Options from the command line.  */

static int force_tag_match = 1;
static char *tag = NULL;
static int tag_validated;
static char *date = NULL;

static int is_rannotate;
int annotate_width = 8;

static int annotate_fileproc(void *callerdat, struct file_info *);
static int rannotate_proc(int argc, char **argv, const char *xwhere,
				 const char *mwhere, const char *mfile, int shorten,
				 int local, const char *mname, const char *msg);

static const char *const annotate_usage[] =
{
    "Usage: %s %s [-lRf] [-r rev] [-D date] [files...]\n",
    "\t-l\tLocal directory only, no recursion.\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-f\tUse head revision if tag/date not found.\n",
    "\t-r rev\tAnnotate file as of specified revision/tag.\n",
    "\t-D date\tAnnotate file as of specified date.\n",
	"\t-w width\tModify width of username field (default 8).\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

/* Command to show the revision, date, and author where each line of a
   file was modified.  */

int annotate (int argc, char **argv)
{
    int local = 0;
    int err = 0;
    int c;

    is_rannotate = (strcmp(command_name, "rannotate") == 0);

    if (argc == -1)
	usage (annotate_usage);

    optind = 0;
	while ((c = getopt (argc, argv, "+lr:D:fRw:")) != -1)
    {
	switch (c)
	{
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case 'r':
	        tag = optarg;
		break;
	    case 'D':
	        date = Make_Date (optarg);
		break;
	    case 'f':
	        force_tag_match = 0;
		break;
		case 'w':
			annotate_width = atoi(optarg);
			break;
	    case '?':
	    default:
		usage (annotate_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    if (current_parsed_root->isremote)
    {
	if (is_rannotate && !supported_request ("rannotate"))
	    error (1, 0, "server does not support rannotate");

	if (local)
	    send_arg ("-l");
	if (!force_tag_match)
	    send_arg ("-f");
	option_with_arg ("-r", tag);
	if (date)
	    client_senddate (date);
	send_arg("--");
	if (is_rannotate)
	{
	    int i;
	    for (i = 0; i < argc; i++)
		send_arg (argv[i]);
	    send_to_server ("rannotate\n", 0);
	}
	else
	{
	    send_files (argc, argv, local, 0, SEND_NO_CONTENTS);
	    send_file_names (argc, argv, SEND_EXPAND_WILD);
	    send_to_server ("annotate\n", 0);
	}
	return get_responses_and_close ();
    }

    if (is_rannotate)
    {
		DBM *db;
		int i;
		db = open_module ();
		for (i = 0; i < argc; i++)
		{
			err += do_module (db, argv[i], MISC, "Annotating", rannotate_proc,
					(char *) NULL, 0, 0, 0, 0, (char *) NULL);
		}
		close_module (db);
    }
    else
    {
	err = rannotate_proc (argc + 1, argv - 1, (char *) NULL,
			 (char *) NULL, (char *) NULL, 0, 0, (char *) NULL,
			 (char *) NULL);
    }

    return err;
}
    

static int rannotate_proc(int argc, char **argv, const char *xwhere,
				 const char *mwhere, const char *mfile, int shorten,
				 int local, const char *mname, const char *msg)
{
    /* Begin section which is identical to patch_proc--should this
       be abstracted out somehow?  */
    char *myargv[2];
    int err = 0;
    int which;
    char *repository;
    char *where;

    if (is_rannotate)
    {
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
			path = (char*)xmalloc (strlen (repository) + strlen (mfile) + 5);
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

		{
		char *mapped_repository = map_repository(repository);

		/* cd to the starting repository */
		if ( CVS_CHDIR (mapped_repository) < 0)
		{
			error (0, errno, "cannot chdir to %s", fn_root(repository));
			xfree (repository);
			xfree (mapped_repository);
			return (1);
		}
		xfree (repository);
		xfree (mapped_repository);
		}
		/* End section which is identical to patch_proc.  */

		which = W_REPOS;
		repository = NULL;
    }
    else
    {
        where = NULL;
        which = W_LOCAL;
        repository = "";
    }

    if (tag != NULL && !tag_validated)
    {
	tag_check_valid (tag, argc - 1, argv + 1, local, 0, repository);
	tag_validated = 1;
    }

    err = start_recursion (annotate_fileproc, (FILESDONEPROC) NULL,
			   (PREDIRENTPROC) NULL, (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
			   argc - 1, argv + 1, local, which, 0, 1,
			   where, repository, 1, verify_read, tag);
    return err;
}


static int annotate_fileproc (void *callerdat, struct file_info *finfo)
{
    char *version;
    Vers_TS *vers;

    if (finfo->rcs == NULL)
        return (1);

	vers = Version_TS (finfo, NULL, tag, date, force_tag_match, 0, 0);
	version=xstrdup(vers->vn_rcs);
    freevers_ts (&vers);
    if (version == NULL)
        return 0;

    /* Distinguish output for various files if we are processing
       several files.  */
    cvs_outerr ("\nAnnotations for ", 0);
    cvs_outerr (fn_root(finfo->fullname), 0);
    cvs_outerr ("\n***************\n", 0);

    RCS_deltas (finfo->rcs, (FILE *) NULL, (struct rcsbuffer *) NULL,
		version, RCS_ANNOTATE, NULL, NULL, NULL, NULL);
    xfree (version);
    return 0;
}
