/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * No Difference
 * 
 * The user file looks modified judging from its time stamp; however it needn't
 * be.  No_difference() finds out whether it is or not. If it is not, it
 * updates the administration.
 * 
 * returns 0 if no differences are found and non-zero otherwise
 */

#include "cvs.h"

int No_Difference (struct file_info *finfo, Vers_TS *vers, int force_nodiff, int ignore_keywords)
{
    Node *p;
    int ret;
    char *ts, *options;
    int retcode = 0;

	if(!force_nodiff)
	{
		/* If ts_user is "Is-modified", we can only conclude the files are
		different (since we don't have the file's contents).  */
		if (vers->ts_user != NULL && strcmp (vers->ts_user, "Is-modified") == 0)
			return -1;

		if (!vers->srcfile || !vers->srcfile->path)
			return -1;			/* different since we couldn't tell */

		if (vers->entdata && vers->entdata->options)
			options = xstrdup (vers->entdata->options);
		else
			options = xstrdup ("");

		TRACE(3,"RCS_cmp_file() called in No_Difference()");
		retcode = RCS_cmp_file (vers->srcfile, vers->vn_user, options,
					finfo->file, ignore_keywords);
	}
	else
	{
		if (vers->entdata && vers->entdata->options)
			options = xstrdup (vers->entdata->options);
		else
			options = xstrdup ("");
		retcode = 0;
	}
    if (retcode == 0)
    {
	/* no difference was found, so fix the entries file */
	ts = time_stamp (finfo->file, 0);
	Register (finfo->entries, finfo->file,
		  vers->vn_user ? vers->vn_user : vers->vn_rcs, ts,
		  options, vers->tag, vers->date, (char *) 0, NULL, NULL, vers->tt_rcs, vers->edit_revision, vers->edit_tag, vers->edit_bugid, NULL);
#ifdef SERVER_SUPPORT
	if (server_active)
	{
	    /* We need to update the entries line on the client side.  */
	    server_update_entries
	      (finfo->file, finfo->update_dir, finfo->repository, SERVER_UPDATED);
	}
#endif
	xfree (ts);

	/* update the entdata pointer in the vers_ts structure */
	p = findnode_fn (finfo->entries, finfo->file);
	vers->entdata = (Entnode *) p->data;

	ret = 0;
    }
    else
	ret = 1;			/* files were really different */

    xfree (options);
    return (ret);
}
