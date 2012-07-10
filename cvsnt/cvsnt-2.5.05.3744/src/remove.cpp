/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Remove a File
 * 
 * Removes entries from the present version. The entries will be removed from
 * the RCS repository upon the next "commit".
 * 
 * "remove" accepts no options, only file names that are to be removed.  The
 * file must not exist in the current directory for "remove" to work
 * correctly.
 */

#include "cvs.h"

static int remove_force_fileproc (void *callerdat,
					 struct file_info *finfo);
static int remove_fileproc (void *callerdat, struct file_info *finfo);
static Dtype remove_dirproc (void *callerdat, char *dir,
				    char *repos, char *update_dir,
				    List *entries, const char *virtual_repository, Dtype hint);

static int force;
static int local;
static int removed_files;
static int bad_files;
static int existing_files;

static const char *const remove_usage[] =
{
    "Usage: %s %s [-flR] [files...]\n",
    "\t-f\tDelete the file before removing it.\n",
    "\t-l\tProcess this directory only (not recursive).\n",
    "\t-R\tProcess directories recursively.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

int cvsremove (int argc, char **argv)
{
    int c, err;

    if (argc == -1)
	usage (remove_usage);

    optind = 0;
    while ((c = getopt (argc, argv, "+flR")) != -1)
    {
	switch (c)
	{
	    case 'f':
		force = 1;
		break;
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case '?':
	    default:
		usage (remove_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    if (current_parsed_root->isremote) {
	/* Call expand_wild so that the local removal of files will
           work.  It's ok to do it always because we have to send the
           file names expanded anyway.  */
	expand_wild (argc, argv, &argc, &argv);
	
	if (force)
	{
	    if (!noexec)
	    {
		start_recursion (remove_force_fileproc, (FILESDONEPROC) NULL,
				 (PREDIRENTPROC) NULL, (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL,
				 (void *) NULL, argc, argv, local, W_LOCAL,
				 0, 0, (char *) NULL, NULL, 0, NULL, NULL);
	    }
	    /* else FIXME should probably act as if the file doesn't exist
	       in doing the following checks.  */
	}

	if (local)
	    send_arg("-l");
	send_arg("--");
	/* FIXME: Can't we set SEND_NO_CONTENTS here?  Needs investigation.  */
	send_files (argc, argv, local, 0, 0);
	send_file_names (argc, argv, 0);
	free_names (&argc, argv);
	send_to_server ("remove\n", 0);
        return get_responses_and_close ();
    }

    /* start the recursion processor */
    err = start_recursion (remove_fileproc, (FILESDONEPROC) NULL,
                           (PREDIRENTPROC) NULL, remove_dirproc, (DIRLEAVEPROC) NULL, NULL,
			   argc, argv,
                           local, W_LOCAL, 0, 1, (char *) NULL, NULL, 1,
			   verify_create, NULL);

    if (removed_files && !really_quiet)
		error (0, 0, "use '%s commit' to remove %s permanently", program_name,
	       (removed_files == 1) ? "this file" : "these files");

    if (existing_files)
	{
		error (0, 0,
	       ((existing_files == 1) ? "%d file exists; remove it first" :	"%d files exist; remove them first"),
	       existing_files);
		err=1;
	}

	if(bad_files)
		err=1;

    return (err);
}

/*
 * This is called via start_recursion if we are running as the client
 * and the -f option was used.  We just physically remove the file.
 */

/*ARGSUSED*/
static int remove_force_fileproc (void *callerdat, struct file_info *finfo)
{
    if (CVS_UNLINK (finfo->file) < 0 && ! existence_error (errno))
	error (0, errno, "unable to remove %s", fn_root(finfo->fullname));
    return 0;
}

/*
 * remove the file, only if it has already been physically removed
 */
/* ARGSUSED */
static int remove_fileproc (void *callerdat, struct file_info *finfo)
{
    Vers_TS *vers;

    if (force)
    {
	if (!noexec)
	{
	    if ( CVS_UNLINK (finfo->file) < 0 && ! existence_error (errno))
	    {
		error (0, errno, "unable to remove %s", fn_root(finfo->fullname));
	    }
	}
	/* else FIXME should probably act as if the file doesn't exist
	   in doing the following checks.  */
    }

    vers = Version_TS (finfo, NULL, NULL, NULL, 0, 0, 0);

    if (vers->ts_user && (!vers->vn_user || strcmp(vers->vn_user,"0")))
    {
	existing_files++;
	if (!quiet)
	    error (0, 0, "file `%s' still in working directory",
		   fn_root(finfo->fullname));
    }
    else if (vers->vn_user == NULL)
    {
	if (!quiet)
	    error (0, 0, "nothing known about `%s'", fn_root(finfo->fullname));
    }
    else if (vers->vn_user[0] == '0' && vers->vn_user[1] == '\0')
    {
	char *fname;

	/*
	 * It's a file that has been added, but not commited yet. So,
	 * remove the ,t file for it and scratch it from the
	 * entries file.  */
	Scratch_Entry (finfo->entries, finfo->file);
	fname = (char*)xmalloc (strlen (finfo->file)
			 + sizeof (CVSADM)
			 + sizeof (CVSEXT_LOG)
			 + 10);
	(void) sprintf (fname, "%s/%s%s", CVSADM, finfo->file, CVSEXT_LOG);
	if (unlink_file (fname) < 0
	    && !existence_error (errno))
	    error (0, errno, "cannot remove %s", CVSEXT_LOG);
	if (!quiet)
	    error (0, 0, "removed `%s'", fn_root(finfo->fullname));

#ifdef SERVER_SUPPORT
	if (server_active)
	    server_checked_in (finfo->file, finfo->update_dir, finfo->repository);
#endif
	xfree (fname);
    }
    else if (vers->vn_user[0] == '-')
    {
	if (!quiet)
	    error (0, 0, "file `%s' already scheduled for removal",
		   fn_root(finfo->fullname));
    }
    else if (vers->tag != NULL && ((isdigit ((unsigned char) *vers->tag)) || !RCS_isbranch(finfo->rcs, vers->tag)))
    {
	/* Commit will just give an error, and so there seems to be
	   little reason to allow the remove.  I mean, conflicts that
	   arise out of parallel development are one thing, but conflicts
	   that arise from sticky tags are quite another.  */
	error (0, 0, "\
cannot remove file `%s' which has a sticky tag of `%s'",
	       fn_root(finfo->fullname), vers->tag);
	bad_files++;
    }
    else
    {
	char *fname;

	/* Re-register it with a negative version number.  */
	fname = (char*)xmalloc (strlen (vers->vn_user) + 5);
	(void) strcpy (fname, "-");
	(void) strcat (fname, vers->vn_user);
	Register (finfo->entries, finfo->file, fname, vers->ts_rcs, vers->options,
		  vers->tag, vers->date, vers->ts_conflict, NULL, NULL, vers->tt_rcs, vers->edit_revision, vers->edit_tag, vers->edit_bugid, NULL);
	if (!quiet)
	    error (0, 0, "scheduling `%s' for removal", fn_root(finfo->fullname));
	removed_files++;

#ifdef SERVER_SUPPORT
	if (server_active)
	    server_checked_in (finfo->file, finfo->update_dir, finfo->repository);
#endif
	xfree (fname);
    }

    freevers_ts (&vers);
    return (0);
}

/*
 * Print a warm fuzzy message
 */
/* ARGSUSED */
static Dtype remove_dirproc (void *callerdat, char *dir, char *repos, char *update_dir, List *entries, const char *virtual_repository, Dtype hint)
{
	if(hint!=R_PROCESS)
		return hint;

    if (!quiet)
	error (0, 0, "Removing %s", update_dir);
    return (R_PROCESS);
}
