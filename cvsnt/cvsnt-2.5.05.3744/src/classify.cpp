/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 */

#include "cvs.h"

static void sticky_ck (struct file_info *finfo, int aflag,
			      Vers_TS * vers);

bool options_same(const char *opt1, const char *opt2)
{
	if(!opt1) opt1 = "";
	if(!opt2) opt2 = "";

	if(!strcmp(opt1,"kv")) opt1="";
	if(!strcmp(opt2,"kv")) opt2="";

	return strcmp(opt1,opt2)?false:true;
}

/*
 * Classify the state of a file
 */
Ctype Classify_File (struct file_info *finfo, const char *tag, const char *date,
    const char *options, int force_tag_match, int aflag, Vers_TS **versp,
    int pipeout, int force_time_mismatch, int ignore_keywords)
    /* Keyword expansion options can be either NULL or "" to
       indicate none are specified here.  */
{
    Vers_TS *vers;
    Ctype ret;
	kflag kf,kf_ent;

    TRACE(3,"Classify_File (%s)",PATCH_NULL(finfo->file));
    /* get all kinds of good data about the file */
    vers = Version_TS (finfo, options, tag, date, force_tag_match, 0, 0);
    TRACE(3,"vn_rcs=%s",PATCH_NULL(vers->vn_rcs));	

	RCS_get_kflags(vers->options,false,kf);
	if(vers->entdata)
		RCS_get_kflags(vers->entdata->options,false,kf_ent);

	if(force_time_mismatch)
	{
		xfree(vers->ts_user);
		vers->ts_user=xstrdup("modified");
	}

    if (vers->vn_user == NULL)
    {
	/* No entry available, ts_rcs is invalid */
	if (vers->vn_rcs == NULL)
	{
	    /* there is no RCS file either */
	    if (vers->ts_user == NULL)
	    {
		/* there is no user file */
		/* FIXME: Why do we skip this message if vers->tag or
		   vers->date is set?  It causes "cvs update -r tag98 foo"
		   to silently do nothing, which is seriously confusing
		   behavior.  "cvs update foo" gives this message, which
		   is what I would expect.  */
		if (!force_tag_match || !(vers->tag || vers->date))
		{
		    if (!really_quiet)
				error (0, 0, "nothing known about %s", fn_root(finfo->fullname));
		}
		ret = T_UNKNOWN;
	    }
	    else
	    {
		/* there is a user file */
		/* FIXME: Why do we skip this message if vers->tag or
		   vers->date is set?  It causes "cvs update -r tag98 foo"
		   to silently do nothing, which is seriously confusing
		   behavior.  "cvs update foo" gives this message, which
		   is what I would expect.  */
		if (!force_tag_match || !(vers->tag || vers->date))
		    if (!really_quiet)
			error (0, 0, "use `%s add' to create an entry for %s",
			       program_name, fn_root(finfo->fullname));
		ret = T_UNKNOWN;
	    }
	}
	else if (RCS_isdead (vers->srcfile, vers->vn_rcs))
	{
	    /* there is an RCS file, but it's dead */
	    if (vers->ts_user == NULL)
		ret = T_UPTODATE;
	    else
	    {
		error (0, 0, "use `%s add' to create an entry for %s",
		       program_name, fn_root(finfo->fullname));
		ret = T_UNKNOWN;
	    }
	}
	else if (vers->ts_user && !strcmp(vers->ts_user,"0") && !strcmp(finfo->file,RCSREPOVERSION))
	{
		/* It's a directory entry, but there isn't one in the repository */
		ret = T_UNKNOWN;
	}
	else if (!pipeout && vers->ts_user && No_Difference(finfo, vers, (kf.flags&KFLAG_STATIC || kf_ent.flags&KFLAG_STATIC), ignore_keywords))
	{
	    /* the files were different so it is a conflict */
	    if (!really_quiet)
		error (0, 0, "move away %s; it is in the way",
		       fn_root(finfo->fullname));
	    ret = T_CONFLICT;
	}
	else
	{
	    /* no user file or no difference, just checkout */
	    ret = T_CHECKOUT;
	}
    }
    else if (strcmp (vers->vn_user, "0") == 0)
    {
	/* An entry for a new-born file; ts_rcs is dummy */

	if (vers->ts_user == NULL)
	{
	    /*
	     * There is no user file, but there should be one; remove the
	     * entry
	     */
		if(!really_quiet)
			error (0, 0, "warning: new-born %s has disappeared", fn_root(finfo->fullname));
		ret = T_REMOVE_ENTRY;
	}
	else if (vers->vn_rcs == NULL ||
		 RCS_isdead (vers->srcfile, vers->vn_rcs))
	    /* No RCS file or RCS file revision is dead  */
	    ret = T_ADDED;
	else
	{
	    if (vers->srcfile->flags & VALID)
	    {
		/* This file has been added on some branch other than
		   the one we are looking at.  In the branch we are
		   looking at, the file was already valid.  */
		if (!really_quiet)
		    error (0, 0,
			   "conflict: %s has been added, but already exists",
			   fn_root(finfo->fullname));
	    }
	    else
	    {
		/*
		 * There is an RCS file, so someone else must have checked
		 * one in behind our back; conflict
		 */
		if (!really_quiet)
		    error (0, 0,
			   "conflict: %s created independently by second party",
			   fn_root(finfo->fullname));
	    }
	    ret = T_CONFLICT;
	}
    }
    else if (vers->vn_user[0] == '-')
    {
	/* An entry for a removed file, ts_rcs is invalid */

	if (vers->ts_user == NULL)
	{
	    /* There is no user file (as it should be) */

	    if (vers->vn_rcs == NULL
		|| RCS_isdead (vers->srcfile, vers->vn_rcs))
	    {

		/*
		 * There is no RCS file; this is all-right, but it has been
		 * removed independently by a second party; remove the entry
		 */
		ret = T_REMOVE_ENTRY;
	    }
	    else if (strcmp (vers->vn_rcs, vers->vn_user + 1) == 0)
		/*
		 * The RCS file is the same version as the user file was, and
		 * that's OK; remove it
		 */
		ret = T_REMOVED;
	    else if (pipeout)
		/*
		 * The RCS file doesn't match the user's file, but it doesn't
		 * matter in this case
		 */
		ret = T_NEEDS_MERGE;
	    else
	    {

		/*
		 * The RCS file is a newer version than the removed user file
		 * and this is definitely not OK; make it a conflict.
		 */
		if (!really_quiet)
		    error (0, 0,
			   "conflict: removed %s was modified by second party",
			   fn_root(finfo->fullname));
		ret = T_CONFLICT;
	    }
	}
	else
	{
	    /* The user file shouldn't be there */
	    if (!really_quiet)
		error (0, 0, "%s should be removed and is still there",
		       fn_root(finfo->fullname));
	    ret = T_REMOVED;
	}
    }
    else
    {
	/* A normal entry, TS_Rcs is valid */
	if (vers->vn_rcs == NULL || RCS_isdead (vers->srcfile, vers->vn_rcs))
	{
	    /* There is no RCS file */

	    if (vers->ts_user == NULL)
	    {
		/* There is no user file, so just remove the entry */
		if (!really_quiet)
		    error (0, 0, "warning: %s is not (any longer) pertinent",
			   fn_root(finfo->fullname));
		ret = T_REMOVE_ENTRY;
	    }
	    else if (!strcmp (vers->ts_user, vers->ts_rcs))
	    {
		/*
		 * The user file is still unmodified, so just remove it from
		 * the entry list
		 */
		if (!really_quiet)
		    error (0, 0, "%s is no longer in the repository",
			   fn_root(finfo->fullname));
		ret = T_REMOVE_ENTRY;
	    }
		else if (No_Difference (finfo, vers, (kf.flags&KFLAG_STATIC || kf_ent.flags&KFLAG_STATIC), ignore_keywords))
	    {
		/* they are different -> conflict */
		if (!really_quiet)
		    error (0, 0,
	       "conflict: %s is modified but no longer in the repository",
			   fn_root(finfo->fullname));
		ret = T_CONFLICT;
	    }
	    else
	    {
		/* they weren't really different */
		if (!really_quiet)
		    error (0, 0,
			   "warning: %s is not (any longer) pertinent",
			   fn_root(finfo->fullname));
		ret = T_REMOVE_ENTRY;
	    }
	}
	else if (strcmp (vers->vn_rcs, vers->vn_user) == 0)
	{
	    /* The RCS file is the same version as the user file */

	    if (vers->ts_user == NULL)
	    {
		/*
		 * There is no user file, so note that it was lost and
		 * extract a new version
		 */
		/* Comparing the command_name against "update", in
		   addition to being an ugly way to operate, means
		   that this message does not get printed by the
		   server.  That might be considered just a straight
		   bug, although there is one subtlety: that case also
		   gets hit when a patch fails and the client fetches
		   a file.  I'm not sure there is currently any way
		   for the server to distinguish those two cases.  */
		if (strcmp (command_name, "update") == 0)
		    if (!really_quiet)
			error (0, 0, "warning: %s was lost", fn_root(finfo->fullname));
		ret = T_CHECKOUT;
	    }
		else if (!strcmp(vers->ts_user,"0") && !strcmp(finfo->file,RCSREPOVERSION))
		{
			/* This is a directory... The only important entry is the version*/
			ret = T_UPTODATE;
		}
	    else if (strcmp (vers->ts_user, vers->ts_rcs) == 0)
	    {
			/* Check for expansion option changes */
			if (options && !options_same(vers->entdata->options,vers->options))
				ret = T_CHECKOUT;
			else
			{
				if(aflag && !options_same(vers->entdata->options, vers->options))
					ret = T_CHECKOUT;
				else
					ret = T_UPTODATE;
				/* Changing the expansion option now requires a commit -f, which makes
				   sense (since you might have a sandbox specially checked out as -kb for example) */
				sticky_ck (finfo, aflag, vers);
			}
	    }
	    else if (vers->entdata->merge_from_tag_1[0]) 
		{
			ret = T_MODIFIED;
		}
		else if (No_Difference (finfo, vers, (kf.flags&KFLAG_STATIC || kf_ent.flags&KFLAG_STATIC), ignore_keywords))
	    {
			ret = T_MODIFIED;
			sticky_ck (finfo, aflag, vers);
	    }
	    else if (!options_same(vers->entdata->options,vers->options))
	    {
			/* file has not changed; check out if -k changed */
			ret = T_CHECKOUT;
	    }
	    else
	    {

		/*
		 * else -> note that No_Difference will Register the
		 * file already for us, using the new tag/date. This
		 * is the desired behaviour
		 */
		ret = T_UPTODATE;
	    }
	}
	else
	{
	    /* The RCS file is a newer version than the user file */

	    if (vers->ts_user == NULL)
	    {
		/* There is no user file, so just get it */

		/* See comment at other "update" compare, for more
		   thoughts on this comparison.  */
		if (strcmp (command_name, "update") == 0)
		    if (!really_quiet)
			error (0, 0, "warning: %s was lost", fn_root(finfo->fullname));
		ret = T_CHECKOUT;
	    }
	    else if (!strcmp (vers->ts_user, vers->ts_rcs))
	    {

		/*
		 * The user file is still unmodified, so just get it as well
		 */
		if (!options_same(vers->entdata->options,vers->options))
		    ret = T_CHECKOUT;
		else
		    ret = T_PATCH;
	    }
		else if (No_Difference (finfo, vers, (kf.flags&KFLAG_STATIC || kf_ent.flags&KFLAG_STATIC), ignore_keywords))
			/* really modified, needs to merge */
			ret = T_NEEDS_MERGE;
	    else if(!options_same(vers->entdata->options,vers->options))
			/* not really modified, check it out */
			ret = T_CHECKOUT;
	    else
			ret = T_PATCH;
	}
    }

    /* free up the vers struct, or just return it */
    if (versp != (Vers_TS **) NULL)
	*versp = vers;
    else
	freevers_ts (&vers);

    /* return the status of the file */
    return (ret);
}

static void sticky_ck (struct file_info *finfo, int aflag, Vers_TS *vers)
{
    if (aflag || vers->tag || vers->date)
    {
	char *enttag = vers->entdata->tag;
	char *entdate = vers->entdata->date;

	if ((enttag && vers->tag && strcmp (enttag, vers->tag)) ||
	    ((enttag && !vers->tag) || (!enttag && vers->tag)) ||
	    (entdate && vers->date && strcmp (entdate, vers->date)) ||
	    ((entdate && !vers->date) || (!entdate && vers->date)))
	{
	    Register (finfo->entries, finfo->file, vers->vn_user, vers->ts_rcs,
		      vers->options, vers->tag, vers->date, vers->ts_conflict, NULL, NULL, vers->tt_rcs, vers->edit_revision, vers->edit_tag, vers->edit_bugid, NULL);

#ifdef SERVER_SUPPORT
	    if (server_active)
	    {
		/* We need to update the entries line on the client side.
		   It is possible we will later update it again via
		   server_updated or some such, but that is OK.  */
		server_update_entries
		  (finfo->file, finfo->update_dir, finfo->repository,
		   strcmp (vers->ts_rcs, vers->ts_user) == 0 ?
		   SERVER_UPDATED : SERVER_MERGED);
	    }
#endif
	}
    }
}
