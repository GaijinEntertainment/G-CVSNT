/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * chown
 * 
 * Changes the owner of a directory to the given name.
 */

#include "cvs.h"
#include "fileattr.h"

static const char *chown_user;

static const char *const chown_usage[] =
{
    "Usage: %s %s [-R] user directory...\n",
	"\t-R\tChange owner recursively\n",
    NULL
};

static const char *const rchown_usage[] =
{
    "Usage: %s %s [-R] user module...\n",
	"\t-R\tChange owner recursively\n",
    NULL
};

static Dtype chown_dirproc (void *callerdat, char *dir, char *repos, char *update_dir, List *entries, const char *virtual_repository, Dtype hint);
static int rchown_proc(int argc, char **argv, const char *xwhere,
			    const char *mwhere, const char *mfile, int shorten,
			    int local_specified, const char *mname, const char *msg);

int chowner (int argc, char **argv)
{
   int c;
   int err = 0;
   int local = 1;
	int is_rchown = !strcmp(command_name,"rchown");

   if (argc == 1 || argc == -1)
	   usage (is_rchown?rchown_usage:chown_usage);

   optind = 0;
   while ((c = getopt (argc, argv, "+R")) != -1)
   {
      switch (c)
      {
	  case 'R':
		  local = 0;
		  break;
      case '?':
      default:
		usage (chown_usage);
		break;
      }
   }
   argc -= optind;
   argv += optind;

   if (argc <= 0)
      usage (chown_usage);

   if (current_parsed_root->isremote)
   {
	   if(is_rchown)
	   {
			if (!supported_request ("rchown"))
				error (1, 0, "server does not support rchown");
	   }
	   else
	   {
			if (!supported_request ("chown"))
				error (1, 0, "server does not support chown");
	   }
		if(!local)
		{
			send_arg("-R");
		}

      send_arg("--");
	  send_arg(argv[0]);
	  argv++;
	  argc--;
		if (is_rchown)
		{
			for (int i = 0; i < argc; i++)
				send_arg (argv[i]);
			send_to_server ("rchown\n", 0);
		}
		else
		{
			send_file_names (argc, argv, SEND_EXPAND_WILD);
			send_files (argc, argv, local, 0, SEND_NO_CONTENTS);
			send_to_server ("chown\n", 0);
		}
		return get_responses_and_close ();
   }

   chown_user = argv[0];
   argv++;
   argc--;

	if(!acl_mode)
		error(1,0,"Access control is disabled on this repository.");

	if (is_rchown)
	{
		DBM *db;
		int i;
		db = open_module ();
		for (i = 0; i < argc; i++)
		{
			err += do_module (db, argv[i], MISC, "Changing", rchown_proc,
					(char *) NULL, 0, local, 0, 0, (char *) NULL);
		}
		close_module (db);
	}
	else
	{
		/* start the recursion processor */
		err = start_recursion ((FILEPROC) NULL, (FILESDONEPROC) NULL,
					(PREDIRENTPROC) NULL, chown_dirproc, (DIRLEAVEPROC) NULL, NULL,
					argc, argv, local,
					W_LOCAL, 0, 1, (char *) NULL, NULL, 1, verify_control, NULL);
	}
   return (err);
}

static Dtype chown_dirproc (void *callerdat, char *dir, char *repos, char *update_dir, List *entries, const char *virtual_repository, Dtype hint)
{
	if(hint!=R_PROCESS)
		return hint;

	if(!quiet)
		printf("Changing owner for directory %s\n",update_dir);
	change_owner(chown_user);
	return R_PROCESS;
}

static int rchown_proc(int argc, char **argv, const char *xwhere,
			    const char *mwhere, const char *mfile, int shorten,
			    int local_specified, const char *mname, const char *msg)
{
    /* Begin section which is identical to patch_proc--should this
       be abstracted out somehow?  */
    char *myargv[2];
    int err = 0;
    char *repository, *mapped_repository;
    char *where;

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

	mapped_repository = map_repository(repository);

	/* cd to the starting repository */
	if ( CVS_CHDIR (mapped_repository) < 0)
	{
	    error (0, errno, "cannot chdir to %s", fn_root(repository));
	    xfree (repository);
	    xfree (mapped_repository);
	    return (1);
	}
	xfree (repository);
	/* End section which is identical to patch_proc.  */

    err = start_recursion (NULL, (FILESDONEPROC) NULL, (PREDIRENTPROC) NULL, chown_dirproc,
			   (DIRLEAVEPROC) NULL, NULL,
			   argc - 1, argv + 1, local_specified, W_REPOS, 0, 1,
			   where, mapped_repository, 1, verify_control, NULL);

	xfree (mapped_repository);
    return err;
}
