/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 */

#include "cvs.h"
#include "getline.h"

/* Determine the name of the RCS repository for directory DIR in the
   current working directory, or for the current working directory
   itself if DIR is NULL.  Returns the name in a newly-malloc'd
   string.  On error, gives a fatal error and does not return.
   UPDATE_DIR is the path from where cvs was invoked (for use in error
   messages), and should contain DIR as its last component.
   UPDATE_DIR can be NULL to signify the directory in which cvs was
   invoked.  */

char *Name_Repository (const char *dir, const char *update_dir)
{
    FILE *fpin;
    const char *xupdate_dir;
    char *repos = NULL;
    size_t repos_allocated = 0;
    char *tmp=NULL;
    char *cp;

    TRACE(3,"Name_Repository(%s,%s)",PATCH_NULL(dir),PATCH_NULL(update_dir));
    if (update_dir && *update_dir)
		xupdate_dir = update_dir;
    else
		xupdate_dir = ".";

    if (dir != NULL)
    {
		size_t tmpsz=strlen (dir) + sizeof (CVSADM_REP) + 10;
		tmp = (char*)xmalloc (tmpsz+sizeof(CVSADM_VIRTREPOS));
		sprintf (tmp, "%s/%s", dir, CVSADM_VIRTREPOS);
		size_t tmplen1 = strlen(tmp);
		if(!isfile(tmp))
			sprintf(tmp, "%s/%s", dir, CVSADM_REP);
		size_t tmplen2 = strlen(tmp);
	    TRACE(3,"Name_Repository allocate tmp of size %d, len1=%d, len2=%d",(int)tmpsz,(int)tmplen1,(int)tmplen2);
    }
    else
	{
		tmp = xstrdup (CVSADM_VIRTREPOS);
	    TRACE(3,"Name_Repository dup tmp is len",(int)strlen(tmp));
		if(!isfile(tmp))
		{
			xfree(tmp);
			tmp=NULL;
			tmp=xstrdup(CVSADM_REP);
		    TRACE(3,"Name_Repository dup tmp is now len",(int)strlen(tmp));
		}
	}
    /*
     * The assumption here is that the repository is always contained in the
     * first line of the "Repository" file.
     */
    TRACE(3,"Name_Repository open %s",tmp);
    fpin = CVS_FOPEN (tmp, "r");

    if (fpin == NULL)
    {
	int save_errno = errno;
	char *cvsadm=NULL;

	if (dir != NULL)
	{
	    cvsadm = (char*)xmalloc (strlen (dir) + sizeof (CVSADM) + 100);
	    (void) sprintf (cvsadm, "%s/%s", dir, CVSADM);
	}
	else
	    cvsadm = xstrdup (CVSADM);
    TRACE(3,"Name_Repository failed to open %s so try %s",tmp,cvsadm);
	if (!isdir (cvsadm))
	{
	    error (0, 0, "in directory %s:", xupdate_dir);
	    error (1, 0, "there is no version here; do '%s checkout' first",
		   program_name);
	}
	xfree (cvsadm);
	cvsadm=NULL;

	if (existence_error (save_errno))
	{
	    /* FIXME: This is a very poorly worded error message.  It
	       occurs at least in the case where the user manually
	       creates a directory named CVS, so the error message
	       should be more along the lines of "CVS directory found
	       without administrative files; use CVS to create the CVS
	       directory, or rename it to something else if the
	       intention is to store something besides CVS
	       administrative files".  */
	    error (0, 0, "in directory %s:", xupdate_dir);
	    error (1, 0, "CVS directory without administration files present.  Cannot continue until this directory is deleted or renamed.");
	}

	error (1, save_errno, "cannot open %s", tmp);
    }

    TRACE(3,"Name_Repository opened %s ok so read a line",tmp);
    if (getline (&repos, &repos_allocated, fpin) < 0)
    {
	/* FIXME: should be checking for end of file separately.  */
	error (0, 0, "in directory %s:", xupdate_dir);
	error (1, errno, "cannot read %s", tmp);
    }
    if (fclose (fpin) < 0)
	error (0, errno, "cannot close %s", tmp);
    TRACE(3,"Name_Repository closed %s",tmp);
    xfree (tmp);
	tmp=NULL;

    TRACE(3,"Name_Repository read 1 %s",repos);
    if ((cp = strrchr (repos, '\n')) != NULL)
	*cp = '\0';			/* strip the newline */
    TRACE(3,"Name_Repository read 2 %s",repos);

    /*
     * If this is a relative repository pathname, turn it into an absolute
     * one by tacking on the CVSROOT environment variable. If the CVSROOT
     * environment variable is not set, die now.
     */
    TRACE(3,"Name_Repository isabsolute( %s )?",repos);
	char *newrepos=NULL;
    if (! isabsolute(repos))
    {

    TRACE(3,"Name_Repository isabsolute( %s )!",repos);
	if (current_parsed_root == NULL)
	{
	    error (0, 0, "in directory %s:", xupdate_dir);
	    error (0, 0, "must set the CVSROOT environment variable\n");
	    error (0, 0, "or specify the '-d' option to %s.", program_name);
	    error (1, 0, "illegal repository setting");
	}
	if (pathname_levels (repos) > 0)
	{
	    error (0, 0, "in directory %s:", xupdate_dir);
	    error (0, 0, "`..'-relative repositories are not supported.");
	    error (1, 0, "illegal source repository");
	}
	newrepos = (char*)xmalloc (strlen (current_parsed_root->directory) + strlen (repos) + 2);
	(void) sprintf (newrepos, "%s/%s", current_parsed_root->directory, repos);
#ifndef HAVE_GETLINE
	xfree (repos);
#else
	free(repos);
#endif
	repos = newrepos;
    }
	else
	{
    TRACE(3,"Name_Repository not isabsolute( %s )",repos);
	newrepos = (char*)xmalloc (strlen (repos) + 2);
	(void) sprintf (newrepos, "%s", repos);
#ifndef HAVE_GETLINE
	xfree (repos);
#else
	free(repos);
#endif
	repos = newrepos;
	}

    TRACE(3,"Name_Repository Sanitize_Repository_Name( %s )!",repos);
	Sanitize_Repository_Name (&repos);

    TRACE(3,"Name_Repository return ( %s )!",repos);
    return repos;
}

/*
 * Return a pointer to the repository name relative to CVSROOT from a
 * possibly fully qualified repository
 */
const char *Short_Repository (const char *repository)
{
    if (repository == NULL)
	return (NULL);

    /* If repository matches CVSroot at the beginning, strip off CVSroot */
    /* And skip leading '/' in rep, in case CVSroot ended with '/'. */
    if (fnncmp (current_parsed_root->directory, repository,
		 strlen (current_parsed_root->directory)) == 0)
    {
	const char *rep = repository + strlen (current_parsed_root->directory);
	return (ISDIRSEP(*rep)) ? rep+1 : rep;
    }
    else
	return (repository);
}

/* Sanitize the repository name (in place) by removing trailing
 * slashes and a trailing "." if present.  It should be safe for
 * callers to use strcat and friends to create repository names.
 * Without this check, names like "/path/to/repos/./foo" and
 * "/path/to/repos//foo" would be created.  For example, one
 * significant case is the CVSROOT-detection code in commit.c.  It
 * decides whether or not it needs to rebuild the administrative file
 * database by doing a string compare.  If we've done a `cvs co .' to
 * get the CVSROOT files, "/path/to/repos/./CVSROOT" and
 * "/path/to/repos/CVSROOT" are the arguments that are compared!
 *
 * This function ends up being called from the same places as
 * strip_path, though what it does is much more conservative.  Many
 * comments about this operation (which was scattered around in
 * several places in the source code) ran thus:
 *
 *    ``repository ends with "/."; omit it.  This sort of thing used
 *    to be taken care of by strip_path.  Now we try to be more
 *    selective.  I suspect that it would be even better to push it
 *    back further someday, so that the trailing "/." doesn't get into
 *    repository in the first place, but we haven't taken things that
 *    far yet.''        --Jim Kingdon (recurse.c, 07-Sep-97)
 *
 * Ahh, all too true.  The major consideration is RELATIVE_REPOS.  If
 * the "/." doesn't end up in the repository while RELATIVE_REPOS is
 * defined, there will be nothing in the CVS/Repository file.  I
 * haven't verified that the remote protocol will handle that
 * correctly yet, so I've not made that change. */

void Sanitize_Repository_Name (char **repository)
{
    size_t len;

    assert (repository && *repository);

    strip_trailing_slashes (*repository);

    len = strlen (*repository);
    if (len >= 2
	&& (*repository)[len - 1] == '.'
	&& ISDIRSEP ((*repository)[len - 2]))
    {
	(*repository)[len - 2] = '\0';
    }

	/* If we're proxying, makes sure we always use the alias name to communicate */
	if(proxy_active && strlen(*repository)>strlen(current_parsed_root->original) &&
		!strncmp(*repository,current_parsed_root->original,strlen(current_parsed_root->original)) &&
		ISDIRSEP((*repository)[strlen(current_parsed_root->original)]))
	{
		char *newrep = (char*)xmalloc(strlen(current_parsed_root->directory) + strlen(*repository));
		sprintf(newrep,"%s%s",current_parsed_root->directory,(*repository)+strlen(current_parsed_root->original));
		xfree(*repository);
		*repository = newrep;
	}
}
