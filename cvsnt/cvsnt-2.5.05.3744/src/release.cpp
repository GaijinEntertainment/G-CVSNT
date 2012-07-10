/*
 * Release: "cancel" a checkout in the history log.
 * 
 * - Enter a line in the history log indicating the "release". - If asked to,
 * delete the local working directory.
 */

#include "cvs.h"
#include "savecwd.h"
#include "getline.h"

static int modified_files;

static const char *const release_usage[] =
{
    "Usage: %s %s [-d [-f]] [-e] [-y] directories...\n",
    "\t-d\tDelete the given directory.\n",
    "\t-f\tDelete contents of directories including non-cvs files.\n",
    "\t-e\tDelete CVS control files in the given directory (export).\n",
    "\t-y\tAssume answer yes to all questions.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

static short force_delete = 0;

#ifdef SERVER_SUPPORT
/* This is the server side of cvs release.  */
static int release_server (int argc, char **argv)
{
    int i;

    /* Note that we skip argv[0].  */
    for (i = 1; i < argc; ++i)
	history_write ('F', argv[i], "", argv[i], "", NULL, NULL);
    return 0;
}

#endif /* SERVER_SUPPORT */

/*ARGSUSED*/
static int release_fileproc (void *callerdat, struct file_info *finfo)
{
	int status, q;

	q=really_quiet;
	really_quiet = 1;
    status = Classify_File (finfo, (char *) NULL, (char *) NULL, (char *) NULL,
			    1, 0, NULL, 0, 0, 0);
	really_quiet = q;

    switch (status)
    {
	case T_NEEDS_MERGE:
	case T_CONFLICT:
	case T_MODIFIED:
	case T_ADDED:
	case T_REMOVED:
		modified_files++;
		break;
	case T_CHECKOUT:
	case T_REMOVE_ENTRY:
	case T_UNKNOWN: 
	case T_UPTODATE:
	case T_PATCH:
		break;
	}

    return 0;
}

/*ARGSUSED*/
static int release_delete_fileproc (void *callerdat, struct file_info *finfo)
{
	unlink_file(finfo->file);
	return 0;
}

/* ARGSUSED */
static int
release_delete_dirleaveproc(void *callerdat, char *dir, int err, char *update_dir, List *entries)
{
    if (strchr (dir, '/') == NULL)
    {
		/* FIXME: chdir ("..") loses with symlinks.  */
		/* Prune empty dirs on the way out - if necessary */
		unlink_file_dir(CVSADM);
		(void) CVS_CHDIR ("..");
		if (force_delete || isemptydir (dir, 0))
		{
			List *list;
			/* I'm not sure the existence_error is actually possible (except
			in cases where we really should print a message), but since
			this code used to ignore all errors, I'll play it safe.	*/
			if (unlink_file_dir (dir) < 0 && !existence_error (errno))
				error (0, errno, "cannot remove %s directory", dir);
			if(isdir(CVSADM))
			{
				list = Entries_Open(0,NULL);
				Subdir_Deregister (list, (char *) NULL, dir);
				Entries_Close(list);
			}
		}
    }

    return (err);
}


static int release_export_dirleaveproc(void *callerdat, char *dir, int err,
				    char *update_dir, List *entries)
{
	unlink_file_dir(CVSADM);
	return 0;
}

int release (int argc, char **argv)
{
    int i, c;
    char *repository;
    char *thisarg;
    int arg_start_idx;
    int err = 0;
    short delete_flag = 0;
	short export_flag = 0;
	short yes_flag=0;
    struct saved_cwd cwd;

#ifdef SERVER_SUPPORT
    if (server_active)
		return release_server (argc, argv);
#endif

    /* Everything from here on is client or local.  */
    if (argc == -1)
		usage (release_usage);
    optind = 0;
    while ((c = getopt (argc, argv, "+Qdeqfy")) != -1)
    {
	switch (c)
	{
	    case 'Q':
	    case 'q':
		error (1, 0,
		       "-q or -Q must be specified before \"%s\"",
		       command_name);
		break;
	    case 'd':
		delete_flag++;
		break;
		case 'f':
		force_delete++;
		break;
		case 'e':
		export_flag++;
		break;
		case 'y':
		yes_flag=1;
		break;
	    case '?':
	    default:
		usage (release_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    /* Remember the directory where "cvs release" was invoked because
       all args are relative to this directory and we chdir around.
       */
    if (save_cwd (&cwd))
        error_exit ();

    arg_start_idx = 0;

    for (i = arg_start_idx; i < argc; i++)
    {
		thisarg = argv[i];

        if (isdir (thisarg))
        {
			if (CVS_CHDIR (thisarg) < 0)
			{
				if (!really_quiet)
					error (0, errno, "can't chdir to: %s", thisarg);
				continue;
			}
			if (!isdir (CVSADM))
			{
				if (!really_quiet)
					error (0, 0, "no repository directory: %s", thisarg);
				if (restore_cwd (&cwd, NULL))
					error_exit ();
				continue;
		    }
		}
		else
		{
			if (!really_quiet)
				error (0, 0, "no such directory: %s", thisarg);
		    continue;
		}

		repository = Name_Repository ((char *) NULL, (char *) NULL);

		if (!really_quiet)
		{
			char *tmp;
			modified_files = 0;
			start_recursion (release_fileproc, (FILESDONEPROC) NULL,
				 (PREDIRENTPROC) NULL, (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL,
				 (void *) NULL, 0, NULL, 0, W_LOCAL,
				 0, 0, (char *) NULL, NULL, 0, NULL, NULL);

	
			tmp=(char*)xmalloc(strlen(thisarg)+1024);
			if(modified_files)
				sprintf (tmp,"You have [%d] altered files in this repository.\n", modified_files);
			else
				*tmp='\0';

			sprintf(tmp+strlen(tmp),"Are you sure you want to release %sdirectory '%s': ",
				delete_flag ? "(and delete) " : export_flag?"(and export) " : "", thisarg);

			if(!yes_flag)
				c=yesno_prompt(tmp,"Modified files",0);
			else
			{
				printf("%sy\n",tmp);
				c='y';
			}

			xfree(tmp);
			if (c!='y')			/* "No" */
			{
				(void) fprintf (stderr, "** `%s' aborted by user choice.\n",
					command_name);
				xfree (repository);
				if (restore_cwd (&cwd, NULL))
						error_exit ();
				continue;
			}
		}

		if (!(current_parsed_root->isremote
			&& (!supported_request ("noop")
				|| !supported_request ("Notify")))
	    )
		{
			/* We are chdir'ed into the directory in question.  
			So don't pass args to unedit.  */
			int argc = 1;
			char *argv[3];
			argv[0] = "dummy";
			argv[1] = NULL;
			err += unedit (argc, argv);
			/* Unedit will have killed our lockserver connection */
			if(!current_parsed_root->isremote)
				lock_register_client(CVS_Username,current_parsed_root->directory);

		}

        if (current_parsed_root->isremote)
        {
			send_to_server ("Argument ", 0);
			send_to_server (thisarg, 0);
			send_to_server ("\n", 1);
			send_to_server ("release\n", 0);
		}
        else
        {
		    history_write ('F', thisarg, "", thisarg, "", NULL, NULL); /* F == Free */
        }

        xfree (repository);

		if (restore_cwd (&cwd, NULL))
		    error_exit ();

		if(!noexec)
		{
			if(delete_flag)
			{
				start_recursion (release_delete_fileproc, (FILESDONEPROC) NULL,
						(PREDIRENTPROC) NULL, (DIRENTPROC) NULL, release_delete_dirleaveproc,
						(void *) NULL, 1, &thisarg, 0, W_LOCAL,
						0, 0, (char *) NULL, NULL, 0, NULL, NULL);
			}
			else if(export_flag)
			{
				start_recursion (NULL, (FILESDONEPROC) NULL,
						(PREDIRENTPROC) NULL, (DIRENTPROC) NULL, release_export_dirleaveproc,
						(void *) NULL, 1, &thisarg, 0, W_LOCAL,
						0, 0, (char *) NULL, NULL, 0, NULL, NULL);
			}
		}

        if (current_parsed_root->isremote)
		    err += get_server_responses ();
    }

    if (restore_cwd (&cwd, NULL))
		error_exit ();
    free_cwd (&cwd);

    if (current_parsed_root->isremote)
    {
	/* Unfortunately, client.c doesn't offer a way to close
	   the connection without waiting for responses.  The extra
	   network turnaround here is quite unnecessary other than
	   that....  */
		send_to_server ("noop\n", 0);
		err += get_responses_and_close ();
    }

    return err;
}
